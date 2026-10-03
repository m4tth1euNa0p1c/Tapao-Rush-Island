// monke_island

#include "Misc/AutomationTest.h"
#include "Village/VillageAmbienceMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Village
BEGIN_DEFINE_SPEC(FVillageAmbienceSpec, "monke_island.Village.Ambience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FVillageAmbienceSpec)

void FVillageAmbienceSpec::Define()
{
	Describe("Lights", [this]()
	{
		It("are full at night and keep their day factor in full day", [this]()
		{
			TestEqual(TEXT("night"), VillageAmbienceMath::LightFactor(0.0f, 0.35f), 1.0f);
			TestEqual(TEXT("day"), VillageAmbienceMath::LightFactor(1.0f, 0.35f), 0.35f);
			TestEqual(TEXT("dusk halfway"), VillageAmbienceMath::LightFactor(0.5f, 0.0f), 0.5f);
			TestEqual(TEXT("clamped"), VillageAmbienceMath::LightFactor(3.0f, 0.2f), 0.2f);
		});
	});

	Describe("Butterflies", [this]()
	{
		It("flap their wings between the folded and the open span", [this]()
		{
			for (int32 Step = 0; Step < 40; ++Step)
			{
				const float Span = VillageAmbienceMath::FlapSpan(Step * 0.013f, 6.0f, 0.4f);
				TestTrue(TEXT("in range"), Span >= 0.15f - KINDA_SMALL_NUMBER && Span <= 1.0f + KINDA_SMALL_NUMBER);
			}
		});

		It("pick their goals inside the village disc and height range", [this]()
		{
			const FVector Center(100, 200, 300);
			for (int32 Step = 0; Step < 25; ++Step)
			{
				const FVector Goal = VillageAmbienceMath::GoalInDisc(Center, 5000.0f, 40.0f, 260.0f, Step / 25.0f, (Step * 7 % 25) / 25.0f, (Step * 3 % 25) / 25.0f);
				TestTrue(TEXT("in the disc"), FVector::Dist2D(Goal, Center) <= 5000.0 + 0.5);
				TestTrue(TEXT("in the height range"), Goal.Z >= Center.Z + 40.0 - 0.5 && Goal.Z <= Center.Z + 260.0 + 0.5);
			}
		});

		It("fly toward their goal without overshooting it", [this]()
		{
			const FVector Goal(100, 0, 0);
			const FVector Near = VillageAmbienceMath::StepToward(FVector(90, 0, 0), Goal, 150.0f, FVector::ZeroVector, 1.0f);
			TestTrue(TEXT("reaches"), Near.Equals(Goal, 0.01));
			const FVector Far = VillageAmbienceMath::StepToward(FVector::ZeroVector, FVector(1000, 0, 0), 150.0f, FVector::ZeroVector, 0.1f);
			TestTrue(TEXT("speed kept"), Far.Equals(FVector(15, 0, 0), 0.01));
		});
	});

	Describe("Fireflies", [this]()
	{
		It("blink between off and a full glow", [this]()
		{
			bool bOff = false;
			bool bOn = false;
			for (int32 Step = 0; Step < 100; ++Step)
			{
				const float Glow = VillageAmbienceMath::Blink(Step * 0.05f, 0.5f, 0.0f);
				TestTrue(TEXT("in range"), Glow >= 0.0f && Glow <= 1.0f);
				bOff |= Glow == 0.0f;
				bOn |= Glow > 0.9f;
			}
			TestTrue(TEXT("goes off"), bOff);
			TestTrue(TEXT("lights up"), bOn);
		});
	});
}

#endif
