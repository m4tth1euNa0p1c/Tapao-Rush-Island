// monke_island

#include "UI/Tapao/TapaoSaveGame.h"

namespace
{
	void Bound(int32& Value, int32 Min, int32 Max, int32& Fixes)
	{
		const int32 Clamped = FMath::Clamp(Value, Min, Max);
		if (Clamped != Value)
		{
			Value = Clamped;
			++Fixes;
		}
	}
}

bool UTapaoSaveGame::SanitizeLoaded(int32& OutFixes)
{
	OutFixes = 0;
	if (Version > CurrentVersion)
	{
		return false;
	}
	Bound(Version, 1, CurrentVersion, OutFixes);
	Bound(Expeditions, 0, MaxCount, OutFixes);
	Bound(Wrecks, 0, MaxCount, OutFixes);
	Bound(Islands, 0, MaxCount, OutFixes);
	Bound(Gold, 0, MaxCount, OutFixes);
	if (!FMath::IsFinite(PlaySeconds) || PlaySeconds < 0.0f || PlaySeconds > static_cast<float>(MaxCount))
	{
		PlaySeconds = FMath::IsFinite(PlaySeconds) ? FMath::Clamp(PlaySeconds, 0.0f, static_cast<float>(MaxCount)) : 0.0f;
		++OutFixes;
	}
	if (LastIsland.Len() > MaxIslandNameChars)
	{
		LastIsland.Reset();
		++OutFixes;
	}
	// the main menu formats this date (FText::AsDate)
	if (LastPlayed.GetTicks() < FDateTime::MinValue().GetTicks() || LastPlayed.GetTicks() > FDateTime::MaxValue().GetTicks())
	{
		LastPlayed = FDateTime::MinValue();
		++OutFixes;
	}
	return true;
}
