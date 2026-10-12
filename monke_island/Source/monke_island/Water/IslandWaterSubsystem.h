// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Water/WaveMath.h"
#include "IslandWaterSubsystem.generated.h"

class AWaveSurfaceActor;
class UWaveRules;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIslandSeaLevelChangedDelegate, float, NewSeaLevel);

/**
 *  Owns the sea level of an island world.
 *  The water surface actors (tagged WaterSurfaceTag) define the initial level and follow every change,
 *  so rising water or a storm surge only has to call SetSeaLevel.
 *  Waves (DA_WaveRules, Gerstner): GetWaterSurfaceAt includes them, on the replicated server clock, so the boat,
 *  the swimmers and the creatures ride the sea that AWaveSurfaceActor draws. Console: monke.Waves 0 = flat sea.
 */
UCLASS()
class MONKE_ISLAND_API UIslandWaterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Wave rules loaded by default */
	static const TCHAR* DefaultWaveRulesPath;

	/** Tag identifying the water surface actors that follow the sea level */
	static const FName WaterSurfaceTag;

	/** Returns the subsystem of the given world context, or nullptr */
	static UIslandWaterSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Returns the current sea level (world Z, cm) */
	UFUNCTION(BlueprintPure, Category="Water")
	float GetSeaLevel() const { return SeaLevel; }

	/** Moves the sea level and every water surface actor (rising water, storm surge) */
	UFUNCTION(BlueprintCallable, Category="Water")
	void SetSeaLevel(float NewSeaLevel);

	/** Returns the water depth above a world location (cm, 0 when dry), flood included */
	UFUNCTION(BlueprintPure, Category="Water")
	float GetWaterDepthAt(const FVector& Location) const;

	/** Returns the water surface height above a location (world Z, cm): the sea level, or a higher local flood */
	UFUNCTION(BlueprintPure, Category="Water")
	float GetWaterSurfaceAt(const FVector& Location) const;

	/** Local water surface on top of the sea level (a flood): returns true and the surface Z where it has water */
	using FSurfaceProvider = TFunction<bool(const FVector& Location, float& OutSurfaceZ)>;

	/** Registers the local water surface of an owner (one at a time); an empty function removes it */
	void SetSurfaceProvider(const UObject* Owner, FSurfaceProvider Provider);

	/** Height of the waves above the sea level at a world location (cm, 0 with a flat sea) */
	float GetWaveHeightAt(const FVector& Location) const;

	/** Clock of the waves (s): the replicated server time, the same on every machine */
	float GetWaveTime() const;

	/** Wave trains in use (empty with a flat sea) */
	const TArray<WaveMath::FTrain>& GetWaveTrains() const { return WaveTrains; }

	/** Static water surface actors (tagged WaterSurfaceTag) */
	const TArray<TWeakObjectPtr<AActor>>& GetWaterSurfaces() const { return WaterSurfaces; }

	/** Broadcast when the sea level changes */
	UPROPERTY(BlueprintAssignable, Category="Water")
	FIslandSeaLevelChangedDelegate OnSeaLevelChanged;

private:

	/** Sea level (world Z, cm), read from the first water surface actor at begin play */
	float SeaLevel = 0.0f;

	/** Water surface actors that follow the sea level */
	TArray<TWeakObjectPtr<AActor>> WaterSurfaces;

	/** Owner of the local water surface */
	TWeakObjectPtr<const UObject> SurfaceProviderOwner;

	/** Local water surface (flood), empty when none */
	FSurfaceProvider SurfaceProvider;

	/** Wave rules (null = flat sea) */
	UPROPERTY(Transient)
	TObjectPtr<UWaveRules> WaveRules;

	/** Wave trains of WaveRules */
	TArray<WaveMath::FTrain> WaveTrains;

	/** Wave surface drawn around the local camera */
	TWeakObjectPtr<AWaveSurfaceActor> WaveSurface;
};
