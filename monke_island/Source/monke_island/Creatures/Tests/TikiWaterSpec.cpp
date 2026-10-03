// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.Water
BEGIN_DEFINE_SPEC(FTikiWaterSpec, "monke_island.Tiki.Water",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiWaterSpec)

void FTikiWaterSpec::Define()
{
	It("wades in the shallows but never steps into deeper water", [this]()
	{
		const UTikiRules* Rules = GetDefault<UTikiRules>();
		TestTrue(TEXT("dry land"), TikiMath::CanWade(0.0f, 0.0f, Rules->MaxWadeDepth));
		TestTrue(TEXT("ankle-deep"), TikiMath::CanWade(15.0f, 0.0f, Rules->MaxWadeDepth));
		TestFalse(TEXT("waist-deep ahead"), TikiMath::CanWade(80.0f, 10.0f, Rules->MaxWadeDepth));
		TestFalse(TEXT("open sea ahead"), TikiMath::CanWade(2000.0f, 0.0f, Rules->MaxWadeDepth));
	});

	It("walks back out when pushed into the sea", [this]()
	{
		const UTikiRules* Rules = GetDefault<UTikiRules>();
		TestTrue(TEXT("toward shallower water"), TikiMath::CanWade(60.0f, 90.0f, Rules->MaxWadeDepth));
		TestFalse(TEXT("not deeper"), TikiMath::CanWade(100.0f, 90.0f, Rules->MaxWadeDepth));
	});
}

#endif
