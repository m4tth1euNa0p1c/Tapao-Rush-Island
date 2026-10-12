// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryPickup.h"
#include "MobWeaponPickup.generated.h"

class UValuableComponent;

/**
 *  A mob's weapon lying on the ground (T-161, lot L5): the spear, the obsidian club or the shell hammer a tiki warrior
 *  lets fall at its death (DroppedWeaponClass of its BP_Tiki*). An inventory pickup ("Ramasser <name>" with F, then a
 *  melee weapon in a slot, DA_Item_<Weapon>), and loot like the BP_Drop_* it replaces: its UValuableComponent gives it
 *  their sale value, counted aboard the galleon and lost on hard impacts. Lifted and carried like any free body.
 *  Server authoritative as every AInventoryPickup (claimed once, reach and sight checked by the server).
 */
UCLASS(Blueprintable)
class MONKE_ISLAND_API AMobWeaponPickup : public AInventoryPickup
{
	GENERATED_BODY()

public:

	AMobWeaponPickup();

	/** Returns the sale value of the weapon */
	UFUNCTION(BlueprintPure, Category="Pickup")
	UValuableComponent* GetValuable() const { return Valuable; }

private:

	/** Sale value of the weapon (BaseValue set in each BP_Pickup_<Weapon>), lost on hard impacts like any loot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UValuableComponent> Valuable;
};
