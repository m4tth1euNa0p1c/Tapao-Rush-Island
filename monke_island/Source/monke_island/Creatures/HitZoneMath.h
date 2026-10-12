// monke_island

#pragma once

#include "CoreMinimal.h"

/** Zones of a body a blow lands in, from the bone it struck (tested by monke_island.Creatures.HitZone) */
namespace HitZones
{
	/** Head, torso, left / right arm, left / right leg; None when the bone tells nothing */
	enum EZone : uint8
	{
		Head,
		Torso,
		ArmL,
		ArmR,
		LegL,
		LegR,
		Count,
		None = 255
	};

	/** Zone of a bone of the tiki and bean skeletons (head, crest, hat, jaw, spine_*, pelvis, upperarm/lowerarm/hand_l/_r,
	 *  weapon_r, thigh/calf/foot_l/_r, coattail_*), None for an unknown or empty bone */
	inline uint8 ZoneOfBone(FName Bone)
	{
		if (Bone.IsNone())
		{
			return None;
		}
		const FString Name = Bone.ToString().ToLower();
		if (Name.Contains(TEXT("head")) || Name.Contains(TEXT("neck")) || Name.Contains(TEXT("crest")) || Name.Contains(TEXT("hat"))
			|| Name.Contains(TEXT("jaw")))
		{
			return Head;
		}
		const bool bLeft = Name.EndsWith(TEXT("_l"));
		if (Name.Contains(TEXT("thigh")) || Name.Contains(TEXT("calf")) || Name.Contains(TEXT("foot")) || Name.Contains(TEXT("leg")))
		{
			return bLeft ? LegL : LegR;
		}
		if (Name.Contains(TEXT("weapon")))
		{
			return bLeft ? ArmL : ArmR;
		}
		if (Name.Contains(TEXT("arm")) || Name.Contains(TEXT("hand")) || Name.Contains(TEXT("clavicle")))
		{
			return bLeft ? ArmL : ArmR;
		}
		if (Name.Contains(TEXT("spine")) || Name.Contains(TEXT("pelvis")) || Name.Contains(TEXT("coattail")) || Name == TEXT("root"))
		{
			return Torso;
		}
		return None;
	}
}
