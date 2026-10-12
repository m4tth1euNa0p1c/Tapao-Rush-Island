// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Settings/MonkeUserSettings.h"
#include "Settings/SettingsRules.h"

/** Pure rules of the settings (tested by SettingsSpec, the microphone by VoiceSpec) */
namespace SettingsMath
{
	/** A voice activation threshold inside the rules' range (a number that is not one: the default) */
	inline float ClampMicThreshold(const USettingsRules& Rules, float Value)
	{
		const float Low = FMath::Max(0.0f, static_cast<float>(FMath::Min(Rules.MicThresholdRange.X, Rules.MicThresholdRange.Y)));
		const float High = FMath::Min(1.0f, static_cast<float>(FMath::Max(Rules.MicThresholdRange.X, Rules.MicThresholdRange.Y)));
		const float Fallback = FMath::IsFinite(Rules.DefaultMicThreshold) ? Rules.DefaultMicThreshold : 0.08f;
		return FMath::Clamp(FMath::IsFinite(Value) ? Value : Fallback, Low, FMath::Max(Low, High));
	}

	/** A microphone mode the game knows (an old or hand-edited ini can hold anything) */
	inline EVoiceMicMode SanitizeMicMode(const USettingsRules& Rules, EVoiceMicMode Mode)
	{
		const auto IsKnown = [](EVoiceMicMode Value) { return static_cast<uint8>(Value) <= static_cast<uint8>(EVoiceMicMode::Off); };
		return IsKnown(Mode) ? Mode : (IsKnown(Rules.DefaultMicMode) ? Rules.DefaultMicMode : EVoiceMicMode::PushToTalk);
	}

	/** Values a first launch starts with */
	inline FMonkeSettingsValues Defaults(const USettingsRules& Rules)
	{
		FMonkeSettingsValues Values;
		for (int32 Index = 0; Index < static_cast<int32>(ESettingsVolume::Count); ++Index)
		{
			Values.Volumes[Index] = FMath::Clamp(Rules.DefaultVolume(static_cast<ESettingsVolume>(Index)), 0.0f, 1.0f);
		}
		Values.Sensitivity = 1.0f;
		Values.bInvertY = false;
		Values.FieldOfView = FMath::Clamp(Rules.DefaultFieldOfView, static_cast<float>(Rules.FieldOfViewRange.X), static_cast<float>(Rules.FieldOfViewRange.Y));
		Values.FrameRateLimit = Rules.DefaultFrameRate;
		Values.MicMode = SanitizeMicMode(Rules, Rules.DefaultMicMode);
		Values.MicThreshold = ClampMicThreshold(Rules, Rules.DefaultMicThreshold);
		return Values;
	}

	/** Brings every value back inside its bounds (a hand-edited or old ini can hold anything) */
	inline void Sanitize(const USettingsRules& Rules, FMonkeSettingsValues& Values)
	{
		for (float& Volume : Values.Volumes)
		{
			Volume = FMath::IsFinite(Volume) ? FMath::Clamp(Volume, 0.0f, 1.0f) : 0.8f;
		}
		Values.Sensitivity = FMath::IsFinite(Values.Sensitivity)
			? FMath::Clamp(Values.Sensitivity, static_cast<float>(Rules.SensitivityRange.X), static_cast<float>(Rules.SensitivityRange.Y)) : 1.0f;
		Values.FieldOfView = FMath::IsFinite(Values.FieldOfView)
			? FMath::Clamp(Values.FieldOfView, static_cast<float>(Rules.FieldOfViewRange.X), static_cast<float>(Rules.FieldOfViewRange.Y)) : Rules.DefaultFieldOfView;
		Values.WindowMode = FMath::Clamp(Values.WindowMode, 0, 2);
		Values.Quality = FMath::Clamp(Values.Quality, -1, 3);
		Values.FrameRateLimit = FMath::Max(0, Values.FrameRateLimit);
		Values.MicMode = SanitizeMicMode(Rules, Values.MicMode);
		Values.MicThreshold = ClampMicThreshold(Rules, Values.MicThreshold);
	}

	/** Controller look scales (yaw, pitch) from the controller's own defaults, scaled by the sensitivity */
	inline FVector2D LookScales(const FVector2D& BaseScales, float Sensitivity, bool bInvertY)
	{
		return FVector2D(BaseScales.X * Sensitivity, BaseScales.Y * Sensitivity * (bInvertY ? -1.0f : 1.0f));
	}

	/** Text of a frame rate choice */
	inline FString FrameRateLabel(int32 Limit)
	{
		return Limit <= 0 ? FString(TEXT("Illimité")) : FString::Printf(TEXT("%d"), Limit);
	}
}
