// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"
#include "ShotgunRules.generated.h"

/**
 *  Tunable rules of the bean's pirate pump shotgun (DA_ShotgunRules; second PC: bean-pirate-shotgun). A cone of
 *  pellets per shot, the pump after each shot (spent shell ejected), a reload shell by shell (ReloadStart,
 *  ReloadInsert x N, ReloadEnd) that a shot interrupts, and ReloadEmpty (three shells then the pump) on an empty gun.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UShotgunRules : public UFirearmRules
{
	GENERATED_BODY()

public:

	UShotgunRules();

	/** Shells the gun holds (tube and chamber) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1"))
	int32 Capacity = 4;

	/** Shells a player carries, those in the gun included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 MaxShells = 15;

	/** Shells in the gun at spawn (taken from MaxShells, at most Capacity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 StartLoadedShells = 4;

	/** Pellets of a shot, each dealing Damage (falling off with the distance) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="1", ClampMax="32"))
	int32 PelletCount = 8;

	/** Pressing fire with an empty gun starts the reload (when a shell is left) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadOnEmptyFire = true;

	/** Event of ReloadInsert / ReloadEmpty where a shell enters the tube */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName InsertEvent = TEXT("InsertSlug");

	/** After ReloadEmpty (three shells then the pump), a gun with room left and shells in the pouch goes on shell by shell
	 *  (ReloadStart, ReloadInsert, ReloadEnd) until full (T-335); off: the reload ends with ReloadEmpty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bTopUpAfterEmptyReload = true;

	/** An empty gun with fewer shells to load than ReloadEmpty shows goes in shell by shell, so that the clip never shows
	 *  shells the pouch does not have (T-335); off: ReloadEmpty whatever the pouch holds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bShellByShellWhenShort = true;

	/** Event of the shot clips where the spent shell leaves the gun */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FName EjectEvent = TEXT("EjectSpentShell");

	/** A tiki killed by the shotgun from at most this distance bursts (cm, combat reactions T-150); 0 = DA_TikiRules only */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", Units="cm"))
	float CloseBurstDistanceCm = 500.0f;

	/** Limbs a lethal shot from at most CloseBurstDistanceCm shreds off a tiki (T-167), drawn between these; the
	 *  limbs its pellets tore count. 0 = the burst of DA_TikiRules only */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="5"))
	int32 CloseShredLimbsMin = 2;

	/** See CloseShredLimbsMin */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="5"))
	int32 CloseShredLimbsMax = 3;

	/** Spent shell thrown out at EjectEvent (SM_Shotgun_Spent); none = no shell */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UStaticMesh> SpentShellMesh;

	/** Speed of the spent shell, relative to the carrier's view (X forward, Y right, Z up; cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FVector SpentShellVelocity = FVector(-20.0f, 220.0f, 160.0f);

	/** Time a spent shell stays before it disappears (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0.1", Units="s"))
	float SpentShellSeconds = 3.0f;
};
