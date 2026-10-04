// monke_island

#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Galleon/GalleonActor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "TimerManager.h"
#include "monke_island.h"

AExpeditionGameState::AExpeditionGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

AExpeditionGameState* AExpeditionGameState::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetGameState<AExpeditionGameState>() : nullptr;
}

void AExpeditionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExpeditionGameState, ExpeditionId);
	DOREPLIFETIME(AExpeditionGameState, FragmentId);
	DOREPLIFETIME(AExpeditionGameState, ClockState);
	DOREPLIFETIME(AExpeditionGameState, Phase);
	DOREPLIFETIME(AExpeditionGameState, Resolution);
	DOREPLIFETIME(AExpeditionGameState, TeamWipedTime);
	DOREPLIFETIME(AExpeditionGameState, DayEndedTime);
}

void AExpeditionGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// The server says which fragment this map plays; the clients receive the id with the game state
		if (const UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this))
		{
			FragmentId = Islands->ResolveFragmentIdForWorld(GetWorld());
		}
		if (!FragmentId.IsNone())
		{
			UE_LOG(Logmonke_island, Log, TEXT("Expedition: playing fragment %s"), *FragmentId.ToString());
		}
	}

	if (HasAuthority())
	{
		// Players may spawn before or after this actor begins play: watch both
		ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AExpeditionGameState::WatchPlayerCharacter));
		for (TActorIterator<AIslanderCharacter> It(GetWorld()); It; ++It)
		{
			WatchPlayerCharacter(*It);
		}
		LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &AExpeditionGameState::HandleLogout);
	}

	if (HasAuthority() && bStartOnBeginPlay)
	{
		StartExpedition();
	}
}

void AExpeditionGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
		World->GetTimerManager().ClearTimer(RestartTimer);
		World->GetTimerManager().ClearTimer(DayEndTimer);
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}

	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);

	Super::EndPlay(EndPlayReason);
}

FExpeditionClockParams AExpeditionGameState::GetParams() const
{
	return Rules ? FExpeditionClockParams::FromRules(*Rules) : FExpeditionClockParams();
}

bool AExpeditionGameState::StartExpedition()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (ClockState.bStarted)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: already started, start ignored"), *ExpeditionId.ToString());
		return false;
	}
	if (!ensureMsgf(Rules, TEXT("%s has no ExpeditionRules (DA_ExpeditionRules): the expedition clock cannot start"), *GetNameSafe(this)))
	{
		return false;
	}

	const FExpeditionClockParams Params = GetParams();
	const double Now = GetServerWorldTimeSeconds();
	const double PreviousFloodStartTime = ClockState.FloodStartTime;

	ExpeditionId = FGuid::NewGuid();
	ClockState = ExpeditionClock::Start(Now, Params);
	Resolution = EExpeditionResolution::None;

	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: started at %.2f s, flood at %.2f s"),
		*ExpeditionId.ToString(), ClockState.StartTime, ClockState.FloodStartTime);

	OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousFloodStartTime);
	RefreshPhase();
	return true;
}

EZoneBonusResult AExpeditionGameState::AddZoneBonus(FName ZoneId)
{
	if (!HasAuthority())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: bonus of zone '%s' requested without authority (%s), ignored"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), *GetNameSafe(this));
		return EZoneBonusResult::NoAuthority;
	}

	// The rule "strictly before the deadline" uses the server clock, never a time given by the caller
	const double ServerTime = GetServerWorldTimeSeconds();

	// The flood may be due before its timer fired this frame: settle the phase first, it is irreversible
	RefreshPhase();

	const double PreviousFloodStartTime = ClockState.FloodStartTime;
	EZoneBonusResult Result = EZoneBonusResult::TooLate;
	if (Phase == EExpeditionPhase::Submerging || Phase == EExpeditionPhase::Resolved)
	{
		// The submersion started: whatever time the caller passes, it never moves the deadline back
		Result = HasZoneBonus(ZoneId) ? EZoneBonusResult::AlreadyGranted : EZoneBonusResult::TooLate;
	}
	else
	{
		Result = ExpeditionClock::TryAddZoneBonus(ClockState, ZoneId, ServerTime, GetParams());
	}

	switch (Result)
	{
	case EZoneBonusResult::Granted:
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: zone %s pacified at %.2f s, flood moved from %.2f s to %.2f s"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), ServerTime, PreviousFloodStartTime, ClockState.FloodStartTime);
		OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousFloodStartTime);
		RefreshPhase();
		break;

	case EZoneBonusResult::AlreadyGranted:
		UE_LOG(Logmonke_island, Verbose, TEXT("Expedition %s: zone %s already gave its bonus"), *ExpeditionId.ToString(), *ZoneId.ToString());
		break;

	case EZoneBonusResult::TooLate:
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: zone %s pacified at %.2f s, after the flood at %.2f s: no bonus"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), ServerTime, ClockState.FloodStartTime);
		break;

	default:
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: bonus of zone '%s' refused (%s)"), *ExpeditionId.ToString(), *ZoneId.ToString(),
			*UEnum::GetValueAsString(Result));
		break;
	}
	return Result;
}

double AExpeditionGameState::GetWaterRise(double ServerTime) const
{
	// Client: the replicated deadline may still move before the submersion; only the server clock decides it started
	if (!HasAuthority() && Phase < EExpeditionPhase::Submerging)
	{
		return 0.0;
	}
	return ExpeditionClock::WaterRiseCm(ClockState, ServerTime, GetParams());
}

double AExpeditionGameState::GetCurrentWaterRise() const
{
	return GetWaterRise(GetServerWorldTimeSeconds());
}

double AExpeditionGameState::GetTimeBeforeSubmersion() const
{
	return ExpeditionClock::TimeBeforeSubmersion(ClockState, GetServerWorldTimeSeconds());
}

double AExpeditionGameState::GetSubmersionEndTime() const
{
	return ExpeditionClock::SubmersionEndTime(ClockState, GetParams());
}

int32 AExpeditionGameState::GetRemainingZoneCount() const
{
	const int32 ZonesCount = GetParams().ZonesCount;
	return ZonesCount > 0 ? FMath::Max(0, ZonesCount - ClockState.BonusedZoneIds.Num()) : -1;
}

void AExpeditionGameState::RefreshPhase()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || Phase == EExpeditionPhase::Resolved)
	{
		return;
	}

	const FExpeditionClockParams Params = GetParams();
	const double Now = GetServerWorldTimeSeconds();
	const EExpeditionPhase Target = ExpeditionClock::PhaseAt(ClockState, Now, Params);

	// Phases only move forward, one step at a time so every server listener sees each of them
	// (clients only receive the latest replicated phase and may skip steps)
	while (Phase < Target)
	{
		SetPhase(static_cast<EExpeditionPhase>(static_cast<uint8>(Phase) + 1));
	}

	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(PhaseTimer);

	double NextTransition = -1.0;
	if (Phase == EExpeditionPhase::Exploring)
	{
		NextTransition = ClockState.FloodStartTime;
	}
	else if (Phase == EExpeditionPhase::Submerging)
	{
		NextTransition = ExpeditionClock::SubmersionEndTime(ClockState, Params);
	}

	if (NextTransition >= 0.0)
	{
		// A timer of rate 0 is cleared: keep a tiny delay, RefreshPhase reschedules if it fires a hair early
		const float Delay = static_cast<float>(FMath::Max(NextTransition - Now, static_cast<double>(UE_KINDA_SMALL_NUMBER)));
		Timers.SetTimer(PhaseTimer, this, &AExpeditionGameState::RefreshPhase, Delay, false);
	}
}

void AExpeditionGameState::SetPhase(EExpeditionPhase NewPhase)
{
	if (NewPhase == Phase)
	{
		return;
	}

	const EExpeditionPhase PreviousPhase = Phase;
	if (NewPhase == EExpeditionPhase::Resolved)
	{
		Resolution = EExpeditionResolution::Submerged;
	}
	Phase = NewPhase;

	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: phase %s -> %s at %.2f s"), *ExpeditionId.ToString(),
		*UEnum::GetValueAsString(PreviousPhase), *UEnum::GetValueAsString(NewPhase), GetServerWorldTimeSeconds());

	OnPhaseChanged.Broadcast(Phase, PreviousPhase);
	if (Phase == EExpeditionPhase::Resolved && Resolution == EExpeditionResolution::Submerged)
	{
		OnSubmersionEnded.Broadcast();
		ResolveDelugeEnd();
	}
}

void AExpeditionGameState::OnRep_ClockState(const FExpeditionClockState& PreviousClockState)
{
	if (ClockState.FloodStartTime != PreviousClockState.FloodStartTime)
	{
		OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousClockState.FloodStartTime);
	}
}

void AExpeditionGameState::OnRep_Phase(EExpeditionPhase PreviousPhase)
{
	OnPhaseChanged.Broadcast(Phase, PreviousPhase);
	if (Phase == EExpeditionPhase::Resolved && Resolution == EExpeditionResolution::Submerged)
	{
		OnSubmersionEnded.Broadcast();
	}
}

double AExpeditionGameState::GetRestartTime() const
{
	return IsTeamWiped() ? TeamWipedTime + (Rules ? Rules->GameOverRestartDelay : 0.0f) : -1.0;
}

void AExpeditionGameState::WatchPlayerCharacter(AActor* Actor)
{
	AIslanderCharacter* Character = Cast<AIslanderCharacter>(Actor);
	if (!Character || !Rules || !Rules->bPermadeath)
	{
		return;
	}
	// AIslanderCharacter::RespawnDelay: 0 disables respawning. Set through reflection (the member is protected):
	// to be replaced by a call on the character once its file is free.
	if (const FFloatProperty* Delay = FindFProperty<FFloatProperty>(AIslanderCharacter::StaticClass(), TEXT("RespawnDelay")))
	{
		Delay->SetPropertyValue_InContainer(Character, 0.0f);
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition: AIslanderCharacter has no RespawnDelay, %s still respawns"), *GetNameSafe(Character));
	}
	if (UIslanderHealthComponent* Health = Character->GetHealthComponent())
	{
		Health->OnDeath.AddUniqueDynamic(this, &AExpeditionGameState::HandlePlayerDeath);
	}
}

void AExpeditionGameState::HandlePlayerDeath(AActor* Killer)
{
	RecordDeadPlayers();
	CheckTeamWiped();
}

void AExpeditionGameState::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!GameMode || GameMode->GetWorld() != GetWorld())
	{
		return;
	}
	// the player state of the leaver is removed after this event: count the team on the next frame
	GetWorldTimerManager().SetTimerForNextTick(this, &AExpeditionGameState::CheckTeamWiped);
}

void AExpeditionGameState::RecordDeadPlayers()
{
	for (const APlayerState* PlayerState : PlayerArray)
	{
		const AIslanderCharacter* Character = PlayerState ? Cast<AIslanderCharacter>(PlayerState->GetPawn()) : nullptr;
		const UIslanderHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
		if (Health && Health->IsDead())
		{
			DeadPlayers.Add(PlayerState);
		}
	}
}

void AExpeditionGameState::CountPlayers(int32& OutDead, int32& OutAlive, int32& OutPending)
{
	OutDead = 0;
	OutAlive = 0;
	OutPending = 0;
	RecordDeadPlayers();
	for (const APlayerState* PlayerState : PlayerArray)
	{
		if (!PlayerState || PlayerState->IsOnlyASpectator() || PlayerState->IsInactive())
		{
			continue;
		}
		if (DeadPlayers.Contains(PlayerState))
		{
			++OutDead;
		}
		else if (Cast<AIslanderCharacter>(PlayerState->GetPawn()))
		{
			++OutAlive;
		}
		else
		{
			// logged in, still loading or not spawned yet: alive until proven otherwise
			++OutPending;
		}
	}
}

void AExpeditionGameState::CheckTeamWiped()
{
	if (!HasAuthority() || IsTeamWiped() || !Rules || !Rules->bPermadeath)
	{
		return;
	}
	if (IsDaySurvived())
	{
		// the day already ended with survivors: the travel is on its way
		return;
	}
	int32 Dead = 0;
	int32 Alive = 0;
	int32 Pending = 0;
	CountPlayers(Dead, Alive, Pending);
	if (!IsWipe(Dead, Alive, Pending))
	{
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: %d player(s) dead, %d alive, %d still loading"), *ExpeditionId.ToString(), Dead, Alive, Pending);
		return;
	}

	GetWorldTimerManager().ClearTimer(DayEndTimer);
	TeamWipedTime = GetServerWorldTimeSeconds();
	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: every player is dead at %.2f s, game over, restart in %.1f s"),
		*ExpeditionId.ToString(), TeamWipedTime, Rules->GameOverRestartDelay);
	OnTeamWiped.Broadcast();
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AExpeditionGameState::RestartFromBoot,
		FMath::Max(Rules->GameOverRestartDelay, UE_KINDA_SMALL_NUMBER), false);
}

void AExpeditionGameState::ResolveDelugeEnd()
{
	if (!HasAuthority() || !Rules || IsExpeditionOver())
	{
		return;
	}
	TArray<const AGalleonActor*, TInlineAllocator<2>> Galleons;
	for (TActorIterator<AGalleonActor> It(GetWorld()); It; ++It)
	{
		Galleons.Add(*It);
	}
	if (!Rules->bGalleonSavesAtDelugeEnd || Galleons.IsEmpty())
	{
		// no ship to come back to: the sea keeps rising (T-101), the day ends once it reached its highest level
		const float Delay = Rules->PostSubmersionRiseCmPerSecond > 0.0f ? Rules->PostSubmersionMaxRiseCm / Rules->PostSubmersionRiseCmPerSecond : 0.0f;
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: end of the submersion without galleon, the day ends in %.0f s"), *ExpeditionId.ToString(), Delay);
		GetWorldTimerManager().SetTimer(DayEndTimer, this, &AExpeditionGameState::EndDay, FMath::Max(Delay, UE_KINDA_SMALL_NUMBER), false);
		return;
	}

	// ADR-006: whoever is not aboard at the end of the deluge dies. The array is copied: a death may travel the world.
	const TArray<TObjectPtr<APlayerState>> Players = PlayerArray;
	for (const APlayerState* PlayerState : Players)
	{
		AIslanderCharacter* Character = PlayerState ? Cast<AIslanderCharacter>(PlayerState->GetPawn()) : nullptr;
		UIslanderHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
		if (!Health || Health->IsDead())
		{
			continue;
		}
		const bool bAboard = Galleons.ContainsByPredicate([Character](const AGalleonActor* Galleon) { return Galleon->IsActorAboard(Character); });
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: end of the deluge, %s is %s"), *ExpeditionId.ToString(), *GetNameSafe(Character),
			bAboard ? TEXT("aboard the galleon: survives") : TEXT("not aboard the galleon: dies"));
		if (!bAboard)
		{
			Health->ApplyHealthDamage(1.0e6f, this);
		}
	}
	EndDay();
}

void AExpeditionGameState::EndDay()
{
	if (!HasAuthority() || !Rules || IsExpeditionOver())
	{
		return;
	}
	int32 Dead = 0;
	int32 Alive = 0;
	int32 Pending = 0;
	CountPlayers(Dead, Alive, Pending);
	if (Alive + Pending <= 0)
	{
		// nobody left to carry on: a wipe when somebody died, an empty server otherwise
		CheckTeamWiped();
		return;
	}

	DayEndedTime = GetServerWorldTimeSeconds();
	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: the day ends at %.2f s with %d survivor(s) and %d dead, next island in %.1f s"),
		*ExpeditionId.ToString(), DayEndedTime, Alive + Pending, Dead, Rules->DayEndTravelDelay);
	OnDaySurvived.Broadcast();
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AExpeditionGameState::RestartFromBoot,
		FMath::Max(Rules->DayEndTravelDelay, UE_KINDA_SMALL_NUMBER), false);
}

void AExpeditionGameState::OnRep_DayEndedTime()
{
	if (IsDaySurvived())
	{
		OnDaySurvived.Broadcast();
	}
}

double AExpeditionGameState::GetNextTravelTime() const
{
	if (IsTeamWiped())
	{
		return GetRestartTime();
	}
	return IsDaySurvived() ? DayEndedTime + (Rules ? Rules->DayEndTravelDelay : 0.0f) : -1.0;
}

void AExpeditionGameState::OnRep_TeamWipedTime()
{
	if (IsTeamWiped())
	{
		OnTeamWiped.Broadcast();
	}
}

void AExpeditionGameState::RestartFromBoot()
{
	FString Map = Rules ? Rules->RestartMap.GetLongPackageName() : FString();
	if (Map.IsEmpty() && GConfig)
	{
		FString DefaultMap;
		GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), DefaultMap, GEngineIni);
		Map = FSoftObjectPath(DefaultMap).GetLongPackageName();
	}
	UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
	const EFragmentError Error = Islands ? Islands->TravelToMapURL(Map) : EFragmentError::TravelFailed;
	if (Error != EFragmentError::None)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Expedition %s: game over but no restart (map '%s'): the players stay dead on this map"), *ExpeditionId.ToString(), *Map);
	}
}
