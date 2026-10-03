// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/SharkMath.h"
#include "Creatures/SharkRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Shark
BEGIN_DEFINE_SPEC(FSharkSpec, "monke_island.Shark.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSharkSpec)

void FSharkSpec::Define()
{
	Describe("Cahier defaults (§8)", [this]()
	{
		It("detects at 12 m, bites for 25 every 2 s", [this]()
		{
			const USharkRules* Rules = GetDefault<USharkRules>();
			TestEqual(TEXT("detection"), Rules->DetectionRadius, 1200.0f);
			TestEqual(TEXT("damage"), Rules->AttackDamage, 25.0f);
			TestEqual(TEXT("interval"), Rules->AttackInterval, 2.0f);
		});
	});

	Describe("Prey", [this]()
	{
		It("hunts only living swimmers within the radius", [this]()
		{
			TestTrue(TEXT("swimmer in range"), SharkMath::IsValidPrey(true, false, 1000.0f, 1200.0f));
			TestFalse(TEXT("walker on the beach"), SharkMath::IsValidPrey(false, false, 500.0f, 1200.0f));
			TestFalse(TEXT("dead swimmer"), SharkMath::IsValidPrey(true, true, 500.0f, 1200.0f));
			TestFalse(TEXT("too far"), SharkMath::IsValidPrey(true, false, 1300.0f, 1200.0f));
		});
	});

	Describe("Hunt", [this]()
	{
		It("returns when the prey leaves the water, dies or escapes", [this]()
		{
			TestTrue(TEXT("prey lost while chasing"), SharkMath::HuntStep(ESharkState::Chase, false, 1.0f, 15.0f) == ESharkState::Return);
			TestTrue(TEXT("prey lost while detecting"), SharkMath::HuntStep(ESharkState::Detect, false, 0.0f, 15.0f) == ESharkState::Return);
			TestTrue(TEXT("chase too long"), SharkMath::HuntStep(ESharkState::Chase, true, 16.0f, 15.0f) == ESharkState::Return);
			TestTrue(TEXT("keeps chasing"), SharkMath::HuntStep(ESharkState::Chase, true, 5.0f, 15.0f) == ESharkState::Chase);
			TestTrue(TEXT("patrol untouched"), SharkMath::HuntStep(ESharkState::Patrol, false, 0.0f, 15.0f) == ESharkState::Patrol);
		});

		It("bites only in range and after the interval", [this]()
		{
			TestTrue(TEXT("first bite"), SharkMath::CanBite(100.0f, 170.0f, 10.0, -1.0e6, 2.0f));
			TestFalse(TEXT("interval running"), SharkMath::CanBite(100.0f, 170.0f, 10.0, 9.0, 2.0f));
			TestFalse(TEXT("out of range"), SharkMath::CanBite(200.0f, 170.0f, 10.0, 0.0, 2.0f));
		});
	});

	Describe("Water", [this]()
	{
		It("never goes into water shallower than the minimum depth", [this]()
		{
			TestTrue(TEXT("lagoon 3 m"), SharkMath::IsNavigable(0.0f, -300.0f, 140.0f));
			TestFalse(TEXT("reef flat 50 cm"), SharkMath::IsNavigable(0.0f, -50.0f, 140.0f));
			TestFalse(TEXT("land"), SharkMath::IsNavigable(0.0f, 120.0f, 140.0f));
			TestTrue(TEXT("flooded land 2 m"), SharkMath::IsNavigable(300.0f, 100.0f, 140.0f));
		});

		It("turns at most RateDeg per second, the short way round", [this]()
		{
			TestEqual(TEXT("limited step"), SharkMath::TurnToward(0.0f, 90.0f, 100.0f, 0.5f), 50.0f);
			TestEqual(TEXT("reaches the goal"), SharkMath::TurnToward(0.0f, 20.0f, 100.0f, 0.5f), 20.0f);
			TestEqual(TEXT("across 180"), FMath::Abs(SharkMath::TurnToward(170.0f, -170.0f, 100.0f, 0.1f)), 180.0f, 0.01f);
		});
	});
}

#endif
