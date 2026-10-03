// monke_island

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Settings/SettingsRules.h"
#include "MonkeUserSettings.generated.h"

/** Values edited by the settings menu (the pending copy is applied or thrown away as a whole) */
struct FMonkeSettingsValues
{
	/** Volume of each channel, 0..1 */
	float Volumes[static_cast<int32>(ESettingsVolume::Count)] = { 0.8f, 0.5f, 0.8f, 0.8f, 0.8f, 0.8f };

	/** Mouse sensitivity multiplier */
	float Sensitivity = 1.0f;

	/** Invert the vertical look */
	bool bInvertY = false;

	/** Field of view of the first person camera (degrees) */
	float FieldOfView = 90.0f;

	/** Window mode: 0 fullscreen, 1 borderless, 2 windowed (EWindowMode) */
	int32 WindowMode = 1;

	/** Screen resolution */
	FIntPoint Resolution = FIntPoint(1920, 1080);

	/** Vertical sync */
	bool bVSync = false;

	/** Frame rate limit (0 = unlimited) */
	int32 FrameRateLimit = 60;

	/** Overall quality: 0 low, 1 medium, 2 high, 3 ultra, -1 custom */
	int32 Quality = 1;

	float& Volume(ESettingsVolume Channel) { return Volumes[static_cast<int32>(Channel)]; }
	float Volume(ESettingsVolume Channel) const { return Volumes[static_cast<int32>(Channel)]; }

	/** Whether the display part differs (needs ApplySettings) */
	bool DisplayDiffers(const FMonkeSettingsValues& Other) const
	{
		return WindowMode != Other.WindowMode || Resolution != Other.Resolution || bVSync != Other.bVSync
			|| FrameRateLimit != Other.FrameRateLimit || Quality != Other.Quality;
	}
};

/**
 *  Player settings the engine does not keep itself (volumes, mouse, field of view), saved in the user's
 *  GameUserSettings.ini; the display settings live in UGameUserSettings.
 */
UCLASS(config=GameUserSettings)
class MONKE_ISLAND_API UMonkeUserSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Whether the values were ever saved (else the defaults of DA_SettingsRules apply) */
	UPROPERTY(config)
	bool bSaved = false;

	/** Volumes in ESettingsVolume order, 0..1 */
	UPROPERTY(config)
	TArray<float> Volumes;

	/** Mouse sensitivity multiplier */
	UPROPERTY(config)
	float Sensitivity = 1.0f;

	/** Invert the vertical look */
	UPROPERTY(config)
	bool bInvertY = false;

	/** Field of view of the first person camera (degrees) */
	UPROPERTY(config)
	float FieldOfView = 90.0f;
};
