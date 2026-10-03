// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PacificationRules.generated.h"

/**
 *  Tunable rules of the zone pacification (DA_PacificationRules, cahier §6.2): flag planting, the 20 s sermon
 *  recited without moving at the foot of the flag, and what interrupts it. Standard values of the cahier.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UPacificationRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Consecutive seconds the officiant must recite the sermon without being interrupted (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sermon", meta=(ClampMin="1", Units="s"))
	float SermonDuration = 20.0f;

	/** Horizontal distance from the start point the officiant may drift (network jitter) before the sermon breaks (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sermon", meta=(ClampMin="0", Units="cm"))
	float PositionToleranceCm = 10.0f;

	/** Vertical distance from the start point the officiant may drift (slopes, water bobbing) before the sermon breaks; a jump goes over it (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sermon", meta=(ClampMin="0", Units="cm"))
	float VerticalToleranceCm = 25.0f;

	/** Farthest horizontal distance from the flag socket where a player can plant the flag or start the sermon (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flag", meta=(ClampMin="0", Units="cm"))
	float FlagReachRadiusCm = 250.0f;

	/** Radius of the zone perimeter around the flag socket: a living designated threat inside it breaks the sermon (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zone", meta=(ClampMin="0", Units="cm"))
	float PerimeterRadiusCm = 1500.0f;

	/** Seconds between two server checks of the sermon conditions (0 = every frame) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sermon", meta=(ClampMin="0", Units="s"))
	float CheckIntervalSeconds = 0.0f;

	/** Smallest damage that breaks the sermon (0 = any damage event above zero) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sermon", meta=(ClampMin="0"))
	float MinInterruptDamage = 0.0f;

	/** Replicated progress is only resent when it moves by at least this share (0-1): limits the bandwidth */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.001", ClampMax="0.5"))
	float ProgressReplicationStep = 0.01f;
};
