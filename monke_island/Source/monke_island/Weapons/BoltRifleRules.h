// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"
#include "BoltRifleRules.generated.h"

class UStaticMesh;

/**
 *  Tunable rules of the bean's bolt-action marksman rifle with its scope (DA_BoltRifleRules, BP_BoltRifle; owner
 *  2026-10-11: "sniper verrou", T-335; model and clips T-334). Five rounds in the rifle (Capacity), rifle cartridges
 *  (EAmmoKind::RifleCartridge) in the pouch; after each shot the bolt is worked (BoltCycle: BoltOpen, CaseEject,
 *  BoltClose) and no shot leaves before BoltClose; the stripper clip reload (Reload, rounds in at MagIn). The scope is
 *  the exception to "no aiming" (T-254): the right click raises it (AimIn, AimIdle, AimOut), the view narrows to
 *  AimFieldOfView with the eyepiece (AimSightSocket) on the view axis, then the weapon hides behind it. A heavy, far,
 *  precise shot when steady through the scope; wide on the move.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBoltRifleRules : public UFirearmRules
{
	GENERATED_BODY()

public:

	UBoltRifleRules();

	/** Rounds the rifle holds, the chambered one included (one stripper clip) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1", ClampMax="20"))
	int32 Capacity = 5;

	/** Rounds a player carries for it, those in the rifle included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1"))
	int32 MaxRounds = 25;

	/** Rounds in the rifle at spawn, the first one chambered (at most Capacity, taken from MaxRounds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 StartLoadedRounds = 5;

	/** The bolt is worked by itself once the shot clip played (BoltCycle); off: the next press on fire works it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt")
	bool bAutoCycleBolt = true;

	/** Event of the BoltCycle and Reload clips where the bolt closes on a round: chambered, the next press fires (none in
	 *  the clip: at its end) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt")
	FName BoltCloseEvent = TEXT("BoltClose");

	/** Event of the clips where the spent case leaves the rifle (cosmetic: SpentCaseMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt")
	FName CaseEjectEvent = TEXT("CaseEject");

	/** Working the bolt lowers the scope: unscoped meanwhile, raised again while the right click is still held */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt")
	bool bBoltCycleLowersScope = true;

	/** Length of the bolt cycle (s; the BoltCycle clip stretched to it, 0 = the clip's own length) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt", meta=(ClampMin="0", Units="s"))
	float BoltCycleSeconds = 0.0f;

	/** Pressing fire on an empty rifle reloads it (rounds in the pouch) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadOnEmptyFire = true;

	/** The last round of the rifle fired, the reload follows by itself (rounds in the pouch) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadAfterLastShot = true;

	/** Event of the Reload clip where the rounds are pressed from the stripper clip into the magazine: they count from
	 *  there (none in the clip: at its end); a reload cut before it takes nothing from the pouch */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName ReloadLoadedEvent = TEXT("RoundsIn");

	/** Event of the Reload clip where the empty stripper clip is flicked away (cosmetic: EmptyClipMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName ClipEjectEvent = TEXT("ClipEject");

	/** Length of the reload (s; the Reload clip stretched to it, 0 = the clip's own length) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload", meta=(ClampMin="0", Units="s"))
	float ReloadSeconds = 0.0f;

	/** Spread of a steady shot without the scope (deg; steady through the scope: AimSpreadDegrees; on the move:
	 *  HipSpreadDegrees, scope or not) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float NoScopeSpreadDegrees = 1.5f;

	/** Spent case thrown out at CaseEjectEvent (the pack's case; none = no case) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UStaticMesh> SpentCaseMesh;

	/** Weapon bone or socket the case leaves from (the ejection port; the weapon's origin without it) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FName EjectSocket = TEXT("socket_eject");

	/** Speed of the case, relative to the carrier's view (X forward, Y right, Z up; cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FVector SpentCaseVelocity = FVector(-10.0f, 210.0f, 150.0f);

	/** Time a spent case stays before it disappears (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0.1", Units="s"))
	float SpentCaseSeconds = 3.0f;

	/** Empty stripper clip flicked away at ClipEjectEvent (the pack's SM_BoltRifle_EmptyClip; none = nothing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UStaticMesh> EmptyClipMesh;

	/** Weapon bone or socket the empty clip leaves from (the clip guide; EjectSocket without it) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FName ClipSocket = TEXT("socket_clip");

	/** Speed of the empty clip, relative to the carrier's view (X forward, Y right, Z up; cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FVector EmptyClipVelocity = FVector(10.0f, 120.0f, 220.0f);
};
