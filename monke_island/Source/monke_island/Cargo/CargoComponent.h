// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Cargo/CargoLedger.h"
#include "CargoComponent.generated.h"

class UCargoRules;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCargoChangedSignature, UCargoComponent*, Cargo);

/**
 *  Cargo hold of a boat (cahier §5, §6.5): a resource aimed at a free slot is stowed by the server, its physics cut,
 *  attached to the slot (socket Slot_N of the owner's root, else a grid offset from DA_CargoRules) and counted.
 *  An object merely thrown in the boat stays Free and is not counted. Unstowing returns the physics; the ledger
 *  only counts occupied slots, so stow / unstow never double count.
 *
 *  Replication: the FCargoLedger struct is a plain replicated property (12 slots by default, 64 at most).
 *  Unreal replicates arrays inside structs element by element against the shadow state, so a stow sends one slot;
 *  a FastArraySerializer would add the NetCore dependency and per-item bookkeeping for no gain at this size.
 *  The stowed actors replicate their own attachment (they must replicate movement); OnCargoChanged fires on the
 *  server after each change and on clients in OnRep_Ledger.
 */
UCLASS(ClassGroup=(Cargo), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UCargoComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCargoComponent();

	/** Rules of this hold (DA_CargoRules); the class defaults of UCargoRules are used when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo")
	TObjectPtr<UCargoRules> Rules;

	/** Broadcast on the server and every client when the cargo changes */
	UPROPERTY(BlueprintAssignable, Category="Cargo")
	FCargoChangedSignature OnCargoChanged;

	/**
	 *  Stows a resource in a slot (server only). SlotIndex INDEX_NONE takes the first free slot.
	 *  Refused, with a readable OutRefusal and nothing changed, when locked, full (slots or mass), the slot is taken,
	 *  the object is already stowed or lost, not a resource, or too far from the slot.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo")
	bool TryStow(AActor* Resource, int32 SlotIndex, FText& OutRefusal);

	/** Unstows the object of a slot (server only): detached, physics back, state Free */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo")
	bool TryUnstow(int32 SlotIndex, FText& OutRefusal);

	/** Locks or unlocks stowing and unstowing (server only), e.g. while the boat moves (cahier §6.4) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo")
	void SetLocked(bool bInLocked);

	/** Changes the capacity (server only, shop upgrade); never drops a stowed object */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo")
	void SetCapacity(int32 SlotCount, float MaxMassKg);

	/** Locks the hold and returns the frozen list of stowed resources for the extraction (server only) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo")
	FCargoSnapshot FreezeForExtraction();

	/** Every stowed object is lost, e.g. the boat sank (server only) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo")
	void LoseAll();

	/** Frozen copy of the stowed resources, without locking */
	UFUNCTION(BlueprintPure, Category="Cargo")
	FCargoSnapshot MakeSnapshot() const { return Ledger.MakeSnapshot(); }

	/** Totals by category of the stowed resources */
	UFUNCTION(BlueprintPure, Category="Cargo")
	FCargoTotals GetTotals() const { return Ledger.ComputeTotals(); }

	/** Slots */
	UFUNCTION(BlueprintPure, Category="Cargo")
	const TArray<FCargoSlot>& GetSlots() const { return Ledger.Slots; }

	/** Number of slots */
	UFUNCTION(BlueprintPure, Category="Cargo")
	int32 GetSlotCount() const { return Ledger.Slots.Num(); }

	/** Number of free slots */
	UFUNCTION(BlueprintPure, Category="Cargo")
	int32 GetFreeSlotCount() const { return Ledger.GetFreeSlotCount(); }

	/** True when the slot exists and is empty */
	UFUNCTION(BlueprintPure, Category="Cargo")
	bool IsSlotFree(int32 SlotIndex) const { return Ledger.Slots.IsValidIndex(SlotIndex) && !Ledger.Slots[SlotIndex].bOccupied; }

	/** Slot holding this actor, INDEX_NONE if it is not stowed here */
	UFUNCTION(BlueprintPure, Category="Cargo")
	int32 FindSlotOfActor(const AActor* Actor) const { return Ledger.FindSlotOfActor(Actor); }

	/** Mass stowed (kg) */
	UFUNCTION(BlueprintPure, Category="Cargo")
	float GetMassKg() const { return Ledger.GetMassKg(); }

	/** Mass limit (kg) */
	UFUNCTION(BlueprintPure, Category="Cargo")
	float GetMaxMassKg() const { return Ledger.MaxMassKg; }

	/** True while locked */
	UFUNCTION(BlueprintPure, Category="Cargo")
	bool IsLocked() const { return Ledger.bLocked; }

	/** World transform of a slot (socket when present, grid otherwise) */
	UFUNCTION(BlueprintPure, Category="Cargo")
	FTransform GetSlotWorldTransform(int32 SlotIndex) const;

	/** Slot closest to a world location within MaxDistance, free slots only if bFreeOnly; INDEX_NONE otherwise */
	UFUNCTION(BlueprintPure, Category="Cargo")
	int32 FindClosestSlot(const FVector& WorldLocation, float MaxDistance, bool bFreeOnly) const;

	/** Read-only access for the economy and the specs */
	const FCargoLedger& GetLedger() const { return Ledger; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Notifies clients */
	UFUNCTION()
	void OnRep_Ledger();

	/** A stowed actor was destroyed: its slot is freed and nothing is counted for it */
	UFUNCTION()
	void HandleStowedActorDestroyed(AActor* DestroyedActor);

private:

	/** Rules or the class defaults */
	const UCargoRules& GetRules() const;

	/** Parent component and socket of a slot (NAME_None when the grid is used) */
	USceneComponent* GetSlotParent(int32 SlotIndex, FName& OutSocket) const;

	/** Common refusal path: text + log */
	bool Refuse(ECargoRefusal Refusal, const AActor* Resource, int32 SlotIndex, FText& OutRefusal) const;

	/** Replicated bookkeeping */
	UPROPERTY(ReplicatedUsing=OnRep_Ledger)
	FCargoLedger Ledger;
};
