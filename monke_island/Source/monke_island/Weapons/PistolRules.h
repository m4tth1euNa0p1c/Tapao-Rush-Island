// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"
#include "PistolRules.generated.h"

/**
 *  Tunable rules of the bean's pirate pistol (DA_PistolRules; second PC: bean-pirate-pistol). One hand, one charge:
 *  a shot empties the barrel and drops the hammer; the reload (paper cartridge, ramrod) loads and cocks it again;
 *  a loaded pistol with the hammer down must be cocked (Cock) before it fires.
 *  Double pistol (owner 2026-10-09, pack bean-pirate-pistol-double, UDoublePistolRules): Barrels = 2, two shots
 *  before the reload, the second one from SecondMuzzleSocket; the reload fills both barrels.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UPistolRules : public UFirearmRules
{
	GENERATED_BODY()

public:

	UPistolRules();

	/** Charges a player carries, those in the barrels included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 MaxCharges = 15;

	/** Barrels, each holding one charge (1; 2 for the double pistol: two shots, then the reload fills both) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1", ClampMax="4"))
	int32 Barrels = 1;

	/** The pistol starts loaded (those charges count in MaxCharges) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	bool bStartLoaded = true;

	/** The pistol starts with the hammer cocked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	bool bStartCocked = true;

	/** Pressing fire with an empty barrel starts the reload (when a charge is left) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadOnEmptyFire = true;

	/** Fire on a loaded pistol with the hammer down cocks it and shoots at once (no separate Cock gesture first) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoCockOnFire = true;

	/** Event of the Cock clip where the hammer is cocked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName CockEvent = TEXT("CockHammer");

	/** Event of the Reload clip where the charge is in and the hammer cocked; the rest of the clip is the way back */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName ReloadLoadedEvent = TEXT("HammerCocked");

	/** Weapon socket (bone) of the second muzzle of a double pistol: flame and smoke of its second shot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FName SecondMuzzleSocket = TEXT("socket_muzzle2");
};

/**
 *  Rules of the double barrelled pirate pistol (DA_DoublePistolRules, BP_DoublePistol on APistolWeapon; owner
 *  2026-10-09, pack bean-pirate-pistol-double: the 14 gestures of the pistol at 30 fps, sockets muzzle and muzzle2).
 *  Same gameplay as the pistol with two barrels: Fire then AimFire / Fire shoot the second barrel without a reload.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UDoublePistolRules : public UPistolRules
{
	GENERATED_BODY()

public:

	UDoublePistolRules();
};
