// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/PirateMusketRules.h"
#include "RifleRules.generated.h"

/**
 *  Tunable rules of the bean's rifled carbine (DA_RifleRules, BP_Rifle on APirateMusketWeapon; owner 2026-10-09, pack
 *  bean-rifle-anim: the 24 gestures of the musket at 24 fps). No weapon class of its own: the musket's gameplay with
 *  other numbers, a precise long shot (90 damage, 160 m, 0.2 deg aimed) paid by a longer reload (the rifling grips
 *  the patched ball). One cartridge (EAmmoKind::RifleCartridge) per shot.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API URifleRules : public UPirateMusketRules
{
	GENERATED_BODY()

public:

	URifleRules();
};
