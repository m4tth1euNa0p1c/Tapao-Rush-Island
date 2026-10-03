// monke_island

#include "Creatures/TikiCreature.h"
#include "Creatures/TikiAnimInstance.h"
#include "Creatures/TikiCorpse.h"
#include "Creatures/TikiRagdollCorpse.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Creatures/MobHealthBarComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Water/IslandWaterSubsystem.h"
#include "Weapons/CutlassWeapon.h"
#include "monke_island.h"
#include "AIController.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	Super::BeginPlay();

	HeldItem->SetLeaderPoseComponent(GetMesh());
	Health->OnDeath.AddDynamic(this, &ATikiCreature::HandleDeath);

	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ATikiCreature::HandleTakeAnyDamage);
		Health->MaxHealth = GetRulesHealth();
		Health->ResetHealth();
	}
	// eased steps and turns: a body gets up to speed and turns, it does not snap (every machine smooths the same)
	if (Rules)
	{
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->MaxAcceleration = Rules->CreatureAcceleration;
		Move->BrakingDecelerationWalking = Rules->CreatureBraking;
		Move->RotationRate = FRotator(0.0f, Rules->CreatureTurnRate, 0.0f);
	}
	StuckCheckLocation = GetActorLocation();
	StuckCheckTime = GetWorld()->GetTimeSeconds();
	NextIdleVoiceTime = StuckCheckTime + FMath::FRandRange(Voice.IdleIntervalMin, FMath::Max(Voice.IdleIntervalMin, Voice.IdleIntervalMax));
	UpdateAnimation();
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

	if (HasAuthority() && !IsDead() && Rules)
	{
		ServerThink(DeltaSeconds);
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
	if (Sounds.IsEmpty() || GetNetMode() == NM_DedicatedServer)
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
		GetCharacterMovement()->MaxWalkSpeed = Speed;
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
	MulticastPlayAction(Action, Variant);
}

void ATikiCreature::MulticastPlayAction_Implementation(ETikiAction Action, uint8 Variant)
{
	PlayLocalAction(Action, Variant);
}

bool ATikiCreature::IsActionPlaying() const
{
	return GetWorld()->GetTimeSeconds() < ActionEndTime;
}

void ATikiCreature::PlayLocalAction(ETikiAction Action, uint8 Variant)
{
	if (bDeathPlayed)
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
		// variant 0 is the base blow, the others the extra ones of the library
		Clip = Variant > 0 && Anims.AttackVariants.IsValidIndex(Variant - 1) && Anims.AttackVariants[Variant - 1] ? Anims.AttackVariants[Variant - 1].Get() : Anims.Attack.Get();
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
	if (Duration <= 0.0f)
	{
		Duration = Clip->GetPlayLength();
	}
	ActionEndTime = GetWorld()->GetTimeSeconds() + Duration;
	CurrentClip = nullptr;
	PlayClip(Clip, bLoop);
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

void ATikiCreature::PlayClip(UAnimSequenceBase* Clip, bool bLoop)
{
	if (!Clip || Clip == CurrentClip)
	{
		return;
	}
	CurrentClip = Clip;
	if (UTikiAnimInstance* Anim = GetTikiAnim())
	{
		// locomotion crossfades slowly, a blow or a hit cuts in fast and always restarts
		const bool bLocomotion = Clip == Anims.Idle || Clip == Anims.IdleCombat || Clip == Anims.Walk || Clip == Anims.Run;
		const float Blend = bLocomotion ? (Rules ? Rules->LocomotionBlendTime : 0.25f) : (Rules ? Rules->ActionBlendTime : 0.12f);
		Anim->PlayClip(Clip, bLoop, Blend, 1.0f, !bLocomotion);
	}
	else
	{
		GetMesh()->PlayAnimation(Clip, bLoop);
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
	// a cut or a strong blow staggers, a blow in the back throws forward, the others are a plain hit
	ETikiAction HitAction = ETikiAction::Hit;
	if (Anims.Stagger && Rules && (bCut || Damage >= Rules->StaggerDamage))
	{
		HitAction = ETikiAction::Stagger;
	}
	else if (Anims.HitBack && TikiMath::IsBlowFromBehind(LastBlowDirection, GetActorForwardVector()))
	{
		HitAction = ETikiAction::HitBack;
	}
	PlayAction(HitAction);
	OnDamaged(Causer, Damage);

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
	DOREPLIFETIME_CONDITION(ATikiCreature, Rules, COND_InitialOnly);
}

bool ATikiCreature::IsCutBlow(const AActor* DamageCauser)
{
	// the server plays the move it applies the hit of: GetLocalMove is the striking move there
	const ACutlassWeapon* Cutlass = Cast<ACutlassWeapon>(DamageCauser);
	const FCutlassAttack* Attack = Cutlass && Cutlass->Rules ? Cutlass->Rules->GetAttack(Cutlass->GetLocalMove()) : nullptr;
	return Attack && Attack->bCut;
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
	if (!HasAuthority() || IsDead() || CutBones.Contains(Bone) || GetMesh()->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return false;
	}
	// the piece takes the pose of the arm right now, then the arm vanishes from the body on every machine
	if (Rules)
	{
		const FVector Fling = Direction.IsNearlyZero() ? GetActorRightVector() : Direction;
		ATikiCorpse::SpawnPart(GetWorld(), Rules, ATikiCorpse::Snapshot(GetMesh(), HeldItem, CutBones), ETikiBodyPart::Limb, Bone,
			Fling * Rules->CutFlingSpeed + FVector(0.0f, 0.0f, Rules->CutFlingSpeed));
	}
	CutBones.Add(Bone);
	ApplyCutBones();
	UE_LOG(Logmonke_island, Log, TEXT("%s loses %s"), *GetName(), *Bone.ToString());
	return true;
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
	ApplyCutBones();
}

void ATikiCreature::ApplyCutBones()
{
	static const FName HeldBones[] = { TEXT("weapon_r"), TEXT("prop_l") };
	bool bHandGone = false;
	for (const FName& Cut : CutBones)
	{
		GetMesh()->HideBoneByName(Cut, PBO_None);
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

void ATikiCreature::BecomeCorpse()
{
	if (Rules && GetMesh()->GetSkeletalMeshAsset())
	{
		const FTikiBodySnapshot Body = ATikiCorpse::Snapshot(GetMesh(), HeldItem, CutBones);
		const FVector Blow = LastBlowDirection.IsNearlyZero() ? GetActorForwardVector() : LastBlowDirection;
		if (bLastBlowCut && GetMesh()->GetBoneIndex(Rules->HalfCutBone) != INDEX_NONE)
		{
			ATikiCorpse::SpawnPart(GetWorld(), Rules, Body, ETikiBodyPart::Upper, Rules->HalfCutBone,
				Blow * Rules->CutFlingSpeed + FVector(0.0f, 0.0f, Rules->CutFlingSpeed * 0.5f));
			ATikiCorpse::SpawnPart(GetWorld(), Rules, Body, ETikiBodyPart::Lower, Rules->HalfCutBone, -Blow * Rules->CutFlingSpeed * 0.3f);
			UE_LOG(Logmonke_island, Log, TEXT("%s cut in two by the killing blow"), *GetName());
		}
		// the ragdoll goes on in the corpse, bone by bone; without a physics asset, a rigid corpse
		else if (!ATikiRagdollCorpse::Spawn(GetWorld(), Rules, Body, GetMesh()->IsSimulatingPhysics() ? GetMesh() : nullptr))
		{
			ATikiCorpse::SpawnPart(GetWorld(), Rules, Body, ETikiBodyPart::Whole, NAME_None, FVector::ZeroVector);
		}
	}
	Destroy();
}

void ATikiCreature::HandleDeath(AActor* Killer)
{
	ActionEndTime = 0.0;
	bDeathPlayed = true;
	PlayVoice(Voice.Death);

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// the body goes limp and falls as the blow threw it; without a physics asset, the death clip
	if (!StartRagdoll() && Anims.Death)
	{
		CurrentClip = nullptr;
		PlayClip(Anims.Death, false);
	}
	DropWeapon();

	if (HasAuthority())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s killed by %s"), *GetName(), *GetNameSafe(Killer));
		// the body falls limp, then becomes a corpse one can carry (ATikiRagdollCorpse)
		GetWorldTimerManager().SetTimer(CorpseTimer, this, &ATikiCreature::BecomeCorpse, Rules ? Rules->CorpseDelay : 1.5f, false);
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
	// the server knows the blow; the clients' bodies only fall (the corpse carries the server pose)
	if (Rules && !LastBlowDirection.IsNearlyZero())
	{
		Body->SetAllPhysicsLinearVelocity(LastBlowDirection * Rules->DeathImpulse + FVector(0.0f, 0.0f, Rules->DeathImpulse * 0.3f), true);
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

APawn* ATikiCreature::FindNearestPlayer(float MaxDistance) const
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
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Pawn;
		}
	}
	return Best;
}
