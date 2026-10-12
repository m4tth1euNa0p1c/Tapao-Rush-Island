// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeatherRules.generated.h"

class UMaterialParameterCollection;
class UNiagaraSystem;
class USoundBase;
class UWindRules;

/**
 *  Tunable rules of the weather (DA_WeatherRules, T-203, cahier L5): seizing one of the legendary artifacts of a temple
 *  turns the weather in TransitionSeconds (violent wind, rain, dark sky) until the end of the day (the next island is
 *  another world, clear again). The rain also falls in the final storm of the submersion (bRainInFinalStorm).
 *  Loaded by UWeatherSubsystem from its RulesPath (DefaultGame.ini); AWeatherDirector carries it to the clients.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UWeatherRules : public UDataAsset
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ Trigger

	/** Actor classes whose seizure by a player turns the weather (the six legendary artifacts of DA_TempleReliquaryRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger")
	TArray<TSoftClassPtr<AActor>> ArtifactClasses;

	/** Also the classes of the ArtifactPool of every temple of the map (AMaraeSite): an artifact added to the pool later
	 *  turns the weather without editing this list */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger")
	bool bArtifactPoolsOfSites = true;

	/** Only an artifact placed by a temple (spawned by an AMaraeSite) turns the weather (owner request: "l'artefact du
	 *  temple"); the same legendary artifacts in the village shrines (T-189) do not. False: any actor of these classes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger")
	bool bOnlyTempleArtifacts = true;

	/** Seconds between two checks of what the players hold (server): a seizure is seen at most this late */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger", meta=(ClampMin="0.05", Units="s"))
	float TriggerCheckInterval = 0.25f;

	/** Seconds between two searches of the temples of the map for their artifact pools (they are built after the start) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger", meta=(ClampMin="0.5", Units="s"))
	float SiteRefreshInterval = 5.0f;

	// ------------------------------------------------------------------ Transition

	/** Time the weather takes to turn once an artifact is seized (owner request: 10 s), eased in and out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Transition", meta=(ClampMin="0", Units="s"))
	float TransitionSeconds = 10.0f;

	// ------------------------------------------------------------------ Wind

	/** Mean strength the wind no longer drops under once the weather turned: 1 = the full storm the forces of
	 *  DA_WindRules are tuned for (character drift CharacterDriftSpeed, objects pushed, palms bent); the gusts and the
	 *  ultra violent wind of the submersion (DeadlineStrength) still rule when stronger */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0"))
	float WindStrength = 1.0f;

	/** Maps without AWindStormActor (test maps): a wind is spawned with these rules when the weather turns; empty = no
	 *  wind in such maps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind")
	TSoftObjectPtr<UWindRules> WindRulesForMapsWithoutWind;

	// ------------------------------------------------------------------ Sky

	/** Storm weight given to the sky and the light once turned (0-1): the day / night cycle dims the sun, thickens the
	 *  fog and darkens the exposure (DA_DayNightRules StormSunFraction, StormFogMultiplier, StormExposureBias) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky", meta=(ClampMin="0", ClampMax="1"))
	float SkyStormFactor = 1.0f;

	/** Maps without day / night cycle (test maps): fraction of the placed sun intensity kept at full storm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky|Maps without day night cycle", meta=(ClampMin="0", ClampMax="1"))
	float FallbackSunFraction = 0.3f;

	/** Maps without day / night cycle: fraction of the placed sky light intensity kept at full storm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky|Maps without day night cycle", meta=(ClampMin="0", ClampMax="1"))
	float FallbackSkyLightFraction = 0.5f;

	/** Maps without day / night cycle: height fog density multiplier at full storm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky|Maps without day night cycle", meta=(ClampMin="1"))
	float FallbackFogMultiplier = 3.0f;

	/** Maps without day / night cycle: exposure bias at full storm (EV, negative = darker; auto exposure would
	 *  otherwise brighten the dimmed sun back) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky|Maps without day night cycle", meta=(ClampMax="0", Units="EV"))
	float FallbackExposureBias = -0.7f;

	// ------------------------------------------------------------------ Rain

	/** Rain falling around the local camera (NS_Rain: world space GPU sprites, local -Z down, tilted by the game with the
	 *  wind); its material reads MaterialParameters. Spawned on every machine but a dedicated server */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	TSoftObjectPtr<UNiagaraSystem> RainSystem;

	/** Parameters of the rain material (MPC_Weather): RainOpacity (0-1), the fade of the rain written by the game. The
	 *  drops are lit translucent sprites (M_Rain_Streak): day, night and storm light them like the rest of the scene */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	TSoftObjectPtr<UMaterialParameterCollection> MaterialParameters;

	/** The rain also falls from the start of the ultra violent wind of the submersion (final storm, cahier §5) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	bool bRainInFinalStorm = true;

	/** Height of the rain system above the camera (cm): its spawn volume (in the system) is centred there */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0", Units="cm"))
	float RainHeight = 900.0f;

	/** Tilt of the falling drops per unit of wind strength (degrees from the vertical) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0", ClampMax="80", Units="deg"))
	float RainTiltPerWind = 18.0f;

	/** Largest tilt of the drops (degrees): a readable slant even in the ultra violent wind */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0", ClampMax="80", Units="deg"))
	float MaxRainTiltDeg = 35.0f;

	/** Rain kept inside the calm zones (actors tagged WindCalmZone: the galleon and the water around it), 0 = none.
	 *  The cahier does not keep the galleon dry: it rains on it like anywhere else by default */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0", ClampMax="1"))
	float RainInCalmZones = 1.0f;

	/** Time the rain takes to fade in or out when the camera goes under a roof, under water or into a calm zone (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0.05", Units="s"))
	float RainFadeSeconds = 1.0f;

	/** Seconds between two checks of what covers the camera (roof, water, calm zone) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0.05", Units="s"))
	float CoverCheckInterval = 0.25f;

	/** Height above the camera searched for a roof (cm): the drops fall through meshes, so the rain stops under one */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0", Units="cm"))
	float RoofCheckHeight = 2500.0f;

	/** A static mesh above the camera whose name contains one of these words is a roof (houses, kura, temple, fort,
	 *  galleon decks); palms and bushes are not */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	TArray<FString> RoofMeshKeywords = { TEXT("Minka"), TEXT("Village_"), TEXT("House"), TEXT("Fare"), TEXT("Kura"),
		TEXT("Temple_"), TEXT("Fort_"), TEXT("Galleon") };

	// ------------------------------------------------------------------ Sound

	/** Rain loop of the turned weather, 2D, faded with it (SW_Exp_Rain, class SC_Ambience). The loops of the final storm
	 *  are UExpeditionAudioSubsystem's (T-234): this one fades out when the ultra violent wind sets in */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> RainSound;

	/** Volume of the rain loop at full rain */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0", ClampMax="4"))
	float RainVolume = 0.7f;

	/** Storm wind loop of the turned weather, 2D, same rule as RainSound. Empty by default: UExpeditionAudioSubsystem
	 *  (T-234) already plays its strong wind loop while the wind is strong, which the turned weather keeps it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> StormWindSound;

	/** Volume of the storm wind loop once turned */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0", ClampMax="4"))
	float StormWindVolume = 0.5f;

	/** Share of the loops still heard under a roof (muffled by IndoorLowPassHz) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0", ClampMax="1"))
	float IndoorVolume = 0.45f;

	/** Low-pass of the loops under a roof (Hz) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="100", Units="Hz"))
	float IndoorLowPassHz = 1500.0f;

	/** Played once on every machine when the artifact is seized (SW_Exp_ArtifactCurse) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> SeizeSound;

	/** A distant thunder as the sky turns, ThunderDelaySeconds after the seizure; never a flash (cahier §8) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> ThunderSound;

	/** Delay of the thunder after the seizure (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0", Units="s"))
	float ThunderDelaySeconds = 2.0f;

	/** Volume of the seizure sting and of the thunder */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0", ClampMax="4"))
	float StingVolume = 0.8f;
};
