// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/GiantCrabEmergenceMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Creatures.CrabEmergence
BEGIN_DEFINE_SPEC(FGiantCrabEmergenceSpec, "monke_island.Creatures.CrabEmergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGiantCrabEmergenceSpec)

void FGiantCrabEmergenceSpec::Define()
{
	It("takes the beach sand, not the water, the inland nor a slope", [this]()
	{
		TestTrue(TEXT("beach"), GiantCrabEmergenceMath::IsSand(100.0, 0.0, 0.98, 20.0, 260.0, 0.85));
		TestFalse(TEXT("wet, under water"), GiantCrabEmergenceMath::IsSand(5.0, 0.0, 0.98, 20.0, 260.0, 0.85));
		TestFalse(TEXT("inland"), GiantCrabEmergenceMath::IsSand(400.0, 0.0, 0.98, 20.0, 260.0, 0.85));
		TestFalse(TEXT("slope"), GiantCrabEmergenceMath::IsSand(100.0, 0.0, 0.6, 20.0, 260.0, 0.85));
	});

	It("comes out at 5:30 of the day, once", [this]()
	{
		const TArray<float> Times = { 330.0f };
		TestEqual(TEXT("before"), GiantCrabEmergenceMath::DueEmergence(300.0, Times, 0), INDEX_NONE);
		TestEqual(TEXT("at 5:30"), GiantCrabEmergenceMath::DueEmergence(331.0, Times, 0), 0);
		TestEqual(TEXT("done"), GiantCrabEmergenceMath::DueEmergence(500.0, Times, 1), INDEX_NONE);
	});

	It("comes out earlier on the hard days: the first at the minute of the day, the others shifted with it (owner 2026-10-09)", [this]()
	{
		const TArray<float> Times = { 330.0f, 450.0f };
		const TArray<float> Day10 = GiantCrabEmergenceMath::DayTimes(Times, 4.0f);
		TestEqual(TEXT("two crabs"), Day10.Num(), 2);
		if (Day10.Num() == 2)
		{
			TestEqual(TEXT("the first at 4:00"), Day10[0], 240.0f, 1.0e-3f);
			TestEqual(TEXT("the second two minutes later, as in the rules"), Day10[1], 360.0f, 1.0e-3f);
		}
		TestTrue(TEXT("day 0: the rules as they are"), GiantCrabEmergenceMath::DayTimes(Times, 0.0f) == Times);
		TestTrue(TEXT("no rules, no crab"), GiantCrabEmergenceMath::DayTimes(TArray<float>(), 4.0f).IsEmpty());
		// a time of the rules set before the first one (unsorted rules) never goes before the start of the day
		const TArray<float> Early = GiantCrabEmergenceMath::DayTimes({ 330.0f, 100.0f }, 1.0f);
		TestEqual(TEXT("the first at 1:00"), Early[0], 60.0f, 1.0e-3f);
		TestEqual(TEXT("never before the start"), Early[1], 0.0f, 1.0e-3f);
		TestEqual(TEXT("due at 4:00 of the day"), GiantCrabEmergenceMath::DueEmergence(241.0, Day10, 0), 0);
	});

	It("looks around the player at the distance asked", [this]()
	{
		const FVector Point = GiantCrabEmergenceMath::RingPoint(FVector(100.0, 0.0, 0.0), 5000.0, 0, 24, 0.0);
		TestTrue(TEXT("50 m east"), Point.Equals(FVector(5100.0, 0.0, 0.0), 0.5));
		TestEqual(TEXT("out of the ground at the end"), GiantCrabEmergenceMath::RiseHeight(1.0, 280.0), 0.0);
		TestEqual(TEXT("deep at the start"), GiantCrabEmergenceMath::RiseHeight(0.0, 280.0), -280.0);
	});
}

#endif
