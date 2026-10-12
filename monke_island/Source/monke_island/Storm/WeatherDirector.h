// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherDirector.generated.h"

class UWeatherPresentationComponent;
class UWeatherRules;

/** Replicated state of the weather: when it started turning, and why */
USTRUCT()
struct FWeatherTurnState
{
	GENERATED_BODY()

	/** Server world time the weather started turning (s); negative = clear weather, nothing seized */
	UPROPERTY()
	double StartServerTime = -1.0;

	/** What turned it (the seized artifact class, or "console"), for the logs */
	UPROPERTY()
	FName Cause;
};

/**
 *  Weather of a day (T-203): spawned by UWeatherSubsystem on the server, replicated and always relevant. The server
 *  watches what the players hold (UIslanderGrabComponent, every TriggerCheckInterval): seizing a legendary artifact of a
 *  temple (DA_WeatherRules ArtifactClasses, and the ArtifactPool of the temples of the map) turns the weather, once, for
 *  the rest of the day. The turn is a replicated start time: every machine derives the same eased weight from the server
 *  clock (WeatherMath::TurnWeight), which keeps the wind over WindStrength (AWindStormActor reads it) and darkens the sky.
 *  A map without wind gets one at the turn (WindRulesForMapsWithoutWind). The rain, the loops and the stings are the
 *  local presentation (UWeatherPresentationComponent), never on a dedicated server.
 */
UCLASS(NotPlaceable)
class MONKE_ISLAND_API AWeatherDirector : public AActor
{
	GENERATED_BODY()

public:

	AWeatherDirector();

	/** Rules of the weather: set by the subsystem before the director finishes spawning, replicated once */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weather")
	TObjectPtr<UWeatherRules> Rules;

	/** Director of a world, or nullptr */
	static AWeatherDirector* Get(const UObject* WorldContextObject);

	/** Server: turns the weather now (Cause in the logs); once a day, later calls do nothing */
	void TurnWeather(FName Cause);

	/** Server, debug only: back to clear weather */
	void ClearWeather();

	/** True once the weather started turning */
	UFUNCTION(BlueprintPure, Category="Weather")
	bool HasTurned() const { return State.StartServerTime >= 0.0; }

	/** Weight of the turned weather now: 0 clear, eased to 1 over TransitionSeconds (same on every machine) */
	UFUNCTION(BlueprintPure, Category="Weather")
	float GetTurnWeight() const;

	/** Mean wind strength the turned weather keeps the wind over now */
	float GetWindFloor() const;

	/** Storm weight the turned weather gives the sky now (0-1) */
	float GetSkyFactor() const;

	/** Server world time the weather started turning, negative while clear */
	double GetTurnStartServerTime() const { return State.StartServerTime; }

	/** Server world time on this machine (the replicated clock of the game state) */
	double GetServerTime() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Replicated turn of the weather */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FWeatherTurnState State;

	/** A client receives the turn (or its clearing) */
	UFUNCTION()
	void OnRep_State(const FWeatherTurnState& Previous);

	/** Rain, loops, stings and sky of the maps without day / night cycle, on this machine */
	UPROPERTY(VisibleAnywhere, Category="Weather")
	TObjectPtr<UWeatherPresentationComponent> Presentation;

	/** Classes whose seizure turns the weather (rules list and pools of the temples), refreshed every SiteRefreshInterval */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UClass>> TriggerClasses;

	/** World time of the last refresh of TriggerClasses */
	double TriggerClassesTime = -1.0e6;

	/** Server timer of the seizure check */
	FTimerHandle SeizureTimer;

	/** Server: what the players hold; a seized artifact turns the weather */
	void CheckSeizure();

	/** Server: the artifact classes of the rules and of the temples present */
	void RefreshTriggerClasses();

	/** Server: a wind for the maps that have none (WindRulesForMapsWithoutWind) */
	void SpawnWindIfMissing();

	/** Every machine: the state changed (bTurnedNow: it turned during play, not already turned when it arrived) */
	void ApplyState(bool bTurnedNow);
};
