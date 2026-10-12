// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapons/AmmoTypes.h"
#include "AmmoDefinition.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 *  A packet of ammunition lying on the ground or sold by the merchant (DA_Ammo_*; owner 2026-10-09: paper cartridges,
 *  cartridge box, powder horn, pouch, shells, shell box, bag of shot, bag of grenades, mixed crate, barrel): its lots
 *  (kind and count; a crate holds several), its mesh and its name. Picked up with F (AAmmoPickup), the rounds go into
 *  the pouch of the carrier's weapons of that kind; refused without such a weapon.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UAmmoDefinition : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name in the prompt ("Giberne", "Sac de grenades") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	FText DisplayName;

	/** What the packet holds (one lot, several for a crate) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	TArray<FAmmoLot> Lots;

	/** Picture of the shop line (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	TObjectPtr<UTexture2D> Icon;

	/** Mesh of the packet on the ground (SM_Ammo_*, physics body of the pickup); a placeholder box when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup")
	TObjectPtr<UStaticMesh> PickupMesh;

	/** Mass of the packet on the ground (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup", meta=(ClampMin="0.05", Units="kg"))
	float PickupMassKg = 1.0f;

	/** Rounds of Kind in the lots */
	int32 CountOf(EAmmoKind Kind) const
	{
		int32 Count = 0;
		for (const FAmmoLot& Lot : Lots)
		{
			Count += Lot.Kind == Kind ? Lot.Count : 0;
		}
		return Count;
	}
};
