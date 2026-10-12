// monke_island

#include "Creatures/TikiDismember.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"

TArray<int32> UTikiDismemberSet::AnchorsOn(const FReferenceSkeleton& Skeleton) const
{
	TArray<int32> Anchors;
	Anchors.Reserve(Modules.Num());
	for (const FTikiDismemberModule& Module : Modules)
	{
		Anchors.Add(Module.Mesh && !Module.Bone.IsNone() ? Skeleton.FindBoneIndex(Module.Bone) : INDEX_NONE);
	}
	return Anchors;
}

bool UTikiDismemberSet::Select(const FReferenceSkeleton& Skeleton, ETikiBodyPart Part, FName PartBone, const TArray<FName>& CutBones, TArray<bool>& OutShown) const
{
	TArray<int32> Parents;
	Parents.Reserve(Skeleton.GetNum());
	for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
	{
		Parents.Add(Skeleton.GetParentIndex(Index));
	}
	TArray<int32> Hidden;
	for (const FName& Cut : CutBones)
	{
		Hidden.Add(Skeleton.FindBoneIndex(Cut));
	}
	const int32 PartIndex = PartBone.IsNone() ? INDEX_NONE : Skeleton.FindBoneIndex(PartBone);
	return TikiDismember::SelectModules(AnchorsOn(Skeleton), Parents, Part, PartIndex, Hidden, OutShown);
}

bool TikiDismember::MapPose(const FReferenceSkeleton& Body, const FReferenceSkeleton& Module, const TArray<FTransform>& BodyPose, TArray<FTransform>& OutPose)
{
	OutPose.Reset();
	if (BodyPose.Num() != Body.GetNum())
	{
		return false;
	}
	OutPose.SetNum(Module.GetNum());
	for (int32 Index = 0; Index < Module.GetNum(); ++Index)
	{
		const int32 BodyIndex = Body.FindBoneIndex(Module.GetBoneName(Index));
		const int32 Parent = Module.GetParentIndex(Index);
		const int32 BodyParent = BodyIndex == INDEX_NONE ? INDEX_NONE : Body.GetParentIndex(BodyIndex);
		// the bone space transforms only carry over when both bones hang from the same parent
		const bool bSameParent = Parent == INDEX_NONE ? BodyParent == INDEX_NONE
			: BodyParent != INDEX_NONE && Body.GetBoneName(BodyParent) == Module.GetBoneName(Parent);
		if (BodyIndex == INDEX_NONE || !bSameParent)
		{
			OutPose.Reset();
			return false;
		}
		OutPose[Index] = BodyPose[BodyIndex];
	}
	return true;
}

void TikiDismember::ApplyMaterials(USkinnedMeshComponent* Module, const USkeletalMesh* BodyMesh, const TArray<UMaterialInterface*>& BodyMaterials)
{
	const USkeletalMesh* ModuleMesh = Module ? Cast<USkeletalMesh>(Module->GetSkinnedAsset()) : nullptr;
	if (!ModuleMesh || !BodyMesh)
	{
		return;
	}
	const TArray<FSkeletalMaterial>& Slots = ModuleMesh->GetMaterials();
	const TArray<FSkeletalMaterial>& BodySlots = BodyMesh->GetMaterials();
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
	{
		for (int32 BodySlot = 0; BodySlot < BodySlots.Num(); ++BodySlot)
		{
			// same slot name, or the same material asset (the import gives the outer slots the body materials)
			const bool bSame = (!Slots[Slot].MaterialSlotName.IsNone() && Slots[Slot].MaterialSlotName == BodySlots[BodySlot].MaterialSlotName)
				|| (Slots[Slot].MaterialInterface && Slots[Slot].MaterialInterface == BodySlots[BodySlot].MaterialInterface);
			if (bSame)
			{
				if (BodyMaterials.IsValidIndex(BodySlot) && BodyMaterials[BodySlot])
				{
					Module->SetMaterial(Slot, BodyMaterials[BodySlot]);
				}
				break;
			}
		}
	}
}
