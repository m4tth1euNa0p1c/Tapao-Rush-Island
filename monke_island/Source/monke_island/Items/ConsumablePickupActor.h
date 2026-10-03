// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "ConsumablePickupActor.generated.h"

class UConsumableDefinition;
class UStaticMeshComponent;

/**
 *  Pickup placed in the world that a player consumes with the interaction key (F): a fresh coconut on the sand
 *  heals ("Boire la noix de coco (+25 PV)"). Server authoritative: the server re-checks range and sight (grab
 *  component), then CanInteract, heals once and destroys the pickup; clients hide it as soon as it is consumed.
 *  Placed in the level with net dormancy: no network cost until someone drinks it.
 */
UCLASS()
class MONKE_ISLAND_API AConsumablePickupActor : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	AConsumablePickupActor();

	/** What this pickup is and what it restores (DA_Consumable_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable")
	TObjectPtr<UConsumableDefinition> Definition;

	/** Delay before the consumed pickup is destroyed, so the use feedback reaches the clients (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable", meta=(ClampMin="0", Units="s"))
	float DestroyDelay = 0.5f;

	/** Returns the mesh */
	UFUNCTION(BlueprintPure, Category="Consumable")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** True once consumed (the server destroys the pickup right after) */
	UFUNCTION(BlueprintPure, Category="Consumable")
	bool IsConsumed() const { return bConsumed; }

	/** Rule behind CanInteract: not consumed yet, user alive, and hurt unless usable at full health */
	static bool CanConsume(bool bAlreadyConsumed, bool bUserDead, float Health, float MaxHealth, bool bUsableAtFullHealth);

	// IIslanderInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Hides the pickup and stops it from blocking (every machine) */
	UFUNCTION()
	void OnRep_Consumed();

	/** Use feedback (sound) on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastConsumed();

private:

	/** Pickup mesh (root), blocks the visibility trace of the interaction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Set once by the server when a player consumes the pickup */
	UPROPERTY(ReplicatedUsing=OnRep_Consumed)
	bool bConsumed = false;
};
