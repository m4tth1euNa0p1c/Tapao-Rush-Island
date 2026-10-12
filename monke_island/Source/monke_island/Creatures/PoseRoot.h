// monke_island

#pragma once

#include "CoreMinimal.h"
#include "BonePose.h"

/** Pure fix of the root bone of an evaluated pose, shared by the anim instances of the beans and the tikis */
namespace PoseRoot
{
	/**
	 *  Puts the scale of the mesh reference pose back on the root bone. The clips of the maquettes carry the Blender
	 *  armature scale (100) on the root bone unevenly: the library clips keep it, the GoreBox clips of 2026-10-08 come
	 *  with a root at 1, and a root at 1 on a mesh whose reference root is at 100 draws the body a hundred times too
	 *  small (owner 2026-10-08: soldiers and mobs vanishing up close). Only the scale is taken from the reference: the
	 *  root translation and rotation stay those of the clip (root motion, turns).
	 */
	inline void KeepReferenceScale(FCompactPose& Pose)
	{
		if (Pose.GetNumBones() <= 0)
		{
			return;
		}
		const FCompactPoseBoneIndex Root(0);
		Pose[Root].SetScale3D(Pose.GetRefPose(Root).GetScale3D());
	}
}
