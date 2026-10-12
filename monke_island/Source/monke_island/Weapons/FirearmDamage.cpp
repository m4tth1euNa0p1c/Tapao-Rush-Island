// monke_island

#include "Weapons/FirearmDamage.h"
#include "Creatures/GiantCrabBody.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

FName FirearmDamage::FindStruckBone(const USkeletalMeshComponent* Mesh, const FVector& RayStart, const FVector& RayEnd, float MaxDistance)
{
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Asset)
	{
		return NAME_None;
	}
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	const int32 Count = FMath::Min(Skeleton.GetNum(), Mesh->GetNumComponentSpaceTransforms());
	TArray<FVector, TInlineAllocator<96>> Joints;
	Joints.Reserve(Count);
	const FTransform& ToWorld = Mesh->GetComponentTransform();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Joints.Add(Mesh->GetBoneTransform(Index, ToWorld).GetLocation());
	}
	TArray<FVector> Heads;
	TArray<FVector> Tails;
	Heads.Reserve(Count);
	Tails.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Parent = Skeleton.GetParentIndex(Index);
		Tails.Add(Joints[Index]);
		Heads.Add(Joints.IsValidIndex(Parent) ? Joints[Parent] : Joints[Index]);
	}
	const int32 Bone = NearestBone(Tails, Heads, RayStart, RayEnd, MaxDistance);
	return Bone == INDEX_NONE ? NAME_None : Skeleton.GetBoneName(Bone);
}

bool FirearmDamage::TraceHitBodies(const UWorld* World, const FVector& Start, const FVector& End, const FCollisionQueryParams& Params, FHitResult& InOutHit, bool bHadHit)
{
	FHitResult BodyHit;
	// the fixed shapes that stop the pawns around a giant crab in a network session are no bodies: the look goes through
	// them to the legs and claws behind (T-305)
	FCollisionQueryParams BodyParams(Params);
	BodyParams.IgnoreMask |= UGiantCrabBodyComponent::PawnBlockerMaskFilter;
	if (!World || !World->LineTraceSingleByChannel(BodyHit, Start, End, ECC_Pawn, BodyParams))
	{
		return bHadHit;
	}
	const UPrimitiveComponent* Body = BodyHit.GetComponent();
	if (!Body || !Body->ComponentHasTag(HitBodyTag) || (bHadHit && InOutHit.Distance <= BodyHit.Distance))
	{
		return bHadHit;
	}
	InOutHit = BodyHit;
	return true;
}
