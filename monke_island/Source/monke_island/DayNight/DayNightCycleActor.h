// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightCycleActor.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;
class UDayNightRules;
class UDirectionalLightComponent;
class UPostProcessComponent;

/** Part of the day, from the sun elevation */
UENUM(BlueprintType)
enum class EDayPhase : uint8
{
	Day,
	Dusk,
	Night,
	Dawn
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDayPhaseChangedSignature, EDayPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDayChangedSignature, int32, NewDay);

/** Replicated clock: elapsed play time = AnchorElapsed + (server time - AnchorServerTime) * Speed */
USTRUCT()
struct FDayNightClock
{
	GENERATED_BODY()

	/** Server world time of the last anchor (s) */
	UPROPERTY()
	double AnchorServerTime = 0.0;

	/** Elapsed cycle time at the anchor (s) */
	UPROPERTY()
	double AnchorElapsed = 0.0;

	/** Clock speed: 1 in play, changed only by the debug commands */
	UPROPERTY()
	float Speed = 1.0f;
};

/**
 *  Day / night cycle of an island level (cahier §5): the time of day follows the real elapsed play time.
 *  Drives the level sun (rotation on the real solar path, intensity, colour), a moon light owned by this actor,
 *  the sky light, the height fog and the night exposure.
 *  Server authoritative: the server owns the replicated clock; every machine computes the lighting from it.
 *  Debug (non shipping): monke.SetHour <h>, monke.TimeSpeed <x>.
 */
UCLASS()
class MONKE_ISLAND_API ADayNightCycleActor : public AActor
{
	GENERATED_BODY()

public:

	ADayNightCycleActor();

	/** Returns the cycle of the given world, or nullptr */
	static ADayNightCycleActor* Get(const UObject* WorldContextObject);

	/** Rules of the cycle (DA_DayNightRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayNight")
	TObjectPtr<UDayNightRules> Rules;

	/** Sun of the level; the first atmosphere sun light when empty */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="DayNight")
	TObjectPtr<ADirectionalLight> Sun;

	/** Sky light of the level; the first one when empty */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="DayNight")
	TObjectPtr<ASkyLight> SkyLight;

	/** Elapsed play time the clock starts at (s): 0 in play, set it to test a later moment (dusk, storm, flood) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="DayNight|Debug", meta=(ClampMin="0", Units="s"))
	float StartElapsedSeconds = 0.0f;

	/** Height fog of the level; the first one when empty */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="DayNight")
	TObjectPtr<AExponentialHeightFog> HeightFog;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Elapsed play time of the cycle (s) */
	UFUNCTION(BlueprintPure, Category="DayNight")
	double GetElapsedSeconds() const;

	/** Local solar hour (0-24) */
	UFUNCTION(BlueprintPure, Category="DayNight")
	float GetSolarHour() const;

	/** Current part of the day */
	UFUNCTION(BlueprintPure, Category="DayNight")
	EDayPhase GetPhase() const { return Phase; }

	/** Day number: 1 on the first day, +1 each time a night has passed (same on every machine) */
	UFUNCTION(BlueprintPure, Category="DayNight")
	int32 GetDayNumber() const;

	/** Elapsed play time at which a day begins (s), -1 when the clock does not loop */
	UFUNCTION(BlueprintPure, Category="DayNight")
	double GetDayStartElapsed(int32 Day) const;

	/** Broadcast on every machine when a new day begins (the night has passed) */
	UPROPERTY(BlueprintAssignable, Category="DayNight")
	FDayChangedSignature OnDayChanged;

	/** Daylight factor: 0 at night, 1 in full day */
	UFUNCTION(BlueprintPure, Category="DayNight")
	float GetDaylight() const { return Daylight; }

	/** Jumps the clock to a solar hour (server only, debug) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DayNight")
	void SetSolarHour(float Hour);

	/** Jumps the clock to an elapsed play time (server only, debug) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DayNight")
	void SetElapsedSeconds(double Seconds);

	/** Storm weight on the light (0 clear, 1 full storm): dims the sun, thickens the fog, darkens the exposure */
	UFUNCTION(BlueprintCallable, Category="DayNight")
	void SetStormFactor(float Factor) { StormFactor = FMath::Clamp(Factor, 0.0f, 1.0f); }

	/** Changes the clock speed (server only, debug) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DayNight")
	void SetClockSpeed(float NewSpeed);

	/** Broadcast on every machine when the part of the day changes */
	UPROPERTY(BlueprintAssignable, Category="DayNight")
	FDayPhaseChangedSignature OnPhaseChanged;

private:

	/** Moon light, second atmosphere light */
	UPROPERTY(VisibleAnywhere, Category="DayNight")
	TObjectPtr<UDirectionalLightComponent> Moon;

	/** Unbound post process giving the night exposure */
	UPROPERTY(VisibleAnywhere, Category="DayNight")
	TObjectPtr<UPostProcessComponent> NightExposure;

	/** Replicated clock */
	UPROPERTY(Replicated)
	FDayNightClock Clock;

	/** Current part of the day */
	EDayPhase Phase = EDayPhase::Day;

	/** Day number seen at the last tick, for OnDayChanged */
	int32 LastDay = 1;

	/** Daylight factor (0-1) */
	float Daylight = 1.0f;

	/** Storm weight on the light (0-1) */
	float StormFactor = 0.0f;

	/** Fog density placed in the level */
	float FogDayDensity = 0.02f;

	/** Sun intensity placed in the level, used at noon */
	float SunNoonIntensity = 10.0f;

	/** Fog inscattering colours placed in the level, used in the day */
	FLinearColor FogDayInscattering = FLinearColor::White;
	FLinearColor FogDayDirectionalInscattering = FLinearColor::White;

	/** Solar hour of the last sky light recapture */
	double LastSkyCaptureHour = -1000.0;

	/** Server world time, synchronised on clients */
	double GetServerTime() const;

	/** Finds the level lights left empty and records their placed values */
	void BindLevelLights();

	/** Places the sun and the moon and updates the lights for the current hour */
	void UpdateLighting(bool bForce);
};
