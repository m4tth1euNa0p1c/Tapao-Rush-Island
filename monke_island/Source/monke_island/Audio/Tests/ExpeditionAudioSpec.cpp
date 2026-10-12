// monke_island
#include "Misc/AutomationTest.h"
#include "Audio/ExpeditionAudioMath.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionAudioTransitions, "monke_island.Audio.Expedition.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionAudioTransitions::RunTest(const FString& Parameters)
{
	using namespace ExpeditionAudioMath;
	TestFalse(TEXT("Initial replicated state is silent"), Crossed(-1.0, 200.0, 240.0));
	TestTrue(TEXT("Half-time downward boundary"), Crossed(240.1, 240.0, 240.0));
	TestFalse(TEXT("Steady threshold is not repeated"), Crossed(240.0, 239.9, 240.0));
	TestFalse(TEXT("Zone bonus is not a warning"), Crossed(200.0, 320.0, 240.0));
	TestTrue(TEXT("One tick when entering 9 seconds"), CountdownTick(9.1, 9.0, 9));
	TestFalse(TEXT("No tick every poll"), CountdownTick(8.9, 8.8, 9));
	TestFalse(TEXT("No initial late-join countdown"), CountdownTick(-1.0, 5.0, 9));
	TestFalse(TEXT("No countdown after timeout"), CountdownTick(0.1, 0.0, 9));
	TestFalse(TEXT("No countdown during extension"), CountdownTick(5.0, 125.0, 9));
	TestFalse(TEXT("Starting loadout is silent"), GainedItems(-1, 4));
	TestFalse(TEXT("Equipping/rearranging is silent"), GainedItems(4, 4));
	TestFalse(TEXT("Consumption does not fake an object impact"), GainedItems(4, 3));
	TestTrue(TEXT("Collected stack increases count"), GainedItems(4, 5));
	return true;
}
#endif
