// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DayNightRules.generated.h"

/** One key of the clock: at ElapsedSeconds of real play time, the island shows SolarHour */
USTRUCT(BlueprintType)
struct FDayNightKey
{
	GENERATED_BODY()

	/** Real play time since the start of the cycle (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayNight", meta=(ClampMin="0", Units="s"))
	float ElapsedSeconds = 0.0f;

	/** Local solar hour shown at that time, unwrapped (27 = 3 h the next day) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayNight")
	float SolarHour = 9.0f;
};

/**
 *  Tunable rules of the day / night cycle (DA_DayNightRules).
 *  Cahier §5: the time of day follows the real elapsed time, not the remaining timer; dusk at 120 s and night
 *  at 240 s by default. The clock is a piecewise linear curve of keys; when the last key is exactly one day after
 *  the first, the cycle loops.
 *  The sun follows the real solar path of Raraka (latitude, day of the year); the map frame is X = east, Y = south.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UDayNightRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UDayNightRules();

	/** Clock keys, sorted by time and by hour: day 0 s, dusk 120 s, night 240 s, dawn, back to the start hour */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock")
	TArray<FDayNightKey> Keys;

	/** Loops the clock when the last key is one day (24 h) after the first */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock")
	bool bLoop = true;

	/** Seconds between two lighting updates */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock", meta=(ClampMin="0.0", Units="s"))
	float UpdateInterval = 0.05f;

	/** Latitude of the island (degrees, negative south): Raraka */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun", meta=(ClampMin="-90", ClampMax="90"))
	float LatitudeDeg = -16.18f;

	/** Day of the year (1-365) giving the solar declination */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun", meta=(ClampMin="1", ClampMax="365"))
	int32 DayOfYear = 276;

	/** World yaw of the geographic north (degrees): -90 for the map frame X = east, Y = south */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun")
	float NorthYawDeg = -90.0f;

	/** Sun elevation (degrees) under which the sun light is off */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun")
	float SunOffElevationDeg = -4.0f;

	/** Sun elevation (degrees) from which the sun has its full noon intensity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun")
	float SunFullElevationDeg = 12.0f;

	/** Sun colour temperature at the horizon (K) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun", meta=(ClampMin="1500", ClampMax="15000"))
	float SunHorizonTemperature = 2700.0f;

	/** Sun colour temperature high in the sky (K) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun", meta=(ClampMin="1500", ClampMax="15000"))
	float SunNoonTemperature = 6000.0f;

	/** Sun elevation (degrees) from which the sun has its noon temperature */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sun", meta=(ClampMin="1", ClampMax="90"))
	float SunNoonTemperatureElevationDeg = 35.0f;

	/** Moon intensity as a fraction of the sun noon intensity (night stays readable, cahier §8) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Moon", meta=(ClampMin="0", ClampMax="1"))
	float MoonIntensityFraction = 0.06f;

	/** Moon colour temperature (K): cold blue night */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Moon", meta=(ClampMin="1500", ClampMax="15000"))
	float MoonTemperature = 9500.0f;

	/** Lowest elevation of the moon (degrees), so it always lights the island at night */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Moon", meta=(ClampMin="0", ClampMax="90"))
	float MoonMinElevationDeg = 30.0f;

	/** Whether the moon casts shadows (a second shadowed directional light costs GPU time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Moon")
	bool bMoonCastsShadows = false;

	/** Auto-exposure bias at full night (EV): darker than the day, so the night feels like night */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Exposure")
	float NightExposureBias = -1.5f;

	/** Lowest auto-exposure brightness (EV100): stops the eye from turning the night into day */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Exposure")
	float MinExposureEV100 = -1.0f;

	/** Height fog inscattering kept at full night (fraction of the day colour) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog", meta=(ClampMin="0", ClampMax="1"))
	float NightFogFraction = 0.04f;

	/** Sun intensity kept under a full storm (fraction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Storm", meta=(ClampMin="0", ClampMax="1"))
	float StormSunFraction = 0.25f;

	/** Fog density multiplier under a full storm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Storm", meta=(ClampMin="1"))
	float StormFogMultiplier = 4.0f;

	/** Extra exposure bias under a full storm (EV) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Storm")
	float StormExposureBias = -0.7f;

	/** Sun elevation (degrees) above which it is day; dusk / dawn down to minus this value, night below */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Phases", meta=(ClampMin="0", ClampMax="30"))
	float TwilightElevationDeg = 6.0f;

	/** Minimum change of solar hour between two recaptures of a non real-time sky light (h) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky", meta=(ClampMin="0.01"))
	float SkyRecaptureHourStep = 0.25f;
};
