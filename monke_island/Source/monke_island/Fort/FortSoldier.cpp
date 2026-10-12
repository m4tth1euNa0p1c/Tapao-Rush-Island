// monke_island

#include "Fort/FortSoldier.h"
#include "Creatures/HitZoneMath.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiBlood.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiLimbs.h"
#include "Creatures/TikiRules.h"
#include "Fort/FortSoldierGoreMath.h"
#include "Fort/FortSoldierNetMath.h"
#include "Fort/FortSoldierCorpseMath.h"
#include "Fort/FortGarrisonMath.h"
#include "Fort/FortGarrisonZone.h"
#include "Run/RunSubsystem.h"
#include "Creatures/MobHealthBarComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Stealth/StealthRules.h"
#include "Stealth/StealthSubsystem.h"
#include "Weapons/MusketMath.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/MusketRules.h"
#include "monke_island.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

/** The bodies of the dead that stop and start animating again (T-323): verbose, `log LogMonkeFortCorpse Verbose` */
DEFINE_LOG_CATEGORY_STATIC(LogMonkeFortCorpse, Log, All);

namespace
{
	/** Switch of the measure of T-323: 0 animates every dead soldier each frame as before */
	TAutoConsoleVariable<int32> CVarFortCorpseFreeze(TEXT("monke.Fort.CorpseFreeze"), 1,
		TEXT("1: a dead fort soldier whose body lies still stops animating (T-323, DA_FortSoldierRules bFreezeRestingCorpse); ")
		TEXT("0: every dead soldier animates each frame until its body goes, the frozen ones wake within CorpseLODCheckSeconds (to measure the gain)"));

	/** True when a body at rest may stop animating: the rules and the console agree */
	bool CorpseFreezeAllowed(const UFortSoldierRules* Rules)
	{
		return Rules && Rules->bFreezeRestingCorpse && CVarFortCorpseFreeze.GetValueOnGameThread() != 0;
	}

	/** Height of the chest of a target over the middle of its body (cm): where the eyes and the balls aim */
	constexpr float ChestHeight = 15.0f;

	/** A target this far above or below is out of reach of a blow (cm) */
	constexpr float MaxBlowHeight = 150.0f;

	/** Puffs of smoke kept at once */
	constexpr int32 MaxPuffs = 4;

	/** A guard of a building is at its post when its feet are this close to the post's floor (cm): half a storey, a
	 *  post upstairs is not reached from the room under it (T-332) */
	constexpr float PostFloorTolerance = 100.0f;

	float BodyRadius(const AActor* Actor)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 30.0f;
	}
}

AFortSoldier::AFortSoldier()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	// not the engine's 100 updates per second: the rules set the rates at BeginPlay, and again at its death (T-317, R-28)
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(2.0f);

	// a bean of 1.05 m, like the tiki and the player's bean
	GetCapsuleComponent()->InitCapsuleSize(30.0f, 55.0f);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Move->MaxWalkSpeed = 140.0f;
	Move->bRunPhysicsWithNoController = true;
	// the soldiers step around each other
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 150.0f;
	// pushes the props like a person, not with the engine default of 7500 N (T-063)
	Move->bPushForceScaledToMass = false;
	Move->PushForceFactor = UIslanderMovementComponent::HumanPushForce;
	Move->InitialPushForceFactor = UIslanderMovementComponent::HumanPushForce / 30.0f;

	// the Blender exports face +Y with the origin at the feet
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -55.0f), FRotator(0.0f, -90.0f, 0.0f));
	// the bean library played without an AnimBP: this actor builds the frame
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UBeanLibraryAnimInstance::StaticClass());
	GetMesh()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// the server reads the hand (muzzle of the shot) even when nobody looks at it
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	// the bounds of the asset, not of the physics asset: computed from the bean's bodies (bones scaled by 100) they
	// shrank to a point at the feet, and a soldier vanished as soon as the player came close (owner report 2026-10-07);
	// the margin covers the hat, the musket and the sabre, whose own bounds follow the body's
	GetMesh()->bComponentUseFixedSkelBounds = true;
	GetMesh()->SetBoundsScale(1.8f);

	Musket = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Musket"));
	Musket->SetupAttachment(GetMesh());
	Musket->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Musket->SetGenerateOverlapEvents(false);
	Musket->bUseAttachParentBound = true;

	BayonetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bayonet"));
	BayonetMesh->SetupAttachment(Musket);
	BayonetMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BayonetMesh->SetGenerateOverlapEvents(false);
	BayonetMesh->bUseAttachParentBound = true;

	Sabre = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sabre"));
	Sabre->SetupAttachment(GetMesh());
	Sabre->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Sabre->SetGenerateOverlapEvents(false);
	Sabre->bUseAttachParentBound = true;

	Scabbard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Scabbard"));
	Scabbard->SetupAttachment(GetMesh());
	Scabbard->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Scabbard->SetGenerateOverlapEvents(false);
	Scabbard->bUseAttachParentBound = true;

	Health = CreateDefaultSubobject<UIslanderHealthComponent>(TEXT("Health"));
	Health->SetIsReplicated(true);

	HealthBar = CreateDefaultSubobject<UMobHealthBarComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetCapsuleComponent());
	// over the hat (shako and bearskin at 1.3 m)
	HealthBar->Margin = 40.0f;
}

void AFortSoldier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFortSoldier, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AFortSoldier, Skin, COND_InitialOnly);
	DOREPLIFETIME(AFortSoldier, Action);
}

void AFortSoldier::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	OnRep_Skin();
}

void AFortSoldier::OnRep_Skin()
{
	if (Skin && GetMesh()->GetSkeletalMeshAsset() != Skin)
	{
		GetMesh()->SetSkeletalMeshAsset(Skin);
		GearApplied = -1;
		ApplyGear();
		// a new mesh shows every bone again: what was gone is hidden again (a player joining late may receive the skin
		// after the action)
		ReapplyHiddenBones();
	}
}

bool AFortSoldier::IsDead() const
{
	return Health && Health->IsDead();
}

bool AFortSoldier::IsCrawling() const
{
	return FortSoldierGore::StanceOf(Action.LostLimbs) == FortSoldierGore::EStance::Crawl;
}

bool AFortSoldier::IsPanicking() const
{
	return FortSoldierGore::StanceOf(Action.LostLimbs) == FortSoldierGore::EStance::Panic;
}

bool AFortSoldier::IsLimbLost(int32 Limb) const
{
	return (Action.LostLimbs & FortSoldierGore::BitOfLimb(Limb)) != 0;
}

bool AFortSoldier::IsSabreBlow() const
{
	return Action.State == EFortSoldierState::Melee && Action.Blow != EFortSoldierBlow::None && Action.Blow != EFortSoldierBlow::Bayonet;
}

bool AFortSoldier::IsEnemy(const AActor* Actor)
{
	// every side but its own, alive (owner 2026-10-08: the players, the tiki and the giant crab alike)
	return Hostility::IsLivingFoe(EFaction::Soldier, Actor);
}

void AFortSoldier::BeginPlay()
{
	Super::BeginPlay();
	Health->OnDeath.AddDynamic(this, &AFortSoldier::HandleDeath);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("FortSoldier %s: no rules (DA_FortSoldierRules), it stays still"), *GetName());
		return;
	}
	Musket->SetSkeletalMeshAsset(Rules->MusketMesh);
	BayonetMesh->SetStaticMesh(Rules->BayonetMesh);
	BayonetMesh->SetRelativeTransform(FTransform(Rules->BayonetOnMuzzle.GetRotation(), Rules->MuzzleOffset + Rules->BayonetOnMuzzle.GetLocation(),
		Rules->BayonetOnMuzzle.GetScale3D()));
	Sabre->SetStaticMesh(Rules->SabreMesh);
	Scabbard->SetStaticMesh(Rules->ScabbardMesh);
	AttachGear(Scabbard, Rules->HipBone, Rules->ScabbardOnHip);
	GearApplied = -1;
	ApplyGear();
	if (UTikiBloodSubsystem* Blood = Rules->BloodRules ? UTikiBloodSubsystem::Get(this) : nullptr)
	{
		Blood->Preload(Rules->BloodRules);
	}
	// the look of what it lost before this machine met it (a player joining late): hidden, without a piece flying
	ApplyGoreLook();

	if (!HasAuthority())
	{
		return;
	}
	ApplyNetRates();
	const FFortSoldierRoleRules& Rank = Rules->GetRole(Kind);
	Health->MaxHealth = Rules->MaxHealth * Rank.HealthScale;
	Health->ResetHealth();
	ShotsLeft = Rules->Shots;
	bLoaded = ShotsLeft > 0;
	if (!Garrison.IsValid())
	{
		Garrison = AFortGarrisonZone::FindAt(GetWorld(), GetActorLocation());
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// it starts at its first post: the next walk goes to another one
	if (Garrison.IsValid() && FirstPost >= 0 && FirstPost < Garrison->GetPatrolPointCount())
	{
		Post = FirstPost;
	}
	// the garrison does not leave its posts all at once
	PostUntil = Now + FMath::FRandRange(0.5f, FMath::Max(0.5f, Rules->PostSecondsMax));
	NextSense = Now + FMath::FRandRange(0.0f, Rules->SenseInterval);
	SetState(EFortSoldierState::Post);
	UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: %s, %.0f health, %d shots, %s"), *GetName(), *UEnum::GetValueAsString(Kind), Health->MaxHealth,
		ShotsLeft, Garrison.IsValid() ? *FString::Printf(TEXT("garrison of %s%s"), *Garrison->GetName(),
			IsBuildingGuard() ? *FString::Printf(TEXT(", guard of building post %d"), BuildingPost) : TEXT(""))
		: (LeashRadius > 0.0f ? *FString::Printf(TEXT("held in a ring of %.0f cm"), LeashRadius) : TEXT("no fort around it")));
}

void AFortSoldier::JoinGarrison(AFortGarrisonZone* Zone)
{
	Garrison = Zone;
}

void AFortSoldier::SetLeash(const FVector& Center, float InRadius)
{
	LeashCenter = Center;
	LeashRadius = FMath::Max(0.0f, InRadius);
}

void AFortSoldier::AssignBuildingPost(int32 Index, const FVector& World, float Yaw)
{
	BuildingPost = Index;
	BuildingPostLocation = World;
	BuildingPostYaw = Yaw;
}

bool AFortSoldier::IsOnOwnGround(const FVector& Location) const
{
	// a guard of a building keeps to it and its doorstep (T-332): never a step farther out of that disc
	if (IsBuildingGuard() && Rules && !FortGarrisonMath::KeepsToHold(FVector::Dist2D(Location, BuildingPostLocation),
		FVector::Dist2D(GetActorLocation(), BuildingPostLocation), Rules->BuildingHoldRadiusCm))
	{
		return false;
	}
	if (const AFortGarrisonZone* Fort = Garrison.Get())
	{
		return Fort->IsInside(Location);
	}
	return LeashRadius <= 0.0f || FVector::Dist2D(Location, LeashCenter) <= LeashRadius;
}

FVector AFortSoldier::ClampToOwnGround(const FVector& Location) const
{
	// a guard of a building: its goals stay within its hold, then inside the fort (T-332)
	const FVector Held = IsBuildingGuard() && Rules ? FortGarrisonMath::ClampIntoDisc(BuildingPostLocation, Rules->BuildingHoldRadiusCm, Location) : Location;
	if (const AFortGarrisonZone* Fort = Garrison.Get())
	{
		return Fort->ClampInside(Held);
	}
	return LeashRadius > 0.0f ? FortGarrisonMath::ClampIntoDisc(LeashCenter, LeashRadius, Held) : Held;
}

void AFortSoldier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && Rules && !IsDead())
	{
		if (bFiringSquad)
		{
			SquadThink(DeltaSeconds);
		}
		else
		{
			ServerThink(DeltaSeconds);
		}
	}
	if (Rules)
	{
		UpdateAnimation(DeltaSeconds);
	}
	UpdateEffects(DeltaSeconds);
	// a dead soldier whose body lies still stops animating (T-323)
	if (IsDead())
	{
		UpdateCorpseRest(DeltaSeconds);
	}
}

float AFortSoldier::StateTime() const
{
	return static_cast<float>(GetWorld()->GetTimeSeconds() - StateStart);
}

float AFortSoldier::ClipLength(const UAnimSequence* Clip)
{
	return Clip ? Clip->GetPlayLength() : 0.0f;
}

float AFortSoldier::TargetEdge() const
{
	const AActor* Foe = Target.Get();
	return Foe ? FortGarrisonMath::EdgeDistance(GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleRadius(), Foe->GetActorLocation(), BodyRadius(Foe))
		: TNumericLimits<float>::Max();
}

float AFortSoldier::RoleDamage() const
{
	return Rules ? Rules->GetRole(Kind).DamageScale : 1.0f;
}

bool AFortSoldier::WantsBayonet() const
{
	// no musket left to fix it on once an arm is gone
	return Rules && Rules->GetRole(Kind).bBayonet && ShotsLeft > 0 && Rules->Bayonet.Clip && !FortSoldierGore::MusketGone(Action.LostLimbs);
}

float AFortSoldier::GroundSpeed(bool bRun) const
{
	// without a leg it drags itself (GoreBox, owner 2026-10-09)
	if (IsCrawling())
	{
		return Rules->CrawlSpeed;
	}
	return bRun ? Rules->RunSpeed : Rules->PatrolSpeed;
}

// ---------------------------------------------------------------------------------------------- decisions (server)

void AFortSoldier::ServerThink(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	// on the ground without a leg it bleeds out (GoreBox: Legless_Crawl_Death after CrawlBleedOutSeconds)
	if (IsCrawling() && Rules->CrawlBleedOutSeconds > 0.0f && Now - CrawlStart >= Rules->CrawlBleedOutSeconds)
	{
		BleedOut();
		return;
	}
	if (Now >= NextSense)
	{
		NextSense = Now + Rules->SenseInterval;
		Sense();
	}
	AActor* Foe = Target.Get();
	switch (Action.State)
	{
	case EFortSoldierState::Post:
		if (Foe)
		{
			SetState(EFortSoldierState::Alert);
		}
		// a panicking soldier never sheathes: the sabre fell with its arm, it would come back to the hip
		else if (Action.bSabre && !IsPanicking() && Now - LastFight >= Rules->SheatheAfterSeconds && Rules->SabreSheathe)
		{
			SetState(EFortSoldierState::Sheathe);
		}
		else if (Now >= PostUntil)
		{
			GoToNextPost();
		}
		else if (IsBuildingGuard())
		{
			// a guard at its post faces what it guards: the door of its room, the outside from a gallery (T-332)
			TurnToward(GetActorLocation() + FRotator(0.0f, BuildingPostYaw, 0.0f).Vector() * 100.0f, DeltaSeconds);
		}
		break;
	case EFortSoldierState::Patrol:
		if (Foe)
		{
			StopMoving();
			SetState(EFortSoldierState::Alert);
			break;
		}
		DirectStep();
		if (HasArrived())
		{
			StopMoving();
			SetState(EFortSoldierState::Post);
			PostUntil = Now + FMath::FRandRange(Rules->PostSecondsMin, FMath::Max(Rules->PostSecondsMin, Rules->PostSecondsMax));
		}
		break;
	case EFortSoldierState::Alert:
		if (Foe)
		{
			TurnToward(bSeesTarget ? Foe->GetActorLocation() : LastSeenAt, DeltaSeconds);
		}
		if (StateTime() >= Rules->ReactionSeconds)
		{
			Decide();
		}
		break;
	case EFortSoldierState::Chase:
		if (!Foe)
		{
			Decide();
			break;
		}
		DirectStep();
		// standing (in reach, or held by the walls): it faces the one it hunts
		if (!bMoving || HasArrived())
		{
			TurnToward(bSeesTarget ? Foe->GetActorLocation() : LastSeenAt, DeltaSeconds);
		}
		if (Now >= NextRepath)
		{
			Decide();
		}
		break;
	case EFortSoldierState::Aim:
		if (!Foe || !bSeesTarget || TargetEdge() <= Rules->MeleeSwitchCm)
		{
			Decide();
			break;
		}
		TurnToward(Foe->GetActorLocation(), DeltaSeconds, Rules->FireYawOffsetDeg);
		SetAimPitchToward(Foe->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight));
		if (StateTime() >= AimSeconds)
		{
			SetState(EFortSoldierState::Fire);
		}
		break;
	case EFortSoldierState::Fire:
		if (Foe && !bShotDone)
		{
			TurnToward(Foe->GetActorLocation(), DeltaSeconds, Rules->FireYawOffsetDeg);
			SetAimPitchToward(Foe->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight));
		}
		if (!bShotDone && StateTime() >= GetShotDelay())
		{
			Shoot();
		}
		if (bShotDone && StateTime() >= FMath::Max(ClipLength(GetFireClip()) - GetFireStartTime(), GetShotDelay()))
		{
			if (ShotsLeft > 0 && GetReloadClip())
			{
				SetState(EFortSoldierState::Reload);
			}
			else
			{
				bLoaded = ShotsLeft > 0;
				Decide();
			}
		}
		break;
	case EFortSoldierState::Reload:
		// pressed: the reload is dropped for steel
		if (Foe && TargetEdge() <= Rules->MeleeSwitchCm)
		{
			Decide();
			break;
		}
		if (StateTime() >= ClipLength(GetReloadClip()))
		{
			bLoaded = true;
			Decide();
		}
		break;
	case EFortSoldierState::Draw:
		if (!bGearSwapped && StateTime() >= Rules->DrawAttachTime)
		{
			bGearSwapped = true;
			Action.bSabre = true;
			ForceNetUpdate();
			ApplyGear();
		}
		if (StateTime() >= FMath::Max(ClipLength(Rules->SabreDraw), Rules->DrawAttachTime))
		{
			Decide();
		}
		break;
	case EFortSoldierState::Sheathe:
		if (!bGearSwapped && StateTime() >= ClipLength(Rules->SabreSheathe) * 0.6f)
		{
			bGearSwapped = true;
			Action.bSabre = false;
			ForceNetUpdate();
			ApplyGear();
		}
		if (StateTime() >= ClipLength(Rules->SabreSheathe))
		{
			if (Foe)
			{
				Decide();
			}
			else
			{
				SetState(EFortSoldierState::Post);
				PostUntil = Now + 1.0;
			}
		}
		break;
	case EFortSoldierState::Melee:
		UpdateBlow(DeltaSeconds);
		break;
	case EFortSoldierState::HitReact:
		if (StateTime() >= ClipLength(GetStateClip(EFortSoldierState::HitReact, Action.Blow)))
		{
			Decide();
		}
		break;
	case EFortSoldierState::Flee:
		// running from its foe: a new way at each turn, or when the walls stopped it
		DirectStep();
		if (HasArrived() || Now >= NextRepath)
		{
			Decide();
		}
		break;
	default:
		break;
	}
}

void AFortSoldier::Sense()
{
	const double Now = GetWorld()->GetTimeSeconds();
	AActor* Current = Target.Get();
	if (Current && !IsEnemy(Current))
	{
		// dead, gone, or no longer a foe
		Target = nullptr;
		Current = nullptr;
		bSeesTarget = false;
	}
	const double SightSq = FMath::Square(static_cast<double>(Rules->SightRadiusCm));
	// the one it hunts is followed without the view cone: it keeps an eye on it
	if (Current && FVector::DistSquared(Current->GetActorLocation(), GetActorLocation()) <= SightSq && CanSee(Current, false))
	{
		bSeesTarget = true;
		bHeardOnly = false;
		LastSeenAt = Current->GetActorLocation();
		LastSeenTime = Now;
		return;
	}
	AActor* Best = nullptr;
	double BestDistSq = SightSq;
	// the nearest foe it sees, whatever its side (a player, a tiki, the giant crab), from the registry of the combatants
	if (UHostilitySubsystem* Registry = UHostilitySubsystem::Get(this))
	{
		for (const FHostileCombatant& Other : Registry->GetCombatants())
		{
			ACharacter* Candidate = Other.Character.Get();
			if (!Candidate || Candidate == this || !Other.IsLivingFoeOf(EFaction::Soldier))
			{
				continue;
			}
			const double DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
			if (DistSq <= BestDistSq && CanSee(Candidate, true))
			{
				Best = Candidate;
				BestDistSq = DistSq;
			}
		}
	}
	if (Best)
	{
		Target = Best;
		bSeesTarget = true;
		bHeardOnly = false;
		LastSeenAt = Best->GetActorLocation();
		LastSeenTime = Now;
		if (!Current)
		{
			UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: spots %s (%.0f m)"), *GetName(), *Best->GetName(), FMath::Sqrt(BestDistSq) / 100.0);
			if (AFortGarrisonZone* Fort = Garrison.Get())
			{
				Fort->RaiseAlarm(this, Best, Best->GetActorLocation(), Rules->AlarmRadiusCm);
			}
		}
		return;
	}
	bSeesTarget = false;
	// nothing in sight: it listens; a noise of a foe sends it there to look, the one it hunts is followed by its noises
	// (T-262)
	if (const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this))
	{
		FStealthNoise Noise;
		AActor* Source = nullptr;
		if (Stealth->FindHeardNoise(this, Stealth->GetRules().SoldierHearingScale, Noise) && Noise.Time > HeardNoiseTime)
		{
			HeardNoiseTime = Noise.Time;
			Source = Noise.Instigator.Get();
		}
		if (Source && IsEnemy(Source) && (!Current || Current == Source))
		{
			Target = Source;
			LastSeenAt = Noise.Location;
			LastSeenTime = Now;
			if (!Current)
			{
				bHeardOnly = true;
				UE_LOG(LogMonkeStealth, Log, TEXT("FortSoldier %s: hears %s at %.0f m, goes to look"), *GetName(), *Source->GetName(),
					FVector::Dist(Noise.Location, GetActorLocation()) / 100.0);
				if (Action.State == EFortSoldierState::Post || Action.State == EFortSoldierState::Patrol || Action.State == EFortSoldierState::Sheathe)
				{
					StopMoving();
					SetState(EFortSoldierState::Alert);
				}
			}
			return;
		}
	}
	if (Current && Now - LastSeenTime > Rules->LoseTargetSeconds)
	{
		UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: lost %s"), *GetName(), *Current->GetName());
		Target = nullptr;
		bHeardOnly = false;
	}
}

bool AFortSoldier::CanSee(const AActor* Actor, bool bNeedView) const
{
	if (!Actor)
	{
		return false;
	}
	// a crouched player, or any player at night, only at contact (T-262); the range is the caller's
	if (const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this); Stealth && !Stealth->CanSee(this, Actor, 0.0f))
	{
		return false;
	}
	const FVector Eyes = GetActorLocation() + FVector(0.0f, 0.0f, Rules->EyeHeightCm);
	const FVector Chest = Actor->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight);
	const FVector To = Chest - Eyes;
	if (bNeedView && To.Size2D() > Rules->NoticeRadiusCm && !FortGarrisonMath::IsInView(GetActorForwardVector(), To, Rules->SightHalfAngleDegrees))
	{
		return false;
	}
	// walls, buildings and bushes hide; bodies do not
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FortSoldierSight), true, this);
	Params.AddIgnoredActor(Actor);
	return !GetWorld()->LineTraceTestByObjectType(Eyes, Chest, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

bool AFortSoldier::HearAlarm(AActor* InTarget, const FVector& From)
{
	if (IsDead() || !Rules || Target.IsValid() || !IsEnemy(InTarget))
	{
		return false;
	}
	// a guard of a building holds it: only a foe seen close to it brings it to its door (T-332)
	if (!FortGarrisonMath::AnswersAlarm(IsBuildingGuard(), FVector::Dist(From, GetActorLocation()), Rules->BuildingAlarmRadiusCm))
	{
		return false;
	}
	Target = InTarget;
	LastSeenAt = From;
	LastSeenTime = GetWorld()->GetTimeSeconds();
	bSeesTarget = false;
	bHeardOnly = false;
	if (Action.State == EFortSoldierState::Post || Action.State == EFortSoldierState::Patrol || Action.State == EFortSoldierState::Sheathe)
	{
		StopMoving();
		SetState(EFortSoldierState::Alert);
	}
	return true;
}

void AFortSoldier::Decide()
{
	const double Now = GetWorld()->GetTimeSeconds();
	NextRepath = Now + Rules->RepathInterval;
	AActor* Foe = Target.Get();
	// without the arm of its weapon it only runs (GoreBox panic, owner 2026-10-09): from its foe, round and round
	// without one, for good
	if (IsPanicking())
	{
		Flee();
		return;
	}
	if (!Foe)
	{
		StopMoving();
		SetState(EFortSoldierState::Post);
		PostUntil = Now + FMath::FRandRange(1.0f, 3.0f);
		return;
	}
	LastFight = Now;
	// on the ground without a leg it only drags itself toward the one it hunts: no shot, no blow
	if (IsCrawling())
	{
		ChaseFoe(0.0f);
		return;
	}
	const float Edge = TargetEdge();
	const EFortSoldierMove Move = FortGarrisonMath::PickMove(Edge, bSeesTarget, ShotsLeft, bLoaded, Rules->MeleeSwitchCm, Rules->MinFireCm,
		Rules->MaxFireCm);
	// the sabre drawn, it keeps to steel until the fight is over
	if (Move == EFortSoldierMove::Melee || Action.bSabre)
	{
		if (!Action.bSabre && !WantsBayonet())
		{
			StopMoving();
			SetState(EFortSoldierState::Draw);
			return;
		}
		const EFortSoldierBlow Blow = Action.bSabre ? PickBlow() : EFortSoldierBlow::Bayonet;
		const FFortSoldierBlowRules* Steel = Rules->GetBlow(Blow);
		if (!Steel || !Steel->Clip)
		{
			ChaseFoe(0.0f);
			return;
		}
		const FVector To = Foe->GetActorLocation() - GetActorLocation();
		if (Edge <= Steel->ReachCm && bSeesTarget)
		{
			if (Now >= NextBlow && FortGarrisonMath::IsInView(GetActorForwardVector(), To, 35.0f) && FMath::Abs(To.Z) <= MaxBlowHeight)
			{
				StopMoving();
				SetState(EFortSoldierState::Melee, Blow);
				return;
			}
			// in reach: faces it until the blow is ready
			ChaseFoe(Steel->ReachCm);
			return;
		}
		ChaseFoe(FMath::Max(0.0f, Steel->ReachCm * 0.7f));
		return;
	}
	switch (Move)
	{
	case EFortSoldierMove::Fire:
		StopMoving();
		AimSeconds = FMath::FRandRange(Rules->AimSecondsMin, FMath::Max(Rules->AimSecondsMin, Rules->AimSecondsMax));
		SetState(EFortSoldierState::Aim);
		return;
	case EFortSoldierMove::Reload:
		StopMoving();
		SetState(EFortSoldierState::Reload);
		return;
	default:
		ChaseFoe(0.0f);
		return;
	}
}

void AFortSoldier::Flee()
{
	if (Action.State != EFortSoldierState::Flee)
	{
		SetState(EFortSoldierState::Flee);
	}
	const AActor* Foe = Target.Get();
	const FVector Here = GetActorLocation();
	auto RingPoint = [this, &Here]()
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
		return Here + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Rules->PanicRingRadiusCm;
	};
	FVector Goal;
	if (Foe)
	{
		// away from it, a little aside so that a wall in the way does not pin it
		const FVector From = bSeesTarget ? Foe->GetActorLocation() : LastSeenAt;
		const FVector Away = (Here - From).GetSafeNormal2D(UE_SMALL_NUMBER, GetActorForwardVector());
		Goal = Here + Away.RotateAngleAxis(FMath::FRandRange(-35.0f, 35.0f), FVector::UpVector) * Rules->PanicFleeDistanceCm;
	}
	else
	{
		Goal = RingPoint();
	}
	// held on its ground (the fort, the ring): a goal the walls bring back to its feet is drawn elsewhere
	if (FVector::Dist2D(ClampToOwnGround(Goal), Here) < Rules->AcceptanceRadiusCm * 2.0f)
	{
		Goal = RingPoint();
	}
	MoveTo(Goal, true);
}

void AFortSoldier::ChaseFoe(float StopEdge)
{
	const AActor* Foe = Target.Get();
	if (Action.State != EFortSoldierState::Chase)
	{
		SetState(EFortSoldierState::Chase);
	}
	if (!Foe)
	{
		StopMoving();
		return;
	}
	if (bSeesTarget && TargetEdge() <= StopEdge)
	{
		StopMoving();
		return;
	}
	// a foe only heard, never seen: an alert walk to the noise (T-262); a foe seen, or lost, at a run
	MoveTo(bSeesTarget ? Foe->GetActorLocation() : LastSeenAt, bSeesTarget || !bHeardOnly);
}

void AFortSoldier::GoToNextPost()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const AFortGarrisonZone* Fort = Garrison.Get();
	// a guard of a building stands at its post (T-332): back to it when away, else it stays there a while longer
	if (IsBuildingGuard())
	{
		// at its post: on its floor (a post upstairs is not reached from the room under it), within the acceptance radius
		const FVector Here = GetActorLocation();
		if (FVector::Dist2D(Here, BuildingPostLocation) <= Rules->AcceptanceRadiusCm
			&& FMath::Abs(Here.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - BuildingPostLocation.Z) <= PostFloorTolerance)
		{
			PostUntil = Now + FMath::FRandRange(Rules->PostSecondsMin, FMath::Max(Rules->PostSecondsMin, Rules->PostSecondsMax));
			return;
		}
		MoveTo(BuildingPostLocation, false);
		SetState(EFortSoldierState::Patrol);
		return;
	}
	const int32 Count = Fort ? Fort->GetPatrolPointCount() : 0;
	if (Count == 0)
	{
		PostUntil = Now + Rules->PostSecondsMax;
		return;
	}
	int32 Next = Post == INDEX_NONE && FirstPost >= 0 && FirstPost < Count ? FirstPost : INDEX_NONE;
	// a post no other soldier of the fort walks to or stands at (the garrison's registry, T-332)
	for (int32 Try = 0; Try < 8 && Next == INDEX_NONE; ++Try)
	{
		const int32 Candidate = FortGarrisonMath::NextPatrolPoint(Count, Post, FMath::FRand());
		if (!Fort->IsPatrolPostTaken(Candidate, this) || Try == 7)
		{
			Next = Candidate;
		}
	}
	Post = Next;
	MoveTo(Fort->GetPatrolPoint(Post), false);
	SetState(EFortSoldierState::Patrol);
}

void AFortSoldier::MoveTo(const FVector& InGoal, bool bRun)
{
	const FVector Goal = ClampToOwnGround(InGoal);
	GetCharacterMovement()->MaxWalkSpeed = GroundSpeed(bRun);
	MoveGoal = Goal;
	bMoving = true;
	bDirectMove = false;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Goal, Rules->AcceptanceRadiusCm, true, true, true, false, nullptr, true);
		if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			bMoving = false;
			return;
		}
		if (Result == EPathFollowingRequestResult::RequestSuccessful)
		{
			return;
		}
	}
	// no navigation mesh there: straight
	bDirectMove = true;
	DirectProgressCheckAt = GetWorld()->GetTimeSeconds() + Rules->DirectProgressSeconds;
	DirectLastLeft = FVector::Dist2D(Goal, GetActorLocation());
}

bool AFortSoldier::IsWallAhead(const FVector& Direction) const
{
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Center = GetActorLocation();
	const FVector Reach = Direction * (Radius + Rules->DirectLookAheadCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FortSoldierWallAhead), false, this);
	const FCollisionObjectQueryParams Walls(FCollisionObjectQueryParams::InitType::AllStaticObjects);
	FHitResult Hit;
	for (const float Lift : { 0.0f, 0.6f * GetCapsuleComponent()->GetScaledCapsuleHalfHeight() })
	{
		const FVector Start = Center + FVector(0.0, 0.0, Lift);
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Reach, Walls, Params))
		{
			return false;
		}
	}
	return true;
}

void AFortSoldier::DirectStep()
{
	if (!bMoving || !bDirectMove)
	{
		return;
	}
	const FVector To = MoveGoal - GetActorLocation();
	if (To.Size2D() <= Rules->AcceptanceRadiusCm)
	{
		bMoving = false;
		return;
	}
	const FVector Direction = To.GetSafeNormal2D();
	// never a step off its ground (the fort, or the ring)
	if (!IsOnOwnGround(GetActorLocation() + Direction * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 30.0f)))
	{
		bMoving = false;
		return;
	}
	// T-319: with no path, a soldier walked into a wall of its fort and ground there for minutes ("is stuck and failed
	// to move" 60 times a second): a wall right ahead, or no headway, ends the walk (it takes its post, then another)
	const double Now = GetWorld()->GetTimeSeconds();
	if (IsWallAhead(Direction))
	{
		bMoving = false;
		return;
	}
	if (Now >= DirectProgressCheckAt)
	{
		const double Left = To.Size2D();
		if (DirectLastLeft - Left < Rules->DirectMinProgressCm)
		{
			bMoving = false;
			return;
		}
		DirectLastLeft = Left;
		DirectProgressCheckAt = Now + Rules->DirectProgressSeconds;
	}
	AddMovementInput(Direction, 1.0f);
}

void AFortSoldier::StopMoving()
{
	if (bMoving && !bDirectMove)
	{
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->StopMovement();
		}
	}
	bMoving = false;
	bDirectMove = false;
}

bool AFortSoldier::HasArrived() const
{
	if (!bMoving)
	{
		return true;
	}
	if (bDirectMove)
	{
		return FVector::Dist2D(MoveGoal, GetActorLocation()) <= Rules->AcceptanceRadiusCm;
	}
	const AAIController* AI = Cast<AAIController>(GetController());
	return !AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle;
}

void AFortSoldier::OnPutBackInside()
{
	StopMoving();
	NextRepath = 0.0;
}

float AFortSoldier::TurnToward(const FVector& Point, float DeltaSeconds, float YawOffset)
{
	const FVector To = Point - GetActorLocation();
	if (To.SizeSquared2D() < 1.0)
	{
		return 0.0f;
	}
	const float Current = GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(static_cast<float>(To.Rotation().Yaw) + YawOffset - Current);
	const float MaxStep = Rules->TurnRateDegrees * DeltaSeconds;
	const float Step = FMath::Clamp(Delta, -MaxStep, MaxStep);
	if (!FMath::IsNearlyZero(Step))
	{
		SetActorRotation(FRotator(0.0f, Current + Step, 0.0f));
	}
	return Delta - Step;
}

void AFortSoldier::SetAimPitchToward(const FVector& Point)
{
	// from about the shoulder of a bean
	const FVector To = Point - (GetActorLocation() + FVector(0.0f, 0.0f, 35.0f));
	const int32 Pitch = FMath::Clamp(FMath::RoundToInt32(FMath::RadiansToDegrees(FMath::Atan2(To.Z, To.Size2D()))), -60, 60);
	if (Pitch != Action.AimPitch)
	{
		Action.AimPitch = static_cast<int8>(Pitch);
	}
}

void AFortSoldier::StartFiringSquad(AActor* Victim, double AimAt, double FireAt, const FVector& Push)
{
	if (!HasAuthority())
	{
		return;
	}
	bFiringSquad = true;
	bSquadFired = false;
	Target = Victim;
	SquadAimAt = AimAt;
	SquadFireAt = FMath::Max(FireAt, AimAt);
	SquadPush = Push;
	ShotsLeft = FMath::Max(ShotsLeft, 1);
	bLoaded = true;
	StopMoving();
	if (Action.bSabre)
	{
		Action.bSabre = false;
		ApplyGear();
	}
	SetState(EFortSoldierState::Post);
	UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: firing squad, aims at %s in %.1f s, fires in %.1f s"), *GetName(), *GetNameSafe(Victim),
		AimAt - GetWorld()->GetTimeSeconds(), SquadFireAt - GetWorld()->GetTimeSeconds());
}

void AFortSoldier::SquadThink(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const AActor* Victim = Target.Get();
	if (Victim && !bSquadFired)
	{
		const FVector Chest = Victim->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight);
		TurnToward(Chest, DeltaSeconds, Action.State == EFortSoldierState::Aim || Action.State == EFortSoldierState::Fire ? Rules->FireYawOffsetDeg : 0.0f);
		SetAimPitchToward(Chest);
	}
	switch (Action.State)
	{
	case EFortSoldierState::Aim:
		if (Now >= SquadFireAt)
		{
			SetState(EFortSoldierState::Fire);
		}
		break;
	case EFortSoldierState::Fire:
		if (!bShotDone && StateTime() >= GetShotDelay())
		{
			SquadShoot();
		}
		if (bShotDone && StateTime() >= FMath::Max(ClipLength(GetFireClip()) - GetFireStartTime(), GetShotDelay()) + 0.4f)
		{
			// the musket grounded, at attention
			SetState(EFortSoldierState::Post);
		}
		break;
	default:
		if (!bSquadFired && Now >= SquadAimAt)
		{
			SetState(EFortSoldierState::Aim);
		}
		break;
	}
}

void AFortSoldier::SquadShoot()
{
	bShotDone = true;
	bSquadFired = true;
	const FVector Muzzle = GetMuzzle();
	AActor* Victim = Target.Get();
	const FVector Aim = Victim ? Victim->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight) : Muzzle + GetActorForwardVector() * Rules->MaxFireCm;
	const FVector Direction = (Aim - Muzzle).GetSafeNormal();
	MulticastShot(Muzzle, Direction);
	UIslanderHealthComponent* Life = Victim ? Victim->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	if (Life && !Life->IsDead())
	{
		// the ball throws the body off the plank: the death ragdoll starts at the character's speed
		if (ACharacter* Character = Cast<ACharacter>(Victim))
		{
			Character->GetCharacterMovement()->Velocity = SquadPush;
		}
		const FHitResult Hit(Victim, nullptr, Aim, -Direction);
		UGameplayStatics::ApplyPointDamage(Victim, Rules->SquadDamage, Direction, Hit, GetController(), this, nullptr);
	}
	UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: the squad fires at %s"), *GetName(), *GetNameSafe(Victim));
}

void AFortSoldier::SetState(EFortSoldierState NewState, EFortSoldierBlow Blow)
{
	Action.State = NewState;
	Action.Blow = Blow;
	++Action.Serial;
	StateStart = GetWorld()->GetTimeSeconds();
	bShotDone = false;
	bGearSwapped = false;
	StruckWindows.Reset();
	if (NewState == EFortSoldierState::Melee && Rules && Rules->SwingSound)
	{
		MulticastSound(Rules->SwingSound, GetActorLocation());
	}
	if (NewState == EFortSoldierState::Draw && Rules && Rules->DrawSound)
	{
		MulticastSound(Rules->DrawSound, GetActorLocation());
	}
	ForceNetUpdate();
	OnRep_Action();
}

void AFortSoldier::OnRep_Action()
{
	ApplyGear();
	ApplyGoreLook();
}

EFortSoldierBlow AFortSoldier::PickBlow() const
{
	// without its left arm: the one-armed blow (GoreBox Armless_Fight_On), when its clip is set
	if (FortSoldierGore::StanceOf(Action.LostLimbs) == FortSoldierGore::EStance::OneArm && Rules->ArmlessFightOn.Clip)
	{
		return EFortSoldierBlow::OneArm;
	}
	const EFortSoldierBlow Blows[] = { EFortSoldierBlow::SlashRight, EFortSoldierBlow::SlashLeft, EFortSoldierBlow::Thrust, EFortSoldierBlow::Combo };
	float Total = 0.0f;
	for (const EFortSoldierBlow Blow : Blows)
	{
		const FFortSoldierBlowRules* Candidate = Rules->GetBlow(Blow);
		Total += Candidate && Candidate->Clip ? FMath::Max(0.0f, Candidate->Weight) : 0.0f;
	}
	float Draw = FMath::FRand() * Total;
	for (const EFortSoldierBlow Blow : Blows)
	{
		const FFortSoldierBlowRules* Candidate = Rules->GetBlow(Blow);
		const float Weight = Candidate && Candidate->Clip ? FMath::Max(0.0f, Candidate->Weight) : 0.0f;
		if (Weight > 0.0f && Draw <= Weight)
		{
			return Blow;
		}
		Draw -= Weight;
	}
	return EFortSoldierBlow::SlashRight;
}

void AFortSoldier::UpdateBlow(float DeltaSeconds)
{
	const FFortSoldierBlowRules* Blow = Rules->GetBlow(Action.Blow);
	if (!Blow || !Blow->Clip)
	{
		Decide();
		return;
	}
	const float Time = StateTime();
	AActor* Foe = Target.Get();
	const float FirstHit = Blow->HitWindows.IsEmpty() ? 0.0f : static_cast<float>(Blow->HitWindows[0].X);
	// the windup still follows the one it strikes
	if (Foe && Time < FirstHit)
	{
		TurnToward(Foe->GetActorLocation(), DeltaSeconds);
	}
	// the lunge, held on its ground (the fort, or the ring)
	if (Blow->LungeSpeed > 0.0f && !Blow->HitWindows.IsEmpty() && Time >= FirstHit - 0.1f && Time <= Blow->HitWindows[0].Y)
	{
		const FVector Forward = GetActorForwardVector();
		if (IsOnOwnGround(GetActorLocation() + Forward * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 40.0f)))
		{
			GetCharacterMovement()->MaxWalkSpeed = Blow->LungeSpeed;
			AddMovementInput(Forward, 1.0f);
		}
	}
	for (int32 Window = 0; Window < Blow->HitWindows.Num(); ++Window)
	{
		const FVector2D& Span = Blow->HitWindows[Window];
		if (Time < Span.X || Time > Span.Y || StruckWindows.Contains(Window))
		{
			continue;
		}
		// every foe in the reach and the cone, once per window (gathered first: a blow may make the registry scan again)
		TArray<ACharacter*, TInlineAllocator<8>> Victims;
		if (UHostilitySubsystem* Registry = UHostilitySubsystem::Get(this))
		{
			for (const FHostileCombatant& Other : Registry->GetCombatants())
			{
				ACharacter* Candidate = Other.Character.Get();
				if (!Candidate || Candidate == this || !Other.IsLivingFoeOf(EFaction::Soldier))
				{
					continue;
				}
				const FVector To = Candidate->GetActorLocation() - GetActorLocation();
				if (FMath::Abs(To.Z) > MaxBlowHeight
					|| FortGarrisonMath::EdgeDistance(GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleRadius(), Candidate->GetActorLocation(), BodyRadius(Candidate)) > Blow->ReachCm
					|| !FortGarrisonMath::IsInView(GetActorForwardVector(), To, Blow->ConeDegrees))
				{
					continue;
				}
				Victims.Add(Candidate);
			}
		}
		for (ACharacter* Victim : Victims)
		{
			UGameplayStatics::ApplyDamage(Victim, Blow->Damage * RoleDamage(), GetController(), this, nullptr);
			UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: %s strikes %s (%.0f)"), *GetName(), *UEnum::GetValueAsString(Action.Blow), *Victim->GetName(),
				Blow->Damage * RoleDamage());
		}
		const bool bStruck = !Victims.IsEmpty();
		if (bStruck)
		{
			StruckWindows.Add(Window);
			if (Rules->HitSound)
			{
				MulticastSound(Rules->HitSound, GetActorLocation() + GetActorForwardVector() * 60.0f);
			}
		}
	}
	if (Time >= ClipLength(Blow->Clip))
	{
		NextBlow = GetWorld()->GetTimeSeconds() + Blow->Recovery;
		Decide();
	}
}

void AFortSoldier::Shoot()
{
	bShotDone = true;
	ShotsLeft = FMath::Max(0, ShotsLeft - 1);
	bLoaded = false;
	const FVector Muzzle = GetMuzzle();
	AActor* Foe = Target.Get();
	const FVector Aim = Foe ? Foe->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight) : Muzzle + GetActorForwardVector() * Rules->MaxFireCm;
	FVector Direction = (Aim - Muzzle).GetSafeNormal();
	bool bHit = false;
	float Chance = 0.0f;
	if (Foe && IsEnemy(Foe) && CanSee(Foe, false))
	{
		Chance = FortGarrisonMath::HitChance(static_cast<float>(FVector::Dist(Muzzle, Aim)), Rules->HitChanceNearCm, Rules->HitChanceFarCm,
			Rules->HitChanceNear, Rules->HitChanceFar);
		// the soldiers shoot better as the campaign goes on (the difficulty of the day, owner 2026-10-09)
		Chance = FMath::Min(1.0f, Chance * (Garrison.IsValid() ? Garrison->GetHitChanceScale() : URunSubsystem::DifficultyOf(this).SoldierHitChanceScale));
		bHit = FMath::FRand() < Chance;
	}
	if (bHit)
	{
		const FHitResult Hit(Foe, nullptr, Aim, -Direction);
		UGameplayStatics::ApplyPointDamage(Foe, Rules->MusketDamage * RoleDamage(), Direction, Hit, GetController(), this, nullptr);
	}
	else
	{
		// the ball passes beside the target
		const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		const FVector Miss = Aim + Side * (FMath::FRandRange(-1.0f, 1.0f) * Rules->MissSpreadCm)
			+ FVector(0.0f, 0.0f, FMath::FRandRange(-0.3f, 0.6f) * Rules->MissSpreadCm);
		Direction = (Miss - Muzzle).GetSafeNormal();
	}
	MulticastShot(Muzzle, Direction);
	UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: fires at %s, %s (chance %.0f %%), %d shot(s) left"), *GetName(), Foe ? *Foe->GetName() : TEXT("nothing"),
		bHit ? TEXT("hit") : TEXT("missed"), Chance * 100.0f, ShotsLeft);
}

// ---------------------------------------------------------------------------------------------- damage and death

FName AFortSoldier::BoneOfHit(const FDamageEvent& DamageEvent) const
{
	if (!DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return NAME_None;
	}
	// a shot of pellets names its bones; a plain hit on the body names one; a hit on the capsule names none
	if (DamageEvent.IsOfType(FFirearmDamageEvent::ClassID))
	{
		const FFirearmDamageEvent& Shot = static_cast<const FFirearmDamageEvent&>(DamageEvent);
		if (!Shot.BoneHits.IsEmpty() && !Shot.BoneHits[0].Bone.IsNone())
		{
			return Shot.BoneHits[0].Bone;
		}
	}
	const FHitResult& Hit = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo;
	FName Bone = Hit.GetComponent() == GetMesh() ? Hit.BoneName : NAME_None;
	if (Bone.IsNone() && Rules && !Hit.TraceStart.Equals(Hit.TraceEnd))
	{
		Bone = FirearmDamage::FindStruckBone(GetMesh(), Hit.TraceStart, Hit.TraceEnd, Rules->HitBoneRadiusCm);
	}
	return Rules ? HeadOrHat(Bone, Hit) : Bone;
}

FName AFortSoldier::HeadOrHat(FName Bone, const FHitResult& Hit) const
{
	// the hat bone runs from the head joint up to the crown, so a blow anywhere on the skull lands on it: only the
	// upper share of that span is the headwear, lower it is the face
	USkeletalMeshComponent* Body = GetMesh();
	if (Bone != Rules->HatBone || !Body || Body->GetBoneIndex(Rules->HeadBone) == INDEX_NONE || Body->GetBoneIndex(Rules->HatBone) == INDEX_NONE)
	{
		return Bone;
	}
	const FVector HeadJoint = Body->GetSocketLocation(Rules->HeadBone);
	const FVector Crown = Body->GetSocketLocation(Rules->HatBone);
	const FVector Span = Crown - HeadJoint;
	if (Span.IsNearlyZero())
	{
		return Bone;
	}
	const FVector Where = Hit.TraceStart.Equals(Hit.TraceEnd) ? FVector(Hit.ImpactPoint) : FMath::ClosestPointOnSegment(Crown, Hit.TraceStart, Hit.TraceEnd);
	const float Along = static_cast<float>(FVector::DotProduct(Where - HeadJoint, Span) / Span.SizeSquared());
	return Along < Rules->HatBoneShare ? Rules->HeadBone : Bone;
}

uint8 AFortSoldier::ZoneOfHit(const FDamageEvent& DamageEvent) const
{
	return HitZones::ZoneOfBone(BoneOfHit(DamageEvent));
}

float AFortSoldier::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !Rules || IsDead())
	{
		return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	}
	// the blow: the bone and zone it struck, where and from where, whether it is a shot and from how far; known
	// before the health may kill, the death clip and look depend on them (GoreBox, owner 2026-10-08)
	const FPointDamageEvent* Point = DamageEvent.IsOfType(FPointDamageEvent::ClassID) ? static_cast<const FPointDamageEvent*>(&DamageEvent) : nullptr;
	const bool bShot = DamageEvent.IsOfType(FFirearmDamageEvent::ClassID);
	PendingHitBone = BoneOfHit(DamageEvent);
	PendingHitZone = HitZones::ZoneOfBone(PendingHitBone);
	PendingHitPoint = Point && !Point->HitInfo.ImpactPoint.IsNearlyZero() ? FVector(Point->HitInfo.ImpactPoint) : GetActorLocation();
	PendingHitDirection = Point && !Point->ShotDirection.IsNearlyZero() ? Point->ShotDirection.GetSafeNormal() : GetActorForwardVector();
	const float ShotDistance = Point && Point->HitInfo.Distance > 0.0f ? Point->HitInfo.Distance
		: (DamageCauser ? static_cast<float>(FVector::Dist(DamageCauser->GetActorLocation(), PendingHitPoint)) : 0.0f);
	const FortSoldierGore::FDeathClips Clips{ Rules->DeathHeadshot != nullptr, Rules->DeathDecapitated != nullptr, Rules->DeathExplode != nullptr,
		Rules->DeathBisected != nullptr, Rules->LeglessCrawlDeath != nullptr };
	// a blade in the torso may cut the body in two, a soldier already crawling dies its crawl death (owner 2026-10-09);
	// a blade whose bone is unknown (the sabre of another soldier) struck the body
	FortSoldierGore::FBlowFacts Facts;
	Facts.bHead = PendingHitZone == HitZones::Head;
	Facts.bBlade = IsBladeBlow(DamageCauser, bShot);
	Facts.bTorso = PendingHitZone == HitZones::Torso || (Facts.bBlade && PendingHitZone == HitZones::None);
	Facts.bShot = bShot;
	Facts.ShotDistance = ShotDistance;
	Facts.bCrawling = IsCrawling();
	Facts.bDecapitationDrawn = FMath::FRand() < Rules->DecapitationChance;
	Facts.bBisectDrawn = FMath::FRand() < Rules->BisectChance;
	PendingDeath = FortSoldierGore::DeathOfBlow(Facts, Rules->ExplodeRangeCm, Clips);

	const double Now = GetWorld()->GetTimeSeconds();
	APawn* Attacker = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	if (!Attacker && DamageCauser)
	{
		Attacker = Cast<APawn>(DamageCauser) ? Cast<APawn>(DamageCauser) : DamageCauser->GetInstigator();
	}
	// whoever hurts it is hunted, and the fort hears of it
	if (Attacker && IsEnemy(Attacker) && (!Target.IsValid() || !bSeesTarget))
	{
		const bool bNew = !Target.IsValid();
		Target = Attacker;
		LastSeenAt = Attacker->GetActorLocation();
		LastSeenTime = Now;
		bHeardOnly = false;
		if (bNew && Garrison.IsValid())
		{
			Garrison->RaiseAlarm(this, Attacker, GetActorLocation(), Rules->AlarmRadiusCm);
		}
	}
	const bool bBusy = Action.State == EFortSoldierState::Melee || Action.State == EFortSoldierState::Draw;

	// a blow on the headwear knocks it off and hurts nothing (GoreBox: the hat takes no damage)
	if (!Action.bHatOff && Rules->HatKnockedOff && !PendingHitBone.IsNone() && PendingHitBone == Rules->HatBone)
	{
		Action.bHatOff = true;
		if (!bBusy)
		{
			StopMoving();
			Action.Flinch = static_cast<uint8>(EFortSoldierFlinch::HatOff);
			Action.HitZone = HitZones::None;
			SetState(EFortSoldierState::HitReact);
		}
		else
		{
			ForceNetUpdate();
			OnRep_Action();
		}
		UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: hat knocked off by %s"), *GetName(), *GetNameSafe(DamageCauser));
		return 0.0f;
	}

	// the blows on the arms and legs wear them down; past their health they are torn off, the soldier living or not
	// (GoreBox dismemberment, owner 2026-10-09)
	TArray<int32, TInlineAllocator<4>> Torn;
	WearLimbs(DamageEvent, DamageAmount, Torn);

	const float Dealt = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Dealt > 0.0f && Point && Rules->BloodRules)
	{
		MulticastBloodHit(PendingHitPoint, PendingHitDirection);
	}
	bool bTorn = false;
	for (const int32 Limb : Torn)
	{
		bTorn |= TearLimb(Limb);
	}
	if (bTorn)
	{
		// the loss goes to every machine at once (the pieces fly, the stance changes), a state change below too
		ForceNetUpdate();
		OnRep_Action();
	}
	if (IsDead())
	{
		return Dealt;
	}
	if (bTorn)
	{
		// the loss is the reaction: it falls to crawl, runs in a panic, or draws the sabre for the hand it has left
		StopMoving();
		Decide();
		return Dealt;
	}
	// crawling, it dies of its wounds once its health is low
	if (IsCrawling() && Rules->CrawlDeathHealth > 0.0f && Health->GetHealth() <= Rules->CrawlDeathHealth)
	{
		BleedOut();
		return Dealt;
	}
	// the flinch: thrown on its back by a shot (KnockbackMusket), the clip of the zone the blow struck, the plain flinch;
	// none on the ground (the flinches stand)
	const uint8 Zone = PendingHitZone;
	const bool bZoneClip = Rules->GetZoneHit(Zone) != nullptr;
	const bool bKnockback = bShot && Rules->KnockbackMusket != nullptr;
	if (Dealt >= Rules->HitReactDamage && Now >= NextHitReact && !bBusy && !IsCrawling() && (Rules->HitReact || bZoneClip || bKnockback))
	{
		NextHitReact = Now + Rules->HitReactCooldown;
		StopMoving();
		Action.HitZone = bZoneClip ? Zone : HitZones::None;
		Action.Flinch = static_cast<uint8>(bKnockback ? EFortSoldierFlinch::Knockback : EFortSoldierFlinch::Zone);
		SetState(EFortSoldierState::HitReact);
		UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: struck at %s (zone %d, %s), plays %s"), *GetName(), *PendingHitBone.ToString(), Zone,
			bKnockback ? TEXT("knocked back") : TEXT("flinch"), *GetNameSafe(GetStateClip(EFortSoldierState::HitReact, Action.Blow)));
	}
	else if (Action.State == EFortSoldierState::Post || Action.State == EFortSoldierState::Patrol)
	{
		StopMoving();
		SetState(EFortSoldierState::Alert);
	}
	return Dealt;
}

bool AFortSoldier::IsBladeBlow(const AActor* DamageCauser, bool bShot) const
{
	if (bShot || !DamageCauser)
	{
		return false;
	}
	// the player's cutlass when its move cuts; the sabre of another soldier
	if (ATikiCreature::IsCutBlow(DamageCauser))
	{
		return true;
	}
	const AFortSoldier* Other = Cast<AFortSoldier>(DamageCauser);
	return Other && Other->IsSabreBlow();
}

void AFortSoldier::WearLimbs(const FDamageEvent& DamageEvent, float DamageAmount, TArray<int32, TInlineAllocator<4>>& OutTorn)
{
	if (DamageAmount <= 0.0f || !DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return;
	}
	// the hits by bone: those of a gun shot (one group per bone struck, scaled by what the weapon asks of the limbs,
	// T-167), else the bone of the blow
	TArray<TPair<FName, float>, TInlineAllocator<8>> Hits;
	float LimbScale = 1.0f;
	if (DamageEvent.IsOfType(FFirearmDamageEvent::ClassID))
	{
		const FFirearmDamageEvent& Shot = static_cast<const FFirearmDamageEvent&>(DamageEvent);
		for (const FFirearmBoneHit& Group : Shot.BoneHits)
		{
			Hits.Emplace(Group.Bone, Group.Damage);
		}
		LimbScale = Shot.LimbDamageScale;
	}
	if (Hits.IsEmpty())
	{
		Hits.Emplace(PendingHitBone, DamageAmount);
	}
	for (const TPair<FName, float>& Hit : Hits)
	{
		const int32 Limb = FortSoldierGore::LimbOfZone(HitZones::ZoneOfBone(Hit.Key));
		if (Limb == INDEX_NONE || IsLimbLost(Limb))
		{
			continue;
		}
		const float LimbHealth = FortSoldierGore::LimbHealth(Limb, Rules->LimbHealthArm, Rules->LimbHealthLeg);
		if (TikiLimbs::AddDamage(LimbDamage, Limb, Hit.Value * LimbScale, LimbHealth))
		{
			OutTorn.AddUnique(Limb);
		}
	}
}

bool AFortSoldier::TearLimb(int32 Limb)
{
	const uint8 Bit = FortSoldierGore::BitOfLimb(Limb);
	if (Bit == 0 || (Action.LostLimbs & Bit) != 0)
	{
		return false;
	}
	const uint8 Before = Action.LostLimbs;
	Action.LostLimbs |= Bit;
	// the musket leaves with the first arm: no shot left, nothing to reload
	if (FortSoldierGore::MusketGone(Action.LostLimbs) && !FortSoldierGore::MusketGone(Before))
	{
		ShotsLeft = 0;
		bLoaded = false;
	}
	// it falls with the first leg: on the ground it bleeds out
	if ((Action.LostLimbs & FortSoldierGore::LegBits) != 0 && (Before & FortSoldierGore::LegBits) == 0)
	{
		CrawlStart = GetWorld()->GetTimeSeconds();
	}
	static const TCHAR* StanceNames[] = { TEXT("standing"), TEXT("fights on with one arm"), TEXT("panics"), TEXT("crawls") };
	UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: %s torn off (%.0f / %.0f), %s"), *GetName(), *LimbBone(Limb).ToString(),
		LimbDamage.IsValidIndex(Limb) ? LimbDamage[Limb] : 0.0f, FortSoldierGore::LimbHealth(Limb, Rules->LimbHealthArm, Rules->LimbHealthLeg),
		StanceNames[static_cast<uint8>(FortSoldierGore::StanceOf(Action.LostLimbs))]);
	return true;
}

void AFortSoldier::BleedOut()
{
	// a death of its wounds, no blow: the crawl death (the plain one without its clip)
	PendingHitBone = NAME_None;
	PendingHitZone = HitZones::None;
	PendingHitPoint = GetActorLocation();
	PendingHitDirection = GetActorForwardVector();
	PendingDeath = Rules->LeglessCrawlDeath ? EFortSoldierDeath::CrawlDeath : EFortSoldierDeath::Plain;
	UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: bleeds out on the ground"), *GetName());
	Health->ApplyHealthDamage(FMath::Max(Health->GetHealth(), 1.0f), this);
}

void AFortSoldier::HandleDeath(AActor* Killer)
{
	if (HasAuthority())
	{
		StopMoving();
		Target = nullptr;
		// the death the blow causes has its own clip and look (GoreBox, owner 2026-10-08)
		Action.HitZone = PendingHitZone;
		Action.Death = static_cast<uint8>(PendingDeath);
		// its body keeps the stance it died in: a leg torn off by this very blow (after this) leaves it standing (T-332)
		Action.bDiedCrawling = IsCrawling();
		SetState(EFortSoldierState::Dead);
		// killed in the air (thrown by the giant crab, which hurts after the throw; knocked back by a blow or a blast):
		// the body falls to the ground under gravity and lies still once it lands (Landed); a frozen movement would
		// leave it hanging in the air (owner 2026-10-10)
		UCharacterMovementComponent* Move = GetCharacterMovement();
		if (Move->IsMovingOnGround())
		{
			Move->DisableMovement();
		}
		else
		{
			Move->Velocity = FVector(Move->Velocity.X * 0.3f, Move->Velocity.Y * 0.3f, FMath::Min(Move->Velocity.Z, 0.0f));
			Move->SetMovementMode(MOVE_Falling);
		}
		UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: dead, killed by %s at %s (zone %d, %s), plays %s"), *GetName(), Killer ? *Killer->GetName() : TEXT("?"),
			*PendingHitBone.ToString(), Action.HitZone, *UEnum::GetValueAsString(PendingDeath), *GetNameSafe(GetStateClip(EFortSoldierState::Dead, Action.Blow)));
		// T-317 (R-28): the dead rate while the body falls, then dormant on the ground, then gone out of the crew's sight
		DeathTime = GetWorld()->GetTimeSeconds();
		ApplyNetRates();
		if (Rules)
		{
			const double First = FortSoldierNet::NextCheckDelay(0.0, false, Rules->DeadDormancySeconds, Rules->DeadLifetimeSeconds, Rules->DeadCheckSeconds);
			GetWorldTimerManager().SetTimer(DeadNetTimer, this, &AFortSoldier::UpdateDeadNet, static_cast<float>(FMath::Max(First, 0.25)), false);
		}
	}
	// the body on the ground no longer blocks the way nor the shots
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCharacterMovement()->bUseRVOAvoidance = false;
}

void AFortSoldier::ApplyNetRates()
{
	if (!HasAuthority() || !Rules)
	{
		return;
	}
	const FortSoldierNet::FRates Rates = FortSoldierNet::PickRates(IsDead(), Rules->NetUpdateFrequency, Rules->MinNetUpdateFrequency,
		Rules->DeadNetUpdateFrequency);
	SetNetUpdateFrequency(Rates.Frequency);
	SetMinNetUpdateFrequency(Rates.MinFrequency);
	SetNetCullDistanceSquared(FMath::Square(FMath::Max(1000.0f, Rules->NetCullDistance)));
}

double AFortSoldier::NearestPlayerDistance() const
{
	double Nearest = TNumericLimits<double>::Max();
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return Nearest;
	}
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		if (const APawn* Pawn = Player ? Player->GetPawn() : nullptr)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()));
		}
	}
	return Nearest;
}

void AFortSoldier::UpdateDeadNet()
{
	if (!HasAuthority() || !Rules || !IsDead() || IsActorBeingDestroyed() || DeathTime < 0.0)
	{
		return;
	}
	const double Since = GetWorld()->GetTimeSeconds() - DeathTime;
	// its last state goes out before the channel closes; a limb torn off the body later flushes it (ForceNetUpdate)
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bSettled = !GetAttachParentActor() && (!Move || !Move->IsFalling());
	if (NetDormancy == DORM_Awake && FortSoldierNet::ShouldGoDormant(Since, Rules->DeadDormancySeconds, bSettled))
	{
		SetNetDormancy(DORM_DormantAll);
		UE_LOG(Logmonke_island, Verbose, TEXT("FortSoldier %s: dead and still for %.1f s, no more replicated (dormant)"), *GetName(), Since);
	}
	if (FortSoldierNet::ShouldRemoveBody(Since, Rules->DeadLifetimeSeconds, NearestPlayerDistance(), Rules->DeadRemoveClearanceCm))
	{
		UE_LOG(Logmonke_island, Log, TEXT("FortSoldier %s: body removed %.0f s after its death, no player within %.0f m"), *GetName(), Since,
			Rules->DeadRemoveClearanceCm / 100.0f);
		Destroy();
		return;
	}
	const double Next = FortSoldierNet::NextCheckDelay(Since, NetDormancy != DORM_Awake, Rules->DeadDormancySeconds, Rules->DeadLifetimeSeconds,
		Rules->DeadCheckSeconds);
	if (Next > 0.0)
	{
		GetWorldTimerManager().SetTimer(DeadNetTimer, this, &AFortSoldier::UpdateDeadNet, static_cast<float>(Next), false);
	}
}

void AFortSoldier::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	if (IsDead())
	{
		return;
	}
	Super::LaunchCharacter(LaunchVelocity, bXYOverride, bZOverride);
}

void AFortSoldier::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (IsDead() && HasAuthority())
	{
		// the body that fell (or was let go by the crab's claw) lies where it landed
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
}

// ---------------------------------------------------------------------------------------------- a body at rest (every machine, T-323)

void AFortSoldier::PostNetReceiveLocationAndRotation()
{
	// a correction of its place received after its body froze (a packet lost before its dormancy): the movement ticks
	// again and smooths the mesh to the new place
	WakeCorpse(TEXT("place corrected"));
	Super::PostNetReceiveLocationAndRotation();
}

FortSoldierCorpse::FRestFacts AFortSoldier::GatherRestFacts() const
{
	FortSoldierCorpse::FRestFacts Facts;
	Facts.bDead = IsDead();
	// the death clip of this machine (a client sees it later, a player joining late plays it again): shown, over and
	// fully in, the upper body layer out
	const bool bDeathShown = Action.State == EFortSoldierState::Dead && PlayedSerial == Action.Serial;
	Facts.bClipDone = bDeathShown && FortSoldierCorpse::IsClipDone(StateClip.Clip != nullptr, StateClip.Time, ClipLength(StateClip.Clip),
		StateClipWeight, UpperWeight);
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	Facts.bFalling = Move && Move->IsFalling();
	Facts.bCarried = GetAttachParentActor() != nullptr;
	Facts.Speed = static_cast<float>(GetVelocity().Size());
	USkeletalMeshComponent* Body = GetMesh();
	Facts.bRagdollAwake = Body && Body->IsSimulatingPhysics() && Body->IsAnyRigidBodyAwake();
	const FTimerManager& Timers = GetWorldTimerManager();
	Facts.bLookPending = Timers.IsTimerActive(DeathTimer) || Timers.IsTimerActive(HatTimer) || !Puffs.IsEmpty() || FlashLeft > 0.0f;
	return Facts;
}

void AFortSoldier::UpdateCorpseRest(float DeltaSeconds)
{
	if (!Rules || bCorpseFrozen)
	{
		return;
	}
	CorpseRest.Advance(FortSoldierCorpse::IsAtRest(GatherRestFacts(), Rules->CorpseStillSpeed), DeltaSeconds);
	if (FortSoldierCorpse::ShouldFreeze(CorpseFreezeAllowed(Rules), CorpseRest, Rules->CorpseRestSeconds))
	{
		FreezeCorpse();
	}
}

void AFortSoldier::FreezeCorpse()
{
	if (bCorpseFrozen || !Rules)
	{
		return;
	}
	bCorpseFrozen = true;
	// nothing left to build each frame: its animation frame (this tick), its pose and bones (the skeleton keeps the last
	// ones, the renderer its last skinning), the bones of its musket, its movement (it lies in MOVE_None); the physics
	// blend of a ragdoll is a tick of its own and keeps moving its bones when the body is pushed
	SetActorTickEnabled(false);
	GetMesh()->SetComponentTickEnabled(false);
	if (Musket)
	{
		Musket->SetComponentTickEnabled(false);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetComponentTickEnabled(false);
	}
	const float Every = FortSoldierCorpse::WatchDelay(Rules->CorpseLODCheckSeconds);
	GetWorldTimerManager().SetTimer(CorpseWatchTimer, this, &AFortSoldier::WatchFrozenCorpse, Every, true);
	UE_LOG(LogMonkeFortCorpse, Verbose, TEXT("FortSoldier %s: body at rest, no longer animated (LOD %d)"), *GetName(), GetMesh()->GetPredictedLODLevel());
}

void AFortSoldier::WakeCorpse(const TCHAR* Why)
{
	CorpseRest.Reset();
	if (!bCorpseFrozen)
	{
		return;
	}
	bCorpseFrozen = false;
	GetWorldTimerManager().ClearTimer(CorpseWatchTimer);
	SetActorTickEnabled(true);
	GetMesh()->SetComponentTickEnabled(true);
	if (Musket)
	{
		Musket->SetComponentTickEnabled(true);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetComponentTickEnabled(true);
	}
	UE_LOG(LogMonkeFortCorpse, Verbose, TEXT("FortSoldier %s: body animated again (%s)"), *GetName(), Why);
}

void AFortSoldier::WatchFrozenCorpse()
{
	if (!bCorpseFrozen)
	{
		GetWorldTimerManager().ClearTimer(CorpseWatchTimer);
		return;
	}
	if (!IsDead() || !CorpseFreezeAllowed(Rules))
	{
		WakeCorpse(TEXT("switched off"));
		return;
	}
	// the detail level the renderer wants (a player came closer or went away): the skeleton refreshed once at that
	// level, the musket follows the body's
	const bool bBodyLOD = GetMesh()->UpdateLODStatus();
	const bool bMusketLOD = Musket && Musket->GetSkeletalMeshAsset() && Musket->IsVisible() && Musket->UpdateLODStatus();
	if (bBodyLOD || bMusketLOD)
	{
		WakeCorpse(TEXT("detail level"));
	}
}

// ---------------------------------------------------------------------------------------------- look (every machine)

FVector AFortSoldier::AttachGear(USceneComponent* Gear, FName Bone, const FTransform& Grip)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Gear || !Body || !Body->GetSkeletalMeshAsset())
	{
		return FVector::OneVector;
	}
	const FName Socket = Body->DoesSocketExist(Bone) ? Bone : NAME_None;
	Gear->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	// the bean bones carry a scale of 100 (Blender FBX export): the grip is in cm from an unscaled bone, so the bone
	// scale is taken out, otherwise the gear is 100 times too big and 100 times too far (like AMusketWeapon)
	const FVector World = Body->GetSocketTransform(Socket, RTS_World).GetScale3D();
	const FVector Own = Body->GetComponentScale();
	FVector BoneScale(World.X / FMath::Max(FMath::Abs(Own.X), UE_KINDA_SMALL_NUMBER), World.Y / FMath::Max(FMath::Abs(Own.Y), UE_KINDA_SMALL_NUMBER),
		World.Z / FMath::Max(FMath::Abs(Own.Z), UE_KINDA_SMALL_NUMBER));
	if (BoneScale.GetAbsMin() < UE_KINDA_SMALL_NUMBER)
	{
		BoneScale = FVector::OneVector;
	}
	Gear->SetRelativeTransform(FTransform(Grip.GetRotation(), Grip.GetLocation() / BoneScale, Grip.GetScale3D() / BoneScale));
	return BoneScale;
}

UAnimSequence* AFortSoldier::GetFireClip() const
{
	const UMusketRules* Poses = Rules ? Rules->MusketPoses.Get() : nullptr;
	return Poses && Poses->BodyFireAnimation ? Poses->BodyFireAnimation.Get() : (Rules ? Rules->Fire.Get() : nullptr);
}

UAnimSequence* AFortSoldier::GetReloadClip() const
{
	const UMusketRules* Poses = Rules ? Rules->MusketPoses.Get() : nullptr;
	return Poses && Poses->BodyReloadAnimation ? Poses->BodyReloadAnimation.Get() : (Rules ? Rules->Reload.Get() : nullptr);
}

float AFortSoldier::GetFireStartTime() const
{
	const UMusketRules* Poses = Rules ? Rules->MusketPoses.Get() : nullptr;
	return Poses && Poses->BodyFireAnimation ? Poses->FireAnimationTriggerTime : 0.0f;
}

float AFortSoldier::GetShotDelay() const
{
	const UMusketRules* Poses = Rules ? Rules->MusketPoses.Get() : nullptr;
	return Poses && Poses->BodyFireAnimation ? Poses->FireDelaySeconds : (Rules ? Rules->FireShotTime : 0.0f);
}

void AFortSoldier::UpdateMusketGrip(float DeltaSeconds)
{
	// only the musket in the hand (ApplyGear: bit 1 the sabre drawn, bit 2 the musket gone with an arm)
	if (!Rules || Action.bSabre || GearApplied < 0 || (GearApplied & 3) != 0)
	{
		MusketPose = EMusketPose::Hand;
		MusketPoseAlpha = 1.0f;
		return;
	}
	// in the hand: the grip of the rules (the bean bones carry a scale, see AttachGear)
	const FTransform& Grip = Rules->MusketInHand;
	const FTransform InHand(Grip.GetRotation(), Grip.GetLocation() / MusketBoneScale, FVector::OneVector / MusketBoneScale);

	// the pose the state wants (owner 2026-10-07: with the tracks of the player's musket the soldiers held theirs across
	// the shoulder while firing, and threw it about while reloading)
	EMusketPose Wanted = EMusketPose::Hand;
	if (StateClip.Clip && bStateClipActive)
	{
		if (StateClip.Clip == GetFireClip() && (Action.State == EFortSoldierState::Aim || Action.State == EFortSoldierState::Fire))
		{
			Wanted = EMusketPose::Aim;
		}
		else if (StateClip.Clip == GetReloadClip() && Action.State == EFortSoldierState::Reload)
		{
			Wanted = EMusketPose::Reload;
		}
	}
	const FTransform Actor = GetActorTransform();
	if (Wanted != MusketPose)
	{
		MusketFromActor = Musket->GetComponentTransform().GetRelativeTransform(Actor);
		MusketPose = Wanted;
		MusketPoseAlpha = 0.0f;
	}
	MusketPoseAlpha = FMath::Min(1.0f, MusketPoseAlpha + DeltaSeconds / FMath::Max(Rules->MusketPoseBlendSeconds, 0.01f));
	if (MusketPose == EMusketPose::Hand && MusketPoseAlpha >= 1.0f)
	{
		Musket->SetRelativeTransform(InHand);
		return;
	}

	const FTransform Held = InHand * GetMesh()->GetSocketTransform(Rules->HandBone, RTS_World);
	FTransform Wished = Held;
	if (MusketPose == EMusketPose::Aim)
	{
		// shouldered: the grip in the right hand, the barrel toward the target (the body may be turned from it)
		const FRotator Line(static_cast<float>(Action.AimPitch), GetActorRotation().Yaw - Rules->FireYawOffsetDeg, 0.0f);
		Wished = FTransform(Line.Quaternion() * Rules->AimMusketOffset.Quaternion(), Held.GetLocation(), Held.GetScale3D());
	}
	else if (MusketPose == EMusketPose::Reload)
	{
		// stood up and held by the barrel: the right hand loads it
		const FQuat Turn = GetActorQuat() * Rules->ReloadMusketRotation.Quaternion();
		const FVector HoldAt = GetMesh()->GetSocketLocation(Rules->ReloadHoldBone);
		Wished = FTransform(Turn, HoldAt - Turn.RotateVector(Rules->ReloadHoldPoint * Held.GetScale3D()), Held.GetScale3D());
	}
	if (MusketPoseAlpha < 1.0f)
	{
		FTransform Blended;
		Blended.Blend(MusketFromActor * Actor, Wished, FMath::SmoothStep(0.0f, 1.0f, MusketPoseAlpha));
		Wished = Blended;
	}
	Musket->SetWorldTransform(Wished);
}

void AFortSoldier::ApplyGear()
{
	if (!Rules)
	{
		return;
	}
	// the gear lost with an arm stays hidden (its piece flew, TearLimbLook): the musket with the first arm, the sabre
	// with the right one when it was in hand (owner 2026-10-09)
	const bool bMusketGone = FortSoldierGore::MusketGone(Action.LostLimbs);
	const bool bSabreGone = FortSoldierGore::SabreGone(Action.LostLimbs, Action.bSabre);
	const int32 Wanted = (Action.bSabre ? 1 : 0) | (bMusketGone ? 2 : 0) | (bSabreGone ? 4 : 0);
	if (GearApplied == Wanted)
	{
		return;
	}
	GearApplied = Wanted;
	Musket->SetVisibility(!bMusketGone, true);
	Sabre->SetVisibility(!bSabreGone);
	if (Action.bSabre)
	{
		AttachGear(Musket, Rules->BackBone, Rules->MusketOnBack);
		AttachGear(Sabre, Rules->HandBone, Rules->SabreInHand);
	}
	else
	{
		MusketBoneScale = AttachGear(Musket, Rules->HandBone, Rules->MusketInHand);
		AttachGear(Sabre, Rules->HipBone, Rules->SabreSheathed);
	}
}

FVector AFortSoldier::GetMuzzle() const
{
	static const FName MuzzleSocket(TEXT("muzzle"));
	if (Musket && Musket->DoesSocketExist(MuzzleSocket))
	{
		return Musket->GetSocketLocation(MuzzleSocket);
	}
	return Musket ? Musket->GetComponentTransform().TransformPosition(Rules ? Rules->MuzzleOffset : FVector::ZeroVector) : GetActorLocation();
}

UAnimSequence* AFortSoldier::GetStateClip(EFortSoldierState State, EFortSoldierBlow Blow) const
{
	switch (State)
	{
	case EFortSoldierState::Aim:
		// with the player's musket the aim is the start of its shot, held at the trigger
		return Rules->MusketPoses && Rules->MusketPoses->BodyFireAnimation ? GetFireClip() : Rules->Aim.Get();
	case EFortSoldierState::Fire:
		return GetFireClip();
	case EFortSoldierState::Reload:
		return GetReloadClip();
	case EFortSoldierState::Draw:
		return Rules->SabreDraw;
	case EFortSoldierState::Sheathe:
		return Rules->SabreSheathe;
	case EFortSoldierState::Melee:
		return Rules->GetBlow(Blow) ? Rules->GetBlow(Blow)->Clip.Get() : nullptr;
	case EFortSoldierState::HitReact:
		switch (static_cast<EFortSoldierFlinch>(Action.Flinch))
		{
		case EFortSoldierFlinch::HatOff: return Rules->HatKnockedOff ? Rules->HatKnockedOff.Get() : Rules->HitReact.Get();
		case EFortSoldierFlinch::Knockback: return Rules->KnockbackMusket ? Rules->KnockbackMusket.Get() : Rules->HitReact.Get();
		default: return Rules->GetZoneHit(Action.HitZone) ? Rules->GetZoneHit(Action.HitZone) : Rules->HitReact.Get();
		}
	case EFortSoldierState::Dead:
		switch (static_cast<EFortSoldierDeath>(Action.Death))
		{
		case EFortSoldierDeath::Headshot: return Rules->DeathHeadshot ? Rules->DeathHeadshot.Get() : Rules->Death.Get();
		case EFortSoldierDeath::Decapitated: return Rules->DeathDecapitated ? Rules->DeathDecapitated.Get() : Rules->Death.Get();
		case EFortSoldierDeath::Exploded: return Rules->DeathExplode ? Rules->DeathExplode.Get() : Rules->Death.Get();
		case EFortSoldierDeath::Bisected: return Rules->DeathBisected ? Rules->DeathBisected.Get() : Rules->Death.Get();
		case EFortSoldierDeath::CrawlDeath: return Rules->LeglessCrawlDeath ? Rules->LeglessCrawlDeath.Get() : Rules->Death.Get();
		default: return Rules->Death.Get();
		}
	default:
		return nullptr;
	}
}

void AFortSoldier::UpdateAnimation(float DeltaSeconds)
{
	UBeanLibraryAnimInstance* Anim = Cast<UBeanLibraryAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Anim)
	{
		return;
	}
	const float Blend = FMath::Max(Rules->BlendSeconds, 0.01f);
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bSabre = Action.bSabre;

	// locomotion: at a post, walking (patrol step, musket at port arms), running; on the ground without a leg or in a
	// panic without the arm of its weapon, the GoreBox loops (owner 2026-10-09)
	const FortSoldierGore::EStance Stance = FortSoldierGore::StanceOf(Action.LostLimbs);
	const bool bWounded = Stance == FortSoldierGore::EStance::Crawl || Stance == FortSoldierGore::EStance::Panic;
	float Rate = 1.0f;
	const UAnimSequence* Loco = bWounded ? WoundedLocomotion(Stance, Speed, Rate) : nullptr;
	if (!Loco)
	{
		if (Speed < 20.0f)
		{
			Loco = bSabre && Rules->SabreIdle ? Rules->SabreIdle.Get() : Rules->IdleGuard.Get();
		}
		else if (Speed < Rules->RunAboveSpeed)
		{
			Loco = bSabre || !Rules->Patrol ? Rules->Walk.Get() : Rules->Patrol.Get();
			Rate = FortGarrisonMath::LocomotionRate(Speed, Rules->WalkNoSlideSpeed, Rules->MinPlayRate, Rules->MaxPlayRate);
		}
		else
		{
			Loco = Rules->Run;
			Rate = FortGarrisonMath::LocomotionRate(Speed, Rules->RunNoSlideSpeed, Rules->MinPlayRate, Rules->MaxPlayRate);
		}
	}
	if (Loco && Loco != BaseTo.Clip)
	{
		BaseFrom = BaseTo;
		BaseTo = FFortSoldierClipPlay{ Loco, 0.0f, Rate };
		BaseAlpha = BaseFrom.Clip ? 0.0f : 1.0f;
	}
	BaseTo.Rate = Rate;
	BaseTo.Time += DeltaSeconds * BaseTo.Rate;
	BaseFrom.Time += DeltaSeconds * BaseFrom.Rate;
	BaseAlpha = FMath::Min(1.0f, BaseAlpha + DeltaSeconds / Blend);
	Frame.Base.Reset();
	if (BaseFrom.Clip && BaseAlpha < 1.0f)
	{
		Frame.Base.Add(FBeanAnimSample{ static_cast<const UAnimSequence*>(BaseFrom.Clip), BaseFrom.Time, 1.0f - BaseAlpha, true });
	}
	if (BaseTo.Clip)
	{
		Frame.Base.Add(FBeanAnimSample{ static_cast<const UAnimSequence*>(BaseTo.Clip), BaseTo.Time, BaseAlpha, true });
	}

	// the clip of the state over it (aim, fire, reload, blows, flinch, death)
	if (PlayedSerial != Action.Serial)
	{
		PlayedSerial = Action.Serial;
		if (const UAnimSequence* Clip = GetStateClip(Action.State, Action.Blow))
		{
			StateClip = FFortSoldierClipPlay{ Clip, Action.State == EFortSoldierState::Fire ? GetFireStartTime() : 0.0f, 1.0f };
			bStateClipActive = true;
		}
		else
		{
			bStateClipActive = false;
		}
	}
	if (StateClip.Clip)
	{
		StateClip.Time += DeltaSeconds;
		// the aim shoulders the musket and holds it there until the shot
		if (Action.State == EFortSoldierState::Aim && bStateClipActive && StateClip.Clip == GetFireClip() && GetFireStartTime() > 0.0f)
		{
			StateClip.Time = FMath::Min(StateClip.Time, GetFireStartTime());
		}
	}
	StateClipWeight = bStateClipActive ? FMath::Min(1.0f, StateClipWeight + DeltaSeconds / Blend) : FMath::Max(0.0f, StateClipWeight - DeltaSeconds / Blend);
	if (!bStateClipActive && StateClipWeight <= 0.0f)
	{
		StateClip.Clip = nullptr;
	}
	Frame.FullBody = FBeanAnimSample{ StateClip.Clip, StateClip.Time, StateClipWeight, false };

	// running, the arms hold the musket (or the sabre) instead of swinging; nothing once the musket fell with an arm,
	// nor on the wounded loops
	const UAnimSequence* UpperClip = Speed >= Rules->RunAboveSpeed && !bStateClipActive && !bWounded
		? (bSabre ? Rules->SabreIdle.Get() : (FortSoldierGore::MusketGone(Action.LostLimbs) ? nullptr : Rules->MusketHold.Get())) : nullptr;
	if (UpperClip && UpperClip != Upper.Clip)
	{
		Upper = FFortSoldierClipPlay{ UpperClip, 0.0f, 1.0f };
	}
	Upper.Time += DeltaSeconds;
	UpperWeight = FMath::FInterpConstantTo(UpperWeight, UpperClip ? 1.0f : 0.0f, DeltaSeconds, 1.0f / Blend);
	Frame.Upper.Reset();
	if (Upper.Clip && UpperWeight > 0.0f)
	{
		Frame.Upper.Add(FBeanAnimSample{ Upper.Clip, Upper.Time, 1.0f, true });
	}
	Frame.UpperWeight = UpperWeight;
	Frame.UpperBodyRoot = Rules->UpperBodyRootBone;
	Anim->SetFrame(Frame);
	UpdateMusketGrip(DeltaSeconds);
}

void AFortSoldier::MulticastShot_Implementation(FVector_NetQuantize Muzzle, FVector_NetQuantizeNormal Direction)
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (Rules->ShotSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rules->ShotSound, Muzzle, 1.0f, 1.0f, 0.0f, Rules->ShotAttenuation);
	}
	const FVector Forward = FVector(Direction).GetSafeNormal();
	// the ball as a tracer round, to the first thing in its way on this machine (owner 2026-10-10)
	if (Rules->MusketTracer.bEnabled)
	{
		FHitResult TracerHit;
		FCollisionQueryParams TracerParams(SCENE_QUERY_STAT(FortSoldierTracer), false, this);
		const FVector TracerEnd = FVector(Muzzle) + Forward * Rules->MusketTracer.MaxDistanceCm;
		const bool bTracerHit = GetWorld()->LineTraceSingleByChannel(TracerHit, Muzzle, TracerEnd, ECC_Visibility, TracerParams);
		AFirearmTracer::Launch(GetWorld(), Rules->MusketTracer, Muzzle, bTracerHit ? FVector(TracerHit.ImpactPoint) : TracerEnd);
	}
	if (Rules->FlashMesh)
	{
		if (!Flash)
		{
			Flash = NewObject<UStaticMeshComponent>(this, TEXT("MuzzleFlash"));
			Flash->SetStaticMesh(Rules->FlashMesh);
			Flash->SetMaterial(0, Rules->FlashMaterial);
			Flash->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Flash->SetCastShadow(false);
			Flash->SetAbsolute(true, true, true);
			Flash->RegisterComponent();
		}
		// the flame of a 1 m sphere stretched along the shot, its back at the muzzle
		Flash->SetWorldLocationAndRotation(Muzzle + Forward * (50.0f * Rules->FlashScale.X), Forward.Rotation());
		Flash->SetWorldScale3D(Rules->FlashScale);
		Flash->SetVisibility(true);
		FlashLeft = Rules->FlashSeconds;
	}
	if (Rules->SmokeSystem)
	{
		// billowing, lit puffs of the particle system
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Rules->SmokeSystem, Muzzle + Forward * 10.0f, Forward.Rotation(),
			FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
	else if (Rules->SmokeMesh)
	{
		if (Puffs.Num() >= MaxPuffs)
		{
			if (UStaticMeshComponent* Oldest = Puffs[0].Mesh.Get())
			{
				Oldest->DestroyComponent();
			}
			Puffs.RemoveAt(0);
		}
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
		Puff->SetStaticMesh(Rules->SmokeMesh);
		Puff->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Puff->SetCastShadow(false);
		Puff->SetAbsolute(true, true, true);
		Puff->RegisterComponent();
		Puff->SetWorldLocation(Muzzle + Forward * 30.0f);
		Puff->SetWorldScale3D(FVector(Rules->SmokeStartScale));
		UMaterialInstanceDynamic* Material = Rules->SmokeMaterial ? Puff->CreateDynamicMaterialInstance(0, Rules->SmokeMaterial) : nullptr;
		Puffs.Add(FFortSoldierSmokePuff{ Puff, Material, 0.0f });
	}
}

void AFortSoldier::MulticastSound_Implementation(USoundBase* Sound, FVector_NetQuantize Location)
{
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
}

void AFortSoldier::UpdateEffects(float DeltaSeconds)
{
	if (Flash && FlashLeft > 0.0f)
	{
		FlashLeft -= DeltaSeconds;
		if (FlashLeft <= 0.0f)
		{
			Flash->SetVisibility(false);
		}
	}
	if (Puffs.IsEmpty() || !Rules)
	{
		return;
	}
	const float Life = FMath::Max(Rules->SmokeSeconds, 0.1f);
	for (int32 Index = Puffs.Num() - 1; Index >= 0; --Index)
	{
		FFortSoldierSmokePuff& Puff = Puffs[Index];
		UStaticMeshComponent* PuffMesh = Puff.Mesh.Get();
		Puff.Age += DeltaSeconds;
		if (!PuffMesh || Puff.Age >= Life)
		{
			if (PuffMesh)
			{
				PuffMesh->DestroyComponent();
			}
			Puffs.RemoveAt(Index);
			continue;
		}
		const float Alpha = Puff.Age / Life;
		// the puff swells, rises a little in the breeze and fades
		PuffMesh->SetWorldScale3D(FVector(FMath::Lerp(Rules->SmokeStartScale, Rules->SmokeEndScale, FMath::Sqrt(Alpha))));
		PuffMesh->AddWorldOffset(FVector(0.0f, 0.0f, 25.0f * DeltaSeconds));
		if (UMaterialInstanceDynamic* Material = Puff.Material.Get())
		{
			Material->SetScalarParameterValue(Rules->SmokeOpacityParameter, 1.0f - Alpha);
		}
	}
}

// ---------------------------------------------------------------------------------------------- GoreBox look (every machine)

void AFortSoldier::MulticastBloodHit_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction)
{
	if (UTikiBloodSubsystem* Blood = Rules && Rules->BloodRules ? UTikiBloodSubsystem::Get(this) : nullptr)
	{
		Blood->SpawnHit(Rules->BloodRules, Location, FVector(Direction), this);
	}
}

void AFortSoldier::ApplyGoreLook()
{
	// a frozen body shows the change: its skeleton refreshes again for a while (T-323)
	WakeCorpse(TEXT("gore"));
	// the stance of the lost limbs matters to every machine, a dedicated server too (the capsule, the crawl speed)
	ApplyStance();
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// a body lying on the ground reaches past the box of a standing one (T-332)
	ApplyBodyBounds();
	// the hat flies at the moment of its clip when the flinch plays it, at once otherwise (a busy soldier, a late joiner)
	if (Action.bHatOff && !bHatHidden)
	{
		bHatHidden = true;
		const bool bClip = Action.State == EFortSoldierState::HitReact && Action.Flinch == static_cast<uint8>(EFortSoldierFlinch::HatOff);
		if (bClip && Rules->HatOffTime > 0.0f)
		{
			GetWorldTimerManager().SetTimer(HatTimer, this, &AFortSoldier::KnockHatOff, Rules->HatOffTime, false);
		}
		else
		{
			KnockHatOff();
		}
	}
	// the limbs torn since the last look: hidden, their pieces flying, the stumps bleeding; those of a soldier met
	// already maimed (a player joining late, before its BeginPlay) are only hidden (owner 2026-10-09)
	if (const uint8 NewLost = static_cast<uint8>(Action.LostLimbs & ~AppliedLost))
	{
		const bool bFresh = HasActorBegunPlay();
		for (int32 Limb = 0; Limb < FortSoldierGore::LimbCount; ++Limb)
		{
			if (NewLost & FortSoldierGore::BitOfLimb(Limb))
			{
				TearLimbLook(Limb, bFresh);
			}
		}
		AppliedLost = Action.LostLimbs;
	}
	if (Action.State == EFortSoldierState::Dead && GoreSerial != Action.Serial)
	{
		GoreSerial = Action.Serial;
		switch (static_cast<EFortSoldierDeath>(Action.Death))
		{
		case EFortSoldierDeath::Decapitated:
			GetWorldTimerManager().SetTimer(DeathTimer, this, &AFortSoldier::TakeHeadOff, FMath::Max(Rules->DecapitationTime, 0.01f), false);
			break;
		case EFortSoldierDeath::Exploded:
			GetWorldTimerManager().SetTimer(DeathTimer, this, &AFortSoldier::BurstBody, FMath::Max(Rules->ExplodeRagdollTime, 0.01f), false);
			break;
		case EFortSoldierDeath::Bisected:
			GetWorldTimerManager().SetTimer(DeathTimer, this, &AFortSoldier::CutInTwo, FMath::Max(Rules->BisectTime, 0.01f), false);
			break;
		default:
			break;
		}
	}
}

UStaticMeshComponent* AFortSoldier::LaunchPiece(UStaticMesh* PieceMesh, FName Bone, const FVector& Offset, const FVector& Velocity)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!PieceMesh || !Body || Bone.IsNone() || Body->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return nullptr;
	}
	// the piece keeps the turn of the body (the pieces are exported upright, like the body), at the bone's place
	// shifted by Offset in the body's frame
	const FTransform BoneWorld = Body->GetSocketTransform(Bone, RTS_World);
	const FRotator Turn = Body->GetComponentRotation();
	return LaunchPieceAt(PieceMesh, FTransform(Turn, BoneWorld.GetLocation() + Turn.RotateVector(Offset)), Velocity);
}

UStaticMeshComponent* AFortSoldier::LaunchPieceAt(UStaticMesh* PieceMesh, const FTransform& World, const FVector& Velocity)
{
	if (!PieceMesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	Piece->SetStaticMesh(PieceMesh);
	Piece->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Piece->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Piece->SetAbsolute(true, true, true);
	Piece->RegisterComponent();
	// the piece keeps its own size (cm)
	Piece->SetWorldTransform(FTransform(World.GetRotation(), World.GetLocation(), FVector::OneVector));
	Piece->SetSimulatePhysics(true);
	Piece->SetPhysicsLinearVelocity(Velocity);
	Piece->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-180.0f, 180.0f)));
	Pieces.Add(Piece);
	return Piece;
}

void AFortSoldier::KnockHatOff()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Rules || Rules->HatBone.IsNone() || Body->GetBoneIndex(Rules->HatBone) == INDEX_NONE)
	{
		return;
	}
	WakeCorpse(TEXT("hat"));
	LaunchPiece(HatMesh, Rules->HatBone, Rules->HatMeshOffset, -GetActorForwardVector() * Rules->PieceSpeed * 0.5f + FVector(0.0f, 0.0f, Rules->PieceSpeed));
	Body->HideBoneByName(Rules->HatBone, PBO_None);
}

void AFortSoldier::TakeHeadOff()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Rules || bHeadHidden || Rules->HeadBone.IsNone() || Body->GetBoneIndex(Rules->HeadBone) == INDEX_NONE)
	{
		return;
	}
	bHeadHidden = true;
	WakeCorpse(TEXT("head"));
	const FVector Neck = Body->GetSocketLocation(Rules->HeadBone);
	const FVector Up = (FVector::UpVector + GetActorForwardVector() * 0.3f).GetSafeNormal();
	const FVector Fling = GetActorForwardVector() * Rules->PieceSpeed * 0.4f + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.8f);
	// the headwear goes with the head
	if (!bHatHidden)
	{
		bHatHidden = true;
		LaunchPiece(HatMesh, Rules->HatBone, Rules->HatMeshOffset, Fling + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.4f));
	}
	LaunchPiece(HeadMesh, Rules->HeadBone, Rules->HeadMeshOffset, Fling);
	Body->HideBoneByName(Rules->HeadBone, PBO_None);
	if (UTikiBloodSubsystem* Blood = Rules->BloodRules ? UTikiBloodSubsystem::Get(this) : nullptr)
	{
		// the jet out of the neck hangs from the bone under the head (the hidden head bone has no size left)
		Blood->SpawnSliceSpray(Rules->BloodRules, Body, Body->GetParentBone(Rules->HeadBone), Neck, Up);
		Blood->SpawnChunks(Rules->BloodRules, Neck, Up);
		Blood->SpawnSplat(Rules->BloodRules, Neck, this);
	}
}

void AFortSoldier::BurstBody()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Rules || bBurst)
	{
		return;
	}
	bBurst = true;
	WakeCorpse(TEXT("burst"));
	const FVector Chest = Body->GetBoneIndex(Rules->BackBone) != INDEX_NONE ? Body->GetSocketLocation(Rules->BackBone) : GetActorLocation();
	const FVector Away = -GetActorForwardVector();
	TakeHeadOff();
	// the body thrown back as a ragdoll
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCollisionProfileName(FName(TEXT("Ragdoll")));
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->SetAllPhysicsLinearVelocity(Away * Rules->ExplodeImpulse + FVector(0.0f, 0.0f, Rules->ExplodeImpulse * 0.6f));
	if (UTikiBloodSubsystem* Blood = Rules->BloodRules ? UTikiBloodSubsystem::Get(this) : nullptr)
	{
		Blood->SpawnChunks(Rules->BloodRules, Chest, Away);
		Blood->SpawnChunks(Rules->BloodRules, Chest, FVector::UpVector);
		Blood->SpawnSplat(Rules->BloodRules, Chest, this);
	}
}

// ---------------------------------------------------------------------------------------------- limbs look (every machine, owner 2026-10-09)

FName AFortSoldier::LimbBone(int32 Limb) const
{
	if (!Rules)
	{
		return NAME_None;
	}
	switch (Limb)
	{
	case FortSoldierGore::ArmL: return Rules->ArmLBone;
	case FortSoldierGore::ArmR: return Rules->ArmRBone;
	case FortSoldierGore::LegL: return Rules->LegLBone;
	case FortSoldierGore::LegR: return Rules->LegRBone;
	default: return NAME_None;
	}
}

UStaticMesh* AFortSoldier::LimbPiece(int32 Limb) const
{
	switch (Limb)
	{
	case FortSoldierGore::ArmL: return ArmLMesh;
	case FortSoldierGore::ArmR: return ArmRMesh;
	case FortSoldierGore::LegL: return LegLMesh;
	case FortSoldierGore::LegR: return LegRMesh;
	default: return nullptr;
	}
}

void AFortSoldier::ApplyStance()
{
	if (!Rules)
	{
		return;
	}
	// a living soldier without a leg crawls; a dead one keeps the stance it died in (T-332: a crawl stance taken after
	// the death, its movement stopped, hung the body 25 cm over the ground)
	const int8 Wanted = FortSoldierGore::IsCrawlStance(Action.LostLimbs, Action.State == EFortSoldierState::Dead, Action.bDiedCrawling) ? 1 : 0;
	if (StanceApplied == Wanted)
	{
		return;
	}
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (StanceApplied < 0)
	{
		// the standing sizes, from the blueprint: the mesh offset the character cached at its start for the network
		// smoothing (on a client the smoothing may already have moved the mesh itself)
		StandingHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		StandingMeshOffset = GetBaseTranslationOffset();
	}
	StanceApplied = Wanted;
	// on the ground: a low capsule (the movement lets it down onto the floor), the mesh kept at its feet; the clips of
	// the crawl lie on the ground at the feet origin
	const float HalfHeight = Wanted ? FMath::Min(Rules->CrawlCapsuleHalfHeight, StandingHalfHeight) : StandingHalfHeight;
	Capsule->SetCapsuleHalfHeight(HalfHeight, true);
	const FVector MeshOffset(StandingMeshOffset.X, StandingMeshOffset.Y,
		FortSoldierGore::StanceMeshZ(StandingMeshOffset.Z, StandingHalfHeight, HalfHeight, Wanted ? Rules->CrawlMeshOffset : 0.0f));
	GetMesh()->SetRelativeLocation(MeshOffset);
	// T-332 ("les soldats dépop sous la mitrailleuse", seen from a client): the network smoothing of a simulated proxy
	// sets the mesh back to the offset the character cached (BaseTranslationOffset) at every move the server sends; left
	// at the standing offset over a capsule 25 cm lower, every client drew the crawling soldier, then its body, 25 cm
	// under the ground. The engine's crouch moves this cached offset the same way.
	CacheInitialMeshOffset(MeshOffset, GetBaseRotationOffsetRotator());
	if (HasAuthority() && Wanted)
	{
		GetCharacterMovement()->MaxWalkSpeed = Rules->CrawlSpeed;
	}
}

void AFortSoldier::ApplyBodyBounds()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Rules || !Body)
	{
		return;
	}
	if (StandingBoundsScale < 0.0f)
	{
		StandingBoundsScale = Body->BoundsScale;
	}
	// a body on the ground (crawling or dead) lies about a metre along it from its feet: past the box of a standing one
	const bool bLying = Action.State == EFortSoldierState::Dead || StanceApplied == 1;
	const float Wanted = FortSoldierCorpse::BodyBoundsScale(bLying, StandingBoundsScale, Rules->LyingBoundsScale);
	if (!FMath::IsNearlyEqual(Body->BoundsScale, Wanted))
	{
		Body->SetBoundsScale(Wanted);
	}
}

void AFortSoldier::TearLimbLook(int32 Limb, bool bFresh)
{
	USkeletalMeshComponent* Body = GetMesh();
	const FName Bone = LimbBone(Limb);
	if (!Body || !Rules || Bone.IsNone() || Body->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return;
	}
	if (bFresh)
	{
		// the piece flies aside and up; the jet out of the stump hangs from the bone above the cut (the hidden bone has
		// no size left), along the lost bone
		const FVector Side = GetActorRightVector() * (FortSoldierGore::IsLeft(Limb) ? -1.0f : 1.0f);
		const FVector Fling = Side * Rules->PieceSpeed * 0.5f + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.6f) - GetActorForwardVector() * Rules->PieceSpeed * 0.2f;
		LaunchPiece(LimbPiece(Limb), Bone, FortSoldierGore::IsArm(Limb) ? Rules->ArmMeshOffset : Rules->LegMeshOffset, Fling);
		if (UTikiBloodSubsystem* Blood = Rules->BloodRules ? UTikiBloodSubsystem::Get(this) : nullptr)
		{
			const FName Parent = Body->GetParentBone(Bone);
			const FVector Joint = Body->GetSocketLocation(Bone);
			const FVector Along = (Joint - Body->GetSocketLocation(Parent)).GetSafeNormal(UE_SMALL_NUMBER, Side);
			Blood->SpawnSliceSpray(Rules->BloodRules, Body, Parent, Joint, Along);
			Blood->SpawnChunks(Rules->BloodRules, Joint, Along);
			Blood->SpawnSplat(Rules->BloodRules, Joint, this);
		}
		DropGearLook(Limb);
	}
	Body->HideBoneByName(Bone, PBO_None);
}

void AFortSoldier::DropGearLook(int32 Limb)
{
	if (!Rules || !FortSoldierGore::IsArm(Limb))
	{
		return;
	}
	// from where the gear is now (hidden by ApplyGear already, its place is still that of the hand or the back)
	const FVector Fling = -GetActorForwardVector() * Rules->PieceSpeed * 0.3f + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.5f);
	if (!bMusketFlown && FortSoldierGore::MusketGone(Action.LostLimbs))
	{
		bMusketFlown = true;
		LaunchPieceAt(Rules->MusketDropMesh, Musket->GetComponentTransform(), Fling);
	}
	if (!bSabreFlown && FortSoldierGore::SabreGone(Action.LostLimbs, Action.bSabre))
	{
		bSabreFlown = true;
		LaunchPieceAt(Rules->SabreMesh, Sabre->GetComponentTransform(), Fling);
	}
}

void AFortSoldier::CutInTwo()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Rules || bBisected || Rules->BisectBone.IsNone() || Body->GetBoneIndex(Rules->BisectBone) == INDEX_NONE)
	{
		return;
	}
	bBisected = true;
	WakeCorpse(TEXT("cut"));
	const FName Parent = Body->GetParentBone(Rules->BisectBone);
	const FVector Waist = Body->GetSocketLocation(Rules->BisectBone);
	const FVector Away = -GetActorForwardVector();
	const FVector Fling = Away * Rules->PieceSpeed * 0.4f + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.5f);
	// what hangs from the waist flies: the upper torso, the head and its hat, the arms still on
	LaunchPiece(TorsoUpperMesh, Rules->BisectBone, Rules->TorsoUpperMeshOffset, Fling);
	if (!bHeadHidden)
	{
		bHeadHidden = true;
		LaunchPiece(HeadMesh, Rules->HeadBone, Rules->HeadMeshOffset, Fling + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.3f));
		if (!bHatHidden)
		{
			bHatHidden = true;
			LaunchPiece(HatMesh, Rules->HatBone, Rules->HatMeshOffset, Fling + FVector(0.0f, 0.0f, Rules->PieceSpeed * 0.6f));
		}
	}
	for (const int32 Arm : { FortSoldierGore::ArmL, FortSoldierGore::ArmR })
	{
		if (!IsLimbLost(Arm))
		{
			const FVector Side = GetActorRightVector() * (FortSoldierGore::IsLeft(Arm) ? -1.0f : 1.0f);
			LaunchPiece(LimbPiece(Arm), LimbBone(Arm), Rules->ArmMeshOffset, Fling + Side * Rules->PieceSpeed * 0.4f);
		}
	}
	Body->HideBoneByName(Rules->BisectBone, PBO_None);
	if (UTikiBloodSubsystem* Blood = Rules->BloodRules ? UTikiBloodSubsystem::Get(this) : nullptr)
	{
		Blood->SpawnSliceSpray(Rules->BloodRules, Body, Parent, Waist, FVector::UpVector);
		Blood->SpawnChunks(Rules->BloodRules, Waist, FVector::UpVector);
		Blood->SpawnChunks(Rules->BloodRules, Waist, Away);
		Blood->SpawnSplat(Rules->BloodRules, Waist, this);
	}
}

void AFortSoldier::ReapplyHiddenBones()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Rules || !Body)
	{
		return;
	}
	// a new mesh on a frozen body: its skeleton refreshes again for a while (T-323)
	WakeCorpse(TEXT("skin"));
	auto Hide = [Body](FName Bone)
	{
		if (!Bone.IsNone() && Body->GetBoneIndex(Bone) != INDEX_NONE)
		{
			Body->HideBoneByName(Bone, PBO_None);
		}
	};
	if (bHatHidden)
	{
		Hide(Rules->HatBone);
	}
	if (bHeadHidden)
	{
		Hide(Rules->HeadBone);
	}
	if (bBisected)
	{
		Hide(Rules->BisectBone);
	}
	for (int32 Limb = 0; Limb < FortSoldierGore::LimbCount; ++Limb)
	{
		if (IsLimbLost(Limb))
		{
			TearLimbLook(Limb, false);
		}
	}
	AppliedLost = Action.LostLimbs;
}

const UAnimSequence* AFortSoldier::WoundedLocomotion(FortSoldierGore::EStance Stance, float Speed, float& OutRate) const
{
	OutRate = 1.0f;
	const bool bStill = Speed < 20.0f;
	if (Stance == FortSoldierGore::EStance::Crawl)
	{
		// dragging itself; writhing on the ground when still
		if (bStill && Rules->WritheGroundLoop)
		{
			return Rules->WritheGroundLoop;
		}
		if (!bStill)
		{
			OutRate = FortGarrisonMath::LocomotionRate(Speed, Rules->CrawlNoSlideSpeed, Rules->MinPlayRate, Rules->MaxPlayRate);
		}
		return Rules->LeglessCrawlLoop;
	}
	if (Stance == FortSoldierGore::EStance::Panic)
	{
		// running from its foes, panicking on the spot
		if (!bStill && Rules->FleeLoop)
		{
			OutRate = FortGarrisonMath::LocomotionRate(Speed, Rules->RunNoSlideSpeed, Rules->MinPlayRate, Rules->MaxPlayRate);
			return Rules->FleeLoop;
		}
		return Rules->ArmlessPanicLoop;
	}
	return nullptr;
}
