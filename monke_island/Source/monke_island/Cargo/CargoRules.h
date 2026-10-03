// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CargoRules.generated.h"

/**
 *  Tunable rules of a cargo hold (DA_CargoRules, cahier §5): capacity, slot placement, reach.
 *  The class defaults are the cahier values (12 slots, 150 kg); a boat without rules uses them.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UCargoRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Number of cargo slots, one object each (cahier §5: 12; shop upgrade +4 later) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capacity", meta=(ClampMin="1", ClampMax="64"))
	int32 SlotCount = 12;

	/** Mass limit of the stowed objects (kg, cahier §5: 150) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capacity", meta=(ClampMin="0"))
	float MaxMassKg = 150.0f;

	/** Prefix of the slot sockets on the hull (Slot_0, Slot_1...); missing sockets use the grid below */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement")
	FString SlotSocketPrefix = TEXT("Slot_");

	/** Fallback grid: number of columns across the beam */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement", meta=(ClampMin="1"))
	int32 SlotColumns = 2;

	/** Fallback grid: distance between rows (X, along the keel) and columns (Y) (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement")
	FVector2D SlotSpacingCm = FVector2D(55.0f, 60.0f);

	/** Fallback grid: centre of the grid in the hull's local space (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement")
	FVector SlotGridOrigin = FVector(0.0f, 0.0f, 10.0f);

	/** Server check: the object must be this close to its slot to be stowed (cm, 0 = no check) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="0"))
	float MaxStowDistanceCm = 400.0f;

	/** Height an unstowed object is lifted before its physics resumes, to clear the hull (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="0"))
	float UnstowLiftCm = 15.0f;

	/** Stowed objects keep only query collision (still aimable, no push on the hull) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules")
	bool bQueryOnlyWhenStowed = true;
};
