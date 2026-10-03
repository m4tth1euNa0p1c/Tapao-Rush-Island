// monke_island

#pragma once

#include "CoreMinimal.h"
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
}
