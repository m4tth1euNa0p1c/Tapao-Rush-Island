// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VillageAmbienceActor.generated.h"

class UInstancedStaticMeshComponent;
class ULightComponent;
class UVillageAmbienceRules;

/**
 *  Life of one Japanese village (docs/features/villages-japonais.md), cosmetic and local to every machine (not
 *  replicated): its lights (actors tagged VillageLight and "Village.<Id>", LightKind.Interior or LightKind.Lantern)
 *  follow the day, butterflies fly around it by day and fireflies glow at night. The flyers only move while a local
 *  camera is within ActiveRadius; farther, the actor ticks twice a second for the lights.
 */
UCLASS()
class MONKE_ISLAND_API AVillageAmbienceActor : public AActor
{
	GENERATED_BODY()

public:

	AVillageAmbienceActor();

	/** Rules of the ambience (DA_VillageAmbience) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	TObjectPtr<UVillageAmbienceRules> Rules;

	/** Id of the village (V01...): its lights carry the tag "Village.<Id>" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	FName VillageId;

	/** Radius of the village around this actor (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="100"))
	float VillageRadius = 9000.0f;

	/** Adds a light that is not an actor of the level (a village built at runtime by AVillageSpawner); its current
	 *  intensity is its night intensity */
	void AddLight(ULightComponent* Light, bool bInterior);

protected:

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Butterflies, one instance each */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Butterflies;

	/** Fireflies, one instance each */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Fireflies;

	/** A butterfly or a firefly */
	struct FFlyer
	{
		FVector Location = FVector::ZeroVector;
		FVector Goal = FVector::ZeroVector;
		float Phase = 0.0f;
	};

	/** A light of the village and its placed (night) intensity */
	struct FVillageLight
	{
		TWeakObjectPtr<ULightComponent> Light;
		float BaseIntensity = 0.0f;
		bool bInterior = true;
	};

	/** Flyers state */
	TArray<FFlyer> ButterflyState;
	TArray<FFlyer> FireflyState;

	/** Lights of the village */
	TArray<FVillageLight> Lights;

	/** Random goals */
	FRandomStream Random;

	/** Time since the start, drives the flapping and the blinking (s) */
	float Clock = 0.0f;

	/** Countdowns of the light update and of the camera check (s) */
	float LightTimer = 0.0f;
	float NearTimer = 0.0f;

	/** A local camera is near the village */
	bool bNear = false;

	/** Flyers shown (butterflies by day, fireflies at night) */
	bool bShowButterflies = false;
	bool bShowFireflies = false;

	/** Daylight of ADayNightCycleActor, 1 when the level has no cycle */
	float GetDaylight() const;

	/** True when a local player camera is within ActiveRadius */
	bool IsLocalViewNear() const;

	/** Collects the lights of the village */
	void FindLights();

	/** Sets the intensity of the lights for the daylight */
	void UpdateLights();

	/** New goal of a flyer in the village */
	FVector NewGoal(const FVector2D& Height, float RadiusFraction);

	/** Creates the instances of a kind of flyer */
	void InitFlyers(TArray<FFlyer>& Flyers, UInstancedStaticMeshComponent* Component, int32 Count, const FVector2D& Height, float RadiusFraction);

	/** Moves the butterflies one frame (flapping by squeezing the wings) */
	void UpdateButterflies(float DeltaSeconds);

	/** Moves the fireflies one frame (blinking by scale) */
	void UpdateFireflies(float DeltaSeconds);
};
