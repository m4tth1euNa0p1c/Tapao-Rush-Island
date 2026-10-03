// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Settings/MonkeUserSettings.h"
#include "Settings/SettingsRules.h"

/** Pure rules of the settings (tested by SettingsSpec) */
namespace SettingsMath
{
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
