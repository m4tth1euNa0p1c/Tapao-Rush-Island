// monke_island

#include "Misc/AutomationTest.h"
#include "Execution/ExecutionMath.h"
#include "Execution/ExecutionRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Execution
BEGIN_DEFINE_SPEC(FExecutionSpec, "monke_island.Execution.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FExecutionSpec)

void FExecutionSpec::Define()
{
	Describe("Plank", [this]()
	{
		It("puts the crew one after the other along the plank, never past its end", [this]()
		{
			const FVector Start(250.0, 0.0, 235.0);
			const FVector Along(1.0, 0.0, 0.0);
			TestEqual(TEXT("first"), ExecutionMath::PlankSpot(Start, Along, 230.0f, 80.0f, 520.0f, 0), FVector(480.0, 0.0, 235.0));
			TestEqual(TEXT("second"), ExecutionMath::PlankSpot(Start, Along, 230.0f, 80.0f, 520.0f, 1), FVector(560.0, 0.0, 235.0));
			TestEqual(TEXT("fourth"), ExecutionMath::PlankSpot(Start, Along, 230.0f, 80.0f, 520.0f, 3), FVector(720.0, 0.0, 235.0));
			TestEqual(TEXT("fifth held on the board"), ExecutionMath::PlankSpot(Start, Along, 230.0f, 80.0f, 520.0f, 4), FVector(745.0, 0.0, 235.0));
			TestEqual(TEXT("any heading, flat"), ExecutionMath::PlankSpot(Start, FVector(0.0, 2.0, 1.0), 100.0f, 80.0f, 520.0f, 0), FVector(250.0, 100.0, 235.0));
		});
	});

	Describe("Squad", [this]()
	{
		It("lines the soldiers abreast across the way they face, centred", [this]()
		{
			const FVector Centre(-120.0, 0.0, 165.0);
			const FVector Facing(1.0, 0.0, 0.0);
			TestEqual(TEXT("left end"), ExecutionMath::SquadSpot(Centre, Facing, 100.0f, 0, 4), FVector(-120.0, -150.0, 165.0));
			TestEqual(TEXT("right end"), ExecutionMath::SquadSpot(Centre, Facing, 100.0f, 3, 4), FVector(-120.0, 150.0, 165.0));
			TestEqual(TEXT("one alone in the middle"), ExecutionMath::SquadSpot(Centre, Facing, 100.0f, 0, 1), Centre);
		});

		It("shares the crew out between the soldiers", [this]()
		{
			TestEqual(TEXT("solo: everyone on the one"), ExecutionMath::VictimOf(3, 1), 0);
			TestEqual(TEXT("two: in turn"), ExecutionMath::VictimOf(3, 2), 1);
			TestEqual(TEXT("nobody"), ExecutionMath::VictimOf(0, 0), INDEX_NONE);
		});
	});

	Describe("Timeline", [this]()
	{
		It("shows the title, then the scene, then the end", [this]()
		{
			const UExecutionRules* Rules = GetDefault<UExecutionRules>();
			TestEqual(TEXT("title first"), ExecutionMath::PhaseAt(1.0f, Rules->PlaceAtSeconds, Rules->EndAtSeconds), ExecutionMath::EPhase::Title);
			TestEqual(TEXT("scene"), ExecutionMath::PhaseAt(Rules->FireAtSeconds, Rules->PlaceAtSeconds, Rules->EndAtSeconds), ExecutionMath::EPhase::Scene);
			TestEqual(TEXT("over"), ExecutionMath::PhaseAt(Rules->EndAtSeconds + 0.1f, Rules->PlaceAtSeconds, Rules->EndAtSeconds), ExecutionMath::EPhase::Over);
			TestTrue(TEXT("place, aim, fire, end in order"), Rules->PlaceAtSeconds < Rules->AimAtSeconds && Rules->AimAtSeconds < Rules->FireAtSeconds
				&& Rules->FireAtSeconds + Rules->VolleySpreadSeconds < Rules->EndAtSeconds);
			TestEqual(TEXT("title in"), ExecutionMath::TitleAlpha(0.45f, 0.2f, 3.0f), 0.5f, 1.0e-4f);
			TestEqual(TEXT("title out"), ExecutionMath::TitleAlpha(3.4f, 0.2f, 3.0f), 0.0f, 1.0e-4f);
			TestEqual(TEXT("title full"), ExecutionMath::TitleAlpha(1.5f, 0.2f, 3.0f), 1.0f, 1.0e-4f);
		});
	});
}

#endif
