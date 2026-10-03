// monke_island

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IslanderInteractable.generated.h"

class APawn;

UINTERFACE(MinimalAPI, Blueprintable)
class UIslanderInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Something the player uses with the interaction key (F): boat seat, chest, lever...
 *  The crosshair shows "F  <prompt>" when it is in reach and in sight (cahier §4: target and action only within
 *  range and line of sight); the server re-checks range and sight, then calls Interact.
 *  Free physics objects without this interface are lifted instead (R.E.P.O. style grab).
 */
class MONKE_ISLAND_API IIslanderInteractable
{
	GENERATED_BODY()

public:

	/** Action shown next to the key, e.g. "Monter dans la barque" */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	FText GetInteractionPrompt(APawn* User) const;

	/** False hides the prompt and refuses the interaction (busy, locked...) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	bool CanInteract(APawn* User) const;

	/** Server only, after the range and sight checks */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction")
	void Interact(APawn* User);
};
