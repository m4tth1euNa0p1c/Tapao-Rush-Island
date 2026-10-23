// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "Tidemark/Rules/TidemarkRules.h"
#include "Tidemark/Solver/TidemarkSchedule.h"

#if WITH_AUTOMATION_TESTS

/**
 * Tidemark.Schedule.* (TM-10): the sea level schedule is a pure function of the step index.
 * Reference values are computed by hand (650 x 0.5^1.6 = 214.42, 120 x e^-0.5 = 72.78...), not with the same
 * FMath calls as the code, so a wrong formula cannot pass by construction.
 * The Timeline case of the game's FloodGridSpec ("rises the sea from 0 to RiseCm over RiseSeconds") lives here as
 * Rules.DefaultScheduleRisesToSixHundredFifty.
 *
 * Run with: ./scripts/ue.ps1 test -Filter Tidemark.Schedule
 */
BEGIN_DEFINE_SPEC(FTidemarkScheduleSpec, "Tidemark.Schedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** The ramp of the spec: 650 cm over 1 200 steps (60 s at 0.05 s), exponent 1.6, from step 0. */
	static FSeaLevelSchedule Ramp650()
	{
		return Tidemark::Schedule::MakeRamp(FName(TEXT("Ramp650")), 650.0f, 0, 1200, 1.6f);
	}

	/** A tide of 30 cm with a 400 step period and no phase, from step 0. */
	static FSeaLevelSchedule Tide30(float PhaseSteps = 0.0f)
	{
		FSeaLevelSchedule Schedule;
		Schedule.Id = FName(TEXT("Tide30"));
		Schedule.bHoldRise = false;
		FSeaLevelTerm& Term = Schedule.Terms.AddDefaulted_GetRef();
		Term.Kind = ETidemarkScheduleTerm::Tide;
		Term.AmplitudeCm = 30.0f;
		Term.PeriodSteps = 400.0f;
		Term.PhaseSteps = PhaseSteps;
		return Schedule;
	}

	void ExpectLevel(const TCHAR* What, const FSeaLevelSchedule& Schedule, uint32 Step, float ExpectedCm, float ToleranceCm = 0.01f)
	{
		const float Level = Tidemark::Schedule::LevelCm(Schedule, Step);
		TestTrue(FString::Printf(TEXT("%s: step %u = %.4f cm, expected %.4f cm (+-%.3f)"), What, Step, Level, ExpectedCm, ToleranceCm),
			FMath::IsNearlyEqual(Level, ExpectedCm, ToleranceCm));
	}

END_DEFINE_SPEC(FTidemarkScheduleSpec)

void FTidemarkScheduleSpec::Define()
{
	using namespace Tidemark::Schedule;

	Describe("Ramp", [this]()
	{
		It("rises 650 cm over 1200 steps with exponent 1.6: 0 % 50 % 100 %", [this]()
		{
			// 650 x 0.5^1.6 = 214.4200 cm; 650 x 0.25^1.6 = 70.7322 cm; 650 x 0.75^1.6 = 410.2149 cm (computed by hand).
			const FSeaLevelSchedule Schedule = Ramp650();
			ExpectLevel(TEXT("0 %"), Schedule, 0, 0.0f, 0.0001f);
			ExpectLevel(TEXT("25 %"), Schedule, 300, 70.7322f);
			ExpectLevel(TEXT("50 %"), Schedule, 600, 214.4200f);
			ExpectLevel(TEXT("75 %"), Schedule, 900, 410.2149f);
			ExpectLevel(TEXT("100 %"), Schedule, 1200, 650.0f, 0.0001f);
			// Slow start: exponent > 1 keeps the first half under the linear ramp.
			TestTrue(TEXT("under the linear ramp at 50 % (325 cm)"), LevelCm(Schedule, 600) < 325.0f);
		});

		It("holds the full height after the ramp and nothing before StartStep", [this]()
		{
			const FSeaLevelSchedule Schedule = MakeRamp(FName(TEXT("Late")), 650.0f, 1000, 1200, 1.6f);
			ExpectLevel(TEXT("before the start"), Schedule, 999, 0.0f, 0.0001f);
			ExpectLevel(TEXT("at the start"), Schedule, 1000, 0.0f, 0.0001f);
			ExpectLevel(TEXT("half way (step 1600)"), Schedule, 1600, 214.4200f);
			ExpectLevel(TEXT("at the end (step 2200)"), Schedule, 2200, 650.0f, 0.0001f);
			ExpectLevel(TEXT("long after (step 100000)"), Schedule, 100000, 650.0f, 0.0001f);
		});

		It("jumps to the height at StartStep when the duration is 0", [this]()
		{
			const FSeaLevelSchedule Schedule = MakeRamp(FName(TEXT("Jump")), 100.0f, 50, 0, 1.0f);
			ExpectLevel(TEXT("step 49"), Schedule, 49, 0.0f, 0.0001f);
			ExpectLevel(TEXT("step 50"), Schedule, 50, 100.0f, 0.0001f);
		});

		It("is linear with exponent 1 and never decreases", [this]()
		{
			const FSeaLevelSchedule Schedule = MakeRamp(FName(TEXT("Linear")), 400.0f, 0, 800, 1.0f);
			ExpectLevel(TEXT("25 %"), Schedule, 200, 100.0f);
			ExpectLevel(TEXT("50 %"), Schedule, 400, 200.0f);
			float Previous = -1.0f;
			bool bMonotonic = true;
			for (uint32 Step = 0; Step <= 1000; ++Step)
			{
				const float Level = LevelCm(Schedule, Step);
				bMonotonic &= Level >= Previous;
				Previous = Level;
			}
			TestTrue(TEXT("monotonic over 1000 steps"), bMonotonic);
		});
	});

	Describe("Tide", [this]()
	{
		It("swings +-30 cm with a 400 step period: values at the quarters", [this]()
		{
			const FSeaLevelSchedule Schedule = Tide30();
			ExpectLevel(TEXT("0/4"), Schedule, 0, 0.0f);
			ExpectLevel(TEXT("1/4 (high)"), Schedule, 100, 30.0f);
			ExpectLevel(TEXT("2/4"), Schedule, 200, 0.0f);
			ExpectLevel(TEXT("3/4 (low)"), Schedule, 300, -30.0f);
			ExpectLevel(TEXT("4/4"), Schedule, 400, 0.0f);
			ExpectLevel(TEXT("5/4 (high again)"), Schedule, 500, 30.0f);
		});

		It("starts at high tide with a phase of a quarter period", [this]()
		{
			const FSeaLevelSchedule Schedule = Tide30(100.0f);
			ExpectLevel(TEXT("step 0 is high tide"), Schedule, 0, 30.0f);
			ExpectLevel(TEXT("step 200 is low tide"), Schedule, 200, -30.0f);
		});

		It("contributes nothing with a zero period", [this]()
		{
			FSeaLevelSchedule Schedule = Tide30();
			Schedule.Terms[0].PeriodSteps = 0.0f;
			ExpectLevel(TEXT("step 100"), Schedule, 100, 0.0f, 0.0001f);
		});
	});

	Describe("Surge", [this]()
	{
		It("peaks at the center and falls like a gaussian of the given width", [this]()
		{
			// 120 x e^-0.5 = 72.7837 cm at one sigma; 120 x e^-4.5 = 1.3331 cm at three sigma; ~0 at ten sigma.
			const FSeaLevelSchedule Schedule = MakeSurge(FName(TEXT("Surge")), 120.0f, 500.0f, 50.0f);
			ExpectLevel(TEXT("center"), Schedule, 500, 120.0f, 0.0001f);
			ExpectLevel(TEXT("center - sigma"), Schedule, 450, 72.7837f);
			ExpectLevel(TEXT("center + sigma"), Schedule, 550, 72.7837f);
			ExpectLevel(TEXT("center + 3 sigma"), Schedule, 650, 1.3331f);
			ExpectLevel(TEXT("step 0 (10 sigma)"), Schedule, 0, 0.0f, 0.0001f);
			TestEqual(TEXT("symmetric: the same bits at center -+ sigma"), LevelCm(Schedule, 450), LevelCm(Schedule, 550));
		});

		It("contributes nothing with a zero width", [this]()
		{
			const FSeaLevelSchedule Schedule = MakeSurge(FName(TEXT("Flat")), 120.0f, 500.0f, 0.0f);
			ExpectLevel(TEXT("center"), Schedule, 500, 0.0f, 0.0001f);
		});
	});

	Describe("Sum", [this]()
	{
		It("adds its terms in order and an empty schedule is 0", [this]()
		{
			FSeaLevelSchedule Schedule = Ramp650();
			FSeaLevelTerm& Constant = Schedule.Terms.AddDefaulted_GetRef();
			Constant.Kind = ETidemarkScheduleTerm::Constant;
			Constant.LevelCm = 100.0f;
			Schedule.Terms.Append(Tide30().Terms);
			// step 600: 214.42 (ramp) + 100 (constant) + 30 x sin(2 pi 600 / 400) = 0 (tide, 1.5 periods)
			ExpectLevel(TEXT("ramp + constant + tide at step 600"), Schedule, 600, 314.4200f);
			// step 1300: 650 + 100 + 30 x sin(2 pi 1300 / 400) = 650 + 100 + 30 (3.25 periods)
			ExpectLevel(TEXT("ramp + constant + tide at step 1300"), Schedule, 1300, 780.0f);

			const FSeaLevelSchedule Empty;
			ExpectLevel(TEXT("empty schedule"), Empty, 12345, 0.0f, 0.0f);
		});

		It("gates a constant on its StartStep", [this]()
		{
			FSeaLevelSchedule Schedule;
			FSeaLevelTerm& Constant = Schedule.Terms.AddDefaulted_GetRef();
			Constant.Kind = ETidemarkScheduleTerm::Constant;
			Constant.LevelCm = -40.0f;
			Constant.StartStep = 10;
			ExpectLevel(TEXT("step 9"), Schedule, 9, 0.0f, 0.0f);
			ExpectLevel(TEXT("step 10"), Schedule, 10, -40.0f, 0.0f);
		});

		It("is pure: the same step gives the same bits whatever was asked before", [this]()
		{
			const FSeaLevelSchedule Schedule = Ramp650();
			const float First = LevelCm(Schedule, 600);
			LevelCm(Schedule, 1200);
			LevelCm(Schedule, 0);
			TestEqual(TEXT("step 600 twice"), LevelCm(Schedule, 600), First);
		});
	});

	Describe("HoldRise", [this]()
	{
		It("never goes down on a tide when bHoldRise is set, and follows it otherwise", [this]()
		{
			FSeaLevelSchedule Held = Tide30();
			Held.bHoldRise = true;
			const FSeaLevelSchedule Free = Tide30();

			float HeldLevel = 0.0f;
			float Previous = -1000.0f;
			bool bNeverDown = true;
			bool bFreeWentDown = false;
			float PreviousFree = -1000.0f;
			for (uint32 Step = 0; Step <= 800; ++Step)
			{
				HeldLevel = HeldLevelCm(Held, Step, HeldLevel);
				bNeverDown &= HeldLevel >= Previous;
				Previous = HeldLevel;

				const float FreeLevel = HeldLevelCm(Free, Step, PreviousFree);
				bFreeWentDown |= FreeLevel < PreviousFree;
				PreviousFree = FreeLevel;
			}
			TestTrue(TEXT("held level never decreased over two periods"), bNeverDown);
			TestTrue(FString::Printf(TEXT("held level ends at the high tide (30 cm): %.4f"), HeldLevel), FMath::IsNearlyEqual(HeldLevel, 30.0f, 0.01f));
			TestTrue(TEXT("without HoldRise the level did go down"), bFreeWentDown);
			TestTrue(FString::Printf(TEXT("without HoldRise the level is back to 0 at step 800: %.4f"), PreviousFree), FMath::IsNearlyEqual(PreviousFree, 0.0f, 0.01f));
		});

		It("keeps the previous held level when the schedule says lower (moved from FloodSchedule::HoldRise)", [this]()
		{
			const FSeaLevelSchedule Schedule = Ramp650(); // bHoldRise true by MakeRamp
			TestEqual(TEXT("step 0 with 500 cm already held"), HeldLevelCm(Schedule, 0, 500.0f), 500.0f);
			TestEqual(TEXT("step 1200 with 500 cm held rises to 650"), HeldLevelCm(Schedule, 1200, 500.0f), 650.0f);
		});
	});

	Describe("StepsFromSeconds", [this]()
	{
		It("floors the clock to whole steps of 0.05 s", [this]()
		{
			const float StepSeconds = Tidemark::Rules::DefaultStepSeconds;
			TestEqual(TEXT("0 s"), StepsFromSeconds(0.0f, StepSeconds), 0u);
			TestEqual(TEXT("0.049 s"), StepsFromSeconds(0.049f, StepSeconds), 0u);
			TestEqual(TEXT("0.05 s"), StepsFromSeconds(0.05f, StepSeconds), 1u);
			TestEqual(TEXT("0.074 s"), StepsFromSeconds(0.074f, StepSeconds), 1u);
			TestEqual(TEXT("60 s (1199.99998 with 0.05 as a float: a boundary within 1e-6 relative counts)"), StepsFromSeconds(60.0f, StepSeconds), 1200u);
			TestEqual(TEXT("600 s"), StepsFromSeconds(600.0f, StepSeconds), 12000u);
			TestEqual(TEXT("3 600 s"), StepsFromSeconds(3600.0f, StepSeconds), 72000u);
		});

		It("returns 0 for a negative time or a non-positive step", [this]()
		{
			TestEqual(TEXT("-1 s"), StepsFromSeconds(-1.0f, 0.05f), 0u);
			TestEqual(TEXT("step 0"), StepsFromSeconds(10.0f, 0.0f), 0u);
			TestEqual(TEXT("step -0.05"), StepsFromSeconds(10.0f, -0.05f), 0u);
		});
	});

	Describe("Rules", [this]()
	{
		It("has the defaults of CONVENTIONS and the cahier", [this]()
		{
			const UTidemarkRules* Rules = GetDefault<UTidemarkRules>();
			TestEqual(TEXT("StepSeconds"), Rules->StepSeconds, 0.05f);
			TestEqual(TEXT("MaxStepsPerFrame"), Rules->MaxStepsPerFrame, 4);
			TestEqual(TEXT("Gravity"), Rules->Gravity, 9.81f);
			TestEqual(TEXT("FluxDamping"), Rules->FluxDamping, 0.995f);
			TestEqual(TEXT("OpenSeaDepthCm"), Rules->OpenSeaDepthCm, 150.0f);
			TestEqual(TEXT("SimGridCells"), Rules->SimGridCells, 512);
			TestEqual(TEXT("SimCellSizeCm"), Rules->SimCellSizeCm, 600.0f);
			TestEqual(TEXT("FallbackGridCells"), Rules->FallbackGridCells, 320);
			TestEqual(TEXT("FallbackCellSizeCm"), Rules->FallbackCellSizeCm, 800.0f);
			TestEqual(TEXT("HashIntervalSeconds"), Rules->HashIntervalSeconds, 1.0f);
			TestEqual(TEXT("DivergenceThreshold"), Rules->DivergenceThreshold, 3);
			TestEqual(TEXT("MaxCatchUpSteps"), Rules->MaxCatchUpSteps, 200);
			TestEqual(TEXT("RenderUpdateHz"), Rules->RenderUpdateHz, 5.0f);
			TestEqual(TEXT("BlockSize"), Rules->BlockSize, 32);
			TestEqual(TEXT("QuantizeCm"), Rules->QuantizeCm, 0.1f);
			TestEqual(TEXT("VolumeToleranceRelative"), Rules->VolumeToleranceRelative, 1.0e-5, 1.0e-12);
			TestTrue(TEXT("bHoldRise"), Rules->bHoldRise);
			TestEqual(TEXT("MaxCatchUpSteps is 10 s of steps"), Rules->StepsFromSeconds(10.0f), static_cast<uint32>(Rules->MaxCatchUpSteps));
		});

		It("DefaultScheduleRisesToSixHundredFifty: 0 at step 0, 650 cm after 60 s of steps (Timeline of FloodGridSpec)", [this]()
		{
			const UTidemarkRules* Rules = GetDefault<UTidemarkRules>();
			TestEqual(TEXT("one term"), Rules->DefaultSchedule.Terms.Num(), 1);
			TestTrue(TEXT("the term is a ramp"), Rules->DefaultSchedule.Terms.Num() == 1 && Rules->DefaultSchedule.Terms[0].Kind == ETidemarkScheduleTerm::Ramp);
			ExpectLevel(TEXT("start"), Rules->DefaultSchedule, 0, 0.0f, 0.0001f);
			const uint32 EndStep = Rules->StepsFromSeconds(60.0f);
			TestEqual(TEXT("60 s is 1200 steps"), EndStep, 1200u);
			ExpectLevel(TEXT("end"), Rules->DefaultSchedule, EndStep, 650.0f, 0.1f);
			ExpectLevel(TEXT("half way, exponent 1.6"), Rules->DefaultSchedule, EndStep / 2, 214.4200f);
			TestTrue(TEXT("the default schedule holds the rise"), Rules->DefaultSchedule.bHoldRise);
			TestTrue(TEXT("HoldsRise with the rule on"), Rules->HoldsRise(Tide30()));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
