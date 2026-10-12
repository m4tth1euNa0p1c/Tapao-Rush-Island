// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/PirateMusketWeapon.h"
#include "BlunderbussWeapon.generated.h"

class UBlunderbussRules;

/**
 *  Bean's blunderbuss (BP_Blunderbuss, DA_BlunderbussRules; owner 2026-10-09, pack bean-blunderbuss-anim). The
 *  musket's gestures and ammunition (one charge in the barrel, the reload butt on the ground), but the charge throws
 *  PelletCount pellets in a wide cone: each pellet its own trace, distance, damage and bone, the hits of one target
 *  added up into one FFirearmDamageEvent grouped by bone (T-167), a lethal shot at close range shredding limbs.
 */
UCLASS()
class MONKE_ISLAND_API ABlunderbussWeapon : public APirateMusketWeapon
{
	GENERATED_BODY()

public:

	ABlunderbussWeapon();

	/** A tiki killed from at most this distance bursts (cm, from DA_BlunderbussRules; 0 = none) */
	UFUNCTION(BlueprintPure, Category="Blunderbuss")
	float GetCloseBurstDistance() const;

protected:

	virtual void BeginPlay() override;
	virtual int32 GetProjectilesPerShot() const override;
	virtual void DescribeShot(FFirearmDamageEvent& Shot, float Distance) override;

private:

	/** Rules of the blunderbuss (Rules cast) */
	const UBlunderbussRules* GetBlunderbussRules() const;
};
