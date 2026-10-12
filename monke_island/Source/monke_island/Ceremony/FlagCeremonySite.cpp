// monke_island

#include "Ceremony/FlagCeremonySite.h"
#include "Ceremony/FlagCeremonyMath.h"
#include "Ceremony/FlagCeremonyRules.h"
#include "Boats/BoatRiderComponent.h"
#include "Creatures/TikiDirector.h"
#include "Emotes/TrumpetEmoteComponent.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunSubsystem.h"
#include "Village/VillageSpawner.h"
#include "monke_island.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFlagCeremonySite::AFlagCeremonySite()
{
	PrimaryActorTick.bCanEverTick = true;
	// ticks only while the flag rises
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	// a few per fragment, and the flag is seen from afar
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Cairn = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cairn"));
	Cairn->SetupAttachment(RootComponent);
	Cairn->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Cairn->SetCanEverAffectNavigation(false);

	Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	Pole->SetupAttachment(RootComponent);
	Pole->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// a thin pole must not pull the camera in front of the player
	Pole->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Pole->SetCanEverAffectNavigation(false);
	Pole->SetHiddenInGame(true);

	Flag = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flag"));
	Flag->SetupAttachment(Pole);
	Flag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flag->SetCanEverAffectNavigation(false);
	Flag->SetHiddenInGame(true);
}

void AFlagCeremonySite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFlagCeremonySite, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AFlagCeremonySite, ZoneId, COND_InitialOnly);
	DOREPLIFETIME(AFlagCeremonySite, State);
}

AFlagCeremonySite* AFlagCeremonySite::FindAt(const UObject* WorldContextObject, const FVector& Location)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AFlagCeremonySite> It(World); It; ++It)
	{
		if (It->IsInArea(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

AFlagCeremonySite* AFlagCeremonySite::FindNearest(const UObject* WorldContextObject, const FVector& Location, float MaxDistance)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AFlagCeremonySite* Best = nullptr;
	double BestDistance = FMath::Square(static_cast<double>(MaxDistance));
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AFlagCeremonySite> It(World); It; ++It)
	{
		const double Distance = FVector::DistSquared(It->GetActorLocation(), Location);
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

bool AFlagCeremonySite::IsInArea(const FVector& Location) const
{
	return Rules && FlagCeremonyMath::IsInArea(GetActorLocation(), Location, Rules->Radius, Rules->HalfHeight);
}

bool AFlagCeremonySite::CanStart(const APawn* Pawn) const
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Pawn);
	if (!Rules || !Islander || State.Phase != EFlagCeremonyPhase::Idle || Islander->IsRagdoll() || !IsInArea(Islander->GetActorLocation()))
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = Islander->GetHealthComponent(); Health && Health->IsDead())
	{
		return false;
	}
	if (const UBoatRiderComponent* Rider = Islander->FindComponentByClass<UBoatRiderComponent>(); Rider && Rider->IsSeated())
	{
		return false;
	}
	const UCharacterMovementComponent* Move = Islander->GetCharacterMovement();
	return Move && !Move->IsSwimming() && !Move->IsFalling();
}

bool AFlagCeremonySite::TryStart(APawn* Pawn)
{
	if (!HasAuthority() || !CanStart(Pawn))
	{
		return false;
	}
	UTrumpetEmoteComponent* Trumpet = Pawn->FindComponentByClass<UTrumpetEmoteComponent>();
	// the officiant sounds the trumpet all along (the bean); a pawn without it raises the flag in silence
	if (Trumpet && !Trumpet->StartRitual(Rules->RitualSeconds))
	{
		return false;
	}
	State.Phase = EFlagCeremonyPhase::Raising;
	State.StartTime = GetServerNow();
	State.Officiant = Pawn;
	State.bPlanted = true;
	GatherCenter = Pawn->GetActorLocation();
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Pawn))
	{
		if (UIslanderHealthComponent* Health = Islander->GetHealthComponent())
		{
			Health->OnHealthChanged.AddUniqueDynamic(this, &AFlagCeremonySite::OnOfficiantHealthChanged);
		}
	}
	GetWorldTimerManager().SetTimer(CheckTimer, this, &AFlagCeremonySite::CheckOfficiant, Rules->CheckInterval, true);
	ForceNetUpdate();
	ApplyState();
	CheckLocalPlayers();
	ResolveVillage();
	if (ATikiDirector* Director = ATikiDirector::Get(this))
	{
		Director->StartGathering(this);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s starts raising the flag of %s (%.0f s)"), *GetName(), *Pawn->GetName(),
		*ZoneId.ToString(), Rules->RitualSeconds);
	return true;
}

void AFlagCeremonySite::CheckOfficiant()
{
	APawn* Officiant = State.Officiant.Get();
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Officiant);
	if (!IsValid(Officiant) || !Officiant->GetController())
	{
		Finish(EFlagCeremonyEnd::Gone);
		return;
	}
	if (Islander)
	{
		if (const UIslanderHealthComponent* Health = Islander->GetHealthComponent(); Health && Health->IsDead())
		{
			Finish(EFlagCeremonyEnd::Died);
			return;
		}
		if (Islander->IsRagdoll())
		{
			Finish(EFlagCeremonyEnd::Moved);
			return;
		}
	}
	if (FlagCeremonyMath::HasMoved(GatherCenter, Officiant->GetActorLocation(), Rules->MoveTolerance))
	{
		Finish(EFlagCeremonyEnd::Moved);
		return;
	}
	if (GetProgress() >= 1.0f)
	{
		Finish(EFlagCeremonyEnd::Raised);
	}
}

void AFlagCeremonySite::OnOfficiantHealthChanged(float NewHealth, float Delta)
{
	if (Delta < 0.0f && Rules && Rules->bInterruptOnDamage && State.Phase == EFlagCeremonyPhase::Raising)
	{
		Finish(NewHealth <= 0.0f ? EFlagCeremonyEnd::Died : EFlagCeremonyEnd::Hurt);
	}
}

void AFlagCeremonySite::Finish(EFlagCeremonyEnd End)
{
	if (!HasAuthority() || State.Phase != EFlagCeremonyPhase::Raising)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CheckTimer);
	APawn* Officiant = State.Officiant.Get();
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Officiant))
	{
		if (UIslanderHealthComponent* Health = Islander->GetHealthComponent())
		{
			Health->OnHealthChanged.RemoveDynamic(this, &AFlagCeremonySite::OnOfficiantHealthChanged);
		}
	}
	if (UTrumpetEmoteComponent* Trumpet = IsValid(Officiant) ? Officiant->FindComponentByClass<UTrumpetEmoteComponent>() : nullptr)
	{
		Trumpet->StopRitual();
	}
	const bool bRaised = End == EFlagCeremonyEnd::Raised;
	State.Phase = bRaised ? EFlagCeremonyPhase::Raised : EFlagCeremonyPhase::Idle;
	State.Officiant = nullptr;
	State.LastEnd = End;
	++State.EndCount;
	ForceNetUpdate();
	OnRep_State();

	if (ATikiDirector* Director = ATikiDirector::Get(this))
	{
		Director->FinishGathering(this, bRaised);
	}
	static const TCHAR* const EndNames[] = { TEXT("none"), TEXT("raised"), TEXT("moved"), TEXT("hurt"), TEXT("died"), TEXT("gone") };
	UE_LOG(Logmonke_island, Log, TEXT("%s: the flag ceremony of %s ends: %s"), *GetName(), *ZoneId.ToString(),
		EndNames[FMath::Clamp(static_cast<int32>(End), 0, 5)]);
	if (!bRaised)
	{
		return;
	}
	// the crown pays for the land it claims
	if (Rules->ClaimReward > 0)
	{
		if (URunSubsystem* Run = URunSubsystem::Get(this))
		{
			Run->AddMoney(GetWorld(), Rules->ClaimReward);
			UE_LOG(Logmonke_island, Log, TEXT("%s: the crown claims %s, +%d crowns"), *GetName(), *ZoneId.ToString(), Rules->ClaimReward);
		}
	}
	if (Rules->bOpenSealedHouses && VillageIndex != INDEX_NONE)
	{
		if (AVillageSpawner* Spawner = AVillageSpawner::Find(this))
		{
			Spawner->OpenSealedHouses(VillageIndex);
		}
	}
	if (Rules->bGrantZoneBonus && !ZoneId.IsNone())
	{
		if (AExpeditionGameState* Expedition = GetWorld()->GetGameState<AExpeditionGameState>())
		{
			const EZoneBonusResult Result = Expedition->AddZoneBonus(ZoneId);
			UE_LOG(Logmonke_island, Log, TEXT("%s: zone %s pacified, bonus %s"), *GetName(), *ZoneId.ToString(),
				Result == EZoneBonusResult::Granted ? TEXT("granted") : TEXT("not granted"));
		}
	}
}

float AFlagCeremonySite::GetProgress() const
{
	switch (State.Phase)
	{
	case EFlagCeremonyPhase::Raising:
		return Rules ? FlagCeremonyMath::Progress(State.StartTime, GetServerNow(), Rules->RitualSeconds) : 0.0f;
	case EFlagCeremonyPhase::Raised:
		return 1.0f;
	default:
		return 0.0f;
	}
}

float AFlagCeremonySite::GetSecondsLeft() const
{
	return Rules ? FlagCeremonyMath::SecondsLeft(GetProgress(), Rules->RitualSeconds) : 0.0f;
}

bool AFlagCeremonySite::GetLastEnd(EFlagCeremonyEnd& OutEnd, float& OutAge) const
{
	OutEnd = State.LastEnd;
	OutAge = LocalEndTime < 0.0 ? 0.0f : static_cast<float>(GetWorld()->GetTimeSeconds() - LocalEndTime);
	return LocalEndTime >= 0.0 && State.LastEnd != EFlagCeremonyEnd::None;
}

double AFlagCeremonySite::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

void AFlagCeremonySite::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRules();
}

void AFlagCeremonySite::BeginPlay()
{
	Super::BeginPlay();
	ApplyRules();
	ApplyState();
	SeenEndCount = State.EndCount;
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_FlagCeremonyRules, no flag ceremony here"), *GetName());
		return;
	}
	if (HasAuthority())
	{
		ResolveVillage();
	}
	// a dedicated server has no local player
	if (GetNetMode() != NM_DedicatedServer && Rules->CeremonyMapping)
	{
		GetWorldTimerManager().SetTimer(LocalTimer, this, &AFlagCeremonySite::CheckLocalPlayers, Rules->LocalCheckInterval, true);
	}
}

void AFlagCeremonySite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	GetWorldTimerManager().ClearTimer(LocalTimer);
	for (const TWeakObjectPtr<ULocalPlayer>& Player : Mapped)
	{
		SetMapped(Player.Get(), false);
	}
	Mapped.Reset();
	Super::EndPlay(EndPlayReason);
}

void AFlagCeremonySite::OnRep_Rules()
{
	ApplyRules();
	ApplyState();
	// rules arriving after the start of play: the key is watched from now on
	if (HasActorBegunPlay() && Rules && Rules->CeremonyMapping && GetNetMode() != NM_DedicatedServer && !GetWorldTimerManager().IsTimerActive(LocalTimer))
	{
		GetWorldTimerManager().SetTimer(LocalTimer, this, &AFlagCeremonySite::CheckLocalPlayers, Rules->LocalCheckInterval, true);
	}
}

void AFlagCeremonySite::OnRep_State()
{
	ApplyState();
	if (State.EndCount != SeenEndCount)
	{
		SeenEndCount = State.EndCount;
		LocalEndTime = GetWorld()->GetTimeSeconds();
	}
}

void AFlagCeremonySite::ApplyRules()
{
	if (!Rules)
	{
		return;
	}
	Cairn->SetStaticMesh(Rules->CairnMesh);
	Pole->SetStaticMesh(Rules->PoleMesh);
	Pole->SetRelativeLocation(Rules->PoleOffset);
	Flag->SetStaticMesh(Rules->FlagMesh);
	Flag->SetRelativeLocation(FVector(0.0f, 0.0f, GetFlagHeight()));
}

void AFlagCeremonySite::ApplyState()
{
	const bool bShown = State.bPlanted;
	Pole->SetHiddenInGame(!bShown);
	Flag->SetHiddenInGame(!bShown);
	Pole->SetCollisionEnabled(bShown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Flag->SetRelativeLocation(FVector(0.0f, 0.0f, GetFlagHeight()));
	SetActorTickEnabled(State.Phase == EFlagCeremonyPhase::Raising);
}

float AFlagCeremonySite::GetFlagHeight() const
{
	return Rules ? FlagCeremonyMath::FlagHeight(GetProgress(), Rules->FlagStartHeight, Rules->FlagEndHeight) : 0.0f;
}

void AFlagCeremonySite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Flag->SetRelativeLocation(FVector(0.0f, 0.0f, GetFlagHeight()));
}

void AFlagCeremonySite::ResolveVillage()
{
	if (VillageIndex != INDEX_NONE)
	{
		return;
	}
	const AVillageSpawner* Spawner = AVillageSpawner::Find(this);
	if (!Spawner || !Spawner->IsBuilt())
	{
		return;
	}
	const TArray<FVillageLayout>& Villages = Spawner->GetVillages();
	double Best = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Villages.Num(); ++Index)
	{
		const double Distance = FVector::DistSquared2D(Villages[Index].Center, GetActorLocation());
		if (Distance <= FMath::Square(static_cast<double>(Villages[Index].Radius)) && Distance < Best)
		{
			Best = Distance;
			VillageIndex = Index;
		}
	}
	if (VillageIndex != INDEX_NONE && ZoneId.IsNone())
	{
		ZoneId = Villages[VillageIndex].Id;
	}
}

void AFlagCeremonySite::CheckLocalPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		ULocalPlayer* Player = Controller && Controller->IsLocalController() ? Controller->GetLocalPlayer() : nullptr;
		if (!Player)
		{
			continue;
		}
		const bool bWants = CanStart(Controller->GetPawn());
		const bool bHas = Mapped.Contains(Player);
		if (bWants != bHas)
		{
			SetMapped(Player, bWants);
			if (bWants)
			{
				Mapped.Add(Player);
			}
			else
			{
				Mapped.Remove(Player);
			}
		}
	}
}

void AFlagCeremonySite::SetMapped(ULocalPlayer* Player, bool bMapped)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem || !Rules || !Rules->CeremonyMapping)
	{
		return;
	}
	if (bMapped)
	{
		Subsystem->AddMappingContext(Rules->CeremonyMapping, Rules->MappingPriority);
	}
	else
	{
		Subsystem->RemoveMappingContext(Rules->CeremonyMapping);
	}
}
