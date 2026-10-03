// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WindRules.generated.h"

class UMaterialParameterCollection;

/**
 *  Tunable rules of the storm wind (DA_WindRules): when it rises, how hard it pushes and how it gusts.
 *  Forces from the drag of a standing adult: F = 0.5 * rho * Cd * A * v^2 (about 260 N at 25 m/s).
 *  Times are elapsed play seconds on the day / night clock.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UWindRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Elapsed play time when the wind starts rising (s): 1:20 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float StartSeconds = 80.0f;

	/** Time for the wind to reach its full strength (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.1", Units="s"))
	float RampSeconds = 20.0f;

	/** Drift of a character pushed by the full mean wind (cm/s): ground braking cannot cancel it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Force", meta=(ClampMin="0", Units="cm/s"))
	float CharacterDriftSpeed = 220.0f;

	/** Seconds between two refreshes of the character drift */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Force", meta=(ClampMin="0.05", Units="s"))
	float DriftRefreshInterval = 0.2f;

	/** Wind pressure on objects at full mean wind (Pa): 0.5 * rho * Cd * v^2, about 375 at 25 m/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Force", meta=(ClampMin="0"))
	float WindPressurePa = 375.0f;

	/** Highest acceleration the wind gives an object (m/s2), so a pebble does not fly away */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Force", meta=(ClampMin="0"))
	float MaxObjectAcceleration = 25.0f;

	/** Gust strength on top of the mean wind (fraction of the mean) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gusts", meta=(ClampMin="0", ClampMax="3"))
	float GustAmplitude = 0.7f;

	/** Gust frequency (Hz) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gusts", meta=(ClampMin="0.01"))
	float GustFrequency = 0.35f;

	/** Swing of the wind direction around its mean (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gusts", meta=(ClampMin="0", ClampMax="180"))
	float DirectionSwingDeg = 25.0f;

	/** World yaw the wind blows toward (degrees); 180 = from the east trade winds toward the west (map X = east) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Direction")
	float BlowTowardYawDeg = 180.0f;

	/** Wind speed at full strength for foliage and cloth (m/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0"))
	float VisualWindSpeed = 25.0f;

	/** Parameters read by the foliage materials (MPC_Wind: WindStrength, WindDirection) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	TSoftObjectPtr<UMaterialParameterCollection> MaterialParameters;

	/** Storm weight given to the sky and light at full strength (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0", ClampMax="1"))
	float StormSkyFactor = 1.0f;
};
