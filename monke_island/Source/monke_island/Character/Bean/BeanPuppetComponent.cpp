// monke_island

#include "Character/Bean/BeanPuppetComponent.h"
#include "Character/Bean/BeanDroppedHat.h"
#include "Character/Bean/BeanSwimAnim.h"
#include "CoreGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Water/IslandWaterSubsystem.h"
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
	SetIsReplicatedByDefault(true);

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
	// Existing Blueprint components may have serialized the old non-replicated default.
	SetIsReplicated(true);

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
	if (GetOwner()->HasAuthority() && IsValid(DroppedHat))
	{
		DroppedHat->Destroy();
	}
	if (UIslanderHealthComponent* Health = GetOwner()->FindComponentByClass<UIslanderHealthComponent>())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &UBeanPuppetComponent::HandleOwnerHealthChanged);
	}
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
	BaseRelativeRotation = GetRelativeRotation().Quaternion();
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
		HatComponent->SetHiddenInGame(bHatDropped);
	}
}

void UBeanPuppetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReady && SourceMesh && SourceMesh->GetComponentSpaceTransforms().Num() == SourceRefCS.Num())
	{
		UpdateStance(DeltaTime);
		UpdateLean(DeltaTime);
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

void UBeanPuppetComponent::UpdateLean(float DeltaTime)
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Islander ? Islander->GetCharacterMovement() : nullptr;
	if (!Move || DeltaTime <= 0.f)
	{
		return;
	}

	// a body leans into its acceleration (starting, braking) and into the turns (centripetal): about atan(a / g)
	// to keep its balance. Measured from the velocity, so it works the same for remote players (smoothed moves)
	const FVector Velocity = Islander->GetVelocity();
	FVector2D Acceleration = FVector2D::ZeroVector;
	if (bHasLastVelocity)
	{
		Acceleration = FVector2D(Velocity - LastVelocity) / DeltaTime;
	}
	LastVelocity = Velocity;
	bHasLastVelocity = true;
	SmoothedAcceleration = FMath::Lerp(SmoothedAcceleration, Acceleration, FMath::Clamp(DeltaTime / AccelerationSmoothing, 0.f, 1.f));

	// standing on the ground only; upright in the air, in the water, knocked out, and for the owner's first person
	// view (the camera rides this component)
	const bool bOwnerView = Islander->IsLocallyControlled() && Islander->IsFirstPerson();
	const bool bGrounded = Move->IsMovingOnGround() && !Islander->IsRagdoll() && !bOwnerView;
	FVector2D Target = FVector2D::ZeroVector;
	if (bGrounded)
	{
		const float Gravity = FMath::Max(FMath::Abs(GetWorld()->GetGravityZ()), 1.f);
		const float Magnitude = SmoothedAcceleration.Size();
		const float Angle = FMath::Min(LeanFactor * FMath::Atan(Magnitude / Gravity), FMath::DegreesToRadians(MaxLeanDegrees));
		Target = Magnitude > KINDA_SMALL_NUMBER ? SmoothedAcceleration / Magnitude * Angle : FVector2D::ZeroVector;
	}

	// damped spring toward the target, in sub steps so a long frame cannot blow it up
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / 0.008f), 1, 8);
	const float Step = DeltaTime / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		LeanSpeed += ((Target - Lean) * LeanStiffness - LeanSpeed * LeanDamping) * Step;
		Lean += LeanSpeed * Step;
	}
	const float Limit = FMath::DegreesToRadians(MaxLeanDegrees) * 1.3f;
	if (Lean.Size() > Limit)
	{
		Lean = Lean.GetSafeNormal() * Limit;
	}

	// tilt about the feet (the pivot of the bean), axis = up x lean direction, expressed in the parent's space
	FQuat Tilt = FQuat::Identity;
	const float Angle = Lean.Size();
	if (Angle > 1e-4f)
	{
		const FVector AxisWorld = FVector::CrossProduct(FVector::UpVector, FVector(Lean.X, Lean.Y, 0.f)).GetSafeNormal();
		const USceneComponent* Parent = GetAttachParent();
		const FVector AxisLocal = Parent ? Parent->GetComponentQuat().UnrotateVector(AxisWorld) : AxisWorld;
		Tilt = FQuat(AxisLocal, Angle);
	}
	SetRelativeRotation(Tilt * BaseRelativeRotation);
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

	// Fall Guys squash while crouched; lifted in the water (the capsule floats at an adult's height). The bean's own
	// swim clips float it at the water surface they are authored for (SetWaterStance, given every frame by
	// UBeanAnimDriverComponent); without them, SwimLift while swimming
	float Lift = SwimLift * SwimAlpha;
	if (WaterStanceFrame != 0 && GFrameCounter <= WaterStanceFrame + 1)
	{
		Lift = WaterStanceAlpha > KINDA_SMALL_NUMBER ? WaterStanceAlpha * WaterStanceLift() : 0.f;
	}
	SetRelativeScale3D(BaseRelativeScale * FMath::Lerp(FVector::OneVector, CrouchScale, CrouchAlpha));
	SetRelativeLocation(BaseRelativeLocation + FVector(0.0, 0.0, Lift));

	// Knocked out: the server drops the hat. Recovery, proximity and time never equip it.
	if (const AIslanderCharacter* Knocked = Cast<AIslanderCharacter>(Character))
	{
		const bool bRagdoll = Knocked->IsRagdoll();
		if (bRagdoll && !bWasRagdoll)
		{
			DropHat(Knocked->GetVelocity());
		}
		bWasRagdoll = bRagdoll;
	}

	// the hat pops back on the head (scale 0 -> 1)
	if (HatComponent && !bHatDropped && HatPopAlpha < 1.f)
	{
		HatPopAlpha = FMath::Min(1.f, HatPopAlpha + DeltaTime / FMath::Max(HatPopTime, 0.01f));
		const float Pop = FMath::Sin(HatPopAlpha * UE_HALF_PI) * (1.f + 0.15f * FMath::Sin(HatPopAlpha * UE_PI));
		HatComponent->SetRelativeScale3D(HatRelativeTransform.GetScale3D() * FMath::Max(Pop, 0.01f));
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

	// first person: the hair and the moustache ride the head bone in front of the camera; the owner's machine alone
	// makes them see-through, the other players still see them
	{
		const AIslanderCharacter* Viewer = Cast<AIslanderCharacter>(Character);
		const bool bHideHead = Viewer && Viewer->IsLocallyControlled() && Viewer->IsFirstPerson() && !Viewer->IsRagdoll();
		if (bHideHead != bHeadSlotsHidden)
		{
			SetHeadSlotsHidden(bHideHead);
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

	ApplyBasePoseSource();
	ApplyBoneOverrideSource();

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

void UBeanPuppetComponent::SetBoneOverrideSource(USkeletalMeshComponent* Source, const TArray<FName>& Bones, float Alpha)
{
	BoneOverrideAlpha = Source ? FMath::Clamp(Alpha, 0.f, 1.f) : 0.f;
	if (BoneOverrideSource.Get() != Source || BoneOverrideNames != Bones)
	{
		BoneOverrideSource = Source;
		BoneOverrideNames = Bones;
		BoneOverrideIndices.Reset();
		if (Source)
		{
			// the source evaluates first, the copy reads its pose of this frame
			AddTickPrerequisiteComponent(Source);
		}
		if (const USkinnedAsset* Asset = GetSkinnedAsset())
		{
			for (const FName Bone : Bones)
			{
				const int32 Index = Asset->GetRefSkeleton().FindBoneIndex(Bone);
				if (Index != INDEX_NONE)
				{
					BoneOverrideIndices.Add(Index);
				}
			}
		}
	}
}

void UBeanPuppetComponent::SetBasePoseSource(USkeletalMeshComponent* Source, float Alpha)
{
	BasePoseAlpha = Source ? FMath::Clamp(Alpha, 0.f, 1.f) : 0.f;
	if (BasePoseSource.Get() != Source)
	{
		BasePoseSource = Source;
		if (Source)
		{
			// the source evaluates first, the copy reads its pose of this frame
			AddTickPrerequisiteComponent(Source);
		}
	}
}

void UBeanPuppetComponent::SetWaterStance(float Alpha, float RootDepthCm, float MaxLiftCm)
{
	WaterStanceAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	WaterStanceRootDepth = RootDepthCm;
	WaterStanceMaxLift = MaxLiftCm;
	WaterStanceFrame = GFrameCounter;
}

float UBeanPuppetComponent::WaterStanceLift() const
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Water)
	{
		return 0.f;
	}
	// dry placement of the root (the capsule's feet), then the sea surface right there, waves and flood included
	const USceneComponent* Parent = GetAttachParent();
	const FVector DryRoot = Parent ? Parent->GetComponentTransform().TransformPosition(BaseRelativeLocation) : BaseRelativeLocation;
	const float WorldLift = BeanSwimAnim::FloatLift(Water->GetWaterSurfaceAt(DryRoot), static_cast<float>(DryRoot.Z),
		WaterStanceRootDepth, WaterStanceMaxLift);
	// the character stays upright: the parent's Z is the world's, only its scale may differ
	const float ParentScale = Parent ? static_cast<float>(Parent->GetComponentScale().Z) : 1.f;
	return WorldLift / FMath::Max(ParentScale, KINDA_SMALL_NUMBER);
}

void UBeanPuppetComponent::ApplyBasePoseSource()
{
	USkeletalMeshComponent* Source = BasePoseSource.Get();
	if (!Source || BasePoseAlpha <= KINDA_SMALL_NUMBER || Source->GetSkinnedAsset() != GetSkinnedAsset())
	{
		return;
	}
	const TArrayView<const FTransform> SourceLocal = Source->GetBoneSpaceTransformsView();
	if (SourceLocal.Num() != BoneSpaceTransforms.Num())
	{
		return;
	}
	for (int32 Index = 0; Index < BoneSpaceTransforms.Num(); ++Index)
	{
		// the hat keeps its levelling; a punching or guarding arm keeps its procedural direction
		if (Index == HatIndex)
		{
			continue;
		}
		const float Override = OverrideAlpha.IsValidIndex(Index) ? OverrideAlpha[Index] : 0.f;
		const float Alpha = BasePoseAlpha * (1.f - Override);
		if (Alpha <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		BoneSpaceTransforms[Index].SetRotation(FQuat::Slerp(BoneSpaceTransforms[Index].GetRotation(), SourceLocal[Index].GetRotation(), Alpha).GetNormalized());
		if (Index == PelvisIndex)
		{
			// same skeleton: the clip's pelvis height and sway as they are; bone lengths elsewhere stay the bean's
			BoneSpaceTransforms[Index].SetLocation(FMath::Lerp(BoneSpaceTransforms[Index].GetLocation(), SourceLocal[Index].GetLocation(), Alpha));
		}
	}
	RebuildComponentSpace();
}

void UBeanPuppetComponent::RebuildComponentSpace()
{
	const FReferenceSkeleton& TargetRef = GetSkinnedAsset()->GetRefSkeleton();
	for (int32 Index = 0; Index < BoneSpaceTransforms.Num(); ++Index)
	{
		const int32 Parent = TargetRef.GetParentIndex(Index);
		TargetCS[Index] = Parent == INDEX_NONE ? BoneSpaceTransforms[Index] : BoneSpaceTransforms[Index] * TargetCS[Parent];
	}
}

void UBeanPuppetComponent::ApplyBoneOverrideSource()
{
	USkeletalMeshComponent* Source = BoneOverrideSource.Get();
	if (!Source || BoneOverrideAlpha <= KINDA_SMALL_NUMBER || BoneOverrideIndices.IsEmpty()
		|| Source->GetSkinnedAsset() != GetSkinnedAsset())
	{
		return;
	}
	const TArrayView<const FTransform> SourceLocal = Source->GetBoneSpaceTransformsView();
	if (SourceLocal.Num() != BoneSpaceTransforms.Num())
	{
		return;
	}
	for (const int32 Index : BoneOverrideIndices)
	{
		// rotations only: the bean's bone lengths and the walk's pelvis stay
		const FQuat Rotation = FQuat::Slerp(BoneSpaceTransforms[Index].GetRotation(), SourceLocal[Index].GetRotation(), BoneOverrideAlpha);
		BoneSpaceTransforms[Index].SetRotation(Rotation.GetNormalized());
	}
	// component space again
	RebuildComponentSpace();
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
	if (!GetOwner()->HasAuthority() || !bHatFallsOff || bHatDropped || !IsValid(HatComponent) || !HatComponent->GetStaticMesh())
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform DropTransform = HatComponent->GetComponentTransform();
	// A second hit during the equip pop still drops a full-size hat.
	DropTransform.SetScale3D(HatRelativeTransform.GetScale3D() * GetSocketTransform(HatBone).GetScale3D());
	DroppedHat = GetWorld()->SpawnActor<ABeanDroppedHat>(ABeanDroppedHat::StaticClass(), DropTransform, Params);
	if (!DroppedHat)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Could not drop the hat for %s"), *GetNameSafe(GetOwner()));
		return;
	}
	DroppedHat->Initialize(HatComponent->GetStaticMesh(), Velocity, HatDropKick);
	bHatDropped = true;
	OnRep_HatDropped();
	GetOwner()->ForceNetUpdate();
}

bool UBeanPuppetComponent::TryPickUpHat(ABeanDroppedHat* Hat, APawn* User)
{
	if (!GetOwner()->HasAuthority() || !bHatDropped || !IsValid(Hat) || Hat != DroppedHat
		|| !IsValid(HatComponent) || !Hat->CanInteract_Implementation(User))
	{
		return false;
	}

	bHatDropped = false;
	OnRep_HatDropped();
	Hat->Destroy();
	DroppedHat = nullptr;
	GetOwner()->ForceNetUpdate();
	return true;
}

void UBeanPuppetComponent::OnRep_HatDropped()
{
	if (IsValid(HatComponent))
	{
		HatComponent->SetHiddenInGame(bHatDropped);
		HatComponent->SetRelativeTransform(HatRelativeTransform);
		HatPopAlpha = bHatDropped ? 1.f : 0.f;
	}
}

void UBeanPuppetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBeanPuppetComponent, bHatDropped);
}

void UBeanPuppetComponent::SetHeadSlotsHidden(bool bHide)
{
	bHeadSlotsHidden = bHide;
	if (bHide)
	{
		UMaterialInterface* Hidden = FirstPersonHiddenMaterial.LoadSynchronous();
		if (!Hidden)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s: no see-through material (%s), the hair stays in the first person view"),
				*GetNameSafe(GetOwner()), *FirstPersonHiddenMaterial.ToString());
			return;
		}
		HeadSlotMaterials.Reset();
		for (const FName Slot : FirstPersonHiddenSlots)
		{
			const int32 Index = GetMaterialIndex(Slot);
			if (Index != INDEX_NONE)
			{
				HeadSlotMaterials.Add(Index, OverrideMaterials.IsValidIndex(Index) ? OverrideMaterials[Index].Get() : nullptr);
				SetMaterial(Index, Hidden);
			}
		}
		return;
	}
	for (const TPair<int32, TObjectPtr<UMaterialInterface>>& Saved : HeadSlotMaterials)
	{
		SetMaterial(Saved.Key, Saved.Value);
	}
	HeadSlotMaterials.Reset();
}
