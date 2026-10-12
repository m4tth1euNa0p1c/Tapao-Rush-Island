// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "InventoryPickup.generated.h"

class AInventoryItemActor;
class UInventoryItemDefinition;
class UNetRestComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  An inventory item lying on the ground (T-157, lot L5): a physics body showing the item's PickupMesh, picked up
 *  with the interaction key (F, "Ramasser <name>", IIslanderInteractable) and lifted like any free object with E.
 *  Dropped by a player (G), it keeps the item actor itself, hidden, so its state survives; placed in a level, it only
 *  has a Definition and the item is spawned on pickup.
 *  Server authoritative: the server re-checks reach and sight (grab component, then CanInteract) and the pickup is
 *  claimed once (bClaimed), so two players pressing F on the same frame never get two copies (cahier recette 2/3).
 *  At rest it stops replicating, and on the galleon's deck or in the barque it rides glued to its base on every machine,
 *  freed as soon as a player lifts it (UNetRestComponent, T-316); taken with F it is destroyed, which every client gets,
 *  its channel asleep or not.
 */
UCLASS()
class MONKE_ISLAND_API AInventoryPickup : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	AInventoryPickup();

	/** Mesh shown when the definition has no PickupMesh (placeholder) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup")
	TObjectPtr<UStaticMesh> FallbackMesh;

	/** Scale of FallbackMesh (the engine cube is 1 m: a 60 x 10 x 10 cm stick by default) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup")
	FVector FallbackMeshScale = FVector(0.6, 0.1, 0.1);

	/**
	 *  Server: spawns a pickup holding a dropped item (hidden in it, state kept) and launches it; the value the item
	 *  carries (a mob weapon, T-309) goes back to the pickup's UValuableComponent before it begins play
	 */
	static AInventoryPickup* SpawnForItem(UWorld* World, TSubclassOf<AInventoryPickup> PickupClass, AInventoryItemActor* Item,
		const FTransform& Transform, const FVector& Velocity);

	/** Server: spawns a pickup of a definition alone (the item is spawned on pickup) */
	static AInventoryPickup* SpawnForDefinition(UWorld* World, TSubclassOf<AInventoryPickup> PickupClass, UInventoryItemDefinition* InDefinition,
		const FTransform& Transform);

	/**
	 *  Server: takes the item out of the pickup for a new carrier, at most once; the pickup hides and is destroyed.
	 *  Returns the stored item (or a new one spawned from the definition), null when already claimed. The pickup's value
	 *  (UValuableComponent: losses and zone multiplier) goes with the item (T-309, network review R-39).
	 */
	AInventoryItemActor* Claim(AActor* NewCarrier);

	/** Item definition (name in the prompt, mesh, item class); set in the level or by the drop */
	UFUNCTION(BlueprintPure, Category="Pickup")
	UInventoryItemDefinition* GetDefinition() const { return Definition; }

	/** Item actor kept by a drop (server only), null for a level pickup */
	AInventoryItemActor* GetStoredItem() const { return StoredItem; }

	/** True once a player took the item */
	UFUNCTION(BlueprintPure, Category="Pickup")
	bool IsClaimed() const { return bClaimed; }

	/** Returns the mesh */
	UFUNCTION(BlueprintPure, Category="Pickup")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** Returns the network rest of the pickup (dormancy at rest, glue to a moving base) */
	UNetRestComponent* GetNetRest() const { return NetRest; }

	// IIslanderInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Applies the definition's mesh and mass, then simulates (every machine) */
	UFUNCTION()
	void OnRep_Definition();

	/** Hides the claimed pickup and stops it from blocking (every machine) */
	UFUNCTION()
	void OnRep_Claimed();

private:

	/** Server: a pickup of a definition spawned deferred (FinishSpawning is the caller's); null without world or definition */
	static AInventoryPickup* BeginSpawn(UWorld* World, TSubclassOf<AInventoryPickup> PickupClass, UInventoryItemDefinition* InDefinition,
		const FTransform& Transform);

	/** Shows the definition's mesh (or the placeholder) */
	void ApplyMesh();

	/** Physics mesh (root): blocks the visibility trace of the interaction, ignores pawns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Network rest: dormant at rest, glued to the deck of the galleon or the hull of the barque (T-316, review R-43) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetRestComponent> NetRest;

	/** What lies here (DA_Item_*) */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Definition, Category="Pickup", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInventoryItemDefinition> Definition;

	/** Set once by the server when a player takes the item */
	UPROPERTY(ReplicatedUsing=OnRep_Claimed)
	bool bClaimed = false;

	/** Dropped item kept with its state (server only; destroyed with the pickup if nobody takes it) */
	UPROPERTY(Transient)
	TObjectPtr<AInventoryItemActor> StoredItem;
};
