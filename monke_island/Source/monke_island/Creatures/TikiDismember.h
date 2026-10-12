// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Creatures/TikiBody.h"
#include "ReferenceSkeleton.h"
#include "TikiDismember.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class USkinnedMeshComponent;

/** One closed module of a tiki body (T-155): a part of the mesh (head, torso, pelvis, an arm, a leg) with its cut
 *  faces modelled (flesh, bone), skinned to the skeleton of the creature */
USTRUCT(BlueprintType)
struct FTikiDismemberModule
{
	GENERATED_BODY()

	/** Name of the part (Head, Torso, Pelvis, Arm_L, Arm_R, Leg_L, Leg_R), for the logs and the scripts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	FName Part;

	/** First bone of the part: the module shows while this bone is kept, a cut at this bone takes it off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	FName Bone;

	/** Module mesh, on the skeleton of the creature (SK_<Mob>_<Part>, Characters/<Mob>/Dismember) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	TObjectPtr<USkeletalMesh> Mesh;
};

/**
 *  Closed modules of a tiki body (DA_<Mob>_DismemberSet, T-155). Once a creature loses a part, its body and its
 *  pieces are drawn with these modules instead of the whole mesh: every cut shows a closed face of flesh and bone,
 *  on the stump and on the piece. The set is used only when it is complete (every module has a mesh and a bone of
 *  the skeleton) and every cut bone starts a module; otherwise the whole mesh with its bones hidden is kept (T-072).
 *  Filled by scripts/character/set_dismember_sets.py after scripts/character/import_dismember_parts.py.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTikiDismemberSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** The modules: together they make the whole body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	TArray<FTikiDismemberModule> Modules;

	/** Bone indices of the module bones on a skeleton, INDEX_NONE for a module without mesh or with an unknown bone */
	TArray<int32> AnchorsOn(const FReferenceSkeleton& Skeleton) const;

	/** Modules to draw for a part of a body of this skeleton; false when the set cannot draw it (then the whole mesh
	 *  with hidden bones is used) */
	bool Select(const FReferenceSkeleton& Skeleton, ETikiBodyPart Part, FName PartBone, const TArray<FName>& CutBones, TArray<bool>& OutShown) const;
};

/** Pure rules of the dismemberment modules, tested without a world */
namespace TikiDismember
{
	/**
	 *  Modules shown for a part. Anchors: the first bone of each module (INDEX_NONE: module missing). Returns false
	 *  (fall back on the whole mesh) when a module is missing, when there is none, or when a cut bone (the part bone
	 *  of a piece, a bone cut before) does not start a module: the modules could not close that cut.
	 */
	inline bool SelectModules(const TArray<int32>& Anchors, const TArray<int32>& Parents, ETikiBodyPart Part, int32 PartBone,
		const TArray<int32>& Hidden, TArray<bool>& OutShown)
	{
		OutShown.Reset();
		if (Anchors.IsEmpty() || Anchors.Contains(INDEX_NONE))
		{
			return false;
		}
		if (Part != ETikiBodyPart::Whole && !Anchors.Contains(PartBone))
		{
			return false;
		}
		for (const int32 Cut : Hidden)
		{
			if (!Anchors.Contains(Cut))
			{
				return false;
			}
		}
		bool bAny = false;
		for (const int32 Anchor : Anchors)
		{
			const bool bShown = TikiBody::Keeps(Part, Anchor, PartBone, Hidden, Parents);
			OutShown.Add(bShown);
			bAny |= bShown;
		}
		return bAny;
	}

	/** Pose of a module from the pose of the whole body, bone by name: false when a module bone or its parent does
	 *  not match the body skeleton */
	bool MapPose(const FReferenceSkeleton& Body, const FReferenceSkeleton& Module, const TArray<FTransform>& BodyPose, TArray<FTransform>& OutPose);

	/** Materials of a module: the slots that use a material of the body mesh take the material the body shows in
	 *  that slot (BodyMaterials, by slot of the body mesh: a variant or an override), the cut faces keep their own */
	void ApplyMaterials(USkinnedMeshComponent* Module, const USkeletalMesh* BodyMesh, const TArray<UMaterialInterface*>& BodyMaterials);
}
