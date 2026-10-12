// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShoreAmbienceRules.generated.h"

class USoundBase;

/**
 *  Tunable rules of the shore ambience (DA_ShoreAmbienceRules): waves breaking on the sand, heard from the
 *  nearest shore line around the local player, and the open sea, louder the more water there is around.
 *  Cosmetic and local: never replicated, nothing on a dedicated server.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UShoreAmbienceRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Waves breaking on the sand (mono loop), placed on the nearest shore line */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds")
	TSoftObjectPtr<USoundBase> SurfSound;

	/** Open sea and distant reef (stereo loop), around the listener */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds")
	TSoftObjectPtr<USoundBase> SeaSound;

	/** Volume of the surf */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0"))
	float SurfVolume = 0.25f;

	/** Volume of the open sea when there is only water around */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0"))
	float SeaVolume = 0.12f;

	/** The surf is at full volume within this distance of the shore line (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surf", meta=(ClampMin="0", Units="cm"))
	float SurfInnerRadius = 1000.0f;

	/** Distance over which the surf fades out beyond SurfInnerRadius (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surf", meta=(ClampMin="1", Units="cm"))
	float SurfFalloff = 6000.0f;

	/** Distances at which the ground is probed around the listener (cm, increasing); the shore is searched up to the last one */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Probe")
	TArray<float> ProbeRadii = { 400.0f, 1200.0f, 2500.0f, 4500.0f, 7000.0f };

	/** Directions probed around the listener */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Probe", meta=(ClampMin="4", ClampMax="64"))
	int32 ProbeDirections = 16;

	/** Time between two probes (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Probe", meta=(ClampMin="0.05", Units="s"))
	float ProbeInterval = 0.5f;

	/** Probe traces start this high above the water surface and go as deep below it (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Probe", meta=(ClampMin="100", Units="cm"))
	float ProbeHeight = 5000.0f;

	/** Speed at which the surf slides along the shore to its new spot (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surf", meta=(ClampMin="0"))
	float SurfMoveSpeed = 1500.0f;

	/** Speed of the volume fades (1/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0"))
	float FadeSpeed = 1.5f;
};
