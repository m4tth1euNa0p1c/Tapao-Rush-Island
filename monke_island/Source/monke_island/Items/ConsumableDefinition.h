// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ConsumableDefinition.generated.h"

class USoundBase;
class UStaticMesh;

/**
 *  Something a player drinks or eats with the interaction key (DA_Consumable_*), e.g. a fresh coconut.
 *  Owner request (T-048; the cahier leaves hunger for later, §4): restores health once, server authoritative.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UConsumableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Action shown after the key, e.g. "Boire la noix de coco" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	FText ActionText;

	/** Health restored on use */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable", meta=(ClampMin="0"))
	float HealAmount = 25.0f;

	/** Also usable at full health (otherwise the prompt stays hidden and the item is kept for later) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	bool bUsableAtFullHealth = false;

	/** Mesh of the pickup in the world (optional: the Blueprint may set its own) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	TObjectPtr<UStaticMesh> Mesh;

	/** Sound played where the item is used (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	TObjectPtr<USoundBase> UseSound;
};
