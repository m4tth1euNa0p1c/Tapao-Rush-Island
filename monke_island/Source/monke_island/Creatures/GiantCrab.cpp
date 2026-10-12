// monke_island

#include "Creatures/GiantCrab.h"
#include "Arena/ArenaZone.h"
#include "Creatures/GiantCrabBody.h"
#include "Creatures/GiantCrabFeedback.h"
#include "Creatures/GiantCrabInternal.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"
#include "Creatures/Hostility.h"
#include "Creatures/MobHealthBarComponent.h"
#include "Creatures/TikiAnimInstance.h"
#include "Creatures/TikiDirector.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Stealth/StealthSubsystem.h"
#include "monke_island.h"
#include "AIController.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AGiantCrab::AGiantCrab()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	// kit: capsule 0.95 m x 1 m; the bean walks under the belly (1.25 m): pawns pass through the capsule (the hit
	// bodies on its bones stop them at the legs, claws and shell), shots and blows strike it
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(95.0f, 100.0f);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->MaxWalkSpeed = GiantCrabInternal::KitSideSpeed;
	Move->bRunPhysicsWithNoController = true;
	Move->Mass = 850.0f;

	// the kit faces +Y like the bean: turned to +X, its feet on the ground
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -100.0f), FRotator(0.0f, -90.0f, 0.0f));
	// clips crossfaded by the native animation instance of the tiki (no AnimBP)
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UTikiAnimInstance::StaticClass());
	GetMesh()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// the server reads the claw sockets (grab) and moves the hit bodies even when nobody looks at it
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Health = CreateDefaultSubobject<UIslanderHealthComponent>(TEXT("Health"));
	Health->SetIsReplicated(true);

	HealthBar = CreateDefaultSubobject<UMobHealthBarComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetCapsuleComponent());
	// above the shell (back at 1.95 m)
	HealthBar->Margin = 70.0f;

	Bodies = CreateDefaultSubobject<UGiantCrabBodyComponent>(TEXT("HitBodies"));
	Feedback = CreateDefaultSubobject<UGiantCrabFeedbackComponent>(TEXT("Feedback"));

	// its legs make their own steps (UGiantCrabFeedbackComponent): the generic footsteps of the characters may skip it
	Tags.Add(TEXT("NoGenericFootsteps"));
}

void AGiantCrab::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AGiantCrab, Rules, COND_InitialOnly);
	DOREPLIFETIME(AGiantCrab, Action);
	DOREPLIFETIME(AGiantCrab, Phase);
	DOREPLIFETIME(AGiantCrab, bEngaged);
	DOREPLIFETIME(AGiantCrab, HeldPrey);
	DOREPLIFETIME(AGiantCrab, bRising);
}

bool AGiantCrab::IsDead() const
{
	return Health && Health->IsDead();
}

bool AGiantCrab::IsPrey(const AActor* Actor)
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	if (!Character || Character->IsA<AGiantCrab>() || !IsValid(Character))
	{
		return false;
	}
	const UIslanderHealthComponent* PreyHealth = Character->FindComponentByClass<UIslanderHealthComponent>();
	return PreyHealth && !PreyHealth->IsDead();
}

void AGiantCrab::BeginPlay()
{
	// the boss bar at the bottom of the screen stands for the bar above its shell
	if (Rules && Rules->bBossBar && HealthBar)
	{
		HealthBar->MaxDistance = 0.0f;
	}
	Super::BeginPlay();
	Health->OnDeath.AddDynamic(this, &AGiantCrab::HandleDeath);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("GiantCrab %s: no rules (DA_GiantCrabRules), it stays still"), *GetName());
		return;
	}
	// every machine: the hit bodies on its bones and the fixed shapes that stop the pawns, its steps and cues, its boss
	// bar, its arena
	const int32 BodyCount = Bodies ? Bodies->Build(*Rules, GetMesh()) : 0;
	ApplyBodyBlocking();
	if (Feedback)
	{
		Feedback->Setup(Rules, GetMesh(), Bodies);
	}
	if (UBossBarSubsystem* Bars = UBossBarSubsystem::Get(this))
	{
		Bars->Register(this);
	}
	Home = GetActorLocation();
	for (TActorIterator<AArenaZone> It(GetWorld()); It; ++It)
	{
		if (It->IsInside(Home))
		{
			Arena = *It;
			break;
		}
	}
	// a client that joins late sees the phase it is in, the crab rising, and holds its own pawn if the crab holds it
	OnRep_Phase();
	if (!HasAuthority())
	{
		OnRep_Rising();
		SyncLocalHeld();
		return;
	}
	Health->MaxHealth = Rules->MaxHealth;
	Health->ResetHealth();
	GetCharacterMovement()->MaxAcceleration = Rules->MaxAcceleration;
	const double Now = GetWorld()->GetTimeSeconds();
	NextTaunt = Now + FMath::FRandRange(Rules->TauntIntervalMin, Rules->TauntIntervalMax);
	CircleSign = FMath::RandBool() ? 1.0f : -1.0f;
	SetState(EGiantCrabState::Idle);
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: %.0f health, %d phases, %d hit bodies, %d pawn pillars, %s"), *GetName(), Rules->MaxHealth,
		Rules->Phases.Num(), BodyCount, Bodies ? Bodies->GetBlockers().Num() : 0,
		Arena.IsValid() ? *FString::Printf(TEXT("held in %s (%.0f cm)"), *Arena->GetName(), Arena->Radius)
			: *FString::Printf(TEXT("leash %.0f cm"), Rules->LeashRadiusCm));
}

void AGiantCrab::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBossBarSubsystem* Bars = UBossBarSubsystem::Get(this))
	{
		Bars->Unregister(this);
	}
	// never leave a pawn hanging, its movement off, from a crab that is gone (R-33)
	if (HasAuthority())
	{
		ReleaseHeldPrey(true);
	}
	else
	{
		HeldPrey = nullptr;
		SyncLocalHeld();
	}
	Super::EndPlay(EndPlayReason);
}

void AGiantCrab::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && Rules && !IsDead())
	{
		ServerThink(DeltaSeconds);
		// the caught one hangs from the claw until the throw
		UpdateHeldPrey();
		RecordWeakPoints();
	}
	else if (!HasAuthority())
	{
		// a player held: its own machine holds it at the claw it sees (T-304)
		if (ACharacter* Held = LocalHeld.Get(); Held && IsPrey(Held) && !IsKnockedOut(Held))
		{
			HoldAtClaw(Held);
		}
	}
	// the characters around it, dead or alive: their moves ignore its animated bodies, the pinned ones are freed (T-328)
	UpdatePawns(DeltaSeconds);
	UpdateClips();
}

float AGiantCrab::StateTime() const
{
	return static_cast<float>(GetWorld()->GetTimeSeconds() - StateStart);
}

float AGiantCrab::ClipTime() const
{
	return StateTime() * FMath::Max(0.05f, Action.PlayRate);
}

float AGiantCrab::ClipLength(const UAnimSequenceBase* Clip)
{
	return Clip ? Clip->GetPlayLength() : 0.0f;
}

float AGiantCrab::FindNotifyTime(const UAnimSequenceBase* Clip, FName Notify, float Fallback)
{
	if (Clip && !Notify.IsNone())
	{
		for (const FAnimNotifyEvent& Event : Clip->Notifies)
		{
			if (Event.NotifyName == Notify)
			{
				return Event.GetTriggerTime();
			}
		}
	}
	return Fallback;
}

bool AGiantCrab::IsInTerritory(const FVector& Location) const
{
	if (const AArenaZone* Zone = Arena.Get())
	{
		return Zone->IsInside(Location);
	}
	return FVector::Dist2D(Location, Home) <= (Rules ? Rules->LeashRadiusCm : 2500.0f);
}

const FGiantCrabPhaseRules* AGiantCrab::GetPhaseRules() const
{
	return Rules ? Rules->GetPhase(Phase) : nullptr;
}

const FGiantCrabPhaseRules& AGiantCrab::CurrentPhase() const
{
	static const FGiantCrabPhaseRules Neutral;
	const FGiantCrabPhaseRules* PhaseRules = GetPhaseRules();
	return PhaseRules ? *PhaseRules : Neutral;
}

FVector AGiantCrab::GetGroundPoint() const
{
	return GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

// ---------------------------------------------------------------- decisions (server)

void AGiantCrab::ServerThink(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextRetarget)
	{
		NextRetarget = Now + Rules->RetargetInterval;
		ChooseTarget();
	}
	// it fights: a prey, or a blow a moment ago (the boss bar of the players around)
	bEngaged = Target.IsValid() || Now - LastHurtTime <= Rules->BossBarLingerSeconds;
	// whoever stays under its shell: it drops on them
	if (CheckUnderShell(DeltaSeconds))
	{
		return;
	}
	FireServerMoments();

	switch (Action.State)
	{
	case EGiantCrabState::Idle:
	case EGiantCrabState::Approach:
		if (bRoarPending)
		{
			// back on its feet after a new phase came while it lay on its back: its roar now
			bRoarPending = false;
			SetState(EGiantCrabState::Roar);
		}
		else if (Now < StunnedUntil)
		{
			// out of breath after a charge: the opening
			if (Action.State != EGiantCrabState::Idle)
			{
				SetState(EGiantCrabState::Idle);
			}
		}
		else if (Target.IsValid())
		{
			Approach(DeltaSeconds);
		}
		else if (Rules->Taunt && Now >= NextTaunt)
		{
			SetState(EGiantCrabState::Taunt);
		}
		else if (Action.State != EGiantCrabState::Idle)
		{
			SetState(EGiantCrabState::Idle);
		}
		break;
	case EGiantCrabState::Attack:
		UpdateAttack(DeltaSeconds);
		break;
	case EGiantCrabState::Throw:
		UpdateThrow();
		break;
	case EGiantCrabState::Taunt:
		if (StateTime() >= ClipLength(Rules->Taunt) || Target.IsValid())
		{
			NextTaunt = Now + FMath::FRandRange(Rules->TauntIntervalMin, Rules->TauntIntervalMax);
			SetState(EGiantCrabState::Idle);
		}
		break;
	case EGiantCrabState::Block:
		if (StateTime() >= Rules->BlockSeconds)
		{
			SetState(EGiantCrabState::Idle);
		}
		break;
	case EGiantCrabState::HitReact:
		if (StateTime() >= ClipLength(Rules->HitReact))
		{
			SetState(EGiantCrabState::Idle);
		}
		break;
	case EGiantCrabState::Stagger:
		if (StateTime() >= ClipLength(Rules->StaggerFlip))
		{
			SetState(EGiantCrabState::Flipped);
		}
		break;
	case EGiantCrabState::Flipped:
		if (StateTime() >= Rules->FlippedSeconds)
		{
			SetState(EGiantCrabState::Recover);
		}
		break;
	case EGiantCrabState::Recover:
		if (StateTime() >= ClipLength(Rules->Recover))
		{
			SetState(EGiantCrabState::Idle);
		}
		break;
	case EGiantCrabState::Roar:
		UpdateRoar();
		break;
	case EGiantCrabState::ChargeWindup:
		UpdateChargeWindup(DeltaSeconds);
		break;
	case EGiantCrabState::Charge:
		UpdateCharge(DeltaSeconds);
		break;
	case EGiantCrabState::BellySlam:
		UpdateBellySlam();
		break;
	default:
		break;
	}
}

void AGiantCrab::GatherPrey(FPreyList& OutPrey) const
{
	OutPrey.Reset();
	UHostilitySubsystem* Registry = UHostilitySubsystem::Get(this);
	if (!Registry)
	{
		return;
	}
	// copied first: hurting them may wake code that reads the registry again
	for (const FHostileCombatant& Combatant : Registry->GetCombatants())
	{
		ACharacter* Character = Combatant.Character.Get();
		if (Character && Character != this && Combatant.Faction != EFaction::Crab && Combatant.Health.IsValid() && !Combatant.Health->IsDead()
			&& IsValid(Character))
		{
			OutPrey.Add(Character);
		}
	}
}

bool AGiantCrab::SensesPrey(const ACharacter* Prey, float Gap, const UStealthSubsystem* Stealth, ATikiDirector* Bushes) const
{
	// its legs feel whatever touches them, seen or not
	if (Gap <= Rules->TouchDistanceCm)
	{
		return true;
	}
	// sight of the stealth rules (T-262): crouched, or at night, seen only at contact; in a bush closer still (T-207)
	if (Stealth)
	{
		return Stealth->CanSee(this, Prey, Rules->SightRangeCm);
	}
	// without them: the bush rule of T-207
	FVector Bush;
	return !(Bushes && Prey->bIsCrouched && FVector::Dist2D(Prey->GetActorLocation(), GetActorLocation()) > Rules->BushSpotDistanceCm
		&& Bushes->FindNearestBush(Prey->GetActorLocation(), Rules->BushHideRadiusCm, Bush));
}

void AGiantCrab::ChooseTarget()
{
	const double Now = GetWorld()->GetTimeSeconds();
	FPreyList Prey;
	GatherPrey(Prey);
	const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
	ATikiDirector* Bushes = !Stealth && Rules->BushHideRadiusCm > 0.0f ? ATikiDirector::Get(this) : nullptr;
	const FVector Me = GetActorLocation();
	const float MyRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	TArray<AActor*, TInlineAllocator<16>> Candidates;
	TArray<float> Distances;
	int32 Current = INDEX_NONE;
	for (ACharacter* Character : Prey)
	{
		const FVector There = Character->GetActorLocation();
		if (!IsInTerritory(There))
		{
			continue;
		}
		const bool bCurrent = Character == Target.Get();
		const bool bSensed = SensesPrey(Character, GiantCrabMath::EdgeDistance(Me, MyRadius, There, GiantCrabInternal::PreyRadius(Character)), Stealth, Bushes);
		if (bCurrent && bSensed)
		{
			LastSensedTarget = Now;
		}
		// a prey it lost track of is still hunted a moment
		if (!bSensed && !(bCurrent && Now - LastSensedTarget <= Rules->TargetMemorySeconds))
		{
			continue;
		}
		if (bCurrent)
		{
			Current = Candidates.Num();
		}
		Candidates.Add(Character);
		Distances.Add(static_cast<float>(FVector::Dist2D(There, Me)));
	}
	// it senses nobody: the noise of a prey in its ground (a shot, a run, a voice) gives it away
	FStealthNoise Noise;
	if (Candidates.IsEmpty() && Stealth && Stealth->FindHeardNoise(this, Rules->HearingScale, Noise))
	{
		AActor* Maker = Noise.Instigator.Get();
		APawn* MakerPawn = Cast<APawn>(Maker);
		if (!MakerPawn && Maker)
		{
			MakerPawn = Maker->GetInstigator() ? Maker->GetInstigator() : Cast<APawn>(Maker->GetOwner());
		}
		if (MakerPawn && IsPrey(MakerPawn) && IsInTerritory(MakerPawn->GetActorLocation()))
		{
			Candidates.Add(MakerPawn);
			Distances.Add(static_cast<float>(FVector::Dist2D(MakerPawn->GetActorLocation(), Me)));
			LastSensedTarget = Now;
			UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: hears %s"), *GetName(), *MakerPawn->GetName());
		}
	}
	const int32 Pick = GiantCrabMath::PickTarget(Distances, Current, Rules->SwitchMarginCm);
	AActor* NewTarget = Candidates.IsValidIndex(Pick) ? Candidates[Pick] : nullptr;
	if (NewTarget != Target.Get())
	{
		Target = NewTarget;
		LastSensedTarget = Now;
		Planned = EGiantCrabAttack::None;
		CircleSign = FMath::RandBool() ? 1.0f : -1.0f;
		UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: hunts %s"), *GetName(), NewTarget ? *NewTarget->GetName() : TEXT("nothing"));
	}
}

float AGiantCrab::TurnToward(const FVector& Point, float DeltaSeconds, float RateScale)
{
	const float Angle = GiantCrabMath::SignedAngle(GetActorForwardVector(), Point - GetActorLocation());
	const float MaxStep = Rules->TurnRateDegrees * CurrentPhase().TurnRateScale * RateScale * DeltaSeconds;
	const float Step = FMath::Clamp(Angle, -MaxStep, MaxStep);
	if (!FMath::IsNearlyZero(Step))
	{
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Step, 0.0f));
	}
	return Angle - Step;
}

void AGiantCrab::PlanAttack()
{
	const FGiantCrabPhaseRules& PhaseRules = CurrentPhase();
	const double Now = GetWorld()->GetTimeSeconds();
	const AActor* Prey = Target.Get();
	const float Edge = Prey ? GiantCrabMath::EdgeDistance(GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleRadius(), Prey->GetActorLocation(),
		GiantCrabInternal::PreyRadius(Prey)) : 0.0f;
	// a charge only from afar, once it has its breath back
	const bool bCharge = PhaseRules.ChargeWeight > 0.0f && Now >= NextCharge && Edge >= Rules->Charge.MinDistanceCm;
	const TArray<float> Reaches = { Rules->Swipe.ReachCm, Rules->Slam.ReachCm, Rules->Grab.ReachCm, Rules->Stomp.ReachCm, Rules->Charge.MaxDistanceCm };
	const TArray<float> Weights = { Rules->Swipe.Weight, Rules->Slam.Weight, Rules->Grab.Weight, PhaseRules.StompWeight,
		bCharge ? PhaseRules.ChargeWeight : 0.0f };
	// every blow "reaches" at 0: a pure draw by the weights
	const int32 Pick = GiantCrabMath::PickAttack(Reaches, Weights, 0.0f, FMath::FRand());
	Planned = Pick == INDEX_NONE ? EGiantCrabAttack::Swipe : static_cast<EGiantCrabAttack>(Pick + 1);
}

void AGiantCrab::Walk(const FVector& InDirection, float Speed)
{
	FVector Direction = FVector(InDirection.X, InDirection.Y, 0.0f);
	if (Direction.IsNearlyZero())
	{
		return;
	}
	// never a step out of its ground: the step a metre ahead is held inside, the walk follows the edge
	const FVector Me = GetActorLocation();
	FVector Ahead = Me + Direction.GetSafeNormal() * 100.0f;
	const float Body = GetCapsuleComponent()->GetScaledCapsuleRadius() + Rules->EdgeMarginCm;
	const bool bHeld = Arena.IsValid()
		? ArenaZoneMath::ClampIntoCircle(Arena->GetActorLocation(), FMath::Max(0.0f, Arena->Radius - Body), Ahead)
		: ArenaZoneMath::ClampIntoCircle(Home, FMath::Max(0.0f, Rules->LeashRadiusCm - Body), Ahead);
	if (bHeld)
	{
		Direction = Ahead - Me;
		Direction.Z = 0.0f;
		if (Direction.Size() < 5.0f)
		{
			return;
		}
	}
	Direction = Direction.GetSafeNormal() * FMath::Min(1.0f, InDirection.Size2D());
	// the crab's own gait: fast sideways, slow forward, both quicker in the later phases
	const FVector Local = GetActorRotation().UnrotateVector(Direction);
	const float Sideways = FMath::Abs(Local.Y) / FMath::Max(UE_KINDA_SMALL_NUMBER, FMath::Abs(Local.X) + FMath::Abs(Local.Y));
	GetCharacterMovement()->MaxWalkSpeed = Speed > 0.0f ? Speed
		: FMath::Lerp(Rules->ForwardSpeed, Rules->SideSpeed, Sideways) * CurrentPhase().MoveSpeedScale;
	AddMovementInput(Direction.GetSafeNormal(), Direction.Size(), true);
}

void AGiantCrab::Approach(float DeltaSeconds)
{
	AActor* Prey = Target.Get();
	const FVector Me = GetActorLocation();
	const FVector There = Prey->GetActorLocation();
	const float Edge = GiantCrabMath::EdgeDistance(Me, GetCapsuleComponent()->GetScaledCapsuleRadius(), There, GiantCrabInternal::PreyRadius(Prey));
	const double Now = GetWorld()->GetTimeSeconds();
	if (Planned == EGiantCrabAttack::None || (Planned == EGiantCrabAttack::Charge && (Edge < Rules->Charge.MinDistanceCm || Now < NextCharge)))
	{
		PlanAttack();
	}
	if (Action.State != EGiantCrabState::Approach)
	{
		SetState(EGiantCrabState::Approach);
	}

	// a charge across the arena: from afar, once it may strike again
	if (Planned == EGiantCrabAttack::Charge && Edge <= Rules->Charge.MaxDistanceCm && Now >= NextAttack
		&& FMath::Abs(There.Z - Me.Z) <= GiantCrabInternal::MaxReachHeight)
	{
		GetCharacterMovement()->StopMovementImmediately();
		Planned = EGiantCrabAttack::None;
		SetState(EGiantCrabState::ChargeWindup);
		return;
	}
	const FGiantCrabAttackRules* Next = GetBlow(Planned);

	// far away: its side to the prey, it scuttles there
	if (Edge > Rules->ScuttleDistanceCm)
	{
		const float ToSide = GiantCrabMath::SideTurn(GetActorForwardVector(), There - Me);
		const float MaxStep = Rules->TurnRateDegrees * CurrentPhase().TurnRateScale * DeltaSeconds;
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + FMath::Clamp(ToSide, -MaxStep, MaxStep), 0.0f));
		Walk((There - Me).GetSafeNormal2D(), Rules->ScuttleSpeed * CurrentPhase().MoveSpeedScale);
		return;
	}

	// close: claws to the front, the planned blow when its reach covers the prey (the stomp hits the flanks: no facing)
	const float AngleLeft = TurnToward(There, DeltaSeconds);
	if (Next && Now >= NextAttack && Edge <= Next->ReachCm && (!Next->bNeedsFacing || FMath::Abs(AngleLeft) <= Rules->StrikeAngleDegrees)
		&& FMath::Abs(There.Z - Me.Z) <= GiantCrabInternal::MaxReachHeight)
	{
		GetCharacterMovement()->StopMovementImmediately();
		SetState(EGiantCrabState::Attack, Planned);
		Planned = EGiantCrabAttack::None;
		return;
	}
	// closes in until the planned blow reaches, circling its prey sideways all the while
	Walk(GiantCrabMath::WalkDirection(There - Me, Edge, Next ? Next->ReachCm : Rules->Swipe.ReachCm, Rules->CircleShare, CircleSign));
}

const FGiantCrabAttackRules* AGiantCrab::GetBlow(EGiantCrabAttack Attack) const
{
	if (!Rules)
	{
		return nullptr;
	}
	switch (Attack)
	{
	case EGiantCrabAttack::Swipe:
		return &Rules->Swipe;
	case EGiantCrabAttack::Slam:
		return &Rules->Slam;
	case EGiantCrabAttack::Grab:
		return &Rules->Grab;
	case EGiantCrabAttack::Stomp:
		return &Rules->Stomp;
	default:
		return nullptr;
	}
}

UAnimSequenceBase* AGiantCrab::GetAttackClip(EGiantCrabAttack Attack) const
{
	const FGiantCrabAttackRules* Blow = GetBlow(Attack);
	if (!Blow)
	{
		return nullptr;
	}
	if (Blow->Clip)
	{
		return Blow->Clip;
	}
	// A_GiantCrab_LegStomp comes with T-260: until then the hit reaction jolts the body, the waves are the same
	return Attack == EGiantCrabAttack::Stomp ? Rules->HitReact.Get() : nullptr;
}

// ---------------------------------------------------------------- blows (server)

void AGiantCrab::UpdateAttack(float DeltaSeconds)
{
	const FGiantCrabAttackRules* Blow = GetBlow(Action.Attack);
	const UAnimSequenceBase* Clip = GetAttackClip(Action.Attack);
	if (!Blow || !Clip)
	{
		SetState(EGiantCrabState::Idle);
		return;
	}
	const float Time = ClipTime();
	const float Rate = FMath::Max(0.05f, Action.PlayRate);
	// the windup still follows the prey a little
	if (Time < Blow->HitStart && Target.IsValid())
	{
		TurnToward(Target->GetActorLocation(), DeltaSeconds * 0.5f);
	}
	// the lunge of the kit's root motion, walked by the movement (and held in its ground)
	if (Time >= Blow->LungeStart && Time < Blow->LungeEnd && Blow->LungeCm > 0.0f)
	{
		const FVector Forward = GetActorForwardVector();
		if (IsInTerritory(GetActorLocation() + Forward * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Rules->EdgeMarginCm)))
		{
			GetCharacterMovement()->MaxWalkSpeed = Blow->LungeCm / FMath::Max(0.05f, (Blow->LungeEnd - Blow->LungeStart) / Rate);
			AddMovementInput(Forward, 1.0f, true);
		}
	}
	// the hit window, even when a frame steps over it; the stomp hurts at its moments instead
	const bool bInWindow = Time >= Blow->HitStart && PreviousClipTime <= Blow->HitEnd;
	PreviousClipTime = Time;
	if (Action.Attack != EGiantCrabAttack::Stomp && bInWindow)
	{
		AActor* First = StrikeCone(*Blow);
		if (Action.Attack == EGiantCrabAttack::Slam && !bShockwaveDone)
		{
			bShockwaveDone = true;
			const FVector Center = GetActorLocation() + GetActorForwardVector() * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Blow->ReachCm * 0.6f);
			Shockwave(Center, Blow->ShockwaveRadiusCm, Blow->ShockwaveDamage, Blow->KnockbackSpeed * 0.5f, Blow->KnockbackUp * 1.5f);
		}
		if (Action.Attack == EGiantCrabAttack::Grab && First && Rules->ThrowClip)
		{
			// caught in the claw (unless the blow just killed or knocked it out): carried until the throw, its movement off
			// so that it neither walks, jumps nor takes flight meanwhile, on the server and on the machine of a player
			// held (owner 2026-10-08, T-304; the islander's movement drops a MOVE_Flying it did not ask for back to
			// falling, and leaves MOVE_None alone)
			if (ACharacter* Held = Cast<ACharacter>(First); Held && IsPrey(Held) && !IsKnockedOut(Held))
			{
				SetState(EGiantCrabState::Throw);
				CatchPrey(Held);
				UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: caught %s"), *GetName(), *Held->GetName());
				return;
			}
		}
	}
	if (Time >= ClipLength(Clip))
	{
		if (TryCombo(Action.Attack))
		{
			return;
		}
		NextAttack = GetWorld()->GetTimeSeconds() + Blow->Recovery * CurrentPhase().RecoveryScale;
		SetState(Target.IsValid() ? EGiantCrabState::Approach : EGiantCrabState::Idle);
	}
}

void AGiantCrab::UpdateThrow()
{
	const float Time = ClipTime();
	if (!bThrowDone && Time >= Rules->ThrowRelease)
	{
		ReleaseThrow();
	}
	if (Time >= FMath::Max(ClipLength(Rules->ThrowClip), Rules->ThrowRelease + 0.1f))
	{
		if (TryCombo(EGiantCrabAttack::Grab))
		{
			return;
		}
		NextAttack = GetWorld()->GetTimeSeconds() + Rules->Grab.Recovery * CurrentPhase().RecoveryScale;
		SetState(EGiantCrabState::Idle);
	}
}

bool AGiantCrab::TryCombo(EGiantCrabAttack From)
{
	const FGiantCrabPhaseRules& PhaseRules = CurrentPhase();
	const AActor* Prey = Target.Get();
	if (!Prey || PhaseRules.ComboChance <= 0.0f || PhaseRules.Combos.IsEmpty() || FMath::FRand() >= PhaseRules.ComboChance)
	{
		return false;
	}
	const FVector Me = GetActorLocation();
	const FVector There = Prey->GetActorLocation();
	const float Edge = GiantCrabMath::EdgeDistance(Me, GetCapsuleComponent()->GetScaledCapsuleRadius(), There, GiantCrabInternal::PreyRadius(Prey));
	for (const FGiantCrabComboLink& Link : PhaseRules.Combos)
	{
		const FGiantCrabAttackRules* Next = Link.From == From ? GetBlow(Link.To) : nullptr;
		if (!Next || !GetAttackClip(Link.To) || (Link.To == EGiantCrabAttack::Grab && !Rules->ThrowClip)
			|| Edge > Next->ReachCm * Rules->ComboReachSlack || FMath::Abs(There.Z - Me.Z) > GiantCrabInternal::MaxReachHeight)
		{
			continue;
		}
		UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: combo %s -> %s"), *GetName(), *UEnum::GetValueAsString(From), *UEnum::GetValueAsString(Link.To));
		SetState(EGiantCrabState::Attack, Link.To);
		return true;
	}
	return false;
}

AActor* AGiantCrab::StrikeCone(const FGiantCrabAttackRules& Blow)
{
	AActor* First = nullptr;
	const FVector Me = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FPreyList Prey;
	GatherPrey(Prey);
	for (ACharacter* Victim : Prey)
	{
		if (Struck.Contains(Victim))
		{
			continue;
		}
		const FVector There = Victim->GetActorLocation();
		if (FMath::Abs(There.Z - Me.Z) > GiantCrabInternal::MaxReachHeight
			|| GiantCrabMath::EdgeDistance(Me, Radius, There, GiantCrabInternal::PreyRadius(Victim)) > Blow.ReachCm
			|| !GiantCrabMath::IsInFront(Forward, There - Me, Blow.ConeDegrees))
		{
			continue;
		}
		Struck.Add(Victim);
		HurtPrey(Victim, Blow.Damage);
		if (Action.Attack != EGiantCrabAttack::Grab)
		{
			Victim->LaunchCharacter(GiantCrabMath::Knockback(Me, There, Blow.KnockbackSpeed, Blow.KnockbackUp), true, true);
		}
		First = First ? First : Victim;
		UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: %s strikes %s (%.0f)"), *GetName(), *UEnum::GetValueAsString(Action.Attack),
			*Victim->GetName(), Blow.Damage * CurrentPhase().DamageScale);
	}
	if (First)
	{
		MulticastCue(EGiantCrabCue::Hit, First->GetActorLocation());
	}
	return First;
}

void AGiantCrab::Shockwave(const FVector& Center, float Radius, float Damage, float PushSpeed, float PushUp, float KnockoutRadius)
{
	if (Radius <= 0.0f)
	{
		return;
	}
	FPreyList Prey;
	GatherPrey(Prey);
	for (ACharacter* Victim : Prey)
	{
		const FVector There = Victim->GetActorLocation();
		const float Distance = static_cast<float>(FVector::Dist2D(There, Center));
		if (Distance > Radius || FMath::Abs(There.Z - Center.Z) > GiantCrabInternal::MaxReachHeight)
		{
			continue;
		}
		// thrown first: a dead one cannot be knocked out, a killing wave still flings the body
		Fling(Victim, GiantCrabMath::Knockback(Center, There, PushSpeed, PushUp), Distance <= KnockoutRadius);
		if (Damage > 0.0f)
		{
			HurtPrey(Victim, Damage);
		}
	}
}

void AGiantCrab::HurtPrey(ACharacter* Prey, float Damage)
{
	if (Prey && Damage > 0.0f)
	{
		UGameplayStatics::ApplyDamage(Prey, Damage * CurrentPhase().DamageScale, GetController(), this, nullptr);
	}
}

void AGiantCrab::Fling(ACharacter* Prey, const FVector& Velocity, bool bKnockout)
{
	if (!Prey)
	{
		return;
	}
	if (AIslanderCharacter* Player = Cast<AIslanderCharacter>(Prey); Player && bKnockout)
	{
		// a ragdoll under real gravity, on every machine; it stands up where it lands
		Player->Knockout(Velocity);
		if (Player->IsRagdoll())
		{
			return;
		}
	}
	Prey->LaunchCharacter(Velocity, true, true);
}

void AGiantCrab::ReleaseThrow()
{
	bThrowDone = true;
	ACharacter* Held = HeldPrey;
	// let go: back to falling (a tiki is launched from there), or left alone when it died or was knocked out meanwhile
	ReleaseHeldPrey(true);
	if (!IsValid(Held) || !IsPrey(Held) || IsKnockedOut(Held))
	{
		return;
	}
	// flung over the left claw, up at the angle: the player much farther than a tiki, as a ragdoll under gravity for
	// the whole flight, standing up where it lands (owner 2026-10-08)
	const AIslanderCharacter* Player = Cast<AIslanderCharacter>(Held);
	const float Speed = Player ? Rules->ThrowSpeedPlayer : Rules->ThrowSpeed;
	const FVector Velocity = GiantCrabMath::ThrowVelocity(GetActorRotation(), Speed, Player ? Rules->ThrowAngleDegreesPlayer : Rules->ThrowAngleDegrees);
	// knocked out before the damage: a dead one cannot be knocked out, and a killing throw still flings the body; tikis
	// and a player whose body could not ragdoll (no physics asset) are launched at the same speed
	Fling(Held, Velocity, true);
	HurtPrey(Held, Rules->ThrowDamage);
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: throws %s at %.0f cm/s%s"), *GetName(), *Held->GetName(), Speed,
		Player && Player->IsRagdoll() ? TEXT(" (ragdoll)") : TEXT(""));
}

// ---------------------------------------------------------------- states and clips (every machine)

void AGiantCrab::SetState(EGiantCrabState NewState, EGiantCrabAttack Attack)
{
	if (HeldPrey && NewState != EGiantCrabState::Throw)
	{
		// interrupted (blow, roar, death): the caught one falls free (a dead one keeps its ragdoll)
		ReleaseHeldPrey(true);
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Action.State == EGiantCrabState::Attack && Action.Attack == EGiantCrabAttack::Slam)
	{
		// the claw of the slam leaves the sand with the slam (T-305: a late shot still finds the window it saw)
		StuckClawTo = FMath::Min(StuckClawTo, Now);
	}
	if (NewState == EGiantCrabState::Stagger || NewState == EGiantCrabState::Flipped)
	{
		Action.bOnBack = true;
	}
	else if (NewState != EGiantCrabState::Dead)
	{
		Action.bOnBack = false;
	}
	Action.State = NewState;
	Action.Attack = Attack;
	Action.PlayRate = GiantCrabInternal::UsesAttackRate(NewState) ? FMath::Clamp(CurrentPhase().AttackPlayRate, 0.25f, 4.0f) : 1.0f;
	++Action.Serial;
	StateStart = Now;
	// the clients play the clip as far in as the action is old when it reaches them (T-305, R-24 and R-32)
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Action.StartServerTime = static_cast<float>(GameState ? GameState->GetServerWorldTimeSeconds() : Now);
	if (NewState == EGiantCrabState::Attack && Attack == EGiantCrabAttack::Slam && Rules)
	{
		// the claw will be stuck in the sand between these times (server clock)
		StuckClawFrom = Now + Rules->SlamStuckStart / Action.PlayRate;
		StuckClawTo = Now + Rules->SlamStuckEnd / Action.PlayRate;
	}
	PreviousClipTime = -1.0f;
	Struck.Reset();
	bShockwaveDone = false;
	bThrowDone = false;
	GetActionMoments(Action, ServerMoments);
	ServerMomentsDone = 0;
	if (NewState != EGiantCrabState::Approach && NewState != EGiantCrabState::Idle && NewState != EGiantCrabState::Attack
		&& NewState != EGiantCrabState::Charge)
	{
		GetCharacterMovement()->StopMovementImmediately();
	}
	ForceNetUpdate();
	OnRep_Action();
}

void AGiantCrab::OnRep_Action()
{
	UpdateClips();
	ApplyBodyBlocking();
	if (Feedback && GetNetMode() != NM_DedicatedServer)
	{
		// an action received late plays its moments from its age (no roar nor death cry replayed for a late player)
		TArray<FGiantCrabMoment> Moments;
		GetActionMoments(Action, Moments);
		Feedback->OnActionStarted(Moments, ActionAge());
	}
}

void AGiantCrab::OnRep_Phase()
{
	if (Feedback)
	{
		const FGiantCrabPhaseRules* PhaseRules = GetPhaseRules();
		Feedback->SetFrenzy(PhaseRules && PhaseRules->bFrenzy);
	}
}

UAnimSequenceBase* AGiantCrab::GetStateClip(EGiantCrabState State, EGiantCrabAttack Attack) const
{
	if (!Rules)
	{
		return nullptr;
	}
	switch (State)
	{
	case EGiantCrabState::Attack:
		return GetAttackClip(Attack);
	case EGiantCrabState::Throw:
		return Rules->ThrowClip;
	case EGiantCrabState::Taunt:
		return Rules->Taunt;
	case EGiantCrabState::Block:
		return Rules->BlockLoop ? Rules->BlockLoop : Rules->BlockStart;
	case EGiantCrabState::HitReact:
		return Rules->HitReact;
	case EGiantCrabState::Stagger:
		return Rules->StaggerFlip;
	case EGiantCrabState::Flipped:
		return Rules->FlippedLoop;
	case EGiantCrabState::Recover:
		return Rules->Recover;
	case EGiantCrabState::Dead:
		return Action.bOnBack && Rules->DeathFlipped ? Rules->DeathFlipped : Rules->Death;
	// the clips of T-260, and the old ones that stand in for them meanwhile
	case EGiantCrabState::Roar:
		return Rules->Roar.Clip ? Rules->Roar.Clip : Rules->Taunt;
	case EGiantCrabState::ChargeWindup:
		return Rules->Charge.WindupClip ? Rules->Charge.WindupClip : Rules->Roar.Clip ? Rules->Roar.Clip : Rules->Taunt;
	case EGiantCrabState::Charge:
		return Rules->Charge.Clip ? Rules->Charge.Clip : Rules->WalkSideLeft;
	case EGiantCrabState::BellySlam:
		return Rules->BellySlam.Clip ? Rules->BellySlam.Clip : Rules->Slam.Clip;
	default:
		return Rules->Idle;
	}
}

void AGiantCrab::PlayClip(UAnimSequenceBase* Clip, bool bLoop, float Rate, bool bRestart, float StartTime)
{
	if (UTikiAnimInstance* Anim = Cast<UTikiAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Anim->PlayClip(Clip, bLoop, Rules ? Rules->BlendSeconds : 0.2f, Rate, bRestart, StartTime);
	}
	PlayedClip = Clip;
	if (Feedback)
	{
		Feedback->OnClipPlayed(Clip, bLoop, Rate, bRestart, StartTime);
	}
}

void AGiantCrab::UpdateClips()
{
	UTikiAnimInstance* Anim = Cast<UTikiAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Anim || !Rules)
	{
		return;
	}
	const EGiantCrabState State = Action.State;
	if (State == EGiantCrabState::Idle || State == EGiantCrabState::Approach || State == EGiantCrabState::Charge)
	{
		// the walk that fits the body: sideways left or right, forward, the charge, or still; its rate follows the speed
		const FVector Velocity = GetVelocity();
		const float Speed = static_cast<float>(Velocity.Size2D());
		const FVector Local = GetActorRotation().UnrotateVector(Velocity);
		UAnimSequenceBase* Clip = Rules->Idle;
		float Rate = 1.0f;
		if (State == EGiantCrabState::Charge)
		{
			// sideways like a crab: the loop of the side it runs to (the clip it already plays until it is under way)
			const UAnimSequenceBase* RightClip = Rules->Charge.ClipRight ? Rules->Charge.ClipRight.Get() : Rules->WalkSideRight.Get();
			const bool bRight = Speed > 50.0f ? Local.Y > 0.0f : PlayedClip == RightClip;
			UAnimSequenceBase* Own = bRight ? Rules->Charge.ClipRight.Get() : Rules->Charge.Clip.Get();
			Clip = Own ? Own : (bRight ? Rules->WalkSideRight.Get() : Rules->WalkSideLeft.Get());
			Rate = GiantCrabMath::LocomotionRate(Speed, Own ? Rules->Charge.ClipSpeedCmps : GiantCrabInternal::KitSideSpeed,
				GiantCrabInternal::MinLocomotionRate, Rules->MaxLocomotionPlayRate);
		}
		else if (Speed > 15.0f)
		{
			const bool bSideways = FMath::Abs(Local.Y) >= FMath::Abs(Local.X);
			Clip = bSideways ? (Local.Y > 0.0f ? Rules->WalkSideRight : Rules->WalkSideLeft) : Rules->WalkForward;
			Rate = GiantCrabMath::LocomotionRate(Speed, bSideways ? GiantCrabInternal::KitSideSpeed : GiantCrabInternal::KitForwardSpeed,
				GiantCrabInternal::MinLocomotionRate, Rules->MaxLocomotionPlayRate);
		}
		if (Clip && Clip != PlayedClip)
		{
			PlayClip(Clip, true, Rate, false);
		}
		else
		{
			Anim->SetPlayRate(Rate);
			if (Feedback)
			{
				Feedback->OnClipRate(Rate);
			}
		}
		PlayedSerial = Action.Serial;
		return;
	}
	if (PlayedSerial == Action.Serial)
	{
		return;
	}
	PlayedSerial = Action.Serial;
	if (UAnimSequenceBase* Clip = GetStateClip(State, Action.Attack))
	{
		// as far in as the action is old (T-305): a client's pose of the blow follows the server's, a late player sees a
		// dead crab lying on the sand rather than dying again
		const bool bLoop = State == EGiantCrabState::Block || State == EGiantCrabState::Flipped;
		PlayClip(Clip, bLoop, Action.PlayRate, true, GiantCrabLag::ClipStartTime(ActionAge(), Action.PlayRate, ClipLength(Clip), bLoop));
	}
}
