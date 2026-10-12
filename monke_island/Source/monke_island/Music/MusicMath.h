// monke_island

#pragma once

#include "CoreMinimal.h"

/** The songs of the game (T-220) and the two tracks of the owner's places (T-259) */
enum class EMusicTrack : uint8
{
	/** Hawaiian song: the menus, the island by day */
	Hawaiian,
	/** Night song: on the island while the night lasts */
	Night,
	/** British song: while the flag of a village rises to the trumpet */
	Crown,
	/** Aboard the galleon (owner's galion_song, 2026-10-08: the creaking of the ship) */
	Galleon,
	/** Seated or standing at a place of a boat (owner's barque_song, 2026-10-08: the oar strokes) */
	Boat,
	Count,
	/** No song: swimming out to the island, the arrival and departure scenes */
	None = Count
};

/** Where the local player is, for the tracks of the places (T-259) */
enum class EMusicPlace : uint8
{
	/** Neither in a boat nor aboard the galleon */
	Elsewhere,
	/** Aboard the galleon (its aboard volume) */
	Galleon,
	/** At a place of a boat (the rider component's seat, the standing places included) */
	Boat
};

/** Pure rules of the music (tested by monke_island.Music.Rules), no world */
namespace MusicMath
{
	/**
	 *  Track the local player hears (owner requests 2026-10-07 and 2026-10-08): in a boat the boat's track, aboard the
	 *  galleon the galleon's, on every map (Test Flat has both); elsewhere the menus play the Hawaiian song, and in an
	 *  expedition no song until the player sets foot on the island (swimming out, the arrival scene), then the British
	 *  song while a flag rises nearby, the night song at night, the Hawaiian song by day
	 */
	inline EMusicTrack PickTrack(bool bCeremonyNear, bool bNight, bool bOnIsland, EMusicPlace Place = EMusicPlace::Elsewhere)
	{
		if (Place == EMusicPlace::Boat)
		{
			return EMusicTrack::Boat;
		}
		if (Place == EMusicPlace::Galleon)
		{
			return EMusicTrack::Galleon;
		}
		if (!bOnIsland)
		{
			return EMusicTrack::None;
		}
		if (bCeremonyNear)
		{
			return EMusicTrack::Crown;
		}
		return bNight ? EMusicTrack::Night : EMusicTrack::Hawaiian;
	}

	/** Place of the local player: the boat first (a boat moored alongside may stand in the galleon's aboard volume) */
	inline EMusicPlace PickPlace(bool bInBoat, bool bAboardGalleon)
	{
		return bInBoat ? EMusicPlace::Boat : (bAboardGalleon ? EMusicPlace::Galleon : EMusicPlace::Elsewhere);
	}

	/** Volume of the Hawaiian song from the daylight as the night falls: full at FullAbove and over, silent at
	 *  SilentBelow and under, linear between */
	inline float DaylightVolume(float Daylight, float SilentBelow, float FullAbove)
	{
		if (FullAbove <= SilentBelow)
		{
			return Daylight > SilentBelow ? 1.0f : 0.0f;
		}
		return FMath::Clamp((Daylight - SilentBelow) / (FullAbove - SilentBelow), 0.0f, 1.0f);
	}

	/** One step of a fade of Current toward Target, a full fade (0 to 1) taking FadeSeconds */
	inline float StepFade(float Current, float Target, float DeltaSeconds, float FadeSeconds)
	{
		if (FadeSeconds <= 0.0f)
		{
			return Target;
		}
		return FMath::FInterpConstantTo(Current, Target, DeltaSeconds, 1.0f / FadeSeconds);
	}
}
