// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FootstepRules.generated.h"

class USoundBase;
class USoundAttenuation;

/** Ground under a foot, picks the step sounds */
UENUM(BlueprintType)
enum class EFootstepSurface : uint8
{
	/** Dry coral sand, above the wet band */
	SandDry,
	/** Wet sand near the sea level */
	SandWet,
	/** Ankle-deep water (wading) */
	Water,
	/** Dead palm fronds and leaves */
	Litter,
	/** Coral rubble and reef flat */
	Coral,
	/** Stone, concrete, rock */
	Stone,
	/** Planks: boat, engawa, pier */
	Wood
};

/** Sound variants of one surface */
USTRUCT(BlueprintType)
struct FFootstepSurfaceSounds
{
	GENERATED_BODY()

	/** Variants, one is drawn per step */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps")
	TArray<TObjectPtr<USoundBase>> Sounds;

	/** Level of this surface relative to the others */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps", meta=(ClampMin="0"))
	float Volume = 1.0f;
};

/** Mesh or actor name keywords that give a surface (case-insensitive) */
USTRUCT(BlueprintType)
struct FFootstepKeywords
{
	GENERATED_BODY()

	/** Surface given by these keywords */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps")
	EFootstepSurface Surface = EFootstepSurface::Stone;

	/** Keywords looked for in the floor's mesh, component and actor names */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps")
	TArray<FString> Words;
};

/**
 *  Footsteps of the characters on the grounds of the atoll (DA_FootstepRules), played on every machine that has
 *  sound (cosmetic, never replicated) by UFootstepSubsystem. The surface comes from wading depth, then the floor's
 *  name keywords, then for the terrain the height above the sea (wet sand band).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFootstepRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UFootstepRules();

	/** Sounds per surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds")
	TMap<EFootstepSurface, FFootstepSurfaceSounds> Sounds;

	/** Attenuation of the steps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Random pitch spread around 1 (fraction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0", ClampMax="0.5"))
	float PitchJitter = 0.06f;

	/** Name keywords, checked in order: the first match wins */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surface")
	TArray<FFootstepKeywords> Keywords;

	/** Keyword of the terrain meshes: their surface comes from the height above the sea */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surface")
	FString TerrainKeyword = TEXT("Terrain");

	/** Surface of a floor no keyword matches */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surface")
	EFootstepSurface DefaultSurface = EFootstepSurface::Stone;

	/** Terrain up to this height above the water surface is wet sand (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surface", meta=(ClampMin="0", Units="cm"))
	float WetSandBandCm = 60.0f;

	/** Water this deep at the feet makes the steps splash (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Surface", meta=(ClampMin="0", Units="cm"))
	float WadingDepthCm = 4.0f;

	/** Below this ground speed no step is heard (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cadence", meta=(ClampMin="0", Units="cm/s"))
	float MinSpeed = 60.0f;

	/** Steps per second at MinSpeed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cadence", meta=(ClampMin="0.1"))
	float SlowStepRate = 1.5f;

	/** Walking speed (cm/s) and its steps per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cadence")
	FVector2D WalkCadence = FVector2D(350.0f, 2.6f);

	/** Sprinting speed (cm/s) and its steps per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cadence")
	FVector2D SprintCadence = FVector2D(600.0f, 3.5f);

	/** Volume at MinSpeed and at sprinting speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cadence")
	FVector2D VolumeRange = FVector2D(0.35f, 0.9f);

	/** Falling speed from which landing plays a heavy step (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Landing", meta=(ClampMin="0", Units="cm/s"))
	float LandingMinSpeed = 300.0f;

	/** Volume of a landing at LandingMinSpeed and at twice that speed or more */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Landing")
	FVector2D LandingVolume = FVector2D(0.7f, 1.3f);

	/** Characters farther than this from the listener are not processed (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Performance", meta=(ClampMin="100", Units="cm"))
	float MaxDistanceCm = 3000.0f;

	/** Volume of the non-player characters (tiki, villagers) relative to the players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Performance", meta=(ClampMin="0"))
	float CreatureVolume = 0.7f;
};
