// monke_island

#include "Creatures/GiantCrabBody.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"
#include "Player/IslanderCharacter.h"
#include "Weapons/FirearmDamage.h"
#include "monke_island.h"
#include "AnimationRuntime.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarGiantCrabShowBodies(TEXT("monke.GiantCrab.ShowBodies"), 0,
		TEXT("1: the hit bodies of the giant crabs that appear afterwards are drawn in game (T-261)"), ECVF_Cheat);

	/** Colour of a zone when the bodies are drawn */
	FColor CrabZoneColor(EGiantCrabZone Zone)
	{
		switch (Zone)
		{
		case EGiantCrabZone::Leg:
			return FColor::Green;
		case EGiantCrabZone::Claw:
			return FColor::Cyan;
		case EGiantCrabZone::Joint:
			return FColor::Orange;
		case EGiantCrabZone::Eye:
			return FColor::Red;
		case EGiantCrabZone::Belly:
			return FColor::Yellow;
		case EGiantCrabZone::Mouth:
			return FColor::Magenta;
		default:
			return FColor::Silver;
		}
	}
}

UGiantCrabBodyComponent::UGiantCrabBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

int32 UGiantCrabBodyComponent::Build(const UGiantCrabRules& Rules, USkeletalMeshComponent* Mesh)
{
	Clear();
	AActor* Owner = GetOwner();
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Owner || !Asset)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("GiantCrab %s: no mesh, no hit bodies"), Owner ? *Owner->GetName() : TEXT("?"));
		return 0;
	}
	SnapDistance = Rules.ZoneSnapDistanceCm;
	LegToken = Rules.LegBoneToken;
	ShotChannel = Rules.ShotTraceChannel;
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	const bool bShow = CVarGiantCrabShowBodies.GetValueOnGameThread() != 0;
	TArray<FName> Missing;
	// the fixed shapes hang from the root where the mesh stands under it (the character's own offsets: the smoothing of
	// a client may have moved the mesh a little)
	USceneComponent* Root = Owner->GetRootComponent();
	FTransform MeshInRoot = Mesh->GetRelativeTransform();
	if (const ACharacter* Character = Cast<ACharacter>(Owner); Character && Mesh->GetAttachParent() == Root)
	{
		MeshInRoot = FTransform(Character->GetBaseRotationOffset(), Character->GetBaseTranslationOffset(), Mesh->GetRelativeScale3D());
	}
	for (const FGiantCrabHitBody& Spec : Rules.HitBodies)
	{
		const int32 BoneIndex = Skeleton.FindBoneIndex(Spec.Bone);
		if (BoneIndex == INDEX_NONE)
		{
			Missing.Add(Spec.Bone);
			continue;
		}
		// the shape in the mesh's space at rest, then relative to its bone: the bone's scale (100 on the kit's bones)
		// divides the offset, the shape keeps its own size (absolute scale)
		const FTransform BoneRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, BoneIndex);
		FQuat Rotation = FQuat::Identity;
		if (!Spec.bBox)
		{
			const FVector Axis = BoneRest.GetRotation().RotateVector(Spec.BoneAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector));
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
		}
		const FTransform Relative = FTransform(Rotation, Spec.CenterCm).GetRelativeTransform(BoneRest);

		const FName Name = MakeUniqueObjectName(Owner, UShapeComponent::StaticClass(), FName(*FString::Printf(TEXT("HitBody_%s"), *Spec.Bone.ToString())));
		UShapeComponent* Shape = nullptr;
		if (Spec.bBox)
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(Owner, Name);
			Box->InitBoxExtent(Spec.BoxSizeCm * 0.5);
			Shape = Box;
		}
		else
		{
			UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Owner, Name);
			Capsule->InitCapsuleSize(Spec.RadiusCm, Spec.RadiusCm + Spec.LengthCm * 0.5f);
			Shape = Capsule;
		}
		Shape->SetupAttachment(Mesh, Spec.Bone);
		Shape->SetUsingAbsoluteScale(true);
		Shape->SetRelativeTransform(FTransform(Relative.GetRotation(), Relative.GetTranslation(), FVector::OneVector));

		// query only: it blocks the Pawn channel, the cutlass finds it (object type Pawn), the shots stop on the slow
		// parts (the legs and claws: FirearmDamage::TraceHitBodies looks for them on the Pawn channel). The moves of the
		// characters around the crab ignore it (mask filter, T-305, T-328): it follows the clip of each machine and pressed
		// them down from above, the upright pillars below stop them
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_Pawn);
		Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
		if (Rules.bHitBodiesBlockPawns)
		{
			Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
		Shape->SetMaskFilterOnBodyInstance(AIslanderCharacter::AnimatedHitBodyMaskFilter);
		if (!GiantCrabMath::IsLimbZone(Spec.Zone) || Rules.bLimbsBlockShots)
		{
			Shape->SetCollisionResponseToChannel(Rules.ShotTraceChannel, ECR_Block);
		}
		Shape->SetGenerateOverlapEvents(false);
		// the shots find every body, the legs and claws too (FirearmDamage::TraceHitBodies, lead 2026-10-09)
		Shape->ComponentTags.Add(FirearmDamage::HitBodyTag);
		// nobody stands on the crab, steps onto it or paths around it
		Shape->CanCharacterStepUpOn = ECB_No;
		Shape->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));
		Shape->SetCanEverAffectNavigation(false);
		Shape->bReturnMaterialOnMove = false;
		Shape->SetShouldUpdatePhysicsVolume(false);
		Shape->ShapeColor = CrabZoneColor(Spec.Zone);
		Shape->SetHiddenInGame(!bShow);
		Shape->RegisterComponent();

		FGiantCrabBodyPart& Part = Parts.AddDefaulted_GetRef();
		Part.Shape = Shape;
		Part.Bone = Spec.Bone;
		Part.Zone = Spec.Zone;
	}
	if (!Missing.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("GiantCrab %s: %d hit bodies on bones the mesh lacks (first: %s): HitBodies of the rules to update"),
			*Owner->GetName(), Missing.Num(), *Missing[0].ToString());
	}

	// T-328: upright pillars on the root stop the pawns on every machine, alone or in a network session, where the leg
	// tips and the claws stand at rest. The fixed copy of every body (T-305) put claws, legs and shell at head height: a
	// turn of the crab or a rest pose pressed a capsule down onto the sand, which no move can undo. An upright pillar from
	// under the sand to above everyone only ever pushes a character aside
	if (Rules.bHitBodiesBlockPawns && Root && Root != Mesh)
	{
		for (const FGiantCrabPawnPillar& Spec : Rules.PawnPillars)
		{
			const float Radius = FMath::Max(1.0f, Spec.RadiusCm);
			float HalfHeight = 0.0f;
			const FVector InRoot = GiantCrabMath::PillarInRoot(Spec.CenterCm, Rules.PawnPillarBottomCm, Rules.PawnPillarTopCm, MeshInRoot, HalfHeight);
			const FName PillarName = MakeUniqueObjectName(Owner, UCapsuleComponent::StaticClass(), FName(*FString::Printf(TEXT("PawnPillar_%s"), *Spec.Name.ToString())));
			UCapsuleComponent* Pillar = NewObject<UCapsuleComponent>(Owner, PillarName);
			Pillar->InitCapsuleSize(Radius, FMath::Max(Radius, HalfHeight));
			Pillar->SetupAttachment(Root);
			Pillar->SetUsingAbsoluteScale(true);
			// upright with the root (a character's capsule never tilts)
			Pillar->SetRelativeLocationAndRotation(InRoot, FRotator::ZeroRotator);
			// it stops the pawns only. Object type Vehicle, which no query of the game looks for: a WorldDynamic one was
			// found by the object traces of the ground (corpses laid on it, items glued to it); the shots' second look on
			// the Pawn channel skips it by its mask filter
			Pillar->SetCollisionEnabled(bBlockersEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Pillar->SetCollisionObjectType(ECC_Vehicle);
			Pillar->SetCollisionResponseToAllChannels(ECR_Ignore);
			Pillar->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Pillar->SetMaskFilterOnBodyInstance(PawnBlockerMaskFilter);
			Pillar->SetGenerateOverlapEvents(false);
			Pillar->CanCharacterStepUpOn = ECB_No;
			Pillar->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));
			Pillar->SetCanEverAffectNavigation(false);
			Pillar->bReturnMaterialOnMove = false;
			Pillar->SetShouldUpdatePhysicsVolume(false);
			Pillar->ShapeColor = FColor::White;
			Pillar->SetHiddenInGame(!bShow);
			Pillar->RegisterComponent();
			Blockers.Add(Pillar);
		}
	}
	return Parts.Num();
}

float UGiantCrabBodyComponent::FindPin(const ACharacter& Character, FVector& OutAway, FName& OutShape) const
{
	OutAway = FVector::ZeroVector;
	OutShape = NAME_None;
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	if (!Capsule || !Capsule->IsQueryCollisionEnabled())
	{
		return 0.0f;
	}
	const FCollisionShape CapsuleShape = Capsule->GetCollisionShape();
	const FVector Where = Capsule->GetComponentLocation();
	const FQuat Rotation = Capsule->GetComponentQuat();
	const float Reach = CapsuleShape.GetCapsuleRadius() + CapsuleShape.GetCapsuleHalfHeight();
	const ECollisionChannel MoverChannel = Capsule->GetCollisionObjectType();
	const bool bIgnoresBodies = (Capsule->GetMoveIgnoreMask() & AIslanderCharacter::AnimatedHitBodyMaskFilter) != 0;
	float Deepest = 0.0f;
	auto Test = [&](UShapeComponent* Shape)
	{
		// only a shape that stops its moves (both sides block), and near enough to touch it
		if (!IsValid(Shape) || !Shape->IsQueryCollisionEnabled() || Shape->GetCollisionResponseToChannel(MoverChannel) != ECR_Block
			|| Capsule->GetCollisionResponseToChannel(Shape->GetCollisionObjectType()) != ECR_Block
			|| FVector::DistSquared(Shape->Bounds.Origin, Where) > FMath::Square(Shape->Bounds.SphereRadius + Reach))
		{
			return;
		}
		FMTDResult Mtd;
		if (Shape->ComputePenetration(Mtd, CapsuleShape, Where, Rotation) && Mtd.Distance > Deepest)
		{
			Deepest = Mtd.Distance;
			OutAway = Mtd.Direction;
			OutShape = Shape->GetFName();
		}
	};
	if (bBlockersEnabled)
	{
		for (UShapeComponent* Pillar : Blockers)
		{
			Test(Pillar);
		}
	}
	if (!bIgnoresBodies)
	{
		for (const FGiantCrabBodyPart& Part : Parts)
		{
			Test(Part.Shape);
		}
	}
	return Deepest;
}

bool UGiantCrabBodyComponent::IgnoreAnimatedBodies(ACharacter& Character)
{
	UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}
	const FMaskFilter Mask = Capsule->GetMoveIgnoreMask();
	if ((Mask & AIslanderCharacter::AnimatedHitBodyMaskFilter) != 0)
	{
		return false;
	}
	Capsule->SetMoveIgnoreMask(Mask | AIslanderCharacter::AnimatedHitBodyMaskFilter);
	return true;
}

void UGiantCrabBodyComponent::SetBlockersEnabled(bool bEnabled)
{
	if (bBlockersEnabled == bEnabled)
	{
		return;
	}
	bBlockersEnabled = bEnabled;
	for (UShapeComponent* Blocker : Blockers)
	{
		if (IsValid(Blocker))
		{
			Blocker->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
}

EGiantCrabZone UGiantCrabBodyComponent::ResolveZone(const UPrimitiveComponent* Component, FName Bone, const FVector& Point, bool bHasPoint, FName& OutBone) const
{
	// the body struck (a blade sweep, a shot that stopped on it)
	if (Component)
	{
		for (const FGiantCrabBodyPart& Part : Parts)
		{
			if (Part.Shape.Get() == Component)
			{
				OutBone = Part.Bone;
				return Part.Zone;
			}
		}
	}
	// the bone named (a shot that took the bone it passed nearest to)
	if (!Bone.IsNone())
	{
		OutBone = Bone;
		for (const FGiantCrabBodyPart& Part : Parts)
		{
			if (Part.Bone == Bone)
			{
				return Part.Zone;
			}
		}
		return GiantCrabMath::ZoneOfBone(Bone, LegToken);
	}
	// the body nearest the point (a blow on the capsule, an explosion)
	OutBone = NAME_None;
	if (bHasPoint)
	{
		const FGiantCrabBodyPart* Nearest = nullptr;
		float NearestDistance = SnapDistance;
		for (const FGiantCrabBodyPart& Part : Parts)
		{
			FVector Closest;
			const float Distance = Part.Shape ? Part.Shape->GetDistanceToCollision(Point, Closest) : -1.0f;
			if (Distance >= 0.0f && Distance <= NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = &Part;
			}
		}
		if (Nearest)
		{
			OutBone = Nearest->Bone;
			return Nearest->Zone;
		}
	}
	return EGiantCrabZone::Shell;
}

void UGiantCrabBodyComponent::StopBlockingShots()
{
	for (const FGiantCrabBodyPart& Part : Parts)
	{
		if (Part.Shape)
		{
			Part.Shape->SetCollisionResponseToChannel(ShotChannel, ECR_Ignore);
		}
	}
}

const UShapeComponent* UGiantCrabBodyComponent::FindBody(FName Bone) const
{
	for (const FGiantCrabBodyPart& Part : Parts)
	{
		if (Part.Bone == Bone)
		{
			return Part.Shape;
		}
	}
	return nullptr;
}

void UGiantCrabBodyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// the shapes are components of the actor: they go with it
	Parts.Reset();
	Blockers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UGiantCrabBodyComponent::Clear()
{
	for (const FGiantCrabBodyPart& Part : Parts)
	{
		if (IsValid(Part.Shape))
		{
			Part.Shape->DestroyComponent();
		}
	}
	Parts.Reset();
	for (UShapeComponent* Blocker : Blockers)
	{
		if (IsValid(Blocker))
		{
			Blocker->DestroyComponent();
		}
	}
	Blockers.Reset();
}
