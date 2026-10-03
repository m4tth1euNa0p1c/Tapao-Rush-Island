// monke_island

#include "Misc/AutomationTest.h"
#include "Hazards/CoconutFallRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Hazards
BEGIN_DEFINE_SPEC(FCoconutFallSpec, "monke_island.Hazards.Coconuts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UCoconutFallRules* Rules = nullptr;
END_DEFINE_SPEC(FCoconutFallSpec)

void FCoconutFallSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UCoconutFallRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	It("drops nuts every CalmIntervalSeconds in calm weather", [this]()
	{
		TestEqual(TEXT("calm"), Rules->IntervalFor(0.0f), Rules->CalmIntervalSeconds, 0.001f);
	});

	It("drops nuts every StormIntervalSeconds under the full storm, and even more often in gusts", [this]()
	{
		TestEqual(TEXT("storm"), Rules->IntervalFor(1.0f), Rules->StormIntervalSeconds, 0.001f);
		TestTrue(TEXT("gust"), Rules->IntervalFor(1.5f) < Rules->StormIntervalSeconds);
		TestTrue(TEXT("never below 0.1 s"), Rules->IntervalFor(100.0f) >= 0.1f);
	});

	It("knocks a player out from a fall of about 3 m, not from a nut rolling on the sand", [this]()
	{
		const float FromThreeMetres = FMath::Sqrt(2.0f * 980.0f * 300.0f);
		TestTrue(TEXT("3 m fall knocks out"), Rules->KnocksOut(FromThreeMetres));
		TestFalse(TEXT("rolling nut"), Rules->KnocksOut(200.0f));
	});
}

#endif
