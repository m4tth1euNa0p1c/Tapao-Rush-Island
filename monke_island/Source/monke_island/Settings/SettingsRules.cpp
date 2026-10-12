// monke_island

#include "Settings/SettingsRules.h"

USettingsRules::USettingsRules()
{
	// ESettingsVolume order; the voices of the voice chat (T-263) like the other channels
	DefaultVolumes = { 0.8f, 0.5f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f };
	FrameRateChoices = { 30, 60, 90, 120, 144, 165, 240, 0 };
}
