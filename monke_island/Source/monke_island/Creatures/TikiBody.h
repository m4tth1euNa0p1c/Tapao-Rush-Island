// monke_island

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "TikiBody.generated.h"

/** Part of a tiki body: the whole corpse, the two halves of a body cut in two, or a limb cut off */
UENUM(BlueprintType)
enum class ETikiBodyPart : uint8
{
	Whole,
	/** From the cut bone up (spine_02: chest, arms, head) */
	Upper,
	/** Below the cut bone (pelvis, belly, legs) */
	Lower,
	/** The cut bone and what hangs from it (an arm and its hand, with the weapon) */
	Limb
};

/**
 *  Pure pose rules of the tiki bodies, tested without a world. A pose is the array of bone space transforms of a
 *  skeletal mesh (parents before children, as in its reference skeleton); a part hides the bones it does not keep by
 *  collapsing them (scale near 0) at their joint.
 */
namespace TikiBody
{
	/** Scale of a collapsed bone: small enough to vanish, large enough for the inverse to stay finite */
	inline constexpr double CollapsedScale = 0.001;

	/** Component space transforms of a pose */
	inline TArray<FTransform> ComponentSpace(const TArray<FTransform>& Local, const TArray<int32>& Parents)
	{
		TArray<FTransform> Space;
		Space.SetNum(Local.Num());
		for (int32 Index = 0; Index < Local.Num(); ++Index)
		{
			const int32 Parent = Parents.IsValidIndex(Index) ? Parents[Index] : INDEX_NONE;
			Space[Index] = Parent >= 0 && Parent < Index ? Local[Index] * Space[Parent] : Local[Index];
		}
		return Space;
	}

	/** True when Bone is Root or hangs from it */
	inline bool IsInSubtree(int32 Bone, int32 Root, const TArray<int32>& Parents)
	{
		for (int32 Index = Bone; Index >= 0 && Parents.IsValidIndex(Index); Index = Parents[Index])
		{
			if (Index == Root)
			{
				return true;
			}
		}
		return false;
	}

	/** True when the part keeps this bone (not collapsed); Hidden are the bones cut off before (and their children) */
	inline bool Keeps(ETikiBodyPart Part, int32 Bone, int32 PartBone, const TArray<int32>& Hidden, const TArray<int32>& Parents)
	{
		for (const int32 Cut : Hidden)
		{
			if (Cut >= 0 && IsInSubtree(Bone, Cut, Parents))
			{
				return false;
			}
		}
		const bool bBelowCut = PartBone >= 0 && IsInSubtree(Bone, PartBone, Parents);
		switch (Part)
		{
		case ETikiBodyPart::Upper:
		case ETikiBodyPart::Limb:
			return bBelowCut;
		case ETikiBodyPart::Lower:
			return !bBelowCut;
		default:
			return true;
		}
	}

	/**
	 *  Pose of a part: the kept bones stay where they are in component space, the others collapse at their joint.
	 *  Upper and Limb collapse the root and give the part bone back its place; Lower and the hidden bones collapse
	 *  in place.
	 */
	inline TArray<FTransform> MaskPose(const TArray<FTransform>& Local, const TArray<int32>& Parents, ETikiBodyPart Part,
		int32 PartBone, const TArray<int32>& Hidden)
	{
		TArray<FTransform> Out = Local;
		if (Local.IsEmpty())
		{
			return Out;
		}
		const TArray<FTransform> Space = ComponentSpace(Local, Parents);
		const FVector Collapsed(CollapsedScale);
		const bool bKeepOnlySubtree = (Part == ETikiBodyPart::Upper || Part == ETikiBodyPart::Limb) && Local.IsValidIndex(PartBone) && PartBone > 0;
		if (bKeepOnlySubtree)
		{
			Out[0].SetScale3D(Collapsed);
			const TArray<FTransform> After = ComponentSpace(Out, Parents);
			const int32 Parent = Parents.IsValidIndex(PartBone) ? Parents[PartBone] : INDEX_NONE;
			if (Parent >= 0)
			{
				Out[PartBone] = Space[PartBone].GetRelativeTransform(After[Parent]);
			}
		}
		else if (Part == ETikiBodyPart::Lower && Local.IsValidIndex(PartBone))
		{
			Out[PartBone].SetScale3D(Collapsed);
		}
		for (const int32 Cut : Hidden)
		{
			if (Out.IsValidIndex(Cut))
			{
				Out[Cut].SetScale3D(Collapsed);
			}
		}
		return Out;
	}

	/** Box around the joints the part keeps, in the space of WorldOfComponent (usually world), grown by Margin */
	inline FBox PartBounds(const TArray<FTransform>& Local, const TArray<int32>& Parents, ETikiBodyPart Part, int32 PartBone,
		const TArray<int32>& Hidden, const FTransform& WorldOfComponent, float Margin)
	{
		const TArray<FTransform> Space = ComponentSpace(Local, Parents);
		FBox Box(ForceInit);
		for (int32 Index = 0; Index < Space.Num(); ++Index)
		{
			if (Keeps(Part, Index, PartBone, Hidden, Parents))
			{
				Box += WorldOfComponent.TransformPosition(Space[Index].GetLocation());
			}
		}
		return Box.IsValid ? Box.ExpandBy(Margin) : Box;
	}

	/** Collision of one bone (a body of the physics asset), its shapes relative to the bone */
	struct FBoneGeometry
	{
		int32 Bone = INDEX_NONE;
		FKAggregateGeom Geom;
	};

	/**
	 *  Collision of a frozen pose (T-151): the shapes of the bodies the part keeps, placed by the pose, in the space of
	 *  ComponentToTarget (the corpse root). Tapered capsules become capsules of their larger radius, convex hulls their
	 *  box: the collision hugs the posed mesh as the physics asset does.
	 */
	inline FKAggregateGeom PoseGeometry(const TArray<FBoneGeometry>& Bodies, const TArray<FTransform>& Local, const TArray<int32>& Parents,
		ETikiBodyPart Part, int32 PartBone, const TArray<int32>& Hidden, const FTransform& ComponentToTarget)
	{
		FKAggregateGeom Out;
		const TArray<FTransform> Space = ComponentSpace(Local, Parents);
		for (const FBoneGeometry& Body : Bodies)
		{
			if (!Space.IsValidIndex(Body.Bone) || !Keeps(Part, Body.Bone, PartBone, Hidden, Parents))
			{
				continue;
			}
			FTransform BoneTM = Space[Body.Bone] * ComponentToTarget;
			const float Scale = static_cast<float>(BoneTM.GetMaximumAxisScale());
			BoneTM.SetScale3D(FVector::OneVector);
			for (FKSphereElem Elem : Body.Geom.SphereElems)
			{
				Elem.Center = BoneTM.TransformPosition(Elem.Center * Scale);
				Elem.Radius *= Scale;
				Out.SphereElems.Add(Elem);
			}
			for (FKSphylElem Elem : Body.Geom.SphylElems)
			{
				FTransform ElemTM = Elem.GetTransform();
				ElemTM.ScaleTranslation(Scale);
				Elem.SetTransform(ElemTM * BoneTM);
				Elem.Radius *= Scale;
				Elem.Length *= Scale;
				Out.SphylElems.Add(Elem);
			}
			for (FKBoxElem Elem : Body.Geom.BoxElems)
			{
				FTransform ElemTM = Elem.GetTransform();
				ElemTM.ScaleTranslation(Scale);
				Elem.SetTransform(ElemTM * BoneTM);
				Elem.X *= Scale;
				Elem.Y *= Scale;
				Elem.Z *= Scale;
				Out.BoxElems.Add(Elem);
			}
			for (const FKTaperedCapsuleElem& Tapered : Body.Geom.TaperedCapsuleElems)
			{
				FKSphylElem Elem(FMath::Max(Tapered.Radius0, Tapered.Radius1) * Scale, Tapered.Length * Scale);
				FTransform ElemTM = Tapered.GetTransform();
				ElemTM.ScaleTranslation(Scale);
				Elem.SetTransform(ElemTM * BoneTM);
				Out.SphylElems.Add(Elem);
			}
			for (const FKConvexElem& Convex : Body.Geom.ConvexElems)
			{
				if (!Convex.ElemBox.IsValid)
				{
					continue;
				}
				const FVector Size = Convex.ElemBox.GetSize() * Scale;
				FKBoxElem Elem(static_cast<float>(Size.X), static_cast<float>(Size.Y), static_cast<float>(Size.Z));
				FTransform ElemTM = Convex.GetTransform();
				ElemTM.ScaleTranslation(Scale);
				Elem.SetTransform(FTransform(Convex.ElemBox.GetCenter() * Scale) * ElemTM * BoneTM);
				Out.BoxElems.Add(Elem);
			}
		}
		return Out;
	}

	/** Lowest point of a collision placed by ToWorld: its height, under the middle of the shape that reaches lowest;
	 *  false when it has no shape */
	inline bool LowestPoint(const FKAggregateGeom& Geom, const FTransform& ToWorld, FVector& OutPoint)
	{
		FBox Lowest(ForceInit);
		auto Consider = [&Lowest](const FBox& Box)
		{
			if (Box.IsValid && (!Lowest.IsValid || Box.Min.Z < Lowest.Min.Z))
			{
				Lowest = Box;
			}
		};
		for (const FKSphereElem& Elem : Geom.SphereElems)
		{
			Consider(Elem.CalcAABB(ToWorld, 1.0f));
		}
		for (const FKSphylElem& Elem : Geom.SphylElems)
		{
			Consider(Elem.CalcAABB(ToWorld, 1.0f));
		}
		for (const FKBoxElem& Elem : Geom.BoxElems)
		{
			Consider(Elem.CalcAABB(ToWorld, 1.0f));
		}
		if (!Lowest.IsValid)
		{
			return false;
		}
		OutPoint = FVector(Lowest.GetCenter().X, Lowest.GetCenter().Y, Lowest.Min.Z);
		return true;
	}
}
