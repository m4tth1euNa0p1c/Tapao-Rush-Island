// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingsRules.generated.h"

class USoundClass;
class USoundMix;

/** Volume channels of the audio tab (cahier §18) */
UENUM(BlueprintType)
enum class ESettingsVolume : uint8
{
	/** Everything */
	Master,
	/** Music */
	Music,
	/** Gameplay effects: steps, weapons, impacts */
	Effects,
	/** Ambience: waves, wind, village */
	Ambience,
	/** Voices */
	Dialogue,
	/** Menus and HUD */
	Interface,
	Count UMETA(Hidden)
};

/**
 *  Defaults and bounds of the player settings (DA_SettingsRules, cahier §18): sound classes of the volume channels,
 *  the sound mix that applies them, default volumes, the frame rate choices and the field of view range.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API USettingsRules : public UDataAsset
{
	GENERATED_BODY()

public:

	USettingsRules();

	/** Sound class of each channel but Master (the master volume drives the whole audio device) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TMap<ESettingsVolume, TObjectPtr<USoundClass>> SoundClasses;

	/** Mix carrying the channel volumes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TObjectPtr<USoundMix> VolumeMix;

	/** Default volume of each channel, 0..1, in ESettingsVolume order (cahier: master 80, music 50, others 80) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TArray<float> DefaultVolumes;

	/** Frame rate limits offered (0 = unlimited) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display")
	TArray<int32> FrameRateChoices;

	/** Default frame rate limit in game */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Display", meta=(ClampMin="0"))
	int32 DefaultFrameRate = 60;

	/** Field of view range of the first person camera (degrees, horizontal) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Comfort")
	FVector2D FieldOfViewRange = FVector2D(80.0f, 110.0f);

	/** Default field of view (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Comfort")
	float DefaultFieldOfView = 90.0f;

	/** Mouse sensitivity range (multiplier of the base look speed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mouse")
	FVector2D SensitivityRange = FVector2D(0.2f, 3.0f);

	/** Default volume of a channel */
	float DefaultVolume(ESettingsVolume Channel) const
	{
		const int32 Index = static_cast<int32>(Channel);
		return DefaultVolumes.IsValidIndex(Index) ? DefaultVolumes[Index] : 0.8f;
	}
};
