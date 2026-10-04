// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "CargoTypes.generated.h"

class AActor;

/** Resource family (cahier §5): the quota counts Wood and Food, the tax applies to the sale of Riches (§7) */
UENUM(BlueprintType)
enum class EResourceCategory : uint8
{
	Wood	UMETA(DisplayName="Bois"),
	Food	UMETA(DisplayName="Nourriture"),
	Riches	UMETA(DisplayName="Richesses")
};

/** Life cycle of a resource (cahier §5: Libre / Portée / Arrimée / Perdue) */
UENUM(BlueprintType)
enum class EResourceState : uint8
{
	/** Simulates physics in the world, not counted */
	Free	UMETA(DisplayName="Libre"),
	/** Held by a player */
	Carried	UMETA(DisplayName="Portée"),
	/** Secured in a cargo slot, physics off, counted */
	Stowed	UMETA(DisplayName="Arrimée"),
	/** Destroyed, sunk or left behind: never counted again */
	Lost	UMETA(DisplayName="Perdue")
};

/** Why a cargo request was refused; every value maps to a readable message (FCargoLedger::GetRefusalText) */
UENUM(BlueprintType)
enum class ECargoRefusal : uint8
{
	None,
	/** Request made on a client: only the server stows */
	NotAuthority,
	/** Cargo locked (boat moving, extraction frozen) */
	Locked,
	/** Not a resource, or invalid identity, mass or quantity */
	InvalidResource,
	/** Slot index out of range */
	InvalidSlot,
	/** The targeted slot already holds an object */
	SlotTaken,
	/** Nothing to unstow in this slot */
	SlotEmpty,
	/** Every slot is taken */
	NoFreeSlot,
	/** The object would exceed the mass limit */
	TooHeavy,
	/** The object is already stowed (here or elsewhere) */
	AlreadyStowed,
	/** The object is lost */
	ResourceLost,
	/** The object is too far from the slot */
	TooFar
};

/** Stable identity of a resource for the whole expedition (cahier §5), generated once by the server */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FResourceId
{
	GENERATED_BODY()

	/** Unique id, invalid (zero) until the server assigns it */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Resource")
	FGuid Guid;

	/** True once assigned */
	bool IsValid() const { return Guid.IsValid(); }

	/** Creates a new unique id */
	static FResourceId NewId()
	{
		FResourceId Id;
		Id.Guid = FGuid::NewGuid();
		return Id;
	}

	/** Short text for logs */
	FString ToString() const { return Guid.ToString(EGuidFormats::Short); }

	bool operator==(const FResourceId& Other) const { return Guid == Other.Guid; }
	bool operator!=(const FResourceId& Other) const { return Guid != Other.Guid; }

	friend uint32 GetTypeHash(const FResourceId& Id) { return GetTypeHash(Id.Guid); }
};

/** What the cargo counts for one stowed resource, frozen at stow time */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCargoEntry
{
	GENERATED_BODY()

	/** Identity of the resource */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	FResourceId Id;

	/** Family of the resource */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	EResourceCategory Category = EResourceCategory::Wood;

	/** Mass counted against the cargo limit (kg) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	float MassKg = 0.0f;

	/** Sale value (pieces) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 Value = 0;

	/** Units delivered to the quota (1 log = 1 wood unit) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 Quantity = 1;
};

/** One cargo slot, replicated: empty when bOccupied is false */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCargoSlot
{
	GENERATED_BODY()

	/** True when an object is stowed here */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	bool bOccupied = false;

	/** What is counted for the stowed object */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	FCargoEntry Entry;

	/** The stowed actor (may be null in the pure ledger specs, or once destroyed) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	TObjectPtr<AActor> Actor = nullptr;
};

/** Totals by category of the stowed resources: quantities for the quota, values for the sale */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCargoTotals
{
	GENERATED_BODY()

	/** Wood units stowed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 WoodQuantity = 0;

	/** Food units stowed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 FoodQuantity = 0;

	/** Riches units stowed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 RichesQuantity = 0;

	/** Value of the stowed wood (pieces) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 WoodValue = 0;

	/** Value of the stowed food (pieces) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 FoodValue = 0;

	/** Value of the stowed riches (pieces) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 RichesValue = 0;

	/** Total value of the cargo (pieces) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 TotalValue = 0;

	/** Total mass stowed (kg) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	float MassKg = 0.0f;

	/** Number of occupied slots */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	int32 StowedCount = 0;

	/** Adds one entry */
	void Add(const FCargoEntry& Entry)
	{
		switch (Entry.Category)
		{
		case EResourceCategory::Wood:	WoodQuantity += Entry.Quantity; WoodValue += Entry.Value; break;
		case EResourceCategory::Food:	FoodQuantity += Entry.Quantity; FoodValue += Entry.Value; break;
		case EResourceCategory::Riches:	RichesQuantity += Entry.Quantity; RichesValue += Entry.Value; break;
		}
		TotalValue += Entry.Value;
		MassKg += Entry.MassKg;
		++StowedCount;
	}

	/** Quantity of a category */
	int32 GetQuantity(EResourceCategory Category) const
	{
		switch (Category)
		{
		case EResourceCategory::Wood:	return WoodQuantity;
		case EResourceCategory::Food:	return FoodQuantity;
		case EResourceCategory::Riches:	return RichesQuantity;
		}
		return 0;
	}

	/** Value of a category */
	int32 GetValue(EResourceCategory Category) const
	{
		switch (Category)
		{
		case EResourceCategory::Wood:	return WoodValue;
		case EResourceCategory::Food:	return FoodValue;
		case EResourceCategory::Riches:	return RichesValue;
		}
		return 0;
	}
};

/** Frozen list of the stowed resources handed to the extraction / economy (cahier §6.5, §7 step 1) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCargoSnapshot
{
	GENERATED_BODY()

	/** Stowed resources, in slot order; a copy, not affected by later changes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	TArray<FCargoEntry> Entries;

	/** Totals of Entries */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
	FCargoTotals Totals;
};
