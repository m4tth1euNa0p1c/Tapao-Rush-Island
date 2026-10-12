// monke_island

#pragma once

#include "CoreMinimal.h"
#include "RunLoadout.generated.h"

class UInventoryItemDefinition;

/**
 *  One inventory cell kept from a day to the next (ADR-007): the item, its count, for a firearm its rounds and, for a mob
 *  weapon found as loot, the value it has (T-314). A field a save of an older format lacks keeps its default here.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunLoadoutItem
{
	GENERATED_BODY()

	/** BaseValue and Value of an item without a loot value of its own: a bought weapon, or a save older than T-314 */
	static constexpr int32 NoValue = -1;

	/** Cell (bar then bag) */
	UPROPERTY(SaveGame)
	int32 Slot = 0;

	/** What lies there */
	UPROPERTY(SaveGame)
	TSoftObjectPtr<UInventoryItemDefinition> Definition;

	/** Items in the cell */
	UPROPERTY(SaveGame)
	int32 Count = 1;

	/** Firearm: rounds loaded, -1 for "as the rules start it" */
	UPROPERTY(SaveGame)
	int32 LoadedRounds = -1;

	/** Firearm: rounds in the pouch, -1 for "as the rules start it" */
	UPROPERTY(SaveGame)
	int32 ReserveRounds = -1;

	/** Mob weapon (T-309, T-314): value of the intact item, the multiplier of the zone it was found in included; NoValue
	 *  when it has none (its pickup then takes the value of its class where it is dropped) */
	UPROPERTY(SaveGame)
	int32 BaseValue = NoValue;

	/** Mob weapon: the value left of BaseValue after the losses on impacts; NoValue when it has none */
	UPROPERTY(SaveGame)
	int32 Value = NoValue;

	/** Keeps the loot value the item carries: BaseValue at least 0, Value between 0 and BaseValue */
	void SetKeptValue(int32 InBaseValue, int32 InValue)
	{
		BaseValue = FMath::Max(0, InBaseValue);
		Value = FMath::Clamp(InValue, 0, BaseValue);
	}

	/** The loot value kept with the item, Value at most BaseValue; false when it has none (one half below 0 is none) */
	bool GetKeptValue(int32& OutBaseValue, int32& OutValue) const
	{
		if (BaseValue < 0 || Value < 0)
		{
			return false;
		}
		OutBaseValue = BaseValue;
		OutValue = FMath::Min(Value, BaseValue);
		return true;
	}
};

/** What a player carries between two islands */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunLoadout
{
	GENERATED_BODY()

	/** Cells with an item */
	UPROPERTY(SaveGame)
	TArray<FRunLoadoutItem> Items;

	/** Slot in hand (-1 = bare hands) */
	UPROPERTY(SaveGame)
	int32 ActiveSlot = 0;
};
