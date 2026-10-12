// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeatherPresentationComponent.generated.h"

class ADayNightCycleActor;
class AWeatherDirector;
class AWindStormActor;
class UAudioComponent;
class UExponentialHeightFogComponent;
class ULightComponent;
class UMaterialParameterCollection;
class UNiagaraComponent;
class UNiagaraSystem;
class UPostProcessComponent;
class USkyLightComponent;
class USoundBase;
class UWeatherRules;
struct FStreamableHandle;

/** Lights of a map without day / night cycle as placed, darkened by the turned weather (test maps) */
struct FWeatherFallbackSky
{
	TWeakObjectPtr<ULightComponent> Sun;
	float SunIntensity = 0.0f;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	float SkyLightIntensity = 0.0f;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	float FogDensity = 0.0f;
	bool bCaptured = false;
};

/**
 *  Local presentation of the weather (T-203), on every machine but a dedicated server: the rain falling around the
 *  local camera (RainSystem in world units, local -Z leaning downwind with the wind, faded through MPC_Weather
 *  RainOpacity, stopped under a roof and under water), the rain and storm wind loops of the turned weather (2D, faded
 *  out when the final storm's own loops take over), the seizure sting and a distant thunder (no flash, cahier §8), and
 *  the darkening of the sky in the maps that have no day / night cycle. Reads the director, never writes gameplay.
 *  Ticks only while there is weather to show.
 */
UCLASS(ClassGroup=(Weather), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UWeatherPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeatherPresentationComponent();

	/** Rules are known on this machine: loads the rain, the parameters and the sounds in the background */
	void Prepare(UWeatherRules* InRules);

	/** The weather turned on this machine: starts showing it; bJustNow plays the sting and the thunder (not for a player
	 *  who arrives in an already turned weather) */
	void OnTurned(bool bJustNow);

	/** The weather cleared (debug) */
	void OnCleared();

	/** Rain shown now (0-1) */
	float GetRainIntensity() const { return RainIntensity; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Rules of the director */
	UPROPERTY(Transient)
	TObjectPtr<UWeatherRules> Rules;

	/** Rain around the camera (created on first rain) */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> RainEffect;

	/** Rain loop of the turned weather */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RainLoop;

	/** Storm wind loop of the turned weather */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WindLoop;

	/** Exposure of the maps without day / night cycle */
	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> FallbackExposure;

	/** Rain material parameters (MPC_Weather) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> Parameters;

	/** Background load of the rain, the parameters and the sounds */
	TSharedPtr<FStreamableHandle> Load;

	/** Lights of a map without day / night cycle as placed */
	FWeatherFallbackSky Fallback;

	/** Wind and day / night cycle of the world, looked up once a second (never every frame) */
	TWeakObjectPtr<AWindStormActor> Wind;
	TWeakObjectPtr<ADayNightCycleActor> Clock;
	double WorldLookupTime = -1.0e6;

	/** Calm zones (WindCalmZone), with the world lookup */
	TArray<FSphere> CalmZones;

	/** What covers the camera, checked every CoverCheckInterval */
	bool bUnderRoof = false;
	bool bUnderwater = false;
	bool bInCalmZone = false;
	float CoverTimer = 0.0f;

	/** Rain shown (faded toward its target) */
	float RainIntensity = 0.0f;

	/** Final storm seen on this machine (0-1, faded), from the ultra violent wind */
	float FinalStorm = 0.0f;

	/** Volume factor of the loops (faded) */
	float LoopFactor = 0.0f;

	/** Sky storm weight last applied to a map without day / night cycle (negative: none yet) */
	float AppliedFallbackSky = -1.0f;

	/** Thunder after the seizure */
	FTimerHandle ThunderTimer;

	/** Director that owns this component */
	AWeatherDirector* GetDirector() const;

	/** Looks the wind, the clock and the calm zones up again once a second */
	void RefreshWorld();

	/** Roof, water and calm zone over the camera */
	void CheckCover(const FVector& View);

	/** Rain system following the camera, tilted by the wind, faded through the parameters */
	void UpdateRain(const FVector& View);

	/** Loops of the turned weather */
	void UpdateLoops();

	/** Sky of the maps without day / night cycle (and of the maps without wind, through their cycle) */
	void UpdateSky(float SkyFactor);

	/** A 2D looping audio component on the owner for this sound */
	UAudioComponent* MakeLoop(USoundBase* Sound, FName Name);

	/** Plays a 2D sound once */
	void PlaySting(USoundBase* Sound) const;
};
