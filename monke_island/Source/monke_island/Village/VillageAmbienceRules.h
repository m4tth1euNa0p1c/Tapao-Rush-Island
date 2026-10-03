// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VillageAmbienceRules.generated.h"

class UStaticMesh;

/**
 *  Life of a Japanese village, cosmetic only (DA_VillageAmbience): lanterns that follow the day, butterflies by day,
 *  fireflies at night (AVillageAmbienceActor). Computed on every machine, never replicated.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UVillageAmbienceRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** The ambience of a village runs only while a local camera is this close to its centre (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambience", meta=(ClampMin="1000"))
	float ActiveRadius = 18000.0f;

	/** Part of the placed intensity of the house lamps (lights tagged LightKind.Interior) kept in full day */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights", meta=(ClampMin="0", ClampMax="1"))
	float InteriorDayFactor = 0.35f;

	/** Part of the placed intensity of the stone lanterns (LightKind.Lantern) kept in full day */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights", meta=(ClampMin="0", ClampMax="1"))
	float LanternDayFactor = 0.0f;

	/** Seconds between two updates of the lights */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights", meta=(ClampMin="0.1", Units="s"))
	float LightUpdateInterval = 1.0f;

	/** Butterflies flying around the village by day */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Butterflies", meta=(ClampMin="0", ClampMax="64"))
	int32 ButterflyCount = 14;

	/** Butterfly mesh (wings span its Y axis, flapping squeezes Y) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Butterflies")
	TObjectPtr<UStaticMesh> ButterflyMesh;

	/** Flying speed of a butterfly (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Butterflies", meta=(ClampMin="0"))
	float ButterflySpeed = 150.0f;

	/** Wing beats per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Butterflies", meta=(ClampMin="0"))
	float ButterflyFlapHz = 6.0f;

	/** Height range of the butterflies above the village centre (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Butterflies")
	FVector2D ButterflyHeight = FVector2D(40.0f, 260.0f);

	/** Part of the village radius where the butterflies fly */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Butterflies", meta=(ClampMin="0.05", ClampMax="1"))
	float ButterflyRadiusFraction = 0.6f;

	/** Fireflies glowing around the village at night */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireflies", meta=(ClampMin="0", ClampMax="128"))
	int32 FireflyCount = 24;

	/** Firefly mesh (small, emissive) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireflies")
	TObjectPtr<UStaticMesh> FireflyMesh;

	/** Drifting speed of a firefly (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireflies", meta=(ClampMin="0"))
	float FireflySpeed = 45.0f;

	/** Blinks per second of a firefly */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireflies", meta=(ClampMin="0"))
	float FireflyBlinkHz = 0.5f;

	/** Height range of the fireflies above the village centre (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireflies")
	FVector2D FireflyHeight = FVector2D(30.0f, 220.0f);

	/** Daylight (ADayNightCycleActor, 0 at night, 1 in full day) under which the fireflies replace the butterflies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireflies", meta=(ClampMin="0", ClampMax="1"))
	float NightDaylight = 0.3f;
};
