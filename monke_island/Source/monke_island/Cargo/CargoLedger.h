// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Cargo/CargoTypes.h"
#include "CargoLedger.generated.h"

/**
 *  Pure bookkeeping of a cargo hold (cahier §5): slots, mass limit, lock, totals, snapshot.
 *  No world, no physics: UCargoComponent replicates it and drives the actors; the specs (monke_island.Cargo.*)
 *  test it directly. An object is counted only while it occupies a slot, so stow / unstow cannot double count.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCargoLedger
{
	GENERATED_BODY()

	/** Slots; their number is the slot capacity */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	TArray<FCargoSlot> Slots;

	/** Mass limit (kg) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	float MaxMassKg = 0.0f;

	/** Stowing and unstowing refused while locked */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	bool bLocked = false;

	/** Upper bound of the slot count (MEM-08) */
	static constexpr int32 MaxSlotCount = 64;

	/**
	 *  Sets the capacity. Never drops a stowed object: when shrinking, only trailing empty slots are removed.
	 *  Returns the slot count actually kept.
	 */
	int32 Configure(int32 SlotCount, float InMaxMassKg);

	/** Checks a stow request without changing anything; OutSlot receives the resolved slot (INDEX_NONE = first free) */
	ECargoRefusal CanStow(const FCargoEntry& Entry, EResourceState State, int32 SlotIndex, int32& OutSlot) const;

	/** Stows an entry; returns None and the slot used on success, the refusal otherwise (nothing changed) */
	ECargoRefusal Stow(const FCargoEntry& Entry, EResourceState State, int32 SlotIndex, AActor* Actor, int32& OutSlot);

	/** Empties a slot; OutSlot receives what it held */
	ECargoRefusal Unstow(int32 SlotIndex, FCargoSlot& OutSlot);

	/** Empties every slot regardless of the lock (boat sunk); returns what they held */
	TArray<FCargoSlot> RemoveAll();

	/** Slot holding this resource, INDEX_NONE if not stowed */
	int32 FindSlotOf(const FResourceId& Id) const;

	/** Slot holding this actor, INDEX_NONE if not stowed */
	int32 FindSlotOfActor(const AActor* Actor) const;

	/** First free slot, INDEX_NONE when full */
	int32 FindFreeSlot() const;

	/** Number of free slots */
	int32 GetFreeSlotCount() const;

	/** Mass stowed (kg) */
	float GetMassKg() const;

	/** Totals by category */
	FCargoTotals ComputeTotals() const;

	/** Frozen copy of the stowed entries for the extraction */
	FCargoSnapshot MakeSnapshot() const;

	/** True when the slot exists and holds an object */
	bool IsSlotOccupied(int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].bOccupied; }

	/** Readable message for the player (cahier §5: "refus clair") */
	static FText GetRefusalText(ECargoRefusal Refusal);
};

/** Fallback placement of the slots when the hull has no Slot_N socket */
namespace CargoLayout
{
	/**
	 *  Local offset of a slot on a grid centred on Origin: Columns across the beam (Y), rows along the keel (X).
	 *  Spacing.X is the distance between rows, Spacing.Y between columns (cm).
	 */
	MONKE_ISLAND_API FVector ComputeSlotOffset(int32 SlotIndex, int32 SlotCount, int32 Columns, const FVector2D& Spacing, const FVector& Origin);
}
