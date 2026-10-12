// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

class USkeletalMeshComponent;
class UWorld;
struct FCollisionQueryParams;

/** Projectiles of one shot that struck the same bone of a target (T-167): their damage added up */
struct FFirearmBoneHit
{
	/** Bone struck (NAME_None: a target without bones, or none found) */
	FName Bone;

	/** Damage of these projectiles, after the distance falloff */
	float Damage = 0.0f;

	/** Projectiles that struck this bone */
	int32 Pellets = 0;

	/** Where the first of them entered (world) */
	FVector Impact = FVector::ZeroVector;
};

/**
 *  A shot of a pirate firearm on one target (T-167). The point damage part carries the whole of the shot on this
 *  target (one blow: one reaction, one death), HitInfo is the hit of the strongest bone group (BoneName set); on top
 *  of it the hits grouped by bone, the entry point of every projectile (blood) and what the weapon asks of the limbs.
 *  A target that does not know it reads a plain FPointDamageEvent.
 */
struct FFirearmDamageEvent : public FPointDamageEvent
{
	/** Hits grouped by bone, in the order the bones were first struck */
	TArray<FFirearmBoneHit> BoneHits;

	/** Entry point of every projectile that struck the target (world) */
	TArray<FVector> Impacts;

	/** Projectiles of the shot (struck or not) */
	int32 PelletsFired = 1;

	/** Damage a limb takes from these projectiles, relative to their damage (thin limbs tear more easily) */
	float LimbDamageScale = 1.0f;

	/** Limbs a lethal shot shreds off at close range (0: the rules of the target) */
	int32 ShredLimbs = 0;

	/** A tiki killed by this shot from at most this distance bursts (cm, 0: the rules of the target) */
	float CloseBurstDistance = 0.0f;

	/** ID of this event among the damage events (unique) */
	static const int32 ClassID = 1670;

	virtual int32 GetTypeID() const override { return FFirearmDamageEvent::ClassID; }
	virtual bool IsOfType(int32 InID) const override { return InID == FFirearmDamageEvent::ClassID || FPointDamageEvent::IsOfType(InID); }
};

/** Pure rules of the ballistics of the pirate firearms (T-167), tested without a world */
namespace FirearmDamage
{
	/** Share of the damage left at Distance on a piecewise linear curve of (distance cm, share) points sorted by
	 *  distance: flat before the first point and after the last; an empty curve keeps the whole damage */
	inline float ScaleAtDistance(const TArray<FVector2D>& Curve, float Distance)
	{
		if (Curve.IsEmpty())
		{
			return 1.0f;
		}
		if (Distance <= Curve[0].X)
		{
			return FMath::Max(0.0f, static_cast<float>(Curve[0].Y));
		}
		for (int32 Index = 1; Index < Curve.Num(); ++Index)
		{
			const FVector2D& A = Curve[Index - 1];
			const FVector2D& B = Curve[Index];
			if (Distance <= B.X)
			{
				const double Span = B.X - A.X;
				const double Alpha = Span > UE_KINDA_SMALL_NUMBER ? (Distance - A.X) / Span : 1.0;
				return FMath::Max(0.0f, static_cast<float>(FMath::Lerp(A.Y, B.Y, Alpha)));
			}
		}
		return FMath::Max(0.0f, static_cast<float>(Curve.Last().Y));
	}

	/** Adds one projectile to the group of its bone (a new group for a bone not struck yet) */
	inline void AddPellet(TArray<FFirearmBoneHit>& Groups, FName Bone, float Damage, const FVector& Impact)
	{
		FFirearmBoneHit* Group = Groups.FindByPredicate([Bone](const FFirearmBoneHit& Item) { return Item.Bone == Bone; });
		if (!Group)
		{
			Group = &Groups.AddDefaulted_GetRef();
			Group->Bone = Bone;
			Group->Impact = Impact;
		}
		Group->Damage += Damage;
		++Group->Pellets;
	}

	/** Index of the strongest group (most damage), INDEX_NONE when there is none */
	inline int32 StrongestGroup(const TArray<FFirearmBoneHit>& Groups)
	{
		int32 Best = INDEX_NONE;
		for (int32 Index = 0; Index < Groups.Num(); ++Index)
		{
			if (Best == INDEX_NONE || Groups[Index].Damage > Groups[Best].Damage)
			{
				Best = Index;
			}
		}
		return Best;
	}

	/** Bone whose segment (its parent joint to its own joint) passes nearest to the ray RayStart -> RayEnd; Joints and
	 *  ParentJoints by bone index (a root bone: its parent joint is its own joint). INDEX_NONE when there is no bone
	 *  or the nearest one is farther than MaxDistance (cm, 0: no limit) */
	inline int32 NearestBone(const TArray<FVector>& Joints, const TArray<FVector>& ParentJoints, const FVector& RayStart, const FVector& RayEnd, float MaxDistance)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = MaxDistance > 0.0f ? FMath::Square(static_cast<double>(MaxDistance)) : TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Joints.Num() && Index < ParentJoints.Num(); ++Index)
		{
			FVector OnBone;
			FVector OnRay;
			FMath::SegmentDistToSegmentSafe(ParentJoints[Index], Joints[Index], RayStart, RayEnd, OnBone, OnRay);
			const double DistSq = FVector::DistSquared(OnBone, OnRay);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Index;
			}
		}
		return Best;
	}

	/** Bone of a skeletal mesh a ray passes nearest to (its posed bones in the world), NAME_None when none within
	 *  MaxDistance (cm, 0: no limit). For bodies whose mesh does not collide (the capsule takes the trace) */
	MONKE_ISLAND_API FName FindStruckBone(const USkeletalMeshComponent* Mesh, const FVector& RayStart, const FVector& RayEnd, float MaxDistance);

	/** Tag of the hit bodies a shot must find although they do not block the weapons' trace channel: the giant crab's
	 *  legs and claws block the pawns but not the camera channel, or the camera would jump against them (T-261) */
	inline const FName HitBodyTag = TEXT("WeaponHitBody");

	/** Second look along a shot, on the Pawn channel: when its first blocking hit is a body tagged HitBodyTag nearer than
	 *  the shot's own hit (or the shot hit nothing), that body becomes the hit. Returns whether there is a hit */
	MONKE_ISLAND_API bool TraceHitBodies(const UWorld* World, const FVector& Start, const FVector& End, const FCollisionQueryParams& Params, FHitResult& InOutHit, bool bHadHit);
}
