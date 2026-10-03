// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WindRules.generated.h"

class UMaterialParameterCollection;

/**
 *  Tunable rules of the wind (DA_WindRules): random gusts of one minute at least MinGapSeconds apart, stronger every
 *  GustsPerLevel gusts, then an ultra violent wind from DeadlineDay on; how hard it pushes; what shelters from it.
 *  Forces from the drag of a standing adult: F = 0.5 * rho * Cd * A * v^2 (about 260 N at 25 m/s).
 *  Times are elapsed play seconds on the day / night clock.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UWindRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Earliest elapsed play time of the first gust (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gust schedule", meta=(ClampMin="0", Units="s"))
	float FirstGustSeconds = 300.0f;

	/** Length of a gust of wind (s, user request: 1 min) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gust schedule", meta=(ClampMin="1", Units="s"))
	float GustDurationSeconds = 60.0f;

	/** Rise and fall of a gust inside its duration (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gust schedule", meta=(ClampMin="0", Units="s"))
	float GustRampSeconds = 8.0f;

	/** Shortest calm between the end of a gust and the start of the next (s, user request: 5 min) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gust schedule", meta=(ClampMin="0", Units="s"))
	float MinGapSeconds = 300.0f;

	/** Random extra calm added to MinGapSeconds, drawn for each gust (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gust schedule", meta=(ClampMin="0", Units="s"))
	float MaxExtraGapSeconds = 180.0f;

	/** Seed of the gust draw: same seed, same gusts on every machine and in every test */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gust schedule")
	int32 Seed = 1337;

	/** The wind rises one level every this many gusts (user request: 6) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="1"))
	int32 GustsPerLevel = 6;

	/** Strength of a gust at level 0 (1 = the full storm the forces below are tuned for) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="0"))
	float BaseGustStrength = 0.5f;

	/** Strength added to the gusts at each level */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="0"))
	float StrengthPerLevel = 0.25f;

	/** Day on which the ultra violent wind sets in and never stops (user request: day 3) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="1"))
	int32 DeadlineDay = 3;

	/** Strength of the ultra violent wind from the deadline day on */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="0"))
	float DeadlineStrength = 2.0f;

	/** Time the ultra violent wind takes to build up at the start of the deadline day (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="0.1", Units="s"))
	float DeadlineRampSeconds = 45.0f;

	/** Breeze between the gusts, for the foliage only (no push) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="0", ClampMax="1"))
	float CalmStrength = 0.06f;

	/** Below this strength the wind pushes nothing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escalation", meta=(ClampMin="0"))
	float ForceThreshold = 0.1f;

	/** Distance upwind within which a building or a bush shelters (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="0"))
	float ShelterDistance = 600.0f;

	/** Height above the feet of the shelter check (cm): a low sand bank does not shelter a standing person */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="0"))
	float ShelterHeight = 70.0f;

	/** Radius of the shelter check (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="1"))
	float ShelterTraceRadius = 25.0f;

	/** Wind left behind a building (fraction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="0", ClampMax="1"))
	float BuildingShelterFactor = 0.1f;

	/** Wind left behind a bush (fraction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="0", ClampMax="1"))
	float BushShelterFactor = 0.35f;

	/** A mesh whose name contains one of these words is a building */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter")
	TArray<FString> BuildingMeshKeywords = { TEXT("Minka"), TEXT("Village_"), TEXT("House"), TEXT("Fare") };

	/** A mesh whose name contains one of these words is a bush */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter")
	TArray<FString> BushMeshKeywords = { TEXT("MikiMiki"), TEXT("Naupaka"), TEXT("Pandanus"), TEXT("Tahinu") };

	/** Seconds between two shelter checks of the same actor */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="0.05", Units="s"))
	float ShelterRefreshInterval = 0.5f;

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

	/** Turbulence inside a gust (fraction of its strength) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gusts", meta=(ClampMin="0", ClampMax="3"))
	float GustAmplitude = 0.7f;

	/** Turbulence frequency (Hz) */
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
