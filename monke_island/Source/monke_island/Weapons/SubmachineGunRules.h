// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/MachineGunRules.h"
#include "SubmachineGunRules.generated.h"

/**
 *  Tunable rules of the bean's submachine gun with its drum (DA_MitrailletteRules, BP_Mitraillette on AMachineGunWeapon;
 *  owner 2026-10-11: "mitraillette", T-335; model and clips T-334). No weapon class of its own: the machine gun's
 *  trigger held, its server cadence, its drum (the "belt": BeltCapacity), its reload, recoil and bloom, with the numbers
 *  of a gun held in the hands: no crank to spin up (MinCadenceRpm = MaxCadenceRpm, about 11 rounds a second), no heat,
 *  no slowdown, pistol cartridges (EAmmoKind::SmgCartridge). The FireLoop clip loops while it fires (RoundsPerCrankTurn
 *  rounds per loop of the clip); a drum not yet empty is swapped by Reload (MagOut, MagIn: the rounds count, Charge), an
 *  empty one by ReloadEmpty (the drum let fall at MagDrop, DroppedBoxMesh), both with the machine gun's logic.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API USubmachineGunRules : public UMachineGunRules
{
	GENERATED_BODY()

public:

	USubmachineGunRules();
};
