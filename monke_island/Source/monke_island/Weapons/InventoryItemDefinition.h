// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InventoryItemDefinition.generated.h"

class AInventoryItemActor;

/**
 *  An item a player carries in an inventory slot (DA_Item_*): its name and the actor spawned for it.
 *  The actor stays with the player and shows itself only while its slot is the active one.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UInventoryItemDefinition : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown in the slot bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	/** Actor spawned for this item on the server, owned by the player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TSubclassOf<AInventoryItemActor> ItemClass;
};
