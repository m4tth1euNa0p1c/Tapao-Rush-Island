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
	/** The other players' voices in the proximity voice chat (T-263, voice tab) */
	VoiceChat,
	Count UMETA(Hidden)
};

/** When the microphone transmits in the proximity voice chat (T-263, cahier §18 "Voix L7": push-to-talk by default) */
UENUM(BlueprintType)
enum class EVoiceMicMode : uint8
{
	/** Only while the push-to-talk key is held (V by default) */
	PushToTalk,
	/** Open microphone: the voice goes out whenever it is louder than the threshold */
	VoiceActivation,
	/** Never transmits; the others are still heard */
	Off
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

	/** Microphone mode of a first launch (cahier §18 and security.md: push-to-talk) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	EVoiceMicMode DefaultMicMode = EVoiceMicMode::PushToTalk;

	/** Voice activation threshold of a first launch: microphone level (0..1, the engine's silence detection) under which
	 *  nothing goes out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice", meta=(ClampMin="0", ClampMax="1"))
	float DefaultMicThreshold = 0.08f;

	/** Range of the voice activation threshold offered by the menu (microphone level 0..1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	FVector2D MicThresholdRange = FVector2D(0.01f, 0.4f);

	/** Default volume of a channel */
	float DefaultVolume(ESettingsVolume Channel) const
	{
		const int32 Index = static_cast<int32>(Channel);
		return DefaultVolumes.IsValidIndex(Index) ? DefaultVolumes[Index] : 0.8f;
	}
};
