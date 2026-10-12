// monke_island

#include "Creatures/TikiCreature.h"
#include "Creatures/HitZoneMath.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiAnimInstance.h"
#include "Creatures/TikiBlood.h"
#include "Creatures/TikiCorpse.h"
#include "Creatures/TikiDismember.h"
#include "Creatures/TikiRagdollCorpse.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiShredDemo.h"
#include "Creatures/MobHealthBarComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Stealth/StealthRules.h"
#include "Stealth/StealthSubsystem.h"
#include "Water/IslandWaterSubsystem.h"
#include "Weapons/BayonetMusketWeapon.h"
#include "Weapons/CutlassWeapon.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/MusketWeapon.h"
#include "Weapons/PirateMusketWeapon.h"
#include "Weapons/PistolWeapon.h"
#include "Weapons/ShotgunWeapon.h"
#include "monke_island.h"
#include "AIController.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ATikiCreature::ATikiCreature()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// not the engine's 100 updates per second: the rules set the rate of each kind at BeginPlay (T-312, review R-28)
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(2.0f);

	GetCapsuleComponent()->InitCapsuleSize(30.0f, 55.0f);
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Move->MaxWalkSpeed = 120.0f;
	Move->bRunPhysicsWithNoController = true;
	// pushes the boat and props like a person, not with the engine default of 7500 N (T-063)
	Move->bPushForceScaledToMass = false;
	Move->PushForceFactor = UIslanderMovementComponent::HumanPushForce;
	Move->InitialPushForceFactor = UIslanderMovementComponent::HumanPushForce / 30.0f;

	// the Blender exports face +Y with the origin at the feet
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -55.0f), FRotator(0.0f, -90.0f, 0.0f));
	// clips crossfaded by a native animation instance (no AnimBP)
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UTikiAnimInstance::StaticClass());
	GetMesh()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	HeldItem = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeldItem"));
	HeldItem->SetupAttachment(GetMesh());
	HeldItem->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Health = CreateDefaultSubobject<UIslanderHealthComponent>(TEXT("Health"));
	Health->SetIsReplicated(true);

	HealthBar = CreateDefaultSubobject<UMobHealthBarComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetCapsuleComponent());
}

void ATikiCreature::BeginPlay()
{
	// an instance placed in a level may have been saved with empty clips that hide those of its class (T-151): it
	// takes the reactions and the death of its class back
	if (const ATikiCreature* Defaults = GetClass()->GetDefaultObject<ATikiCreature>(); Defaults && Defaults != this)
	{
		if (Anims.Combat.AvailableMask() == 0 && Defaults->Anims.Combat.AvailableMask() != 0)
		{
			Anims.Combat = Defaults->Anims.Combat;
			UE_LOG(Logmonke_island, Warning, TEXT("%s: combat reactions empty on the instance, taken from %s"), *GetName(), *GetClass()->GetName());
		}
		if (!Anims.Death && Defaults->Anims.Death)
		{
			Anims.Death = Defaults->Anims.Death;
		}
	}

	Super::BeginPlay();

	HeldItem->SetLeaderPoseComponent(GetMesh());
	Health->OnDeath.AddDynamic(this, &ATikiCreature::HandleDeath);

	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ATikiCreature::HandleTakeAnyDamage);
		Health->MaxHealth = GetRulesHealth();
		Health->ResetHealth();
		ApplyNetRules();
	}
	// culling boxes that hold the whole model and its weapon, not only its physics bodies (owner 2026-10-10: mobs that
	// vanished at the screen edges or behind cover)
	if (Rules)
	{
		GetMesh()->SetBoundsScale(Rules->MeshBoundsScale);
		HeldItem->SetBoundsScale(Rules->HeldItemBoundsScale);
	}
	// eased steps and turns: a body gets up to speed and turns, it does not snap (every machine smooths the same)
	if (Rules)
	{
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->MaxAcceleration = Rules->CreatureAcceleration;
		Move->BrakingDecelerationWalking = Rules->CreatureBraking;
		Move->RotationRate = FRotator(0.0f, Rules->CreatureTurnRate, 0.0f);
	}
	// the blood effects load in the background, ready for the first hit (nothing on a dedicated server, T-153)
	if (UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this))
	{
		Blood->Preload(Rules);
	}
	// the shredding demos too (T-168): their meshes are heavy, loaded once per set
	if (ShredDemoSet && Rules && Rules->bShredDemos)
	{
		ShredDemoSet->Preload(this, Rules);
	}
	StuckCheckLocation = GetActorLocation();
	StuckCheckTime = GetWorld()->GetTimeSeconds();
	NextIdleVoiceTime = StuckCheckTime + FMath::FRandRange(Voice.IdleIntervalMin, FMath::Max(Voice.IdleIntervalMin, Voice.IdleIntervalMax));
	// a creature arriving in relevancy during its blow shows the rest of its preparation (T-306)
	if (!HasAuthority())
	{
		StartReplicatedAction();
	}
	UpdateAnimation();
}

const FTikiNetRules* ATikiCreature::GetNetRules() const
{
	return Rules ? &Rules->WarriorNet : nullptr;
}

void ATikiCreature::ApplyNetRules()
{
	const FTikiNetRules* Net = GetNetRules();
	if (!HasAuthority() || !Net)
	{
		return;
	}
	SetNetUpdateFrequency(Net->NetUpdateFrequency);
	SetMinNetUpdateFrequency(FMath::Min(Net->MinNetUpdateFrequency, Net->NetUpdateFrequency));
	SetNetCullDistanceSquared(FMath::Square(Net->NetCullDistance));
}

void ATikiCreature::SetDirector(ATikiDirector* InDirector)
{
	Director = InDirector;
}

bool ATikiCreature::IsDead() const
{
	return Health && Health->IsDead();
}

void ATikiCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// a reaction it survives holds its decisions (T-150)
	if (HasAuthority() && !IsDead() && Rules && GetWorld()->GetTimeSeconds() >= ThinkSuspendedUntil)
	{
		// without both arms it only runs in a panic (owner 2026-10-09); on the ground it keeps its decisions, slower
		if (IsArmless() && !IsCrawling())
		{
			PanicThink(DeltaSeconds);
		}
		else
		{
			ServerThink(DeltaSeconds);
		}
	}
	UpdateAnimation();

	// a grunt now and then, only near the local camera
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Voice.Idle.IsEmpty() && Now >= NextIdleVoiceTime && !IsDead() && GetNetMode() != NM_DedicatedServer)
	{
		NextIdleVoiceTime = Now + FMath::FRandRange(Voice.IdleIntervalMin, FMath::Max(Voice.IdleIntervalMin, Voice.IdleIntervalMax));
		const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
		if (Camera && !IsActionPlaying() && FVector::DistSquared(Camera->GetCameraLocation(), GetActorLocation()) < FMath::Square(Voice.IdleDistance))
		{
			PlayVoice(Voice.Idle);
		}
	}
}

void ATikiCreature::PlayVoice(const TArray<TObjectPtr<USoundBase>>& Sounds) const
{
	// the voice of an action received long after it started stays silent (T-306)
	if (Sounds.IsEmpty() || GetNetMode() == NM_DedicatedServer || (Rules && ActionAge > Rules->ActionLateSoundSeconds))
	{
		return;
	}
	if (USoundBase* Sound = Sounds[FMath::RandRange(0, Sounds.Num() - 1)])
	{
		UGameplayStatics::SpawnSoundAttached(Sound, GetMesh(), NAME_None, FVector(0.0f, 0.0f, 70.0f), EAttachLocation::KeepRelativeOffset, true,
			Voice.Volume, Voice.Pitch * FMath::FRandRange(0.94f, 1.06f), 0.0f, Voice.Attenuation);
	}
}

float ATikiCreature::MoveToward(const FVector& Target, float Speed)
{
	FVector Delta = Target - GetActorLocation();
	Delta.Z = 0.0f;
	const float Distance = static_cast<float>(Delta.Size());
	if (Distance > 1.0f)
	{
		// land creatures: no step into water deeper than MaxWadeDepth (unless it leads out of deeper water);
		// the caller sees no progress (UpdateStuck) and picks another goal
		const FVector Direction = Delta / Distance;
		if (Rules && !TikiMath::CanWade(WaterDepthAt(GetActorLocation() + Direction * Rules->WaterProbeDistance),
			WaterDepthAt(GetActorLocation()), Rules->MaxWadeDepth))
		{
			return Distance;
		}
		// a creature that lost a leg limps (T-167)
		GetCharacterMovement()->MaxWalkSpeed = Speed * GetLimbSpeedScale();
		AddMovementInput(Direction, 1.0f, true);
	}
	return Distance;
}

float ATikiCreature::WaterDepthAt(const FVector& Location) const
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Water)
	{
		return 0.0f;
	}
	const float Surface = Water->GetWaterSurfaceAt(Location);
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiWaterProbe), false, this);
	const FVector Top(Location.X, Location.Y, FMath::Max(static_cast<float>(Location.Z), Surface) + 200.0f);
	const FVector Bottom(Location.X, Location.Y, Surface - 2000.0f);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return 2000.0f;
	}
	return FMath::Max(0.0f, Surface - static_cast<float>(Hit.ImpactPoint.Z));
}

bool ATikiCreature::IsOutOfReachInWater(const APawn* Pawn) const
{
	if (!Pawn || !Rules)
	{
		return false;
	}
	const UPawnMovementComponent* Move = Pawn->GetMovementComponent();
	return (Move && Move->IsSwimming()) || WaterDepthAt(Pawn->GetActorLocation()) > Rules->MaxWadeDepth;
}

void ATikiCreature::FaceToward(const FVector& Target)
{
	FVector Delta = Target - GetActorLocation();
	Delta.Z = 0.0f;
	if (!Delta.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, Delta.Rotation().Yaw, 0.0f));
	}
}

bool ATikiCreature::UpdateStuck(bool bWantsToMove)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bWantsToMove)
	{
		StuckCheckLocation = GetActorLocation();
		StuckCheckTime = Now;
		return false;
	}
	if (Now - StuckCheckTime < 2.0)
	{
		return false;
	}
	const bool bStuck = FVector::DistSquared2D(StuckCheckLocation, GetActorLocation()) < FMath::Square(30.0);
	StuckCheckLocation = GetActorLocation();
	StuckCheckTime = Now;
	return bStuck;
}

void ATikiCreature::PlayAction(ETikiAction Action, uint8 Variant)
{
	if (!HasAuthority())
	{
		return;
	}
	// a replicated state, not an unreliable multicast: a lost packet or a client that gets the creature during its
	// preparation still sees the telegraph (T-306, review R-27)
	ActionRep.Action = Action;
	ActionRep.Variant = Variant;
	ActionRep.Serial = ActionRep.Serial == MAX_uint8 ? 1 : ActionRep.Serial + 1;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	ActionRep.StartServerTime = static_cast<float>(GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds());
	LocalActionSerial = ActionRep.Serial;
	ForceNetUpdate();
	ActionAge = 0.0f;
	PlayLocalAction(Action, Variant);
}

void ATikiCreature::OnRep_ActionRep()
{
	// received with the creature, before its BeginPlay (relevancy, a player joining): BeginPlay starts it
	if (HasActorBegunPlay())
	{
		StartReplicatedAction();
	}
}

void ATikiCreature::StartReplicatedAction()
{
	if (HasAuthority() || ActionRep.Serial == 0 || ActionRep.Serial == LocalActionSerial)
	{
		return;
	}
	LocalActionSerial = ActionRep.Serial;
	// as far in as the server is: its blow lands when the server deals it; a clip already over is not played (the clips
	// and the sounds check ActionAge)
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Age = GameState ? GameState->GetServerWorldTimeSeconds() - ActionRep.StartServerTime : 0.0;
	ActionAge = static_cast<float>(FMath::Max(0.0, Age));
	PlayLocalAction(ActionRep.Action, ActionRep.Variant);
	ActionAge = 0.0f;
}

bool ATikiCreature::IsActionPlaying() const
{
	return GetWorld()->GetTimeSeconds() < ActionEndTime;
}

bool ATikiCreature::CanPlayAction() const
{
	// a combat reaction plays to its end
	return !bDeathPlayed && !IsCombatReactionPlaying();
}

void ATikiCreature::PlayOneShot(UAnimSequenceBase* Clip, float Duration, float PlayRate)
{
	if (!Clip || !CanPlayAction())
	{
		return;
	}
	const float Rate = FMath::Max(PlayRate, 0.05f);
	// received late (T-306): that far into the clip, so it ends when the server's does; nothing once it is over
	float Start = 0.0f;
	float Left = 0.0f;
	if (!TikiMath::LateActionStart(ActionAge, Duration, Rate, Clip->GetPlayLength(), false, Start, Left))
	{
		return;
	}
	ActionEndTime = GetWorld()->GetTimeSeconds() + Left;
	CurrentClip = nullptr;
	PlayClip(Clip, false, Start, Rate);
}

bool ATikiCreature::IsClipForBody(const UAnimSequenceBase* Clip) const
{
	const USkeletalMesh* Body = GetMesh() ? GetMesh()->GetSkeletalMeshAsset() : nullptr;
	return Clip && Body && Clip->GetSkeleton() && Clip->GetSkeleton() == Body->GetSkeleton();
}

bool ATikiCreature::IsLocomotionClip(const UAnimSequenceBase* Clip) const
{
	return Clip && (Clip == Anims.Idle || Clip == Anims.IdleCombat || Clip == Anims.Walk || Clip == Anims.Run
		|| Clip == Anims.LeglessCrawl || Clip == Anims.WritheGround || Clip == Anims.ArmlessPanic);
}

void ATikiCreature::PlayLocalAction(ETikiAction Action, uint8 Variant)
{
	// a combat reaction plays to its end
	if (!CanPlayAction())
	{
		return;
	}
	switch (Action)
	{
	case ETikiAction::Attack:
		PlayVoice(Voice.Attack);
		break;
	case ETikiAction::Hit:
	case ETikiAction::HitBack:
	case ETikiAction::Stagger:
		PlayVoice(Voice.Hurt);
		break;
	case ETikiAction::Emerge:
	case ETikiAction::Victory:
		PlayVoice(Voice.Emerge);
		break;
	case ETikiAction::Alert:
	case ETikiAction::Taunt:
		PlayVoice(Voice.Aggro);
		break;
	case ETikiAction::Startle:
		PlayVoice(Voice.Flee);
		break;
	case ETikiAction::Roar:
		PlayVoice(Voice.Aggro);
		return;
	case ETikiAction::Scream:
		PlayVoice(Voice.Flee);
		return;
	default:
		break;
	}
	UAnimSequenceBase* Clip = nullptr;
	bool bLoop = false;
	float Duration = 0.0f;
	switch (Action)
	{
	case ETikiAction::Attack:
		// variant 0 is the base blow, the others the extra ones of the library; with one arm torn off, the one-armed
		// blow (GoreBox, owner 2026-10-09)
		Clip = Variant > 0 && Anims.AttackVariants.IsValidIndex(Variant - 1) && Anims.AttackVariants[Variant - 1] ? Anims.AttackVariants[Variant - 1].Get() : Anims.Attack.Get();
		if (Anims.ArmlessFightOn && CountLimbsLost(ETikiLimbLoss::Arm) == 1)
		{
			Clip = Anims.ArmlessFightOn;
		}
		break;
	case ETikiAction::Hit:
	case ETikiAction::Emerge:
		Clip = Anims.Hit;
		break;
	case ETikiAction::HitBack:
		Clip = Anims.HitBack ? Anims.HitBack.Get() : Anims.Hit.Get();
		break;
	case ETikiAction::Stagger:
		Clip = Anims.Stagger ? Anims.Stagger.Get() : Anims.Hit.Get();
		break;
	case ETikiAction::HitZone:
		Clip = Anims.GetZoneHit(Variant) ? Anims.GetZoneHit(Variant) : Anims.Hit.Get();
		break;
	case ETikiAction::Alert:
		Clip = Anims.Alert;
		break;
	case ETikiAction::Taunt:
		Clip = Anims.Taunt;
		break;
	case ETikiAction::Victory:
		Clip = Anims.Victory;
		break;
	case ETikiAction::Startle:
		Clip = Anims.Startle;
		break;
	case ETikiAction::LookAround:
		Clip = Anims.LookAround;
		break;
	case ETikiAction::Greet:
		Clip = Anims.Greet;
		break;
	case ETikiAction::Gesture:
		Clip = Anims.Gestures.IsEmpty() ? nullptr : Anims.Gestures[Variant % Anims.Gestures.Num()].Get();
		break;
	case ETikiAction::Collect:
		Clip = Anims.Collect ? Anims.Collect.Get() : Anims.Idle.Get();
		bLoop = true;
		Duration = Rules ? Rules->CollectDuration : 2.0f;
		break;
	case ETikiAction::Cower:
		Clip = Anims.Cower;
		bLoop = true;
		Duration = 1.5f;
		break;
	case ETikiAction::Work:
		Clip = Anims.WorkGather ? Anims.WorkGather.Get() : Anims.Collect ? Anims.Collect.Get() : Anims.Idle.Get();
		bLoop = true;
		Duration = Rules ? Rules->ActivityClipDuration : 4.0f;
		break;
	case ETikiAction::Pray:
		// the basket offered to the shrine (Cower is the fear clip, kept for fear)
		Clip = Anims.Collect ? Anims.Collect.Get() : Anims.Idle.Get();
		bLoop = true;
		Duration = Rules ? Rules->ActivityClipDuration : 4.0f;
		break;
	case ETikiAction::Rest:
		Clip = Anims.Idle;
		bLoop = true;
		Duration = Rules ? Rules->ActivityClipDuration : 4.0f;
		break;
	default:
		break;
	}
	if (!Clip)
	{
		return;
	}
	// received late (T-306): that far into the clip, so it ends when the server's does; nothing once it is over
	float Start = 0.0f;
	float Left = 0.0f;
	if (!TikiMath::LateActionStart(ActionAge, Duration, 1.0f, Clip->GetPlayLength(), bLoop, Start, Left))
	{
		return;
	}
	ActionEndTime = GetWorld()->GetTimeSeconds() + Left;
	CurrentClip = nullptr;
	PlayClip(Clip, bLoop, Start);
}

void ATikiCreature::UpdateAnimation()
{
	if (bDeathPlayed || IsActionPlaying())
	{
		return;
	}
	const float WalkSpeed = Rules ? FMath::Min(Rules->VillagerWalkSpeed, Rules->WarriorWalkSpeed) : 120.0f;
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	float ClipSpeed = 0.0f;
	// a creature without a leg drags itself (writhing when still), one without its arms panics: the GoreBox loops of the
	// realistic mobs (owner 2026-10-09); the walk, run and idle as before without the clips
	if (Anims.LeglessCrawl && IsCrawling())
	{
		const bool bStill = Speed < 10.0f;
		PlayClip(bStill && Anims.WritheGround ? Anims.WritheGround.Get() : Anims.LeglessCrawl.Get(), true);
		ClipSpeed = bStill ? 0.0f : Anims.CrawlSpeed;
	}
	else if (Anims.ArmlessPanic && IsArmless())
	{
		PlayClip(Anims.ArmlessPanic, true);
	}
	// a warrior facing its foe while it moves: side steps, walk backward (T-275)
	else if (UAnimSequenceBase* Directional = Speed >= 10.0f ? GetDirectionalClip(Speed, ClipSpeed) : nullptr)
	{
		PlayClip(Directional, true);
	}
	else
	{
		ClipSpeed = 0.0f;
		switch (TikiMath::ChooseLocomotion(Speed, WalkSpeed * 1.2f))
		{
		case ETikiLocomotion::Walk:
			PlayClip(Anims.Walk, true);
			ClipSpeed = Anims.WalkSpeed > 0.0f ? Anims.WalkSpeed : (Rules ? Rules->WalkClipSpeed : 110.0f);
			break;
		case ETikiLocomotion::Run:
			PlayClip(Anims.Run ? Anims.Run.Get() : Anims.Walk.Get(), true);
			ClipSpeed = Anims.RunSpeed > 0.0f ? Anims.RunSpeed : (Rules ? Rules->RunClipSpeed : 380.0f);
			break;
		default:
			PlayClip(GetIdleClip() ? GetIdleClip() : Anims.Idle.Get(), true);
			break;
		}
	}
	// the feet keep up with the body
	if (UTikiAnimInstance* Anim = GetTikiAnim())
	{
		Anim->SetPlayRate(ClipSpeed > 0.0f ? TikiMath::ClipRate(Speed, ClipSpeed, Rules ? Rules->MinClipRate : 0.6f, Rules ? Rules->MaxClipRate : 1.6f) : 1.0f);
	}
}

UTikiAnimInstance* ATikiCreature::GetTikiAnim() const
{
	return Cast<UTikiAnimInstance>(GetMesh()->GetAnimInstance());
}

void ATikiCreature::PlayClip(UAnimSequenceBase* Clip, bool bLoop, float StartTime, float PlayRate)
{
	if (!Clip || Clip == CurrentClip)
	{
		return;
	}
	CurrentClip = Clip;
	if (UTikiAnimInstance* Anim = GetTikiAnim())
	{
		// locomotion crossfades slowly, a blow or a hit cuts in fast and always restarts
		const bool bLocomotion = IsLocomotionClip(Clip);
		const float Blend = bLocomotion ? (Rules ? Rules->LocomotionBlendTime : 0.25f) : (Rules ? Rules->ActionBlendTime : 0.12f);
		Anim->PlayClip(Clip, bLoop, Blend, PlayRate, !bLocomotion, StartTime);
	}
	else
	{
		GetMesh()->PlayAnimation(Clip, bLoop);
		GetMesh()->SetPosition(StartTime, false);
		GetMesh()->SetPlayRate(PlayRate);
	}
}

void ATikiCreature::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.0f)
	{
		return;
	}
	// recorded before the death check (the health component may have dealt the killing blow first): a body killed
	// by a cut becomes two halves
	const bool bCut = bNextBlowCut || IsCutBlow(DamageCauser);
	bNextBlowCut = false;
	bLastBlowCut = bCut;
	const AActor* From = DamageCauser ? DamageCauser : (InstigatedBy ? InstigatedBy->GetPawn() : nullptr);
	LastBlowDirection = From ? (GetActorLocation() - From->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	if (IsDead())
	{
		return;
	}
	AActor* Causer = InstigatedBy && InstigatedBy->GetPawn() ? InstigatedBy->GetPawn() : DamageCauser;
	// a musket shot it survives: stagger then back on guard, its decisions on hold meanwhile (T-150)
	const ETikiCombatClip Reaction = ChooseCombatReaction(false);
	if (Reaction != ETikiCombatClip::None)
	{
		OnDamaged(Causer, Damage);
		StartCombatReaction(Reaction);
	}
	else
	{
		// a strong blow staggers, a blow in the back throws forward, a blow in a zone with its clip (GoreBox, owner
		// 2026-10-08) plays it, then a cut staggers, the others are a plain hit
		ETikiAction HitAction = ETikiAction::Hit;
		uint8 HitVariant = 0;
		const uint8 Zone = HitZones::ZoneOfBone(PendingHitBone);
		if (Anims.Stagger && Rules && Damage >= Rules->StaggerDamage)
		{
			HitAction = ETikiAction::Stagger;
		}
		else if (Anims.HitBack && TikiMath::IsBlowFromBehind(LastBlowDirection, GetActorForwardVector()))
		{
			HitAction = ETikiAction::HitBack;
		}
		else if (Anims.GetZoneHit(Zone))
		{
			HitAction = ETikiAction::HitZone;
			HitVariant = Zone;
			UE_LOG(Logmonke_island, Log, TEXT("%s: hit at %s (zone %d), plays %s"), *GetName(), *PendingHitBone.ToString(), Zone,
				*GetNameSafe(Anims.GetZoneHit(Zone)));
		}
		else if (Anims.Stagger && bCut)
		{
			HitAction = ETikiAction::Stagger;
		}
		PlayAction(HitAction, HitVariant);
		OnDamaged(Causer, Damage);
	}

	// a cut blow that does not kill may take a limb off (cutlass combo)
	if (bCut && Rules && !IsDead() && FMath::FRand() < Rules->LimbCutChance)
	{
		const FName Limb = PickLimbToCut();
		if (!Limb.IsNone())
		{
			CutOffLimb(Limb, LastBlowDirection);
		}
	}
}

void ATikiCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATikiCreature, CutBones);
	DOREPLIFETIME(ATikiCreature, CombatReaction);
	DOREPLIFETIME(ATikiCreature, ActionRep);
	DOREPLIFETIME_CONDITION(ATikiCreature, Rules, COND_InitialOnly);
}

bool ATikiCreature::IsCutBlow(const AActor* DamageCauser)
{
	// the server plays the move it applies the hit of: GetLocalMove is the striking move there; a weapon without an edge
	// (a mob's spear or club, T-161) never cuts
	const ACutlassWeapon* Cutlass = Cast<ACutlassWeapon>(DamageCauser);
	const FCutlassAttack* Attack = Cutlass && Cutlass->Rules ? Cutlass->Rules->GetAttack(Cutlass->GetLocalMove()) : nullptr;
	return Attack && CutlassMath::CutsLimbs(Cutlass->Rules->bEdged, Attack->bCut);
}

FName ATikiCreature::PickLimbToCut() const
{
	if (!Rules)
	{
		return NAME_None;
	}
	TArray<FName, TInlineAllocator<4>> Left;
	for (const FName& Bone : Rules->LimbBones)
	{
		bool bGone = GetMesh()->GetBoneIndex(Bone) == INDEX_NONE;
		for (const FName& Cut : CutBones)
		{
			bGone = bGone || Bone == Cut || GetMesh()->BoneIsChildOf(Bone, Cut);
		}
		if (!bGone)
		{
			Left.Add(Bone);
		}
	}
	return Left.IsEmpty() ? NAME_None : Left[FMath::RandRange(0, Left.Num() - 1)];
}

bool ATikiCreature::CutOffLimb(FName Bone, const FVector& Direction)
{
	return !IsDead() && DetachLimb(Bone, Direction);
}

bool ATikiCreature::DetachLimb(FName Bone, const FVector& Direction)
{
	if (!HasAuthority() || Bone.IsNone() || GetMesh()->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return false;
	}
	for (const FName& Cut : CutBones)
	{
		if (Bone == Cut || GetMesh()->BoneIsChildOf(Bone, Cut))
		{
			return false;
		}
	}
	// the piece takes the pose of the arm right now, then the arm vanishes from the body on every machine
	if (Rules)
	{
		// it drops at the foot of the body, nudged along the blow (bounded by the corpse, T-155)
		const FVector Fling = Direction.IsNearlyZero() ? GetActorRightVector() : Direction;
		ATikiCorpse::SpawnPart(GetWorld(), Rules, ATikiCorpse::Snapshot(GetMesh(), HeldItem, CutBones, DismemberSet), ETikiBodyPart::Limb, Bone,
			Fling * Rules->CutFlingSpeed);
	}
	SpawnStumpBlood(Bone);
	CutBones.Add(Bone);
	BloodCutCount = CutBones.Num();
	ApplyCutBones();
	++BlowCuts;
	if (Rules)
	{
		LegsLost = 0;
		for (int32 Limb = 0; Limb < Rules->Limbs.Num(); ++Limb)
		{
			LegsLost += Rules->Limbs[Limb].Loss == ETikiLimbLoss::Leg && IsLimbGone(Limb) ? 1 : 0;
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s loses %s"), *GetName(), *Bone.ToString());
	return true;
}

float ATikiCreature::GetLimbSpeedScale() const
{
	return Rules ? TikiLimbs::SpeedScale(LegsLost, Rules->LegLossSpeedScale, Rules->BothLegsLossSpeedScale) : 1.0f;
}

int32 ATikiCreature::CountLimbsLost(ETikiLimbLoss Loss) const
{
	int32 Count = 0;
	if (Rules && !CutBones.IsEmpty())
	{
		for (int32 Limb = 0; Limb < Rules->Limbs.Num(); ++Limb)
		{
			// a limb whose bone the skeleton lacks is not lost, it never was there
			const FTikiLimb& Rule = Rules->Limbs[Limb];
			Count += Rule.Loss == Loss && GetMesh()->GetBoneIndex(Rule.Bone) != INDEX_NONE && IsLimbGone(Limb) ? 1 : 0;
		}
	}
	return Count;
}

bool ATikiCreature::IsCrawling() const
{
	return CountLimbsLost(ETikiLimbLoss::Leg) > 0;
}

bool ATikiCreature::IsArmless() const
{
	return CountLimbsLost(ETikiLimbLoss::Arm) >= 2;
}

void ATikiCreature::PanicThink(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Here = GetActorLocation();
	// a new way now and then, at the goal, or when something stops it: away from the nearest foe, round and round
	// without one
	if (Now >= NextPanicGoalTime || FVector::DistSquared2D(PanicGoal, Here) < FMath::Square(60.0) || UpdateStuck(true))
	{
		NextPanicGoalTime = Now + 0.6;
		const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
		if (const APawn* Foe = FindNearestFoe(Rules->AggroRadius))
		{
			const FVector Away = (Here - Foe->GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, GetActorForwardVector());
			PanicGoal = Here + Away.RotateAngleAxis(FMath::FRandRange(-40.0f, 40.0f), FVector::UpVector) * Rules->ArmlessFleeDistance;
		}
		else
		{
			PanicGoal = TikiMath::PointOnRing(Here, Angle, FMath::FRand(), Rules->ArmlessFleeDistance * 0.2f, Rules->ArmlessFleeDistance * 0.5f);
		}
	}
	MoveToward(PanicGoal, Rules->ArmlessPanicSpeed);
	if (Now >= NextPanicScreamTime)
	{
		NextPanicScreamTime = Now + FMath::FRandRange(1.8f, 3.5f);
		PlayAction(ETikiAction::Scream);
	}
}

void ATikiCreature::SpawnWaistBlood()
{
	if (Rules && !IsActorBeingDestroyed())
	{
		SpawnStumpBlood(Rules->HalfCutBone);
	}
}

int32 ATikiCreature::LimbOfBone(FName Bone) const
{
	const USkinnedAsset* Asset = GetMesh()->GetSkinnedAsset();
	if (!Rules || !Asset || Bone.IsNone())
	{
		return INDEX_NONE;
	}
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	TArray<int32> LimbBones;
	TArray<int32> Parents;
	LimbBones.Reserve(Rules->Limbs.Num());
	for (const FTikiLimb& Limb : Rules->Limbs)
	{
		LimbBones.Add(Skeleton.FindBoneIndex(Limb.Bone));
	}
	Parents.Reserve(Skeleton.GetNum());
	for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
	{
		Parents.Add(Skeleton.GetParentIndex(Index));
	}
	return TikiLimbs::LimbOfBone(Skeleton.FindBoneIndex(Bone), LimbBones, Parents);
}

bool ATikiCreature::IsLimbGone(int32 Limb) const
{
	if (!Rules || !Rules->Limbs.IsValidIndex(Limb))
	{
		return true;
	}
	const FName Bone = Rules->Limbs[Limb].Bone;
	if (GetMesh()->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return true;
	}
	for (const FName& Cut : CutBones)
	{
		if (Bone == Cut || GetMesh()->BoneIsChildOf(Bone, Cut))
		{
			return true;
		}
	}
	return false;
}

int32 ATikiCreature::PickShredLimb() const
{
	if (!Rules)
	{
		return INDEX_NONE;
	}
	TArray<int32, TInlineAllocator<8>> Left;
	for (int32 Limb = 0; Limb < Rules->Limbs.Num(); ++Limb)
	{
		if (!IsLimbGone(Limb))
		{
			Left.Add(Limb);
		}
	}
	return Left.IsEmpty() ? INDEX_NONE : Left[FMath::RandRange(0, Left.Num() - 1)];
}

bool ATikiCreature::TearLimb(int32 Limb, const FVector& Direction)
{
	if (!Rules || IsLimbGone(Limb))
	{
		return false;
	}
	const FTikiLimb& Rule = Rules->Limbs[Limb];
	// the hand lets go: the weapon falls as a pickup instead of leaving with the arm piece
	static const FName WeaponBone(TEXT("weapon_r"));
	if (Rule.Loss == ETikiLimbLoss::Arm && Rules->bArmLossDropsWeapon && !IsDead() && HeldItem->IsVisible()
		&& (WeaponBone == Rule.Bone || GetMesh()->BoneIsChildOf(WeaponBone, Rule.Bone)))
	{
		DropWeapon();
	}
	if (!DetachLimb(Rule.Bone, Direction))
	{
		return false;
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s torn off by a shot (%.0f / %.0f)"), *GetName(), *Rule.Part.ToString(),
		LimbDamage.IsValidIndex(Limb) ? LimbDamage[Limb] : 0.0f, Rule.Health);
	return true;
}

FName ATikiCreature::ResolveHitBone(const FHitResult& Hit) const
{
	if (!Hit.BoneName.IsNone() && Hit.GetComponent() == GetMesh())
	{
		return Hit.BoneName;
	}
	// the mesh does not collide, the capsule takes the trace: the bone the shot passes nearest to
	if (!Rules || Hit.TraceStart.Equals(Hit.TraceEnd))
	{
		return NAME_None;
	}
	return FirearmDamage::FindStruckBone(GetMesh(), Hit.TraceStart, Hit.TraceEnd, Rules->LimbHitRadius);
}

void ATikiCreature::ApplyLimbHits(const FDamageEvent& DamageEvent, float& InOutDamage)
{
	if (!Rules || Rules->Limbs.IsEmpty() || !DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return;
	}
	// the hits by bone: those of the weapon (one group per bone struck), else the one of a plain shot
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
	else
	{
		Hits.Emplace(ResolveHitBone(static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo), InOutDamage);
	}
	TArray<int32, TInlineAllocator<8>> ToTear;
	for (const TPair<FName, float>& Hit : Hits)
	{
		const int32 Limb = LimbOfBone(Hit.Key);
		if (Limb != INDEX_NONE && !IsLimbGone(Limb) && TikiLimbs::AddDamage(LimbDamage, Limb, Hit.Value * LimbScale, Rules->Limbs[Limb].Health))
		{
			ToTear.AddUnique(Limb);
		}
	}
	if (!PendingBlowDirection.IsNearlyZero())
	{
		LastBlowDirection = PendingBlowDirection;
	}
	const FVector Direction = LastBlowDirection.IsNearlyZero() ? GetActorRightVector() : LastBlowDirection;
	for (const int32 Limb : ToTear)
	{
		// the head torn off kills: the blow deals at least what health is left, and the death is the decapitated one
		if (TearLimb(Limb, Direction) && Rules->Limbs[Limb].Loss == ETikiLimbLoss::Kills && Health)
		{
			InOutDamage = FMath::Max(InOutDamage, Health->GetHealth());
			PendingBlow.bHeadTorn = true;
		}
	}
}

bool ATikiCreature::IsMaimed() const
{
	static const FName WeaponBone(TEXT("weapon_r"));
	for (const FName& Cut : CutBones)
	{
		if (Cut == WeaponBone || GetMesh()->BoneIsChildOf(WeaponBone, Cut))
		{
			return true;
		}
	}
	return false;
}

void ATikiCreature::OnRep_CutBones()
{
	// the bones cut since the last update bleed; those of a creature arriving already maimed (a player joining late,
	// initial replication before BeginPlay) do not
	if (HasActorBegunPlay())
	{
		for (int32 Index = BloodCutCount; Index < CutBones.Num(); ++Index)
		{
			SpawnStumpBlood(CutBones[Index]);
		}
	}
	BloodCutCount = CutBones.Num();
	ApplyCutBones();
}

void ATikiCreature::SpawnStumpBlood(FName Bone)
{
	UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this);
	const USkeletalMeshComponent* Body = GetMesh();
	const FName Parent = Body->GetParentBone(Bone);
	if (!Blood || !Rules || Parent.IsNone())
	{
		return;
	}
	// the jet leaves the stump along the lost bone (parent joint toward the cut joint), attached to the parent: it
	// follows the body that keeps moving
	const FVector Joint = Body->GetBoneLocation(Bone);
	const FVector Along = (Joint - Body->GetBoneLocation(Parent)).GetSafeNormal(UE_SMALL_NUMBER, GetActorRightVector());
	Blood->SpawnSliceSpray(Rules, GetMesh(), Parent, Joint, Along);
	Blood->SpawnChunks(Rules, Joint, Along);
	Blood->SpawnSplat(Rules, Joint, this);
}

void ATikiCreature::MulticastBloodHits_Implementation(const TArray<FVector_NetQuantize>& Locations, FVector_NetQuantizeNormal Direction)
{
	if (UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this))
	{
		// a spray per pellet, one splat on the ground for the whole shot
		for (int32 Index = 0; Index < Locations.Num(); ++Index)
		{
			Blood->SpawnHit(Rules, Locations[Index], Direction, this, Index == 0);
		}
	}
}

void ATikiCreature::MulticastBloodHit_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction)
{
	if (UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this))
	{
		Blood->SpawnHit(Rules, Location, Direction, this);
	}
}

void ATikiCreature::ApplyCutBones()
{
	static const FName HeldBones[] = { TEXT("weapon_r"), TEXT("prop_l") };
	bool bHandGone = false;
	// the closed modules draw the stumps; without them the cut bones collapse (T-072)
	const bool bModules = ApplyDismemberModules();
	for (const FName& Cut : CutBones)
	{
		if (!bModules)
		{
			GetMesh()->HideBoneByName(Cut, PBO_None);
		}
		for (const FName& HeldBone : HeldBones)
		{
			bHandGone = bHandGone || HeldBone == Cut || GetMesh()->BoneIsChildOf(HeldBone, Cut);
		}
	}
	// the held item goes with the limb piece
	if (bHandGone)
	{
		HeldItem->SetVisibility(false);
	}
}

bool ATikiCreature::ApplyDismemberModules()
{
	USkeletalMeshComponent* Body = GetMesh();
	USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	TArray<bool> Shown;
	const bool bUsable = !CutBones.IsEmpty() && DismemberSet && BodyAsset
		&& DismemberSet->Select(BodyAsset->GetRefSkeleton(), ETikiBodyPart::Whole, NAME_None, CutBones, Shown);
	if (!bUsable)
	{
		// back to the whole mesh (a later cut the modules cannot close)
		if (!DismemberModules.IsEmpty())
		{
			for (USkeletalMeshComponent* Module : DismemberModules)
			{
				if (Module)
				{
					Module->DestroyComponent();
				}
			}
			DismemberModules.Reset();
			Body->SetVisibility(true);
		}
		return false;
	}
	if (DismemberModules.IsEmpty())
	{
		// the modules follow the body pose (leader pose): they need its skeleton
		for (const FTikiDismemberModule& Module : DismemberSet->Modules)
		{
			if (Module.Mesh->GetSkeleton() != BodyAsset->GetSkeleton())
			{
				UE_LOG(Logmonke_island, Warning, TEXT("%s: module %s is not on the skeleton of %s, bones hidden instead"),
					*GetName(), *GetNameSafe(Module.Mesh), *GetNameSafe(BodyAsset));
				return false;
			}
		}
		const TArray<UMaterialInterface*> Materials = Body->GetMaterials();
		for (const FTikiDismemberModule& Module : DismemberSet->Modules)
		{
			USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this);
			Part->SetupAttachment(Body);
			Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Part->SetSkeletalMeshAsset(Module.Mesh);
			// the part's box is the body's physics box: as wide as the body's own (dépop at the screen edges)
			Part->SetBoundsScale(Rules ? Rules->MeshBoundsScale : 1.0f);
			Part->RegisterComponent();
			Part->SetLeaderPoseComponent(Body);
			TikiDismember::ApplyMaterials(Part, BodyAsset, Materials);
			DismemberModules.Add(Part);
		}
		// unseen, the body still animates and simulates: the modules copy its pose every frame
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Body->SetVisibility(false);
	}
	for (int32 Index = 0; Index < DismemberModules.Num(); ++Index)
	{
		if (DismemberModules[Index])
		{
			DismemberModules[Index]->SetVisibility(Shown.IsValidIndex(Index) && Shown[Index]);
		}
	}
	return true;
}

void ATikiCreature::BecomeCorpse()
{
	if (bBecameCorpse)
	{
		return;
	}
	bBecameCorpse = true;
	// replaced by its shredding demo: every machine shows its own fragments, no corpse (T-168)
	if (CombatReaction.bShredDemo)
	{
		Destroy();
		return;
	}
	// a frozen corpse copies the held last frame: the creature stays over it a moment, same pose, so no machine sees
	// a frame without a body (T-151); a limp body goes at once (its ragdoll would push the corpse)
	const bool bHeldPose = !GetMesh()->IsSimulatingPhysics();
	if (Rules && GetMesh()->GetSkeletalMeshAsset())
	{
		const FTikiBodySnapshot Body = ATikiCorpse::Snapshot(GetMesh(), HeldItem, CutBones, DismemberSet);
		const FVector Blow = LastBlowDirection.IsNearlyZero() ? GetActorForwardVector() : LastBlowDirection;
		// a sabre reaction took its limb off at the contact: the body stays whole unless the rules split it too; the
		// bisected death always leaves two halves (owner 2026-10-09)
		const bool bReactionKeepsWhole = bLethalReactionStarted && TikiCombat::IsSabre(ReactionClip) && !Rules->bSabreReactionCutsInTwo;
		const bool bBisected = bLethalReactionStarted && ReactionClip == ETikiCombatClip::BisectedDeath;
		if (((bLastBlowCut && !bReactionKeepsWhole) || bBisected) && GetMesh()->GetBoneIndex(Rules->HalfCutBone) != INDEX_NONE)
		{
			// the halves drop where the body stood, nudged apart along the blow (bounded by the corpse, T-155)
			ATikiCorpse::SpawnPart(GetWorld(), Rules, Body, ETikiBodyPart::Upper, Rules->HalfCutBone, Blow * Rules->CutFlingSpeed);
			ATikiCorpse::SpawnPart(GetWorld(), Rules, Body, ETikiBodyPart::Lower, Rules->HalfCutBone, -Blow * Rules->CutFlingSpeed * 0.3f);
			UE_LOG(Logmonke_island, Log, TEXT("%s cut in two by the killing blow"), *GetName());
		}
		// frozen pose (T-151): the last frame of the clip (or of the fall), rigid, its collision the physics asset in
		// that pose, laid on the ground; never the reference pose
		else if (Rules->CorpseSettleMode == ETikiCorpseSettle::FrozenPose
			|| !ATikiRagdollCorpse::Spawn(GetWorld(), Rules, Body, GetMesh()->IsSimulatingPhysics() ? GetMesh() : nullptr))
		{
			ATikiCorpse::SpawnPart(GetWorld(), Rules, Body, ETikiBodyPart::Whole, NAME_None, FVector::ZeroVector);
		}
	}
	if (bHeldPose && Rules && Rules->CorpseSwapTime > 0.0f)
	{
		SetLifeSpan(Rules->CorpseSwapTime);
	}
	else
	{
		Destroy();
	}
}

void ATikiCreature::HandleDeath(AActor* Killer)
{
	ActionEndTime = 0.0;
	bDeathPlayed = true;
	// a creature a client received already dead (relevant to it again) does not cry a second time (T-300)
	if (!Health || !Health->WasDeadOnArrival())
	{
		PlayVoice(Voice.Death);
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// the bean's musket or sabre: a reaction clip plays before the ragdoll (T-150). The server picks it from the
	// blow; a client starts the replicated one if its OnRep has not done it yet
	if (HasAuthority())
	{
		const ETikiCombatClip Reaction = ChooseCombatReaction(true);
		// a close shot, a burst or a sabre cut with a baked demo: the demo replaces the body (T-168)
		const ETikiCombatClip Demo = ChooseShredDemo(Reaction);
		if (Demo != ETikiCombatClip::None)
		{
			StartCombatReaction(Demo, true);
		}
		else if (TikiCombat::IsLethal(Reaction))
		{
			StartCombatReaction(Reaction);
			if (Reaction == ETikiCombatClip::HeadshotDeath)
			{
				UE_LOG(Logmonke_island, Log, TEXT("%s: killed by a shot in the head (%s), plays %s"), *GetName(), *PendingHitBone.ToString(),
					*GetNameSafe(Anims.Combat.HeadshotDeath));
			}
		}
	}
	else if (!bLethalReactionStarted && CombatReaction.Serial != LocalReactionSerial && TikiCombat::IsLethal(CombatReaction.Clip))
	{
		LocalReactionSerial = CombatReaction.Serial;
		PlayLocalCombatReaction(CombatReaction.Clip);
	}

	if (bLethalReactionStarted)
	{
		// the hand lets go at the contact, or at frame 27 of the articulated fall
		const UAnimSequenceBase* Clip = Anims.Combat.Get(CombatReaction.Clip);
		const float DropTime = CombatReaction.Clip == ETikiCombatClip::MusketArticulatedFall
			? (Rules ? Rules->ArticulatedFallDropTime : 0.9f) : (Rules ? Rules->ReactionContactTime : 0.4f);
		const float Delay = DropTime - (Rules && Clip ? FMath::Min(Rules->ReactionStartTime, Clip->GetPlayLength()) : 0.0f);
		if (Delay > 0.0f && !CombatReaction.bShredDemo)
		{
			GetWorldTimerManager().SetTimer(ReactionDropTimer, this, &ATikiCreature::DropWeapon, Delay, false);
		}
		else
		{
			DropWeapon();
		}
	}
	else
	{
		// frozen pose (T-151): the death clip plays to the ground and holds its last frame, then the corpse copies it;
		// else the body goes limp and falls as the blow threw it (without a physics asset, the death clip)
		const bool bDeathClip = Anims.Death && (!Rules || Rules->CorpseSettleMode == ETikiCorpseSettle::FrozenPose);
		if (bDeathClip || (!StartRagdoll() && Anims.Death))
		{
			CurrentClip = nullptr;
			PlayClip(Anims.Death, false);
			// the server copies the last frame even when nobody sees the body
			GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			DeathClipLength = Anims.Death->GetPlayLength();
		}
		DropWeapon();
	}

	if (HasAuthority())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s killed by %s"), *GetName(), *GetNameSafe(Killer));
		// after the reaction clip, or the death clip played to its last frame, or the limp fall: a corpse one can carry
		const float ReactionLeft = bLethalReactionStarted ? FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(ReactionRagdollTimer)) : 0.0f;
		const float Delay = DeathClipLength > 0.0f && !GetMesh()->IsSimulatingPhysics()
			? DeathClipLength + (Rules ? Rules->CorpseFreezeDelay : 0.1f)
			: ReactionLeft + (Rules ? Rules->CorpseDelay : 1.5f);
		GetWorldTimerManager().SetTimer(CorpseTimer, this, &ATikiCreature::BecomeCorpse, FMath::Max(0.01f, Delay), false);
		OnKilled(Killer);
	}
}

void ATikiCreature::DropWeapon()
{
	USkeletalMesh* WeaponMesh = HeldItem ? HeldItem->GetSkeletalMeshAsset() : nullptr;
	// nothing to drop, or the weapon left with an arm cut off before (its hand is hidden)
	if (!DroppedWeaponClass || !WeaponMesh || !HeldItem->IsVisible())
	{
		return;
	}
	// the grip, and the long axis of the weapon from its geometry at rest (grip toward its middle): the static pickup
	// leaves the hand as it was held, whatever the axes of the bone after the import
	FTransform Grip = HeldItem->GetComponentTransform();
	FVector Middle = FVector::ZeroVector;
	const int32 Bone = WeaponMesh->GetRefSkeleton().FindBoneIndex(DroppedWeaponBone);
	if (Bone != INDEX_NONE)
	{
		Grip = HeldItem->GetSocketTransform(DroppedWeaponBone, RTS_World);
		const FTransform RestBone = FAnimationRuntime::GetComponentSpaceTransformRefPose(WeaponMesh->GetRefSkeleton(), Bone);
		Middle = RestBone.InverseTransformPosition(WeaponMesh->GetImportedBounds().Origin);
	}
	// every machine: the hand lets go (a hidden item stays out of the corpse, ATikiCorpse::Snapshot)
	HeldItem->SetVisibility(false);
	if (!HasAuthority())
	{
		return;
	}
	const FTransform SpawnTransform = DroppedWeaponOffset * TikiMath::WeaponDropTransform(Grip, Middle, GetMesh()->GetComponentScale());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Weapon = GetWorld()->SpawnActor<AActor>(DroppedWeaponClass, SpawnTransform, Params);
	if (!Weapon)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: its weapon %s could not be spawned"), *GetName(), *GetNameSafe(DroppedWeaponClass.Get()));
		return;
	}
	// it flies out of the hand along the killing blow, a little upward
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Weapon->GetRootComponent()); Body && Body->IsSimulatingPhysics())
	{
		const FVector Away = LastBlowDirection.IsNearlyZero() ? GetActorForwardVector() : LastBlowDirection;
		Body->SetPhysicsLinearVelocity(Away * DroppedWeaponThrowSpeed + FVector(0.0f, 0.0f, DroppedWeaponThrowSpeed * 0.5f));
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s drops %s"), *GetName(), *Weapon->GetName());
}

bool ATikiCreature::StartRagdoll()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body->GetPhysicsAsset())
	{
		return false;
	}
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	// calm: damped, joints held together, pushed slowly out of the ground (T-151)
	ATikiRagdollCorpse::CalmRagdoll(Body, Rules);
	// the server knows the blow; the clients' bodies only fall (the corpse carries the server pose). After a lethal
	// reaction clip the body lies on the ground already: no push
	if (Rules && !LastBlowDirection.IsNearlyZero() && !bLethalReactionStarted)
	{
		const FVector Push = LastBlowDirection * Rules->DeathImpulse + FVector(0.0f, 0.0f, Rules->DeathImpulse * 0.3f);
		Body->SetAllPhysicsLinearVelocity(TikiMath::BoundedVelocity(Push, Rules->CorpseMaxSpeed), true);
	}
	return true;
}

APawn* ATikiCreature::PawnOfCauser(AActor* Causer)
{
	if (!Causer)
	{
		return nullptr;
	}
	if (APawn* Pawn = Cast<APawn>(Causer))
	{
		return Pawn;
	}
	// a musket ball or a cutlass: the carrier is its instigator or its owner
	if (APawn* Pawn = Causer->GetInstigator())
	{
		return Pawn;
	}
	return Cast<APawn>(Causer->GetOwner());
}

bool ATikiCreature::IsLivingPlayer(const APawn* Pawn)
{
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return false;
	}
	const UIslanderHealthComponent* PawnHealth = Pawn->FindComponentByClass<UIslanderHealthComponent>();
	return !PawnHealth || !PawnHealth->IsDead();
}

APawn* ATikiCreature::FindNearestPlayer(float MaxDistance, bool bMustSee) const
{
	APawn* Best = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		if (!IsLivingPlayer(Pawn))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation());
		if (DistSq <= BestDistSq && (!bMustSee || CanSpot(Pawn, MaxDistance)))
		{
			BestDistSq = DistSq;
			Best = Pawn;
		}
	}
	return Best;
}

bool ATikiCreature::CanSpot(const AActor* Target, float Range) const
{
	const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
	if (!Stealth)
	{
		// a world without the stealth (an editor preview): seen in range, as before T-262
		return IsValid(Target) && (Range <= 0.0f || FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) <= FMath::Square(static_cast<double>(Range)));
	}
	return Stealth->CanSee(this, Target, Range);
}

bool ATikiCreature::Listen(float HearingScale, FStealthNoise& OutNoise)
{
	const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Stealth || !HasAuthority() || Now < NextListenTime)
	{
		return false;
	}
	NextListenTime = Now + Stealth->GetRules().ListenIntervalSeconds;
	return Stealth->FindHeardNoise(this, HearingScale, OutNoise);
}

APawn* ATikiCreature::FindNearestFoe(float MaxDistance, bool bMustSee) const
{
	UHostilitySubsystem* Registry = UHostilitySubsystem::Get(this);
	if (!Registry)
	{
		return nullptr;
	}
	const EFaction Mine = Hostility::FactionOf(this);
	ACharacter* Best = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
	for (const FHostileCombatant& Other : Registry->GetCombatants())
	{
		ACharacter* Character = Other.Character.Get();
		if (!Character || Character == this || !Other.IsLivingFoeOf(Mine))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Character->GetActorLocation(), GetActorLocation());
		if (DistSq <= BestDistSq && (!bMustSee || CanSpot(Character, MaxDistance)))
		{
			BestDistSq = DistSq;
			Best = Character;
		}
	}
	return Best;
}

float ATikiCreature::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	}
	// the health takes the blow first and may kill (HandleDeath): what dealt it is recorded before
	const APawn* Attacker = EventInstigator && EventInstigator->GetPawn() ? EventInstigator->GetPawn().Get() : PawnOfCauser(DamageCauser);
	const FRotator Aim = Attacker ? Attacker->GetBaseAimRotation() : (DamageCauser ? DamageCauser->GetActorRotation() : FRotator::ZeroRotator);
	const AActor* From = DamageCauser ? DamageCauser : Attacker;
	PendingBlow = TikiCombat::FBlow();
	BlowCuts = 0;
	BlowShredLimbs = 0;
	PendingBlow.Damage = DamageAmount;
	PendingBlow.Roll = FMath::FRand();
	PendingBlow.bInReaction = IsCombatReactionPlaying();
	PendingBlow.VictimRight = GetActorRightVector();
	PendingBlow.AimPitch = FRotator::NormalizeAxis(Aim.Pitch);
	PendingBlowDirection = From ? (GetActorLocation() - From->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	// the bone the blow struck: the capsule takes the traces, the bone is the one the line passes nearest to (T-167);
	// a shot of pellets names its bones
	PendingHitBone = NAME_None;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FFirearmDamageEvent* Pellets = DamageEvent.IsOfType(FFirearmDamageEvent::ClassID) ? static_cast<const FFirearmDamageEvent*>(&DamageEvent) : nullptr;
		PendingHitBone = Pellets && !Pellets->BoneHits.IsEmpty() ? Pellets->BoneHits[0].Bone
			: ResolveHitBone(static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo);
	}
	const uint8 HitZone = HitZones::ZoneOfBone(PendingHitBone);
	PendingBlow.bHeadHit = HitZone == HitZones::Head;
	PendingBlow.bHeadTorn = false;
	// a leg torn off before this blow: it crawls, and dies crawling (owner 2026-10-09)
	PendingBlow.bCrawling = IsCrawling();
	bPendingBlowCut = bNextBlowCut || IsCutBlow(DamageCauser);
	const AShotgunWeapon* Shotgun = Cast<AShotgunWeapon>(DamageCauser);
	// every shot of a firearm takes the musket reactions: the bolt rifle, the submachine gun and the machine gun too (T-335);
	// a thrust of the bayonet musket is a pointed blade, not a ball: it bleeds, without the shot reactions (no head torn off)
	const bool bFirearmShot = DamageEvent.IsOfType(FFirearmDamageEvent::ClassID);
	if (!bFirearmShot && Cast<ABayonetMusketWeapon>(DamageCauser))
	{
		PendingBlow.Weapon = TikiCombat::EWeapon::Blunt;
	}
	else if (bFirearmShot || Cast<AMusketWeapon>(DamageCauser) || Cast<APirateMusketWeapon>(DamageCauser) || Cast<APistolWeapon>(DamageCauser) || Shotgun)
	{
		// the pirate pistol and shotgun (T-156) take the musket reactions; the shotgun bursts at close range
		PendingBlow.Weapon = Shotgun ? TikiCombat::EWeapon::Shotgun
			: (Cast<APistolWeapon>(DamageCauser) ? TikiCombat::EWeapon::Pistol : TikiCombat::EWeapon::Musket);
		PendingBlow.CloseBurstDistance = Shotgun ? Shotgun->GetCloseBurstDistance() : 0.0f;
		if (DamageEvent.IsOfType(FFirearmDamageEvent::ClassID))
		{
			// the shot carries its own close range and how many limbs it shreds there (T-167)
			const FFirearmDamageEvent& Shot = static_cast<const FFirearmDamageEvent&>(DamageEvent);
			PendingBlow.CloseBurstDistance = Shot.CloseBurstDistance > 0.0f ? Shot.CloseBurstDistance : PendingBlow.CloseBurstDistance;
			BlowShredLimbs = Shot.ShredLimbs;
		}
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			PendingBlow.Distance = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.Distance;
		}
	}
	else if (const ACutlassWeapon* Cutlass = Cast<ACutlassWeapon>(DamageCauser))
	{
		// the server plays the move it applies the hit of; a cut from the right sweeps toward the attacker's left. A mob's
		// spear or club picked up (T-161) has no edge: a plain blow, no sabre reaction nor demo
		PendingBlow.Weapon = TikiCombat::MeleeWeapon(!Cutlass->Rules || Cutlass->Rules->bEdged);
		const FVector AttackerRight = FRotationMatrix(FRotator(0.0f, Aim.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
		switch (Cutlass->GetLocalMove())
		{
		case ECutlassMove::SlashRight:
			PendingBlow.Stroke = TikiCombat::EStroke::Horizontal;
			PendingBlow.BladeSweep = -AttackerRight;
			break;
		case ECutlassMove::SlashLeft:
			PendingBlow.Stroke = TikiCombat::EStroke::Horizontal;
			PendingBlow.BladeSweep = AttackerRight;
			break;
		case ECutlassMove::Finisher:
			PendingBlow.Stroke = TikiCombat::EStroke::Vertical;
			break;
		default:
			break;
		}
	}
	// the torso struck: a blade may cut the body in two (owner 2026-10-09); a blade whose bone is unknown struck the body
	PendingBlow.bTorsoHit = HitZone == HitZones::Torso || (PendingBlow.Weapon == TikiCombat::EWeapon::Sabre && HitZone == HitZones::None);
	// the musket ball or the blade enters the body where it struck: a spray on every machine (T-153), lethal or not
	if (DamageAmount > 0.0f && !IsDead() && Rules && Rules->bBloodEnabled && PendingBlow.Weapon != TikiCombat::EWeapon::Other
		&& DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		const FVector Direction = Point.ShotDirection.GetSafeNormal(UE_SMALL_NUMBER, PendingBlowDirection);
		const FFirearmDamageEvent* Shot = DamageEvent.IsOfType(FFirearmDamageEvent::ClassID) ? static_cast<const FFirearmDamageEvent*>(&DamageEvent) : nullptr;
		if (Shot && !Shot->Impacts.IsEmpty())
		{
			// a spray where each pellet entered (T-167)
			TArray<FVector_NetQuantize> Impacts;
			Impacts.Reserve(Shot->Impacts.Num());
			for (const FVector& Impact : Shot->Impacts)
			{
				Impacts.Add(Impact);
			}
			MulticastBloodHits(Impacts, Direction);
		}
		else
		{
			const FVector Impact = Point.HitInfo.bBlockingHit || !Point.HitInfo.ImpactPoint.IsNearlyZero() ? FVector(Point.HitInfo.ImpactPoint) : GetActorLocation();
			MulticastBloodHit(Impact, Direction);
		}
	}
	bHasPendingBlow = true;
	// a blow that kills with a shredding demo (T-168): the demo shows every limb leaving, the hits tear none first
	bPendingShredDemo = Health && !IsDead() && DamageAmount >= Health->GetHealth()
		&& ChooseShredDemo(ChooseCombatReaction(true)) != ETikiCombatClip::None;
	// gun shots fill the health of the limbs they struck and tear off those past it, living or not (T-167)
	float Damage = DamageAmount;
	if (TikiCombat::IsFirearm(PendingBlow.Weapon) && DamageAmount > 0.0f)
	{
		if (!bPendingShredDemo)
		{
			ApplyLimbHits(DamageEvent, Damage);
		}
		else if (!PendingBlowDirection.IsNearlyZero())
		{
			LastBlowDirection = PendingBlowDirection;
		}
		PendingBlow.Damage = Damage;
	}
	const bool bWasDead = IsDead();
	const float Dealt = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	// a close shotgun shot that kills shreds the body: limbs torn off up to its count, the hits' ones included
	if (!bWasDead && IsDead() && BlowShredLimbs > 0 && !(bLethalReactionStarted && CombatReaction.bShredDemo))
	{
		const FVector Direction = LastBlowDirection.IsNearlyZero() ? GetActorRightVector() : LastBlowDirection;
		for (int32 Left = TikiLimbs::ShredLeft(BlowShredLimbs, BlowCuts); Left > 0; --Left)
		{
			const int32 Limb = PickShredLimb();
			if (Limb == INDEX_NONE || !TearLimb(Limb, Direction))
			{
				break;
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s shredded by a close shot: %d limbs off"), *GetName(), BlowCuts);
	}
	bHasPendingBlow = false;
	bPendingShredDemo = false;
	return Dealt;
}

bool ATikiCreature::IsCombatReactionPlaying() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < ReactionEndTime;
}

ETikiCombatClip ATikiCreature::ChooseCombatReaction(bool bLethal) const
{
	if (!bHasPendingBlow || !Rules || !Rules->bCombatReactions)
	{
		return ETikiCombatClip::None;
	}
	TikiCombat::FBlow Blow = PendingBlow;
	Blow.bLethal = bLethal;
	TikiCombat::FTuning Tuning;
	Tuning.BurstDamage = Rules->MusketBurstDamage;
	Tuning.BurstDistance = Rules->MusketBurstDistance;
	Tuning.BurstChance = Rules->MusketBurstChance;
	Tuning.LowAimPitch = Rules->SabreLowAimPitch;
	Tuning.bMirrorSides = Rules->bMirrorSabreSides;
	Tuning.BisectChance = Rules->SabreBisectChance;
	return TikiCombat::ChooseClip(Blow, Tuning, Anims.Combat.AvailableMask());
}

ETikiCombatClip ATikiCreature::ChooseShredDemo(ETikiCombatClip Reaction) const
{
	if (!bHasPendingBlow || !Rules || !ShredDemoSet)
	{
		return ETikiCombatClip::None;
	}
	TikiShredDemo::FChoice Choice;
	Choice.bEnabled = Rules->bShredDemos;
	Choice.Clip = Reaction;
	Choice.DemoMask = ShredDemoSet->AvailableMask();
	Choice.bBlowCuts = bPendingBlowCut;
	Choice.bSabreNeedsCut = Rules->bShredDemoSabreNeedsCut;
	Choice.CutsBefore = CutBones.Num();
	Choice.bFirearm = TikiCombat::IsFirearm(PendingBlow.Weapon);
	Choice.Distance = PendingBlow.Distance;
	Choice.CloseDistance = PendingBlow.CloseBurstDistance;
	Choice.bCloseShotBurstDemo = Rules->bCloseShotBurstDemo;
	return TikiShredDemo::Choose(Choice);
}

bool ATikiCreature::StartLocalShredDemo()
{
	const FTikiShredDemoClip* Demo = ShredDemoSet ? ShredDemoSet->Find(CombatReaction.Clip) : nullptr;
	if (!Demo || !Rules)
	{
		return false;
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		// a client receives the reaction late: the demo starts that much further, its blood only while fresh
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const float Age = HasAuthority() || !GameState ? 0.0f
			: FMath::Max(0.0f, static_cast<float>(GameState->GetServerWorldTimeSeconds() - CombatReaction.StartServerTime));
		// the weapon in hand falls as a pickup at this death (DropWeapon, T-161): the demo leaves its own copy out
		const bool bDropsWeapon = DroppedWeaponClass && HeldItem->GetSkeletalMeshAsset();
		const FName HiddenBone = TikiShredDemo::HiddenFragment(ShredDemoSet->WeaponFragmentBone, bDropsWeapon);
		// the mesh origin is at the feet (the Blender exports), like the demo's
		ATikiShredDemo* Spawned = ATikiShredDemo::Spawn(GetWorld(), Rules, *Demo, GetMesh()->GetComponentLocation(),
			FRotator::DecompressAxisFromShort(CombatReaction.DemoYaw), GetMesh()->GetComponentScale(), Age, this, Age <= Rules->BloodFreshWindow,
			HiddenBone);
		if (!Spawned)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s: shred demo %s could not spawn, the reaction plays instead"), *GetName(),
				*UEnum::GetValueAsString(CombatReaction.Clip));
			return false;
		}
		ShredDemo = Spawned;
	}
	// the body, its held item, its modules and its health bar vanish: the demo is the body now
	SetActorHiddenInGame(true);
	return true;
}

void ATikiCreature::StartCombatReaction(ETikiCombatClip Clip, bool bShredDemo)
{
	if (!HasAuthority() || (!bShredDemo && !Anims.Combat.Get(Clip)))
	{
		return;
	}
	const bool bLethal = TikiCombat::IsLethal(Clip);
	if (bLethal && !PendingBlowDirection.IsNearlyZero())
	{
		LastBlowDirection = PendingBlowDirection;
	}
	// the demo's victim faces where the blow came from (the pack turns it 180 degrees toward the bean, T-168)
	CombatReaction.bShredDemo = bShredDemo && bLethal;
	CombatReaction.DemoYaw = FRotator::CompressAxisToShort(TikiShredDemo::FacingYaw(LastBlowDirection, GetActorRotation().Yaw));
	CombatReaction.Clip = Clip;
	CombatReaction.Serial = CombatReaction.Serial == MAX_uint8 ? 1 : CombatReaction.Serial + 1;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	CombatReaction.StartServerTime = static_cast<float>(GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds());
	LocalReactionSerial = CombatReaction.Serial;
	ForceNetUpdate();
	PlayLocalCombatReaction(Clip);
	UE_LOG(Logmonke_island, Log, TEXT("%s reacts: %s%s"), *GetName(), *UEnum::GetValueAsString(Clip),
		CombatReaction.bShredDemo ? TEXT(" (shred demo)") : TEXT(""));

	if (!TikiCombat::IsLethal(Clip))
	{
		// survived: its decisions wait for the clip (or the time of the rules), then it goes on
		ThinkSuspendedUntil = Rules && Rules->ReactionAISuspendTime > 0.0f ? GetWorld()->GetTimeSeconds() + Rules->ReactionAISuspendTime : ReactionEndTime;
		return;
	}
	// a burst, or a cut that cuts, takes its limb off at the contact of the clip
	ReactionClip = Clip;
	bReactionCuts = bPendingBlowCut;
	bReactionShredded = BlowShredLimbs > 0;
	// the demo shows its limbs leaving: no corpse piece, no stump
	if (CombatReaction.bShredDemo)
	{
		return;
	}
	const float ContactDelay = Rules ? Rules->ReactionContactTime - Rules->ReactionStartTime : 0.0f;
	if (ContactDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ReactionCutTimer, this, &ATikiCreature::CutReactionLimb, ContactDelay, false);
	}
	else
	{
		CutReactionLimb();
	}
}

void ATikiCreature::PlayLocalCombatReaction(ETikiCombatClip Clip)
{
	// replaced by its baked demo (T-168): no clip, no ragdoll; the server removes the mob without a corpse
	if (TikiCombat::IsLethal(Clip) && CombatReaction.bShredDemo && CombatReaction.Clip == Clip && StartLocalShredDemo())
	{
		ReactionEndTime = GetWorld()->GetTimeSeconds();
		ActionEndTime = ReactionEndTime;
		bLethalReactionStarted = true;
		bDeathPlayed = true;
		return;
	}
	UAnimSequenceBase* Anim = Anims.Combat.Get(Clip);
	if (!Anim)
	{
		return;
	}
	// from the contact: the blow has landed already
	const float Length = Anim->GetPlayLength();
	const float Start = Rules ? FMath::Clamp(Rules->ReactionStartTime, 0.0f, Length) : 0.0f;
	ReactionEndTime = GetWorld()->GetTimeSeconds() + (Length - Start);
	ActionEndTime = ReactionEndTime;
	CurrentClip = nullptr;
	PlayClip(Anim, false, Start);
	if (!TikiCombat::IsLethal(Clip))
	{
		PlayVoice(Voice.Hurt);
		return;
	}
	// dying: the clip brings the body to the ground, then a frozen pose or a calm ragdoll (T-151)
	bLethalReactionStarted = true;
	bDeathPlayed = true;
	if (HasAuthority())
	{
		// the corpse copies the last pose of the clip, even when the host does not see the body
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	// cut in two: the waist bleeds at the contact on every machine (the halves come with the corpse, owner 2026-10-09)
	if (Clip == ETikiCombatClip::BisectedDeath && Rules)
	{
		const float BloodDelay = Rules->ReactionContactTime - Start;
		if (BloodDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(ReactionBloodTimer, this, &ATikiCreature::SpawnWaistBlood, BloodDelay, false);
		}
		else
		{
			SpawnWaistBlood();
		}
	}
	const float EndTime = Rules && Rules->ReactionRagdollTime > 0.0f ? FMath::Min(Rules->ReactionRagdollTime, Length) : Length;
	GetWorldTimerManager().SetTimer(ReactionRagdollTimer, this, &ATikiCreature::FinishLethalReaction, FMath::Max(0.01f, EndTime - Start), false);
}

void ATikiCreature::FinishLethalReaction()
{
	if (Rules && Rules->CorpseSettleMode == ETikiCorpseSettle::FrozenPose)
	{
		// the last frame of the clip stays (the animation holds it); the server hands it to the corpse once the
		// animation has reached it, laid on the ground
		if (HasAuthority())
		{
			GetWorldTimerManager().SetTimer(CorpseTimer, this, &ATikiCreature::BecomeCorpse, FMath::Max(0.01f, Rules->CorpseFreezeDelay), false);
		}
		return;
	}
	// a calm ragdoll; without a physics asset the clip holds its last frame until the corpse replaces the body
	StartRagdoll();
}

void ATikiCreature::CutReactionLimb()
{
	if (!HasAuthority() || !Rules)
	{
		return;
	}
	const FVector Direction = LastBlowDirection.IsNearlyZero() ? GetActorRightVector() : LastBlowDirection;
	switch (ReactionClip)
	{
	case ETikiCombatClip::MusketBurst:
		// a close shotgun shot shreds the body itself (TakeDamage): the burst takes nothing more
		for (int32 Index = 0; !bReactionShredded && Index < Rules->MusketBurstLimbCount; ++Index)
		{
			const FName Limb = PickLimbToCut();
			if (Limb.IsNone() || !DetachLimb(Limb, Direction))
			{
				break;
			}
		}
		break;
	case ETikiCombatClip::SabreLeftToRight:
	case ETikiCombatClip::SabreRightToLeft:
	case ETikiCombatClip::SabreDown:
	case ETikiCombatClip::SabreLow:
		if (bReactionCuts)
		{
			const FName Bone = ReactionClip == ETikiCombatClip::SabreLeftToRight ? Rules->SabreLeftToRightBone
				: ReactionClip == ETikiCombatClip::SabreRightToLeft ? Rules->SabreRightToLeftBone
				: ReactionClip == ETikiCombatClip::SabreDown ? Rules->SabreDownBone : Rules->SabreLowBone;
			DetachLimb(Bone, Direction);
		}
		break;
	default:
		break;
	}
}

void ATikiCreature::OnRep_CombatReaction()
{
	if (CombatReaction.Serial == LocalReactionSerial)
	{
		return;
	}
	LocalReactionSerial = CombatReaction.Serial;
	// dead already (its ragdoll or its reaction is on), or a reaction long over for a player joining late
	const UAnimSequenceBase* Anim = Anims.Combat.Get(CombatReaction.Clip);
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Age = GameState ? GameState->GetServerWorldTimeSeconds() - CombatReaction.StartServerTime : 0.0;
	// a demo is still shown while its fragments lie on the ground, even over a body already dead here (T-168)
	const bool bDemo = CombatReaction.bShredDemo && !bLethalReactionStarted;
	const double Window = bDemo ? (Rules ? Rules->ShredDemoHoldTime : 0.0f) + 3.0 : (Anim ? Anim->GetPlayLength() : 0.0f);
	if ((bDeathPlayed && !bDemo) || (!Anim && !bDemo) || Age > Window + ReactionStaleMargin)
	{
		return;
	}
	PlayLocalCombatReaction(CombatReaction.Clip);
}
