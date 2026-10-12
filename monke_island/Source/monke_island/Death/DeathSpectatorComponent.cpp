// monke_island

#include "Death/DeathSpectatorComponent.h"
#include "Death/DeathRules.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Algo/Compare.h"
#include "Algo/Sort.h"
#include "Camera/CameraTypes.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "monke_island.h"

namespace
{
	/** DA_DeathRules, next to the other screens of TAPAO (cooked with /Game/MonkeIsland/UI/Tapao) */
	const TCHAR* DefaultDeathRulesPath = TEXT("/Game/MonkeIsland/UI/Tapao/DA_DeathRules.DA_DeathRules");

	const UIslanderHealthComponent* HealthOf(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	}

#if !UE_BUILD_SHIPPING
	/** Tests of T-135 (PIE solo and host + client): kills a player on the server, with the cause told by its screen */
	FAutoConsoleCommandWithWorldAndArgs GDeathKillCommand(
		TEXT("monke.Death.Kill"),
		TEXT("Kills a player (server, tests): monke.Death.Kill [index in the player list, 0 = the host] [Fall|Drowning|Flood|Coconut|Tiki|Shark]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || World->GetNetMode() == NM_Client)
			{
				UE_LOG(Logmonke_island, Warning, TEXT("monke.Death.Kill: type it in the host's window (the server decides the deaths)"));
				return;
			}
			const AGameStateBase* GameState = World->GetGameState();
			const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
			APlayerState* Victim = GameState && GameState->PlayerArray.IsValidIndex(Index) ? GameState->PlayerArray[Index].Get() : nullptr;
			APawn* Pawn = Victim ? Victim->GetPawn() : nullptr;
			UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
			if (!Health)
			{
				UE_LOG(Logmonke_island, Warning, TEXT("monke.Death.Kill: no player %d with a health"), Index);
				return;
			}
			EDeathCause Cause = EDeathCause::Unknown;
			if (Args.Num() > 1)
			{
				const int64 Value = StaticEnum<EDeathCause>()->GetValueByNameString(Args[1]);
				Cause = Value != INDEX_NONE ? static_cast<EDeathCause>(Value) : EDeathCause::Unknown;
			}
			// a pawn that cannot be damaged (god mode of the test map) stays alive: said in the log
			const float Dealt = Health->ApplyHealthDamage(Health->GetHealth() + 1.0f, nullptr, Cause);
			UE_LOG(Logmonke_island, Log, TEXT("monke.Death.Kill: %s %s (%s)"), *Victim->GetPlayerName(),
				Dealt > 0.0f ? TEXT("killed") : TEXT("cannot be damaged"), *UEnum::GetValueAsString(Cause));
		}));
#endif
}

UDeathSpectatorComponent::UDeathSpectatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	RulesAsset = TSoftObjectPtr<UDeathRules>(FSoftObjectPath(DefaultDeathRulesPath));
}

void UDeathSpectatorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only the dead player needs them (security.md §2: COND_OwnerOnly)
	DOREPLIFETIME_CONDITION(UDeathSpectatorComponent, FollowedPlayer, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDeathSpectatorComponent, ObservablePlayers, COND_OwnerOnly);
}

void UDeathSpectatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// read once while the level starts (the controller begins with it), as the other rules of the project
	if (!RulesAsset.IsNull())
	{
		Rules = RulesAsset.Get();
		if (!Rules)
		{
			Rules = LoadObject<UDeathRules>(nullptr, *RulesAsset.ToSoftObjectPath().ToString(), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (!Rules)
		{
			UE_LOG(Logmonke_island, Verbose, TEXT("Death: %s missing, the class defaults are used"), *RulesAsset.ToString());
		}
	}

	AController* Controller = Cast<AController>(GetOwner());
	if (Controller && Controller->HasAuthority())
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &UDeathSpectatorComponent::HandlePossessedPawnChanged);
		if (APawn* Pawn = Controller->GetPawn())
		{
			HandlePossessedPawnChanged(nullptr, Pawn);
		}
	}
}

void UDeathSpectatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AController* Controller = Cast<AController>(GetOwner()))
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &UDeathSpectatorComponent::HandlePossessedPawnChanged);
	}
	StopWatchingPawn();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

const UDeathRules& UDeathSpectatorComponent::GetRules() const
{
	return Rules ? *Rules : *GetDefault<UDeathRules>();
}

APlayerController* UDeathSpectatorComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

APawn* UDeathSpectatorComponent::GetFollowedPawn() const
{
	if (APawn* Pawn = FollowedPawn.Get())
	{
		return Pawn;
	}
	return FollowedPlayer ? FollowedPlayer->GetPawn() : nullptr;
}

bool UDeathSpectatorComponent::IsOwnerDead() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	const UIslanderHealthComponent* Health = Controller ? HealthOf(Controller->GetPawn()) : nullptr;
	return Health && Health->IsDead();
}

FText UDeathSpectatorComponent::DescribeOwnerDeath() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	const UIslanderHealthComponent* Health = Controller ? HealthOf(Controller->GetPawn()) : nullptr;
	if (!Health || !Health->IsDead())
	{
		return FText::GetEmpty();
	}
	return GetRules().DescribeDeath(Health->GetDeathRecord(), Controller->PlayerState);
}

int32 UDeathSpectatorComponent::PickFollowIndex(const TArray<int32>& CandidateIds, int32 CurrentId, int32 Direction)
{
	if (CandidateIds.IsEmpty())
	{
		return INDEX_NONE;
	}
	if (Direction >= 0)
	{
		// the first after the current one, else round to the first
		for (int32 Index = 0; Index < CandidateIds.Num(); ++Index)
		{
			if (CandidateIds[Index] > CurrentId)
			{
				return Index;
			}
		}
		return 0;
	}
	// the last before the current one, else round to the last (none yet: the last one)
	if (CurrentId != INDEX_NONE)
	{
		for (int32 Index = CandidateIds.Num() - 1; Index >= 0; --Index)
		{
			if (CandidateIds[Index] < CurrentId)
			{
				return Index;
			}
		}
	}
	return CandidateIds.Num() - 1;
}

EDeathScreen UDeathSpectatorComponent::DecideScreen(bool bLocalDead, bool bTeamWiped, bool bDayOver, bool bFollowing)
{
	if (!bLocalDead)
	{
		return EDeathScreen::None;
	}
	// cahier §4, §19: a dead player watches while a teammate lives; the expedition is lost only with the whole crew
	if (bTeamWiped)
	{
		return EDeathScreen::Wreck;
	}
	if (bDayOver)
	{
		return EDeathScreen::None;
	}
	return bFollowing ? EDeathScreen::Following : EDeathScreen::Dead;
}

bool UDeathSpectatorComponent::RequestCycle(int32 Direction)
{
	const APlayerController* Controller = GetPlayerController();
	if (!Controller || !Controller->IsLocalController() || Direction == 0 || !IsOwnerDead())
	{
		return false;
	}
	ServerCycle(Direction > 0 ? 1 : -1);
	return true;
}

bool UDeathSpectatorComponent::ServerCycle_Validate(int8 Direction)
{
	// our client only ever sends -1 or 1
	return Direction == -1 || Direction == 1;
}

void UDeathSpectatorComponent::ServerCycle_Implementation(int8 Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UDeathRules& R = GetRules();
	const double Now = World->GetTimeSeconds();
	// a request that comes too early (the cause to read), too fast, or when there is nothing to follow: refused quietly,
	// it only asks for a view
	if (!IsOwnerDead() || !CanFollow())
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Death: %s asks to follow a teammate while it cannot"), *GetNameSafe(GetOwner()));
		return;
	}
	if (OwnerDeathTime >= 0.0 && Now - OwnerDeathTime < R.SkipDeathScreenAfter)
	{
		return;
	}
	if (LastCycleTime >= 0.0 && Now - LastCycleTime < R.MinCycleInterval)
	{
		return;
	}
	LastCycleTime = Now;
	FollowStep(Direction);
}

void UDeathSpectatorComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	StopWatchingPawn();
	WatchedPawn = NewPawn;
	if (UIslanderHealthComponent* Health = NewPawn ? NewPawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr)
	{
		Health->OnDeath.AddUniqueDynamic(this, &UDeathSpectatorComponent::HandleOwnerDeath);
	}
	if (!NewPawn)
	{
		return;
	}
	if (IsOwnerDead())
	{
		// possessed already dead: as a death now
		HandleOwnerDeath(nullptr);
		return;
	}
	// a living body again (respawn): its own view, set by the engine on possession; nobody followed
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	OwnerDeathTime = -1.0;
	ObservablePlayers.Reset();
	StopFollowing();
}

void UDeathSpectatorComponent::StopWatchingPawn()
{
	if (UIslanderHealthComponent* Health = WatchedPawn.IsValid() ? WatchedPawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr)
	{
		Health->OnDeath.RemoveDynamic(this, &UDeathSpectatorComponent::HandleOwnerDeath);
	}
	WatchedPawn.Reset();
}

void UDeathSpectatorComponent::HandleOwnerDeath(AActor* Killer)
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	OwnerDeathTime = World->GetTimeSeconds();
	FollowedDeathTime = -1.0;
	LastCycleTime = -1.0;
	// the death screen first (the cause, whom one may follow), then a living teammate: Refresh decides
	World->GetTimerManager().SetTimer(RefreshTimer, this, &UDeathSpectatorComponent::Refresh, FMath::Max(GetRules().RefreshSeconds, 0.1f), true);
	Refresh();
}

bool UDeathSpectatorComponent::CanFollow() const
{
	// test maps without expedition, or without permadeath, give the body back: nothing to follow; once the expedition
	// is over the departure scene or NAUFRAGÉ take the screen
	const AExpeditionGameState* Expedition = AExpeditionGameState::Get(this);
	return Expedition && !Expedition->IsExpeditionOver() && (!Expedition->Rules || Expedition->Rules->bPermadeath);
}

void UDeathSpectatorComponent::GatherLivingTeammates(TArray<APlayerState*>& Out) const
{
	Out.Reset();
	const AController* Controller = Cast<AController>(GetOwner());
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!Controller || !GameState)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState || PlayerState == Controller->PlayerState || PlayerState->IsOnlyASpectator() || PlayerState->IsInactive())
		{
			continue;
		}
		const UIslanderHealthComponent* Health = HealthOf(PlayerState->GetPawn());
		if (Health && !Health->IsDead())
		{
			Out.Add(PlayerState);
		}
	}
	Algo::Sort(Out, [](const APlayerState* A, const APlayerState* B) { return A->GetPlayerId() < B->GetPlayerId(); });
}

void UDeathSpectatorComponent::Refresh()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!IsOwnerDead())
	{
		// alive again: HandlePossessedPawnChanged ended the rest
		World->GetTimerManager().ClearTimer(RefreshTimer);
		return;
	}

	// whom the dead player may follow (cahier §19: the teammates he can watch), told to him
	TArray<APlayerState*> Living;
	if (CanFollow())
	{
		GatherLivingTeammates(Living);
	}
	if (ObservablePlayers.Num() != Living.Num() || !Algo::Compare(ObservablePlayers, Living))
	{
		ObservablePlayers.Reset(Living.Num());
		for (APlayerState* PlayerState : Living)
		{
			ObservablePlayers.Add(PlayerState);
		}
	}
	if (!CanFollow())
	{
		// the expedition is over: the scenes and the end screens take over, the view stays where it is
		World->GetTimerManager().ClearTimer(RefreshTimer);
		return;
	}

	const UDeathRules& R = GetRules();
	const double Now = World->GetTimeSeconds();
	if (!FollowedPlayer)
	{
		// the death screen, then the first living teammate
		if (!Living.IsEmpty() && (OwnerDeathTime < 0.0 || Now - OwnerDeathTime >= R.DeathScreenSeconds))
		{
			FollowStep(1);
		}
		return;
	}
	APawn* Pawn = FollowedPawn.Get();
	if (!Pawn)
	{
		// he left the game (his pawn went with him): the next one at once
		FollowStep(1);
		return;
	}
	if (Living.Contains(FollowedPlayer.Get()) && FollowedPlayer->GetPawn() == Pawn)
	{
		FollowedDeathTime = -1.0;
		return;
	}
	// he died: a moment on him, then the next one
	if (FollowedDeathTime < 0.0)
	{
		FollowedDeathTime = Now;
	}
	if (Now - FollowedDeathTime >= R.FollowedDeathSeconds)
	{
		FollowStep(1);
	}
}

void UDeathSpectatorComponent::FollowStep(int32 Direction)
{
	TArray<APlayerState*> Living;
	GatherLivingTeammates(Living);
	TArray<int32> Ids;
	Ids.Reserve(Living.Num());
	for (const APlayerState* PlayerState : Living)
	{
		Ids.Add(PlayerState->GetPlayerId());
	}
	const int32 Index = PickFollowIndex(Ids, FollowedPlayer ? FollowedPlayer->GetPlayerId() : INDEX_NONE, Direction);
	if (Index == INDEX_NONE)
	{
		// nobody left alive: the view stays on the last one (the crew is lost, NAUFRAGÉ follows)
		StopFollowing();
		return;
	}
	Follow(Living[Index]);
}

void UDeathSpectatorComponent::Follow(APlayerState* Target)
{
	APlayerController* Controller = GetPlayerController();
	APawn* Pawn = Target ? Target->GetPawn() : nullptr;
	if (!Controller || !Pawn)
	{
		return;
	}
	const bool bChanged = FollowedPlayer != Target || FollowedPawn.Get() != Pawn;
	FollowedPlayer = Target;
	FollowedPawn = Pawn;
	FollowedDeathTime = -1.0;
	// a cut; the engine tells the owner (ClientSetViewTarget, asked again until his machine knows the pawn)
	if (Controller->GetViewTarget() != Pawn)
	{
		Controller->SetViewTarget(Pawn);
	}
	if (bChanged)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Death: %s follows %s"), *GetNameSafe(Controller->PlayerState), *Target->GetPlayerName());
		OnFollowedPlayerChanged.Broadcast(FollowedPlayer);
	}
}

void UDeathSpectatorComponent::StopFollowing()
{
	const bool bWasFollowing = FollowedPlayer != nullptr;
	FollowedPlayer = nullptr;
	FollowedPawn.Reset();
	FollowedDeathTime = -1.0;
	if (bWasFollowing)
	{
		OnFollowedPlayerChanged.Broadcast(nullptr);
	}
}

void UDeathSpectatorComponent::OnRep_FollowedPlayer()
{
	OnFollowedPlayerChanged.Broadcast(FollowedPlayer);
}

bool UDeathSpectatorComponent::ApplyFollowView(const AActor* ViewTarget, FMinimalViewInfo& InOutPOV, float DeltaTime)
{
	const APawn* Target = Cast<APawn>(ViewTarget);
	const APlayerController* Controller = GetPlayerController();
	const UWorld* World = GetWorld();
	// a teammate's pawn only: one's own body, a scene camera or anything else keep their view
	if (!Target || !Controller || !World || Target == Controller->GetPawn() || !Target->GetPlayerState())
	{
		FollowViewTarget.Reset();
		return false;
	}
	const UDeathRules& R = GetRules();
	// where the teammate looks (his view rotation, sent by the server to the spectator), seen from a little above
	const FRotator Wanted(R.FollowPitch, Target->GetViewRotation().Yaw, 0.0f);
	if (FollowViewTarget.Get() != Target || R.FollowRotationSpeed <= 0.0f)
	{
		// a new teammate: no swing from the last one
		FollowRotation = Wanted;
		FollowViewTarget = Target;
	}
	else
	{
		FollowRotation = FMath::RInterpTo(FollowRotation, Wanted, DeltaTime, R.FollowRotationSpeed);
	}

	const FVector PawnLocation = Target->GetActorLocation();
	const FVector Pivot = R.FollowPivot(PawnLocation);
	const FVector Wished = R.FollowCameraLocation(PawnLocation, static_cast<float>(FollowRotation.Yaw));
	// kept out of walls and rocks, as a spring arm does; the teammate, his gear and one's own body do not block it
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeathFollowCamera), false, Target);
	TArray<AActor*> Gear;
	Target->GetAttachedActors(Gear, true, true);
	Params.AddIgnoredActors(Gear);
	if (const APawn* Body = Controller->GetPawn())
	{
		Params.AddIgnoredActor(Body);
	}
	FVector Location = Wished;
	FHitResult Hit;
	if (R.FollowProbeRadius > 0.0f
		&& World->SweepSingleByChannel(Hit, Pivot, Wished, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(R.FollowProbeRadius), Params))
	{
		Location = Hit.Location;
	}
	InOutPOV.Location = Location;
	InOutPOV.Rotation = FRotator(R.FollowPitch, FollowRotation.Yaw, 0.0f);
	InOutPOV.FOV = R.FollowFieldOfView;
	return true;
}
