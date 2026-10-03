// monke_island

#include "Character/Bean/BeanPuppetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"

namespace BeanPuppet
{
	/** Component-space reference pose of a skeleton */
	static void BuildRefCS(const FReferenceSkeleton& RefSkeleton, TArray<FTransform>& OutCS)
	{
		const TArray<FTransform>& Local = RefSkeleton.GetRefBonePose();
		OutCS.SetNum(Local.Num());
		for (int32 Index = 0; Index < Local.Num(); ++Index)
		{
			const int32 Parent = RefSkeleton.GetParentIndex(Index);
			OutCS[Index] = Parent == INDEX_NONE ? Local[Index] : Local[Index] * OutCS[Parent];
		}
	}

	static FBeanBoneMapping Map(const TCHAR* Source, const TCHAR* Target, const TCHAR* SourceChild = nullptr,
		const TCHAR* TargetChild = nullptr, float Spread = 0.f, float RotationScale = 1.f)
	{
		FBeanBoneMapping Mapping;
		Mapping.SourceBone = Source;
		Mapping.TargetBone = Target;
		Mapping.SourceChild = SourceChild ? FName(SourceChild) : NAME_None;
		Mapping.TargetChild = TargetChild ? FName(TargetChild) : NAME_None;
		Mapping.SpreadDegrees = Spread;
		Mapping.RotationScale = RotationScale;
		return Mapping;
	}
}

UBeanPuppetComponent::UBeanPuppetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// after the source animation (and its ragdoll) has been evaluated
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);

	using BeanPuppet::Map;
	BoneMappings = {
		// the bean trunk is one stiff body: only part of the mannequin's bending is kept, the head stays level
		Map(TEXT("pelvis"), TEXT("pelvis"), nullptr, nullptr, 0.f, 0.5f),
		Map(TEXT("spine_01"), TEXT("spine_01"), nullptr, nullptr, 0.f, 0.45f),
		Map(TEXT("spine_03"), TEXT("spine_02"), nullptr, nullptr, 0.f, 0.4f),
		Map(TEXT("spine_05"), TEXT("head"), nullptr, nullptr, 0.f, 0.3f),
		Map(TEXT("upperarm_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("lowerarm_l"), 25.f),
		Map(TEXT("lowerarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("hand_l"), 25.f),
		Map(TEXT("hand_l"), TEXT("hand_l")),
		Map(TEXT("upperarm_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("lowerarm_r"), 25.f),
		Map(TEXT("lowerarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("hand_r"), 25.f),
		Map(TEXT("hand_r"), TEXT("hand_r")),
		Map(TEXT("thigh_l"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("calf_l")),
		Map(TEXT("calf_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("foot_l")),
		Map(TEXT("foot_l"), TEXT("foot_l")),
		Map(TEXT("thigh_r"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("calf_r")),
		Map(TEXT("calf_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("foot_r")),
		Map(TEXT("foot_r"), TEXT("foot_r")),
	};
}

void UBeanPuppetComponent::BeginPlay()
{
	Super::BeginPlay();

	SpawnSkinParts();
	bReady = InitPuppet();
	if (!bReady)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("BeanPuppet on %s: no usable source mesh or skeleton, the bean stays in its rest pose"),
			*GetNameSafe(GetOwner()));
	}
}

void UBeanPuppetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SourceMesh && bHideSourceMesh)
	{
		SourceMesh->SetHiddenInGame(false);
	}
	Super::EndPlay(EndPlayReason);
}

bool UBeanPuppetComponent::InitPuppet()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	SourceMesh = Character ? Character->GetMesh() : nullptr;
	if (!SourceMesh || !SourceMesh->GetSkinnedAsset() || !GetSkinnedAsset())
	{
		return false;
	}

	const FReferenceSkeleton& SourceRef = SourceMesh->GetSkinnedAsset()->GetRefSkeleton();
	const FReferenceSkeleton& TargetRef = GetSkinnedAsset()->GetRefSkeleton();
	BeanPuppet::BuildRefCS(SourceRef, SourceRefCS);
	BeanPuppet::BuildRefCS(TargetRef, TargetRefCS);
	TargetCS = TargetRefCS;

	Resolved.Reset();
	Resolved.SetNum(TargetRef.GetNum());
	int32 Mapped = 0;
	for (const FBeanBoneMapping& Mapping : BoneMappings)
	{
		const int32 Target = TargetRef.FindBoneIndex(Mapping.TargetBone);
		const int32 Source = SourceRef.FindBoneIndex(Mapping.SourceBone);
		if (Target == INDEX_NONE || Source == INDEX_NONE)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("BeanPuppet: mapping %s -> %s ignored (bone not found)"),
				*Mapping.SourceBone.ToString(), *Mapping.TargetBone.ToString());
			continue;
		}
		FResolvedMapping& Entry = Resolved[Target];
		Entry.Source = Source;
		Entry.SourceChild = SourceRef.FindBoneIndex(Mapping.SourceChild);
		Entry.TargetChild = TargetRef.FindBoneIndex(Mapping.TargetChild);
		Entry.SpreadRadians = FMath::DegreesToRadians(Mapping.SpreadDegrees);
		Entry.RotationScale = FMath::Clamp(Mapping.RotationScale, 0.f, 1.f);
		// +X is the character's left in mesh space (the mesh faces +Y)
		Entry.Side = FMath::Sign(TargetRefCS[Target].GetLocation().X);
		++Mapped;
	}

	OverrideDir.Init(FVector::ZeroVector, TargetRef.GetNum());
	OverrideAlpha.Init(0.f, TargetRef.GetNum());
	UpperArmL = TargetRef.FindBoneIndex(TEXT("upperarm_l"));
	LowerArmL = TargetRef.FindBoneIndex(TEXT("lowerarm_l"));
	UpperArmR = TargetRef.FindBoneIndex(TEXT("upperarm_r"));
	LowerArmR = TargetRef.FindBoneIndex(TEXT("lowerarm_r"));
	HandL = TargetRef.FindBoneIndex(TEXT("hand_l"));
	HandR = TargetRef.FindBoneIndex(TEXT("hand_r"));
	OwnerCombat = GetOwner()->FindComponentByClass<UIslanderCombatComponent>();
	if (UIslanderHealthComponent* Health = GetOwner()->FindComponentByClass<UIslanderHealthComponent>())
	{
		Health->OnHealthChanged.AddUniqueDynamic(this, &UBeanPuppetComponent::HandleOwnerHealthChanged);
	}
	HatIndex = TargetRef.FindBoneIndex(HatBone);
	BaseRelativeLocation = GetRelativeLocation();
	BaseRelativeScale = GetRelativeScale3D();

	// first person camera at the bean's eyes: it follows the crouch squash and the swim lift
	for (UActorComponent* Component : GetOwner()->GetComponentsByTag(UCameraComponent::StaticClass(), TEXT("FirstPerson")))
	{
		UCameraComponent* Camera = Cast<UCameraComponent>(Component);
		Camera->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		Camera->SetRelativeLocation(FirstPersonEyeLocation);
	}

	PelvisIndex = TargetRef.FindBoneIndex(PelvisBone);
	PelvisScale = PelvisTranslationScale;
	if (PelvisIndex != INDEX_NONE && Resolved[PelvisIndex].Source != INDEX_NONE && PelvisScale <= 0.f)
	{
		const float SourceHeight = SourceRefCS[Resolved[PelvisIndex].Source].GetLocation().Z;
		PelvisScale = SourceHeight > KINDA_SMALL_NUMBER ? TargetRefCS[PelvisIndex].GetLocation().Z / SourceHeight : 1.f;
	}

	if (bHideSourceMesh)
	{
		// keep evaluating the hidden animation (and its physics) so the bean can copy it
		SourceMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		SourceMesh->SetHiddenInGame(true);
	}
	AddTickPrerequisiteComponent(SourceMesh);

	UE_LOG(Logmonke_island, Log, TEXT("BeanPuppet on %s: %d bones driven by %s, pelvis scale %.2f"),
		*GetNameSafe(GetOwner()), Mapped, *GetNameSafe(SourceMesh->GetSkinnedAsset()), PelvisScale);
	return Mapped > 0;
}

void UBeanPuppetComponent::SpawnSkinParts()
{
	if (OutfitMesh && !OutfitComponent)
	{
		OutfitComponent = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("BeanOutfit"));
		OutfitComponent->SetSkeletalMesh(OutfitMesh);
		OutfitComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutfitComponent->SetupAttachment(this);
		OutfitComponent->RegisterComponent();
		OutfitComponent->SetLeaderPoseComponent(this);
	}

	if (HatMesh && !HatComponent && GetSkinnedAsset())
	{
		HatComponent = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("BeanHat"));
		HatComponent->SetStaticMesh(HatMesh);
		HatComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HatComponent->SetupAttachment(this, HatBone);
		HatComponent->RegisterComponent();

		// the hat asset is authored upright at the bone head: undo the bone orientation of the rest pose
		TArray<FTransform> RefCS;
		BeanPuppet::BuildRefCS(GetSkinnedAsset()->GetRefSkeleton(), RefCS);
		const int32 HatBoneIndex = GetSkinnedAsset()->GetRefSkeleton().FindBoneIndex(HatBone);
		if (HatBoneIndex != INDEX_NONE)
		{
			const FTransform HatRest(FQuat::Identity, RefCS[HatBoneIndex].GetLocation());
			HatComponent->SetRelativeTransform(HatRest.GetRelativeTransform(RefCS[HatBoneIndex]));
			HatRelativeTransform = HatComponent->GetRelativeTransform();
		}
	}
}

void UBeanPuppetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReady && SourceMesh && SourceMesh->GetComponentSpaceTransforms().Num() == SourceRefCS.Num())
	{
		UpdateStance(DeltaTime);
		CopyPose();
	}
}

void UBeanPuppetComponent::SetArmOverride(int32 UpperIndex, int32 LowerIndex, const FVector& Upper, const FVector& Lower, float Alpha, float MirrorX)
{
	if (Alpha <= KINDA_SMALL_NUMBER || UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE)
	{
		return;
	}
	const FVector Mirror(MirrorX, 1.0, 1.0);
	for (const TPair<int32, FVector>& Entry : { TPair<int32, FVector>(UpperIndex, Upper), TPair<int32, FVector>(LowerIndex, Lower) })
	{
		// the strongest override of the frame wins, blended with the previous one
		const FVector Dir = (Entry.Value * Mirror).GetSafeNormal();
		const float Previous = OverrideAlpha[Entry.Key];
		OverrideDir[Entry.Key] = Previous > 0.f ? (OverrideDir[Entry.Key] * Previous + Dir * Alpha).GetSafeNormal() : Dir;
		OverrideAlpha[Entry.Key] = FMath::Max(Previous, Alpha);
	}
}

void UBeanPuppetComponent::UpdateStance(float DeltaTime)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;

	// targets from the owner state
	float WindupTarget = 0.f;
	float StrikeTarget = 0.f;
	float GuardTarget = 0.f;
	if (OwnerCombat)
	{
		const EIslanderAttackPhase Phase = OwnerCombat->GetAttackPhase();
		WindupTarget = Phase == EIslanderAttackPhase::Windup ? 1.f : 0.f;
		StrikeTarget = Phase == EIslanderAttackPhase::Strike ? 1.f : 0.f;
		GuardTarget = OwnerCombat->IsGuarding() ? 1.f : 0.f;
	}
	const float CrouchTarget = Character && Character->bIsCrouched ? 1.f : 0.f;
	const float SwimTarget = Move && Move->IsSwimming() ? 1.f : 0.f;

	// the strike snaps forward, everything else eases
	WindupAlpha = FMath::FInterpTo(WindupAlpha, WindupTarget, DeltaTime, BlendSpeed);
	StrikeAlpha = StrikeTarget > StrikeAlpha ? StrikeTarget : FMath::FInterpTo(StrikeAlpha, StrikeTarget, DeltaTime, BlendSpeed * 0.6f);
	GuardAlpha = FMath::FInterpTo(GuardAlpha, GuardTarget, DeltaTime, BlendSpeed);
	CrouchAlpha = FMath::FInterpTo(CrouchAlpha, CrouchTarget, DeltaTime, BlendSpeed * 0.6f);
	SwimAlpha = FMath::FInterpTo(SwimAlpha, SwimTarget, DeltaTime, BlendSpeed * 0.25f);

	// Fall Guys squash while crouched; lifted in the water (the capsule floats at an adult's height)
	SetRelativeScale3D(BaseRelativeScale * FMath::Lerp(FVector::OneVector, CrouchScale, CrouchAlpha));
	SetRelativeLocation(BaseRelativeLocation + FVector(0.0, 0.0, SwimLift * SwimAlpha));

	// knocked out: the hat flies off; standing near it again puts it back on
	if (const AIslanderCharacter* Knocked = Cast<AIslanderCharacter>(Character))
	{
		const bool bRagdoll = Knocked->IsRagdoll();
		if (bRagdoll && !bWasRagdoll)
		{
			DropHat(Knocked->GetVelocity());
		}
		bWasRagdoll = bRagdoll;

		const UIslanderHealthComponent* Health = Knocked->GetHealthComponent();
		const bool bAlive = !Health || !Health->IsDead();
		if (bHatDropped && HatComponent && !bRagdoll && bAlive && GetWorld()->GetTimeSeconds() - HatDropTime >= HatPickupDelay)
		{
			const FVector Feet = Knocked->GetActorLocation() - FVector(0.0, 0.0, Knocked->GetSimpleCollisionHalfHeight());
			if (FVector::Dist2D(HatComponent->GetComponentLocation(), Feet) <= HatPickupRadius
				&& FMath::Abs(HatComponent->GetComponentLocation().Z - Feet.Z) <= 2.0 * HatPickupRadius)
			{
				PickUpHat();
			}
		}
	}

	// first person: the hat brim would cover the view (owner only, the others still see it)
	if (HatComponent)
	{
		const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character);
		const bool bHideHat = bHideHatInFirstPerson && !bHatDropped && Islander && Islander->IsFirstPerson() && !Islander->IsRagdoll();
		if (HatComponent->bOwnerNoSee != bHideHat)
		{
			HatComponent->SetOwnerNoSee(bHideHat);
		}
	}

	// first person: the owner sees its own arms instead of the body (cahier §10), the shadow stays
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character);
	const bool bHideBody = bHideBodyInFirstPerson && Islander && Islander->IsFirstPerson() && !Islander->IsRagdoll();
	if (bHideBody != bBodyHiddenForOwner)
	{
		bBodyHiddenForOwner = bHideBody;
		SetOwnerNoSee(bHideBody);
		bCastHiddenShadow = true;
		if (OutfitComponent)
		{
			OutfitComponent->SetOwnerNoSee(bHideBody);
		}
	}

	// arm overrides, mirrored on X for the left arm
	for (float& Alpha : OverrideAlpha)
	{
		Alpha = 0.f;
	}

	// holding an object: both arms reach toward it (seen by the other players)
	const UIslanderGrabComponent* Grab = Islander ? Islander->GetGrabComponent() : nullptr;
	const UPrimitiveComponent* Held = Grab ? Grab->GetHeldComponent() : nullptr;
	GrabAlpha = FMath::FInterpTo(GrabAlpha, Held ? 1.f : 0.f, DeltaTime, BlendSpeed * 0.5f);
	if (GrabAlpha > KINDA_SMALL_NUMBER)
	{
		const FVector Target = Held ? Held->GetComponentLocation() : GetComponentLocation() + GetRightVector() * 100.0;
		const FVector Reach = GetComponentTransform().InverseTransformVectorNoScale(Target - GetComponentLocation()).GetSafeNormal();
		// both arms point at the object, lower arms a bit more level so the mittens meet in front
		const FVector LowerReach = (Reach + FVector(0.0, 0.3, 0.2)).GetSafeNormal();
		SetArmOverride(UpperArmR, LowerArmR, Reach + FVector(-0.25, 0.0, 0.0), LowerReach + FVector(0.15, 0.0, 0.0), GrabAlpha, 1.0);
		SetArmOverride(UpperArmL, LowerArmL, Reach + FVector(0.25, 0.0, 0.0), LowerReach + FVector(-0.15, 0.0, 0.0), GrabAlpha, 1.0);
	}

	SetArmOverride(UpperArmR, LowerArmR, GuardUpperArm, GuardLowerArm, GuardAlpha, 1.0);
	SetArmOverride(UpperArmL, LowerArmL, GuardUpperArm, GuardLowerArm, GuardAlpha, -1.0);
	SetArmOverride(UpperArmR, LowerArmR, PunchWindupUpperArm, PunchWindupLowerArm, WindupAlpha, 1.0);
	SetArmOverride(UpperArmR, LowerArmR, PunchStrikeUpperArm, PunchStrikeLowerArm, StrikeAlpha, 1.0);
}

void UBeanPuppetComponent::CopyPose()
{
	const TArray<FTransform>& SourceCS = SourceMesh->GetComponentSpaceTransforms();
	const FReferenceSkeleton& TargetRef = GetSkinnedAsset()->GetRefSkeleton();
	const TArray<FTransform>& TargetLocal = TargetRef.GetRefBonePose();
	const int32 NumBones = TargetRef.GetNum();
	if (BoneSpaceTransforms.Num() != NumBones)
	{
		return;
	}

	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const int32 Parent = TargetRef.GetParentIndex(Index);
		const FResolvedMapping& Entry = Resolved[Index];

		if (Entry.Source == INDEX_NONE)
		{
			TargetCS[Index] = Parent == INDEX_NONE ? TargetLocal[Index] : TargetLocal[Index] * TargetCS[Parent];
			if (Index == HatIndex && HatLevelBlend > 0.f)
			{
				// the hat sits on the head but mostly stays level, centred on the top of the bean
				TargetCS[Index].SetRotation(FQuat::Slerp(TargetCS[Index].GetRotation(), TargetRefCS[Index].GetRotation(), HatLevelBlend));
			}
		}
		else
		{
			// rotation: same change from the reference pose as the source bone (scaled down for the stiff trunk)
			FQuat Delta = SourceCS[Entry.Source].GetRotation() * SourceRefCS[Entry.Source].GetRotation().Inverse();
			if (Entry.RotationScale < 1.f)
			{
				Delta = FQuat::Slerp(FQuat::Identity, Delta, Entry.RotationScale);
			}
			FQuat Rotation = Delta * TargetRefCS[Index].GetRotation();

			// limbs: aim along the source limb (bean limbs are much shorter, a rotation delta alone drifts)
			if (Entry.SourceChild != INDEX_NONE && Entry.TargetChild != INDEX_NONE)
			{
				FVector SourceDir = (SourceCS[Entry.SourceChild].GetLocation() - SourceCS[Entry.Source].GetLocation()).GetSafeNormal();
				if (Entry.SpreadRadians != 0.f)
				{
					// swing outwards around the forward axis (+Y), left side towards +X
					SourceDir = FQuat(FVector::YAxisVector, -Entry.Side * Entry.SpreadRadians).RotateVector(SourceDir);
				}
				// punch / guard: blend toward the procedural arm direction
				if (OverrideAlpha.IsValidIndex(Index) && OverrideAlpha[Index] > 0.f)
				{
					SourceDir = FMath::Lerp(SourceDir, OverrideDir[Index], OverrideAlpha[Index]).GetSafeNormal();
				}
				const FVector RestDir = (TargetRefCS[Entry.TargetChild].GetLocation() - TargetRefCS[Index].GetLocation()).GetSafeNormal();
				const FVector CurrentDir = Delta.RotateVector(RestDir);
				if (!SourceDir.IsNearlyZero() && !CurrentDir.IsNearlyZero())
				{
					Rotation = FQuat::FindBetweenNormals(CurrentDir, SourceDir) * Rotation;
				}
			}

			FVector Location;
			if (Index == PelvisIndex)
			{
				const FVector SourceOffset = SourceCS[Entry.Source].GetLocation() - SourceRefCS[Entry.Source].GetLocation();
				Location = TargetRefCS[Index].GetLocation() + SourceOffset * PelvisScale;
			}
			else
			{
				Location = Parent == INDEX_NONE ? TargetLocal[Index].GetLocation()
					: TargetCS[Parent].TransformPosition(TargetLocal[Index].GetLocation());
			}
			TargetCS[Index] = FTransform(Rotation.GetNormalized(), Location, TargetRefCS[Index].GetScale3D());
		}

		BoneSpaceTransforms[Index] = Parent == INDEX_NONE ? TargetCS[Index] : TargetCS[Index].GetRelativeTransform(TargetCS[Parent]);
	}

	// first person with the body visible: on the owner's machine only, the body's arms fold into the shoulders
	// (the first person arms take their place); each machine computes its own puppet pose, so the others still see them
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	if (bHideArmsForOwnerInFirstPerson && !bHideBodyInFirstPerson && Islander && Islander->IsLocallyControlled()
		&& Islander->IsFirstPerson() && !Islander->IsRagdoll())
	{
		const int32 ArmBones[] = { UpperArmL, LowerArmL, HandL, UpperArmR, LowerArmR, HandR };
		for (const int32 Bone : ArmBones)
		{
			if (Bone != INDEX_NONE)
			{
				TargetCS[Bone].SetScale3D(FVector(0.001));
			}
		}
		for (const int32 Bone : ArmBones)
		{
			if (Bone != INDEX_NONE)
			{
				BoneSpaceTransforms[Bone] = TargetCS[Bone].GetRelativeTransform(TargetCS[TargetRef.GetParentIndex(Bone)]);
			}
		}
	}

	RefreshBoneTransforms();
}

void UBeanPuppetComponent::HandleOwnerHealthChanged(float NewHealth, float Delta)
{
	// a punch or a hard landing knocks the hat off
	if (Delta < 0.f)
	{
		DropHat(GetOwner()->GetVelocity());
	}
}

void UBeanPuppetComponent::DropHat(FVector Velocity)
{
	if (!bHatFallsOff || bHatDropped || !HatComponent || !HatComponent->GetStaticMesh())
	{
		return;
	}

	bHatDropped = true;
	HatDropTime = GetWorld()->GetTimeSeconds();
	HatComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	HatComponent->SetOwnerNoSee(false);

	// a light physics body that rolls on the ground; the players walk through it and pick it up by touching it
	HatComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	HatComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	HatComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	HatComponent->SetSimulatePhysics(true);
	HatComponent->SetMassOverrideInKg(NAME_None, 0.3f, true);
	const FVector Spin(FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-180.f, 180.f));
	HatComponent->SetPhysicsLinearVelocity(Velocity + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), HatDropKick));
	HatComponent->SetPhysicsAngularVelocityInDegrees(Spin);
}

void UBeanPuppetComponent::PickUpHat()
{
	if (!HatComponent)
	{
		return;
	}

	HatComponent->SetSimulatePhysics(false);
	HatComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HatComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform, HatBone);
	HatComponent->SetRelativeTransform(HatRelativeTransform);
	bHatDropped = false;
}
