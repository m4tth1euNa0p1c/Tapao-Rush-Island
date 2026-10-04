// monke_island

#include "Expedition/Pacification/PacificationZone.h"
#include "Expedition/Pacification/PacificationParticipant.h"
#include "Expedition/Pacification/PacificationRules.h"
#include "Expedition/Pacification/RoyalFlag.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"

namespace
{
	/**
	 *  Alive test shared by threats and officiants: valid, not being destroyed, then the optional interface (first
	 *  choice), else the health component (dead but not destroyed counts as dead)
	 */
	bool IsActorAliveForPacification(const AActor* Actor)
	{
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
		{
			return false;
		}
		if (Actor->GetClass()->ImplementsInterface(UPacificationParticipant::StaticClass()))
		{
			return IPacificationParticipant::Execute_IsPacificationAlive(Actor);
		}
		if (const UIslanderHealthComponent* Health = Actor->FindComponentByClass<UIslanderHealthComponent>())
		{
			return !Health->IsDead();
		}
		return true;
	}
}

APacificationZone::APacificationZone()
{
	// the tick only runs on the server while a sermon is in progress (immobility and interruption checks)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	// few zones per expedition, and the HUD shows the remaining zones and the sermon of any teammate
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	FlagSocket = CreateDefaultSubobject<USceneComponent>(TEXT("FlagSocket"));
	FlagSocket->SetupAttachment(Root);

	FlagClass = ARoyalFlag::StaticClass();
}

void APacificationZone::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (ZoneId.IsNone())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("PacificationZone %s has no ZoneId: its bonus cannot be told apart from other zones"), *GetName());
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("PacificationZone %s (%s) has no rules asset: flag and sermon requests will be refused"), *GetName(), *ZoneId.ToString());
	}
	else if (Rules->CheckIntervalSeconds > 0.0f)
	{
		SetActorTickInterval(Rules->CheckIntervalSeconds);
	}

	if (bBeginObjectiveOnBeginPlay)
	{
		TArray<AActor*> Threats;
		Threats.Reserve(InitialThreats.Num());
		for (AActor* Threat : InitialThreats)
		{
			Threats.Add(Threat);
		}
		BeginObjective(Threats);
	}
}

void APacificationZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AExpeditionGameState* Expedition = BoundExpedition.Get())
	{
		Expedition->OnPhaseChanged.RemoveDynamic(this, &APacificationZone::HandleExpeditionPhaseChanged);
	}
	BoundExpedition.Reset();
	UnbindOfficiant();
	UnbindThreats();
	Sermon.Reset();
	if (HasAuthority() && IsValid(Flag))
	{
		Flag->Destroy();
	}
	Flag = nullptr;
	Super::EndPlay(EndPlayReason);
}

void APacificationZone::SetRules(const UPacificationRules* InRules)
{
	Rules = InRules;
	if (Rules && Rules->CheckIntervalSeconds > 0.0f)
	{
		SetActorTickInterval(Rules->CheckIntervalSeconds);
	}
}

EPacificationRequestResult APacificationZone::BeginObjective(const TArray<AActor*>& Threats)
{
	if (!HasAuthority())
	{
		return EPacificationRequestResult::NotAuthority;
	}
	if (ZoneState != EZoneState::Undiscovered)
	{
		// the designated threats are fixed once: later spawns never block the zone
		return EPacificationRequestResult::WrongState;
	}

	DesignatedThreats.Reset(Threats.Num());
	for (AActor* Threat : Threats)
	{
		if (IsValid(Threat))
		{
			DesignatedThreats.AddUnique(Threat);
		}
	}
	BindThreats();
	RemainingThreats = CountLivingThreats();

	UE_LOG(Logmonke_island, Log, TEXT("PacificationZone %s: objective started with %d designated threats"), *ZoneId.ToString(), DesignatedThreats.Num());
	SetZoneState(EZoneState::Contested);
	return EPacificationRequestResult::Accepted;
}

EPacificationRequestResult APacificationZone::TryPlantFlag(APawn* Planter)
{
	if (!HasAuthority())
	{
		return EPacificationRequestResult::NotAuthority;
	}
	if (!Rules)
	{
		return EPacificationRequestResult::NoRules;
	}
	if (ZoneState != EZoneState::Contested)
	{
		return EPacificationRequestResult::WrongState;
	}
	if (!IsOfficiantAlive(Planter))
	{
		return EPacificationRequestResult::InvalidPawn;
	}
	if (HorizontalDistanceToSocket(Planter->GetActorLocation()) > Rules->FlagReachRadiusCm)
	{
		return EPacificationRequestResult::OutOfReach;
	}

	RemainingThreats = CountLivingThreats();
	if (!FPacificationLogic::CanPlantFlag(ZoneState, RemainingThreats))
	{
		return EPacificationRequestResult::ThreatsRemaining;
	}

	UWorld* World = GetWorld();
	UClass* SpawnClass = FlagClass ? FlagClass.Get() : ARoyalFlag::StaticClass();
	ARoyalFlag* NewFlag = World ? World->SpawnActorDeferred<ARoyalFlag>(SpawnClass, FlagSocket->GetComponentTransform(), this, Planter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
	if (!NewFlag)
	{
		UE_LOG(Logmonke_island, Error, TEXT("PacificationZone %s: the flag %s could not be spawned"), *ZoneId.ToString(), *GetNameSafe(SpawnClass));
		return EPacificationRequestResult::SpawnFailed;
	}
	NewFlag->InitializePlanted(ZoneId, Planter);
	NewFlag->FinishSpawning(FlagSocket->GetComponentTransform());
	Flag = NewFlag;

	UE_LOG(Logmonke_island, Log, TEXT("PacificationZone %s: flag planted by %s"), *ZoneId.ToString(), *GetNameSafe(Planter));
	SetZoneState(EZoneState::Securing);
	return EPacificationRequestResult::Accepted;
}

EPacificationRequestResult APacificationZone::TryStartSermon(APawn* Officiant)
{
	if (!HasAuthority())
	{
		return EPacificationRequestResult::NotAuthority;
	}
	if (!Rules)
	{
		return EPacificationRequestResult::NoRules;
	}
	if (!FPacificationLogic::AcceptsSermon(ZoneState, Flag != nullptr))
	{
		return ZoneState == EZoneState::Contested ? EPacificationRequestResult::FlagNotPlanted : EPacificationRequestResult::WrongState;
	}
	if (Sermon.bActive)
	{
		// a repeated request from the officiant changes nothing; nobody else can join or take over
		return Sermon.Officiant == Officiant ? EPacificationRequestResult::AlreadyOfficiant : EPacificationRequestResult::OfficiantBusy;
	}
	if (!IsOfficiantAlive(Officiant))
	{
		return EPacificationRequestResult::InvalidPawn;
	}
	if (HorizontalDistanceToSocket(Officiant->GetActorLocation()) > Rules->FlagReachRadiusCm)
	{
		return EPacificationRequestResult::OutOfReach;
	}
	if (!IsStandingStill(Officiant))
	{
		return EPacificationRequestResult::NotStandingStill;
	}
	if (IsOfficiatingAnywhere(Officiant))
	{
		return EPacificationRequestResult::OfficiantBusy;
	}
	if (IsThreatInPerimeter())
	{
		return EPacificationRequestResult::ThreatsRemaining;
	}

	AExpeditionGameState* Expedition = AExpeditionGameState::Get(this);
	if (!IsValid(Expedition) || !Expedition->GetExpeditionId().IsValid()
		|| Expedition->GetPhase() == EExpeditionPhase::Preparing)
	{
		return EPacificationRequestResult::NoExpedition;
	}
	if (ZoneId.IsNone())
	{
		return EPacificationRequestResult::InvalidZone;
	}
	if (ExpeditionId.IsValid() && ExpeditionId != Expedition->GetExpeditionId())
	{
		return EPacificationRequestResult::WrongExpedition;
	}
	if (Expedition->GetPhase() != EExpeditionPhase::Exploring
		|| Expedition->GetTimeBeforeSubmersion() <= Rules->SermonDuration)
	{
		return EPacificationRequestResult::TooLate;
	}

	ExpeditionId = Expedition->GetExpeditionId();
	BoundExpedition = Expedition;
	Expedition->OnPhaseChanged.AddUniqueDynamic(this, &APacificationZone::HandleExpeditionPhaseChanged);
	const double Now = GetServerTime();
	Sermon.Begin(Officiant, Officiant->GetActorLocation(), Now, Officiant->GetController() != nullptr);
	BindOfficiant(Officiant);
	SetSermonProgress(0.0f, true);
	SetActorTickEnabled(true);

	UE_LOG(Logmonke_island, Log, TEXT("PacificationZone %s: sermon started by %s at %.2f s"), *ZoneId.ToString(), *GetNameSafe(Officiant), Now);
	return EPacificationRequestResult::Accepted;
}

void APacificationZone::StopSermon(APawn* Officiant)
{
	if (!HasAuthority() || !Sermon.bActive || Sermon.Officiant != Officiant)
	{
		return;
	}
	InterruptSermon(ESermonInterruptReason::Released);
}

void APacificationZone::SetSubmerged()
{
	if (!HasAuthority() || ZoneState == EZoneState::Submerged)
	{
		return;
	}
	if (Sermon.bActive)
	{
		InterruptSermon(ESermonInterruptReason::ZoneSubmerged);
	}
	SetZoneState(EZoneState::Submerged);
}

void APacificationZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && Sermon.bActive)
	{
		ServerUpdateSermon(GetServerTime());
	}
	else
	{
		SetActorTickEnabled(false);
	}
}

void APacificationZone::ServerUpdateSermon(double ServerTime)
{
	if (!HasAuthority() || !Sermon.bActive || !Rules)
	{
		return;
	}

	const AExpeditionGameState* Expedition = BoundExpedition.Get();
	if (!IsValid(Expedition) || Expedition != AExpeditionGameState::Get(this)
		|| ExpeditionId != Expedition->GetExpeditionId())
	{
		InterruptSermon(ESermonInterruptReason::BonusRefused);
		return;
	}
	if (Expedition->GetPhase() >= EExpeditionPhase::Submerging || Expedition->GetTimeBeforeSubmersion() <= 0.0)
	{
		SetSubmerged();
		return;
	}

	APawn* Officiant = Sermon.Officiant;
	FSermonObservation Observation;
	Observation.bOfficiantValid = IsValid(Officiant) && !Officiant->IsActorBeingDestroyed();
	Observation.bOfficiantAlive = Observation.bOfficiantValid && IsOfficiantAlive(Officiant);
	Observation.bOfficiantConnected = Observation.bOfficiantValid && (!Sermon.bHadControllerAtStart || Officiant->GetController() != nullptr);
	Observation.bZoneSubmerged = ZoneState == EZoneState::Submerged;
	Observation.bThreatInPerimeter = IsThreatInPerimeter();
	Observation.Location = Observation.bOfficiantValid ? Officiant->GetActorLocation() : Sermon.StartLocation;

	const ESermonInterruptReason Reason = FPacificationLogic::EvaluateSermon(Sermon, Observation, Rules->PositionToleranceCm, Rules->VerticalToleranceCm);
	if (Reason != ESermonInterruptReason::None)
	{
		InterruptSermon(Reason);
		return;
	}

	// the deadline is validated here, after the last interruption check, never by a client animation or sound
	if (Sermon.IsComplete(ServerTime, Rules->SermonDuration))
	{
		CompleteSermon(ServerTime);
		return;
	}

	SetSermonProgress(Sermon.Progress01(ServerTime, Rules->SermonDuration), false);
}

void APacificationZone::InterruptSermon(ESermonInterruptReason Reason)
{
	if (!Sermon.bActive)
	{
		return;
	}

	UE_LOG(Logmonke_island, Log, TEXT("PacificationZone %s: sermon of %s interrupted (%s), progress reset"),
		*ZoneId.ToString(), *GetNameSafe(Sermon.Officiant), *UEnum::GetValueAsString(Reason));

	UnbindOfficiant();
	Sermon.Reset();
	SetActorTickEnabled(false);
	SetSermonProgress(0.0f, true);

	OnSermonInterrupted.Broadcast(ZoneId, Reason);
	OnSermonInterruptedNative.Broadcast(ZoneId, Reason);
}

void APacificationZone::CompleteSermon(double ServerTime)
{
	const FString OfficiantName = GetNameSafe(Sermon.Officiant);
	// Clear the active sermon before AddZoneBonus broadcasts clock events: reentrant calls cannot complete it twice.
	UnbindOfficiant();
	Sermon.Reset();
	SetActorTickEnabled(false);

	if (bPacificationAwarded)
	{
		// cannot happen while the state machine holds, kept as a guard against a double bonus
		ensureMsgf(false, TEXT("PacificationZone %s validated twice"), *ZoneId.ToString());
		return;
	}
	AExpeditionGameState* Expedition = BoundExpedition.Get();
	const EZoneBonusResult Result = IsValid(Expedition) && ExpeditionId == Expedition->GetExpeditionId()
		? Expedition->AddZoneBonus(ZoneId) : EZoneBonusResult::NotStarted;
	if (Result != EZoneBonusResult::Granted)
	{
		SetSermonProgress(0.0f, true);
		UE_LOG(Logmonke_island, Warning, TEXT("PacificationZone %s: bonus refused (%s), zone not pacified"),
			*ZoneId.ToString(), *UEnum::GetValueAsString(Result));
		OnSermonInterrupted.Broadcast(ZoneId, ESermonInterruptReason::BonusRefused);
		OnSermonInterruptedNative.Broadcast(ZoneId, ESermonInterruptReason::BonusRefused);
		return;
	}
	bPacificationAwarded = true;
	SetSermonProgress(1.0f, true);
	SetZoneState(EZoneState::Pacified);
	UE_LOG(Logmonke_island, Log, TEXT("PacificationZone %s pacified by %s at %.2f s (expedition %s)"), *ZoneId.ToString(), *OfficiantName, ServerTime, *ExpeditionId.ToString());
	OnZonePacified.Broadcast(ExpeditionId, ZoneId, ServerTime);
	OnZonePacifiedNative.Broadcast(ExpeditionId, ZoneId, ServerTime);
}

void APacificationZone::HandleExpeditionPhaseChanged(EExpeditionPhase NewPhase, EExpeditionPhase PreviousPhase)
{
	if (NewPhase >= EExpeditionPhase::Submerging)
	{
		SetSubmerged();
	}
}

void APacificationZone::SetZoneState(EZoneState NewState)
{
	if (ZoneState == NewState)
	{
		return;
	}
	ZoneState = NewState;
	OnZoneStateChanged.Broadcast(ZoneId, ZoneState);
}

void APacificationZone::SetSermonProgress(float NewProgress, bool bForce)
{
	const float Step = Rules ? Rules->ProgressReplicationStep : 0.01f;
	if (!bForce && FMath::Abs(NewProgress - SermonProgress) < Step)
	{
		return;
	}
	SermonProgress = NewProgress;
	OnZoneProgressChanged.Broadcast(ZoneId, SermonProgress);
}

float APacificationZone::GetSermonProgress01() const
{
	if (Sermon.bActive && Rules)
	{
		return Sermon.Progress01(GetServerTime(), Rules->SermonDuration);
	}
	return SermonProgress;
}

double APacificationZone::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

bool APacificationZone::IsThreatAlive(AActor* Threat) const
{
	return IsActorAliveForPacification(Threat);
}

bool APacificationZone::IsOfficiantAlive(APawn* Officiant) const
{
	return IsActorAliveForPacification(Officiant);
}

int32 APacificationZone::CountLivingThreats() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Threat : DesignatedThreats)
	{
		if (IsThreatAlive(Threat.Get()))
		{
			++Count;
		}
	}
	return Count;
}

bool APacificationZone::IsThreatInPerimeter() const
{
	if (!Rules)
	{
		return false;
	}
	for (const TWeakObjectPtr<AActor>& WeakThreat : DesignatedThreats)
	{
		AActor* Threat = WeakThreat.Get();
		if (IsThreatAlive(Threat) && HorizontalDistanceToSocket(Threat->GetActorLocation()) <= Rules->PerimeterRadiusCm)
		{
			return true;
		}
	}
	return false;
}

bool APacificationZone::IsStandingStill(const APawn* Pawn) const
{
	if (!Pawn || !Rules)
	{
		return false;
	}
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && Movement->MovementMode != MOVE_Walking && Movement->MovementMode != MOVE_NavWalking)
		{
			// no sermon while falling, jumping, swimming or flying
			return false;
		}
	}
	return Pawn->GetVelocity().SizeSquared2D() <= FMath::Square(static_cast<double>(Rules->MaxStartSpeedCmS));
}

bool APacificationZone::IsOfficiatingAnywhere(const APawn* Pawn)
{
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	// called on a sermon request only, never per frame: a handful of zones per expedition
	for (TActorIterator<APacificationZone> It(World); It; ++It)
	{
		if (It->Sermon.bActive && It->Sermon.Officiant == Pawn)
		{
			return true;
		}
	}
	return false;
}

void APacificationZone::BindThreats()
{
	for (const TWeakObjectPtr<AActor>& WeakThreat : DesignatedThreats)
	{
		if (AActor* Threat = WeakThreat.Get())
		{
			Threat->OnDestroyed.AddUniqueDynamic(this, &APacificationZone::HandleThreatDestroyed);
			if (UIslanderHealthComponent* Health = Threat->FindComponentByClass<UIslanderHealthComponent>())
			{
				Health->OnDeath.AddUniqueDynamic(this, &APacificationZone::HandleThreatDeath);
			}
		}
	}
}

void APacificationZone::UnbindThreats()
{
	for (const TWeakObjectPtr<AActor>& WeakThreat : DesignatedThreats)
	{
		if (AActor* Threat = WeakThreat.Get())
		{
			Threat->OnDestroyed.RemoveDynamic(this, &APacificationZone::HandleThreatDestroyed);
			if (UIslanderHealthComponent* Health = Threat->FindComponentByClass<UIslanderHealthComponent>())
			{
				Health->OnDeath.RemoveDynamic(this, &APacificationZone::HandleThreatDeath);
			}
		}
	}
}

void APacificationZone::RecountThreats()
{
	RemainingThreats = CountLivingThreats();
}

void APacificationZone::HandleThreatDestroyed(AActor* DestroyedActor)
{
	RecountThreats();
}

void APacificationZone::HandleThreatDeath(AActor* Killer)
{
	RecountThreats();
}

double APacificationZone::HorizontalDistanceToSocket(const FVector& Location) const
{
	return FVector::Dist2D(Location, FlagSocket->GetComponentLocation());
}

void APacificationZone::BindOfficiant(APawn* Officiant)
{
	UnbindOfficiant();
	if (!IsValid(Officiant))
	{
		return;
	}
	Officiant->OnTakeAnyDamage.AddDynamic(this, &APacificationZone::HandleOfficiantDamaged);
	Officiant->OnDestroyed.AddDynamic(this, &APacificationZone::HandleOfficiantDestroyed);
	if (UIslanderHealthComponent* Health = Officiant->FindComponentByClass<UIslanderHealthComponent>())
	{
		Health->OnHealthChanged.AddDynamic(this, &APacificationZone::HandleOfficiantHealthChanged);
		Health->OnDeath.AddDynamic(this, &APacificationZone::HandleOfficiantDeath);
	}
	BoundOfficiant = Officiant;
}

void APacificationZone::UnbindOfficiant()
{
	if (APawn* Officiant = BoundOfficiant.Get())
	{
		Officiant->OnTakeAnyDamage.RemoveDynamic(this, &APacificationZone::HandleOfficiantDamaged);
		Officiant->OnDestroyed.RemoveDynamic(this, &APacificationZone::HandleOfficiantDestroyed);
		if (UIslanderHealthComponent* Health = Officiant->FindComponentByClass<UIslanderHealthComponent>())
		{
			Health->OnHealthChanged.RemoveDynamic(this, &APacificationZone::HandleOfficiantHealthChanged);
			Health->OnDeath.RemoveDynamic(this, &APacificationZone::HandleOfficiantDeath);
		}
	}
	BoundOfficiant.Reset();
}

void APacificationZone::HandleOfficiantDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (!Sermon.bActive || DamagedActor != Sermon.Officiant || !Rules)
	{
		return;
	}
	if (Damage > 0.0f && Damage >= Rules->MinInterruptDamage)
	{
		InterruptSermon(ESermonInterruptReason::Damaged);
	}
}

void APacificationZone::HandleOfficiantHealthChanged(float NewHealth, float Delta)
{
	if (!Sermon.bActive || !Rules || Delta >= 0.0f)
	{
		return;
	}
	if (NewHealth <= 0.0f)
	{
		// lethal blow: OnDeath follows, report the death rather than the damage
		InterruptSermon(ESermonInterruptReason::OfficiantDied);
	}
	else if (-Delta >= Rules->MinInterruptDamage)
	{
		InterruptSermon(ESermonInterruptReason::Damaged);
	}
}

void APacificationZone::HandleOfficiantDeath(AActor* Killer)
{
	if (Sermon.bActive)
	{
		InterruptSermon(ESermonInterruptReason::OfficiantDied);
	}
}

void APacificationZone::HandleOfficiantDestroyed(AActor* DestroyedActor)
{
	if (Sermon.bActive && DestroyedActor == Sermon.Officiant)
	{
		InterruptSermon(ESermonInterruptReason::OfficiantLost);
	}
}

void APacificationZone::OnRep_ZoneState()
{
	OnZoneStateChanged.Broadcast(ZoneId, ZoneState);
}

void APacificationZone::OnRep_Sermon()
{
	// clients only show the sermon (officiant, start time): the progress bar reads GetSermonProgress01()
	OnZoneProgressChanged.Broadcast(ZoneId, GetSermonProgress01());
}

void APacificationZone::OnRep_SermonProgress()
{
	OnZoneProgressChanged.Broadcast(ZoneId, SermonProgress);
}

void APacificationZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(APacificationZone, ZoneId, COND_InitialOnly);
	DOREPLIFETIME(APacificationZone, ZoneState);
	DOREPLIFETIME(APacificationZone, Sermon);
	DOREPLIFETIME(APacificationZone, SermonProgress);
	DOREPLIFETIME(APacificationZone, Flag);
	DOREPLIFETIME(APacificationZone, RemainingThreats);
	DOREPLIFETIME(APacificationZone, ExpeditionId);
}
