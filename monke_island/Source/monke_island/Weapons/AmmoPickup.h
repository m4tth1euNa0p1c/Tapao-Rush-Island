// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "Weapons/AmmoTypes.h"
#include "AmmoPickup.generated.h"

class UAmmoDefinition;
class UNetRestComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Ammunition lying on the ground (owner 2026-10-09; BP_Ammo_* placed in a level, scattered as fort loot, or spawned):
 *  a physics body showing the packet's mesh, picked up with the interaction key (F) like an inventory pickup. The
 *  rounds go into the pouch of the carrier's weapons of each lot's kind (the one in hand first); without such a weapon
 *  the lot is refused and the prompt says so; a full pouch leaves the rest in the packet. Server authoritative: the
 *  server re-checks reach and sight (grab component, then CanInteract), pours the lots and destroys the empty packet.
 *  At rest it stops replicating, and on the galleon's deck or in the barque it rides glued to its base on every machine
 *  (UNetRestComponent, T-316); a packet taken from is woken, so every client sees what is left in it, or that it is empty.
 */
UCLASS()
class MONKE_ISLAND_API AAmmoPickup : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	AAmmoPickup();

	/** Mesh shown when the definition has no PickupMesh (placeholder) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	TObjectPtr<UStaticMesh> FallbackMesh;

	/** Scale of FallbackMesh (the engine cube is 1 m: a 25 x 15 x 12 cm box by default) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	FVector FallbackMeshScale = FVector(0.25, 0.15, 0.12);

	/** Delay before the emptied packet is destroyed, so the clients see it go (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0", Units="s"))
	float DestroyDelay = 0.5f;

	/** Server: spawns a packet of a definition at a transform (null class: AAmmoPickup itself) */
	static AAmmoPickup* Spawn(UWorld* World, TSubclassOf<AAmmoPickup> PickupClass, UAmmoDefinition* InDefinition, const FTransform& Transform);

	/** What lies here (DA_Ammo_*) */
	UFUNCTION(BlueprintPure, Category="Ammo")
	UAmmoDefinition* GetDefinition() const { return Definition; }

	/** Rounds left of Kind in the packet (the definition's count before anyone took from it) */
	UFUNCTION(BlueprintPure, Category="Ammo")
	int32 GetRemainingOf(EAmmoKind Kind) const;

	/** True once emptied (the server destroys the packet right after) */
	UFUNCTION(BlueprintPure, Category="Ammo")
	bool IsTaken() const { return bTaken; }

	/** Returns the mesh */
	UFUNCTION(BlueprintPure, Category="Ammo")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** Returns the network rest of the packet (dormancy at rest, glue to a moving base) */
	UNetRestComponent* GetNetRest() const { return NetRest; }

	// IIslanderInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Applies the definition's mesh and mass, then simulates (every machine) */
	UFUNCTION()
	void OnRep_Definition();

	/** Hides the emptied packet (its mesh: the actor stays relevant until it is destroyed) and stops it from blocking (every
	 *  machine) */
	UFUNCTION()
	void OnRep_Taken();

private:

	/** Shows the definition's mesh (or the placeholder) */
	void ApplyMesh();

	/** Rounds left in lot Index (the definition's count until the server took from it) */
	int32 RemainingOfLot(int32 Index) const;

	/** Server: pours the lots into the user's weapons; false when nothing was taken (the user is told why) */
	bool GiveTo(APawn* User);

	/** "12 balles de mousquet, 3 grenades": what is left in the packet */
	FText DescribeContents() const;

	/** Physics mesh (root): blocks the visibility trace of the interaction, ignores pawns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Network rest: dormant at rest, glued to the deck of the galleon or the hull of the barque (T-316, review R-43) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetRestComponent> NetRest;

	/** What lies here (DA_Ammo_*) */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Definition, Category="Ammo", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAmmoDefinition> Definition;

	/** Rounds left of each lot (same order as the definition's lots); empty until the server took from one */
	UPROPERTY(Replicated)
	TArray<int32> Remaining;

	/** Set once by the server when the packet is emptied */
	UPROPERTY(ReplicatedUsing=OnRep_Taken)
	bool bTaken = false;
};
