// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindStormActor.generated.h"

class UMaterialParameterCollection;
class UWindDirectionalSourceComponent;
class UWindRules;

/**
 *  Storm wind of an island level: from StartSeconds on the day / night clock the wind rises and gusts,
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

	/** Wind at an elapsed play time, from the rules (pure: same result on every machine) */
	static FVector WindAt(const UWindRules& InRules, double Elapsed);

private:

	/** Engine wind for foliage and cloth */
	UPROPERTY(VisibleAnywhere, Category="Wind")
	TObjectPtr<UWindDirectionalSourceComponent> EngineWind;

	/** Foliage material parameters, loaded at begin play */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> MaterialParameters;

	/** Current wind */
	FVector Wind = FVector::ZeroVector;

	/** Seconds until the next refresh of the character drift */
	float DriftTimer = 0.0f;

	/** Pushes characters, physics objects and boats */
	void ApplyForces(float DeltaSeconds);
};
