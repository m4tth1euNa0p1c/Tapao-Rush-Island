// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryItemActor.generated.h"

class AIslanderCharacter;
class UEnhancedInputComponent;
class UInputMappingContext;
class UInventoryItemDefinition;
class ULocalPlayer;

/**
 *  Base of an item held in an inventory slot. Spawned by the server, owned by the player's character and
 *  replicated; it is shown and usable only while equipped (its slot is the active one).
 *  Subclasses add the item's own input through a mapping context pushed while the item can be used.
 *  Dropped on the ground (T-157) the same actor is kept, hidden, inside an AInventoryPickup: its state (balls,
 *  loaded barrel, edge, fuel) survives the drop and goes to whoever picks it up.
 *  Not abstract so that a bare item can be spawned by the inventory tests; real items are subclasses.
 */
UCLASS()
class MONKE_ISLAND_API AInventoryItemActor : public AActor
{
	GENERATED_BODY()

public:

	AInventoryItemActor();

	/** Server: shows the item in the hands or puts it away */
	void SetEquipped(bool bInEquipped);

	/** True while the item's slot is the active one */
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool IsEquipped() const { return bEquipped; }

	/** Definition this item was spawned from */
	UFUNCTION(BlueprintPure, Category="Inventory")
	const UInventoryItemDefinition* GetDefinition() const { return Definition; }

	/** Server: set once at spawn */
	void SetDefinition(const UInventoryItemDefinition* InDefinition) { Definition = InDefinition; }

	/** Server: spawns the item of a definition for a carrier (null when the definition has no ItemClass) */
	static AInventoryItemActor* SpawnFromDefinition(const UInventoryItemDefinition* InDefinition, AActor* Carrier);

	/** Server: hands the item to a new carrier (owner, attachment) and shows it again; not equipped */
	void SetCarrier(AActor* NewCarrier);

	/** Server: puts the item away and hides the whole actor while it lies on the ground in a pickup, or shows it back */
	void SetOnGround(bool bInOnGround);

	/** True while the item lies on the ground in a pickup (server) */
	bool IsOnGround() const { return bOnGround; }

	/**
	 *  Server: the value of the pickup this item was taken from (a mob weapon is loot, T-309): kept with the item through
	 *  the inventory and given back to the pickup it becomes when dropped, the zone multiplier not applied a second time
	 *  (network review R-39)
	 */
	void KeepValue(int32 InBaseValue, int32 InValue);

	/** Server: the value KeepValue kept; false when the item never had one (its pickup then takes the value of its class) */
	bool GetKeptValue(int32& OutBaseValue, int32& OutValue) const;

	/** Character carrying the item */
	AIslanderCharacter* GetCarrier() const;

	/** True on the machine of the player carrying the item */
	bool IsCarrierLocallyControlled() const;

	/** Server: damage that reaches the carrier while this item is in hand (a parry lowers it); unchanged by default */
	virtual float ModifyIncomingDamage(float Damage, const AActor* DamageCauser) { return Damage; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Owner() override;

	/** Called on every machine when the item is equipped or put away */
	virtual void OnEquippedChanged() {}

	/** Called on every machine once the item follows its carrier */
	virtual void OnAttachedToCarrier() {}

	/** Attaches the item to its carrier (owner) */
	void AttachToCarrier();

	/** Pushes or removes the item's mapping context for the local carrier */
	void SetInputContextActive(UInputMappingContext* Context, int32 Priority, bool bActive);

	/** Keeps a binding this item made on its carrier's input: every kept binding is removed when the carrier changes or
	 *  the item ends, so a former carrier never drives an item someone else picked up (the same actor goes from hand to
	 *  hand; T-288, network review R-37) */
	void KeepInputBinding(UEnhancedInputComponent* Input, uint32 Handle);

	/** Removes every kept binding from its input component, then OnInputBindingsRemoved */
	void RemoveInputBindings();

	/** The kept bindings are gone: a subclass forgets it bound its input, and binds again for its next carrier */
	virtual void OnInputBindingsRemoved() {}

	UFUNCTION()
	void OnRep_Equipped();

private:

	/** Equipped state, set by the server */
	UPROPERTY(ReplicatedUsing=OnRep_Equipped)
	bool bEquipped = false;

	/** Definition this item was spawned from */
	UPROPERTY(Replicated)
	TObjectPtr<const UInventoryItemDefinition> Definition;

	/** Local player the mapping context was pushed to (kept to remove it after the pawn is gone) */
	TWeakObjectPtr<ULocalPlayer> ContextPlayer;

	/** Server: the item lies on the ground in a pickup */
	bool bOnGround = false;

	/** Server: KeepValue gave the item a value */
	bool bHasKeptValue = false;

	/** Server: value of the intact item, zone multiplier included, and the value left (see KeepValue) */
	int32 KeptBaseValue = 0;
	int32 KeptValue = 0;

	/** Context currently pushed, if any */
	TWeakObjectPtr<UInputMappingContext> ActiveContext;

	/** A binding kept on a carrier's input component */
	struct FKeptInputBinding
	{
		TWeakObjectPtr<UEnhancedInputComponent> Input;
		uint32 Handle = 0;
	};

	/** Bindings made on the current carrier's input (KeepInputBinding) */
	TArray<FKeptInputBinding> KeptBindings;
};
