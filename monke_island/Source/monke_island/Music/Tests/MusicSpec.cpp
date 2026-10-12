// monke_island

#include "Misc/AutomationTest.h"
#include "Music/MusicMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Music.Rules
BEGIN_DEFINE_SPEC(FMusicSpec, "monke_island.Music.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FMusicSpec)

void FMusicSpec::Define()
{
	Describe("Track", [this]()
	{
		It("plays the Hawaiian song by day on the island (and in the menus)", [this]()
		{
			TestTrue(TEXT("day"), MusicMath::PickTrack(false, false, true) == EMusicTrack::Hawaiian);
			TestTrue(TEXT("day, place given"), MusicMath::PickTrack(false, false, true, EMusicPlace::Elsewhere) == EMusicTrack::Hawaiian);
		});

		It("plays nothing off the island elsewhere than aboard (swimming out, the arrival scene)", [this]()
		{
			TestTrue(TEXT("off the island by day"), MusicMath::PickTrack(false, false, false) == EMusicTrack::None);
			TestTrue(TEXT("off the island at night"), MusicMath::PickTrack(false, true, false) == EMusicTrack::None);
			TestTrue(TEXT("a flag, but off the island"), MusicMath::PickTrack(true, false, false) == EMusicTrack::None);
			TestTrue(TEXT("place given, elsewhere"), MusicMath::PickTrack(false, false, false, EMusicPlace::Elsewhere) == EMusicTrack::None);
		});

		It("plays the galleon's track aboard the galleon, day or night, on every map (T-259)", [this]()
		{
			TestTrue(TEXT("expedition, aboard"), MusicMath::PickTrack(false, false, false, EMusicPlace::Galleon) == EMusicTrack::Galleon);
			TestTrue(TEXT("aboard at night"), MusicMath::PickTrack(false, true, false, EMusicPlace::Galleon) == EMusicTrack::Galleon);
			TestTrue(TEXT("map without expedition (Test Flat)"), MusicMath::PickTrack(false, false, true, EMusicPlace::Galleon) == EMusicTrack::Galleon);
			TestTrue(TEXT("a flag rising, but aboard"), MusicMath::PickTrack(true, false, true, EMusicPlace::Galleon) == EMusicTrack::Galleon);
		});

		It("plays the boat's track at a place of a boat, day or night, on every map (T-259)", [this]()
		{
			TestTrue(TEXT("expedition, rowing out"), MusicMath::PickTrack(false, false, false, EMusicPlace::Boat) == EMusicTrack::Boat);
			TestTrue(TEXT("at night"), MusicMath::PickTrack(false, true, false, EMusicPlace::Boat) == EMusicTrack::Boat);
			TestTrue(TEXT("map without expedition (Test Flat)"), MusicMath::PickTrack(false, false, true, EMusicPlace::Boat) == EMusicTrack::Boat);
			TestTrue(TEXT("a flag rising on the shore"), MusicMath::PickTrack(true, true, true, EMusicPlace::Boat) == EMusicTrack::Boat);
		});

		It("puts the boat before the galleon (a boat alongside may stand in the galleon's volume)", [this]()
		{
			TestTrue(TEXT("both"), MusicMath::PickPlace(true, true) == EMusicPlace::Boat);
			TestTrue(TEXT("boat"), MusicMath::PickPlace(true, false) == EMusicPlace::Boat);
			TestTrue(TEXT("galleon"), MusicMath::PickPlace(false, true) == EMusicPlace::Galleon);
			TestTrue(TEXT("neither"), MusicMath::PickPlace(false, false) == EMusicPlace::Elsewhere);
		});

		It("plays the night song at night on the island", [this]()
		{
			TestTrue(TEXT("night"), MusicMath::PickTrack(false, true, true) == EMusicTrack::Night);
		});

		It("plays the British song while a flag rises nearby, day or night", [this]()
		{
			TestTrue(TEXT("flag by day"), MusicMath::PickTrack(true, false, true) == EMusicTrack::Crown);
			TestTrue(TEXT("flag at night"), MusicMath::PickTrack(true, true, true) == EMusicTrack::Crown);
		});
	});

	Describe("Volumes", [this]()
	{
		It("lowers the Hawaiian song with the daylight until it is silent", [this]()
		{
			TestEqual(TEXT("full day"), MusicMath::DaylightVolume(1.0f, 0.15f, 0.55f), 1.0f);
			TestEqual(TEXT("night"), MusicMath::DaylightVolume(0.1f, 0.15f, 0.55f), 0.0f);
			TestTrue(TEXT("dusk"), FMath::IsNearlyEqual(MusicMath::DaylightVolume(0.35f, 0.15f, 0.55f), 0.5f, 0.001f));
		});

		It("fades at the rate of its fade time", [this]()
		{
			TestTrue(TEXT("a second of a 2 s fade"), FMath::IsNearlyEqual(MusicMath::StepFade(0.0f, 1.0f, 1.0f, 2.0f), 0.5f, 0.001f));
			TestEqual(TEXT("reached"), MusicMath::StepFade(0.9f, 1.0f, 1.0f, 2.0f), 1.0f);
			TestEqual(TEXT("no fade"), MusicMath::StepFade(0.2f, 0.0f, 0.1f, 0.0f), 0.0f);
		});
	});
}

#endif
