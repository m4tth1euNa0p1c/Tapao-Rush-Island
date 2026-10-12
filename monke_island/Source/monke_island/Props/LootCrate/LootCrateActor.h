// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "Props/LootCrate/LootContainer.h"
#include "LootCrateActor.generated.h"

class UBoxComponent;
class ULootCrateRules;
class UNetRestComponent;
class UStaticMeshComponent;

/** One object inside a loot crate */
USTRUCT()
struct FLootCrateEntry
{
	GENERATED_BODY()

	/** The object, hidden and carried inside */
	UPROPERTY()
	TObjectPtr<AActor> Item;

	/** Its mass when it went in (kg): its body is off while it lies inside */
	UPROPERTY()
	float MassKg = 0.0f;
};

/**
 *  Wooden loot crate (T-258, lot L2, cahier §5-6; maquette caisse-transport of the second PC): carries the loot in bulk
 *  from the land to the barque.
 *  - A free physics box (Body, PhysicsActor) carried like any heavy object by the grab component: alone up to the
 *    holder's lift mass, by two players beyond (shared grab), the holders slowed by its weight.
 *  - A container (ILootContainer): F aimed at it while holding an object puts the object in (hidden, carried along,
 *    its mass added, the lid opens and shuts); F with empty hands lifts it ("Porter la caisse (N objets, M kg)");
 *    crouched, F empties it in front of the player.
 *  - In the barque or on the galleon its contents count with it (LootContainers::AppendWithContents): the barque's load
 *    weighs it through its mass, the galleon's cargo and the bilan reach the objects inside.
 *  Server authoritative: the contents and the mass replicate, the lid plays on every machine; a client never decides
 *  what goes in or comes out.
 *  At rest it stops replicating, and on the galleon's deck or in the barque it rides glued to its base on every machine,
 *  with what it holds, until a player lifts it (UNetRestComponent, T-316). Every change of its contents wakes it first,
 *  so a client whose channel slept gets the contents and the lid at once.
 */
UCLASS(Blueprintable)
class MONKE_ISLAND_API ALootCrateActor : public AActor, public IIslanderInteractable, public ILootContainer
{
	GENERATED_BODY()

public:

	ALootCrateActor();

	/** Rules of the crate (DA_LootCrateRules); DefaultRules when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LootCrate")
	TObjectPtr<ULootCrateRules> Rules;

	/** Rules taken when Rules is empty (the bare class, as the galleon spawns it): DA_LootCrateRules once it exists, else
	 *  the defaults of ULootCrateRules */
	UPROPERTY(EditDefaultsOnly, Category="LootCrate")
	TSoftObjectPtr<ULootCrateRules> DefaultRules;

	/** Rules in use: Rules, or the defaults of ULootCrateRules */
	const ULootCrateRules* GetRules() const;

	/** Objects inside */
	UFUNCTION(BlueprintPure, Category="LootCrate")
	int32 GetItemCount() const { return Contents.Num(); }

	/** Mass of the objects inside (kg) */
	UFUNCTION(BlueprintPure, Category="LootCrate")
	float GetContentMassKg() const;

	/** Mass of the crate with what it holds (kg) */
	UFUNCTION(BlueprintPure, Category="LootCrate")
	float GetTotalMassKg() const;

	/** Server: takes every object out, in a fan in front of the crate toward Toward, ready to be picked up again; returns
	 *  how many came out */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="LootCrate")
	int32 EmptyContents(const FVector& Toward);

	/** Physics body (root) */
	UFUNCTION(BlueprintPure, Category="LootCrate")
	UBoxComponent* GetBody() const { return Body; }

	/** Returns the network rest of the crate (dormancy at rest, glue to a moving base) */
	UNetRestComponent* GetNetRest() const { return NetRest; }

	// ILootContainer
	virtual void GetContainedActors(TArray<AActor*>& OutActors) const override;
	virtual bool CanStoreItem(const APawn* User, const AActor* Item) const override;
	virtual bool StoreItem(APawn* User, AActor* Item) override;
	virtual FText GetStorePrompt(const APawn* User, const AActor* Item) const override;

	// IIslanderInteractable: lift it, or empty it when crouched
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	/** Only while the lid moves or, on a client, while objects that came out wait for their own replication */
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Clients: hides what went in, shows again what came out, and weighs the crate */
	UFUNCTION()
	void OnRep_Contents(const TArray<FLootCrateEntry>& PreviousContents);

	/** Every machine: the lid opens and shuts, with the sound of an object going in or of the crate emptied */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastLidPulse(bool bEmptied);

private:

	/** Meshes (when the components have none), body size, lid hinge, damping and mass */
	void ApplyRules();

	/** Mass of the body: empty crate + contents */
	void UpdateMass();

	/** Server: forgets the entries whose object is gone (destroyed while inside), and wakes the crate when one was */
	void PurgeContents();

	/** True when the interaction key of this pawn empties the crate now (crouched, something inside) */
	bool WantsEmpty(const APawn* User) const;

	/** Point of the body closest to Location (a grab point on its surface) */
	FVector GetClosestBodyPoint(const FVector& Location) const;

	/** Turns the lid around its hinge (deg, 0 shut) */
	void SetLidAngle(float AngleDeg);

	/** Hides an object inside (no body, no collision, not drawn) or brings it back, on this machine */
	static void ApplyStoredState(AActor* Item, bool bStored);

	/** Clients (tick): shows again the objects that came out once their own replication took them off the crate */
	void RestorePending();

	/** Physics box (root, PhysicsActor): what the players see, grab and push */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> Body;

	/** Planks of the crate (SM_LootCrate, no collision: the box is the body) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CrateMesh;

	/** Hinge line of the lid (turns around its X) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> LidHinge;

	/** Lid (SM_LootCrate_Lid, no collision) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LidMesh;

	/** Network rest: dormant at rest, glued to the deck of the galleon or the hull of the barque (T-316, review R-04) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetRestComponent> NetRest;

	/** Objects inside, in the order they went in (server authoritative, replicated) */
	UPROPERTY(Transient, ReplicatedUsing=OnRep_Contents)
	TArray<FLootCrateEntry> Contents;

	/** Seconds since the lid started to open, negative when it is shut */
	float LidTime = -1.0f;

	/** Sign of the roll that opens the lid away from its hinge */
	float LidRollSign = 1.0f;

	/** Clients: objects that came out, waiting to be taken off the crate by their own replication */
	TArray<TWeakObjectPtr<AActor>> PendingRestore;

	/** Clients: seconds spent waiting for PendingRestore */
	float PendingRestoreSeconds = 0.0f;
};
