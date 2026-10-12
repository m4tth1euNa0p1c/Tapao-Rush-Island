// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/PirateMusketRules.h"
#include "BlunderbussRules.generated.h"

/**
 *  Tunable rules of the bean's blunderbuss (DA_BlunderbussRules; owner 2026-10-09, pack bean-blunderbuss-anim: the 24
 *  gestures of the musket at 24 fps). A musket whose charge throws PelletCount pellets in a wide cone: devastating at
 *  a few metres (the pellets of one target add up into one blow, the tiki bursts at close range as with the shotgun),
 *  scratches beyond. One charge of shot (EAmmoKind::Shot) per shot, the musket's reload butt on the ground after each.
 *  MaxBalls counts the charges of shot, the one in the barrel included.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBlunderbussRules : public UPirateMusketRules
{
	GENERATED_BODY()

public:

	UBlunderbussRules();

	/** Pellets of a charge, each dealing Damage (falling off with the distance, DamageFalloff) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="1", ClampMax="32"))
	int32 PelletCount = 10;

	/** A tiki killed from at most this distance bursts (cm, combat reactions T-150); 0 = DA_TikiRules only */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", Units="cm"))
	float CloseBurstDistanceCm = 400.0f;

	/** Limbs a lethal shot from at most CloseBurstDistanceCm shreds off a tiki (T-167), drawn between these */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="5"))
	int32 CloseShredLimbsMin = 2;

	/** See CloseShredLimbsMin */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="5"))
	int32 CloseShredLimbsMax = 3;
};
