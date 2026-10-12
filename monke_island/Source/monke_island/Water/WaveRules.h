// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaveRules.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;

/** One Gerstner wave train (GPU Gems 1, ch. 1, Finch 2004): deep water, speed from its wavelength */
USTRUCT(BlueprintType)
struct FIslandGerstnerWave
{
	GENERATED_BODY()

	FIslandGerstnerWave() = default;

	FIslandGerstnerWave(float InDirection, float InWavelength, float InAmplitude, float InSteepness, float InPhase = 0.0f)
		: DirectionDegrees(InDirection), Wavelength(InWavelength), Amplitude(InAmplitude), Steepness(InSteepness), Phase(InPhase)
	{
	}

	/** Direction the wave travels toward, in degrees around Z (0 = +X, 90 = +Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(Units="deg"))
	float DirectionDegrees = 0.0f;

	/** Crest to crest distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="10", Units="cm"))
	float Wavelength = 1000.0f;

	/** Height of a crest above the mean level (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="0", Units="cm"))
	float Amplitude = 10.0f;

	/** 0 = round sine swell, 1 = sharpest crests the sum allows (no loops) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="0", ClampMax="1"))
	float Steepness = 0.5f;

	/** Phase offset (radians), so the trains do not all start on a crest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	float Phase = 0.0f;
};

/**
 *  Waves of the sea (DA_WaveRules): a sum of Gerstner waves evaluated the same way by the water material (shape,
 *  normals, foam) and by UIslandWaterSubsystem::GetWaterSurfaceAt (boat buoyancy, swimmers, sharks, corpses), on
 *  the replicated server clock so every machine sees the same sea.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UWaveRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UWaveRules();

	/** Wave trains, at most MaxWaves are drawn by the material */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Waves")
	TArray<FIslandGerstnerWave> Waves;

	/** Multiplies every amplitude (0 = flat sea) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Waves", meta=(ClampMin="0"))
	float Intensity = 1.0f;

	/** Gravity of the dispersion relation, omega^2 = g k (cm/s^2) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Waves", meta=(ClampMin="1"))
	float Gravity = 980.0f;

	/** Material parameter collection fed every frame (WaveTime, WaveFollow, WaveGridInner, WaveGridGrowth, WaveMaxHeight, Wave<i>A, Wave<i>B) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering")
	TSoftObjectPtr<UMaterialParameterCollection> WaveParameters;

	/** Material of the wave surface around the camera */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering")
	TSoftObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Half size of the wave surface around the camera (cm); the static sea planes are hidden while it is drawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering", meta=(ClampMin="1000", Units="cm"))
	float SurfaceHalfSize = 4000000.0f;

	/** Vertices per side of the wave surface (dense at the camera, wider and wider outward) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering", meta=(ClampMin="16", ClampMax="1024"))
	int32 SurfaceResolution = 768;

	/** Spacing of the vertices at the camera (cm); a wave is drawn where the cells are under a quarter of its length
	 *  and fades out further away (the buoyancy keeps it everywhere) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering", meta=(ClampMin="5", Units="cm"))
	float SurfaceInnerSpacing = 25.0f;

	/** Wave trains the material reads (size of the collection) */
	static constexpr int32 MaxWaves = 8;
};
