// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InventoryItemDefinition.generated.h"

class AInventoryItemActor;
class AInventoryPickup;
class UStaticMesh;
class UTexture2D;

/**
 *  An item a player carries in an inventory slot (DA_Item_*): its name and the actor spawned for it.
 *  The actor stays with the player and shows itself only while its slot is the active one.
 *  On the ground (dropped with G, or placed in a level) the item is an AInventoryPickup showing PickupMesh,
 *  picked up with the interaction key (F). An item without ItemClass cannot be picked up (no prompt).
 *  Stacks (T-166): MaxStack identical items share one inventory cell (1 for a weapon, e.g. 30 for cartridges); the
 *  cell keeps one item actor and a count.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UInventoryItemDefinition : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown in the slot bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	/** Short label of the inventory cell when the item has no icon (a few letters; DisplayName when empty) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText ShortName;

	/** Picture of the inventory cell (square, 128 px is plenty); the kit picture of the class or ShortName when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TObjectPtr<UTexture2D> Icon;

	/** Identical items one inventory cell holds (1 = not stackable, a weapon; e.g. 30 for cartridges) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1", ClampMax="999"))
	int32 MaxStack = 1;

	/** MaxStack of a definition, at least 1 (1 for none) */
	static int32 GetMaxStack(const UInventoryItemDefinition* Definition) { return Definition ? FMath::Max(1, Definition->MaxStack) : 1; }

	/** Actor spawned for this item on the server, owned by the player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TSubclassOf<AInventoryItemActor> ItemClass;

	/** Mesh of the item lying on the ground (physics body of the pickup); a placeholder stick when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup")
	TObjectPtr<UStaticMesh> PickupMesh;

	/** Mass of the item on the ground (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup", meta=(ClampMin="0.05", Units="kg"))
	float PickupMassKg = 3.0f;

	/** Pickup class spawned when the item is dropped; the inventory's PickupClass when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup")
	TSubclassOf<AInventoryPickup> PickupClass;
};
