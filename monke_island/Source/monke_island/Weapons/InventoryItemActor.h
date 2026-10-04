// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryItemActor.generated.h"

class AIslanderCharacter;
class UInputMappingContext;
class UInventoryItemDefinition;
class ULocalPlayer;

/**
 *  Base of an item held in an inventory slot. Spawned by the server, owned by the player's character and
 *  replicated; it is shown and usable only while equipped (its slot is the active one).
 *  Subclasses add the item's own input through a mapping context pushed while the item can be used.
 */
UCLASS(Abstract)
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

	/** Context currently pushed, if any */
	TWeakObjectPtr<UInputMappingContext> ActiveContext;
};
