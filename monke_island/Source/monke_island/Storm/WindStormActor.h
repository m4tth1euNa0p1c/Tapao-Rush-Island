// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Flood/HazardClock.h"
#include "WindStormActor.generated.h"

class AExpeditionGameState;
class UMaterialParameterCollection;
class UWindDirectionalSourceComponent;
class UWindRules;

/** Shelter of one actor, refreshed every ShelterRefreshInterval */
struct FWindShelterSample
{
	/** Wind left at the actor (1 in the open) */
	float Factor = 1.0f;

	/** World time of the check */
	double Time = -1.0e6;
};

/**
 *  Wind of an island level (T-067): random gusts of one minute, at least MinGapSeconds apart, one level stronger
 *  every GustsPerLevel gusts, then an ultra violent wind that never stops. One clock (T-098): in a map with an
 *  expedition, gusts count from its start and the ultra violent wind sets in at the start of the submersion
 *  (or ExpeditionStormLeadSeconds before); without expedition, the day / night clock and DeadlineDay (fallback).
 *  A building or a bush upwind shelters what stands behind it. During a gust the wind
 *  makes the characters drift (additive root motion, predicted), pushes the free physics objects and the boats
 *  in proportion to the area they offer to the wind, drives the engine wind (foliage, cloth)
 *  and darkens the sky through the day / night cycle.
 *  The wind is a pure function of the replicated clock, so every machine computes the same gusts; forces are
 *  applied by the server, plus the owning client for its own character (movement prediction).
 */
UCLASS()
class MONKE_ISLAND_API AWindStormActor : public AActor
{
	GENERATED_BODY()

public:

	AWindStormActor();

	/** Rules of the wind (DA_WindRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind")
	TObjectPtr<UWindRules> Rules;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Current wind: direction (unit, horizontal) scaled by the strength (0 calm, 1 full storm, gusts above) */
	UFUNCTION(BlueprintPure, Category="Wind")
	FVector GetWind() const { return Wind; }

	/** Acceleration the wind gives a physics actor at rest right now (cm/s2), as applied by ApplyForces */
	FVector GetWindAccelerationOn(const AActor* Actor) const;

	/** Wind at an elapsed play time (pure: same result on every machine); DeadlineStartElapsed < 0 = no deadline */
	static FVector WindAt(const UWindRules& InRules, double Elapsed, double DeadlineStartElapsed = -1.0);

	/** Index of the last gust started (0-based), INDEX_NONE before the first */
	UFUNCTION(BlueprintPure, Category="Wind")
	int32 GetGustIndex() const { return GustIndex; }

	/** Level of the gusts (0, then +1 every GustsPerLevel gusts) */
	UFUNCTION(BlueprintPure, Category="Wind")
	int32 GetGustLevel() const;

	/** True while a gust blows or the ultra violent wind */
	UFUNCTION(BlueprintPure, Category="Wind")
	bool IsBlowing() const;

	/** True once the ultra violent wind set in (submersion of the expedition, or deadline day without expedition) */
	UFUNCTION(BlueprintPure, Category="Wind")
	bool IsDeadlineWind() const { return bDeadline; }

	/** Clock that drives the wind (Undecided until the game state is known on this machine) */
	EHazardClockSource GetClockSource() const { return ClockSource; }

	/** Wind left at an actor by the buildings and bushes upwind (1 in the open), cached per actor */
	float GetShelterFactor(const AActor* Actor) const;

	/** Returns the wind of the given world, or nullptr */
	static AWindStormActor* Get(const UObject* WorldContextObject);

private:

	/** Engine wind for foliage and cloth */
	UPROPERTY(VisibleAnywhere, Category="Wind")
	TObjectPtr<UWindDirectionalSourceComponent> EngineWind;

	/** Foliage material parameters, loaded at begin play */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> MaterialParameters;

	/** Current wind */
	FVector Wind = FVector::ZeroVector;

	/** Last gust started */
	int32 GustIndex = INDEX_NONE;

	/** The ultra violent wind has set in */
	bool bDeadline = false;

	/** Clock that drives the wind, decided once */
	EHazardClockSource ClockSource = EHazardClockSource::Undecided;

	/** Expedition followed in expedition mode */
	TWeakObjectPtr<AExpeditionGameState> Expedition;

	/** Decides the clock once the game state is known; true when decided */
	bool ResolveClock();

	/** Shelter per actor */
	mutable TMap<TWeakObjectPtr<const AActor>, FWindShelterSample> ShelterCache;

	/** Shelter check behind the obstacles upwind of a point (1 in the open) */
	float ComputeShelter(const AActor* Actor, const FVector& Feet) const;

	/** Seconds until the next refresh of the character drift */
	float DriftTimer = 0.0f;

	/** Pushes characters, physics objects and boats */
	void ApplyForces(float DeltaSeconds);
};
