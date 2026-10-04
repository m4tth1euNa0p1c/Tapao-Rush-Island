// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Flood/FloodGrid.h"
#include "Flood/FloodRules.h"
#include "Flood/FloodSchedule.h"
#include "HAL/PlatformTime.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Flood.SeaLevel
// T-103, lot L3, cahier §6.1 (the submersion covers the accessible land), recette n°16 (the water never goes down),
// T-101 (after the submersion the sea keeps rising at 10 cm/s up to +15 m). The sea of an expedition is the level of the
// map plus the held rise of the expedition clock (FFloodSeaRise): the open sea of the grid laid on the fragment played,
// and the sea of the whole map only with DA_FloodRules.bRaiseGlobalSea (off by default, owner request of 2026-10-04).
BEGIN_DEFINE_SPEC(FFloodSeaLevelSpec, "monke_island.Flood.SeaLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UExpeditionRules* ExpeditionRules = nullptr;
	UFloodRules* FloodRules = nullptr;
	FExpeditionClockParams Params;
	FExpeditionClockState State;
	double StartTime = 0.0;
	/** Sea level of the map before the expedition (world Z, cm), not 0 on purpose */
	double BaseLevel = -37.5;

	/** Server time T seconds after the start */
	double At(double T) const { return StartTime + T; }

	/** Rise the server reads at T (AExpeditionGameState::GetWaterRise on the server) */
	double ServerRise(double T) const { return ExpeditionClock::WaterRiseCm(State, At(T), Params); }

	/** A sea rise starting from BaseLevel */
	FFloodSeaRise MakeSea() const
	{
		FFloodSeaRise Sea;
		Sea.BaseSeaLevelCm = BaseLevel;
		return Sea;
	}
END_DEFINE_SPEC(FFloodSeaLevelSpec)

void FFloodSeaLevelSpec::Define()
{
	BeforeEach([this]()
	{
		ExpeditionRules = NewObject<UExpeditionRules>();
		ExpeditionRules->AddToRoot();
		// Explicit short custom preset; Standard v1.4 (540 s) is covered by Expedition.Clock.Standard v1.4.
		ExpeditionRules->InitialFloodDelay = 120.0f;
		FloodRules = NewObject<UFloodRules>();
		FloodRules->AddToRoot();
		Params = FExpeditionClockParams::FromRules(*ExpeditionRules);
		StartTime = 1000.0;
		State = ExpeditionClock::Start(StartTime, Params);
	});

	AfterEach([this]()
	{
		ExpeditionRules->RemoveFromRoot();
		ExpeditionRules = nullptr;
		FloodRules->RemoveFromRoot();
		FloodRules = nullptr;
	});

	Describe("Sea level of the expedition", [this]()
	{
		It("is the level of the map plus the rise of the expedition clock", [this]()
		{
			FFloodSeaRise Sea = MakeSea();
			TestEqual(TEXT("level of the map before the deadline"), Sea.Advance(ServerRise(60.0)), BaseLevel);
			TestEqual(TEXT("still at the deadline"), Sea.Advance(ServerRise(120.0)), BaseLevel, 1.e-6);
			for (double T = 120.0; T <= 400.0; T += 2.5)
			{
				const double Level = Sea.Advance(ServerRise(T));
				if (!TestEqual(*FString::Printf(TEXT("base + WaterRiseCm at %.1f s"), T), Level, BaseLevel + ServerRise(T), 1.e-6))
				{
					break;
				}
			}
		});

		It("reaches the 650 cm of DA_ExpeditionRules at the end of the 60 s submersion", [this]()
		{
			FFloodSeaRise Sea = MakeSea();
			TestEqual(TEXT("half way, on the curve"), Sea.Advance(ServerRise(150.0)),
				BaseLevel + Params.MaxWaterRiseCm * FMath::Pow(0.5, Params.WaterRiseExponent), 1.e-6);
			TestEqual(TEXT("end of the submersion"), Sea.Advance(ServerRise(180.0)), BaseLevel + Params.MaxWaterRiseCm, 1.e-6);
		});
	});

	Describe("After the submersion (T-101)", [this]()
	{
		It("keeps rising at PostSubmersionRiseCmPerSecond up to the cap, then stays", [this]()
		{
			FFloodSeaRise Sea = MakeSea();
			Sea.Advance(ServerRise(180.0));
			TestEqual(TEXT("+30 s"), Sea.Advance(ServerRise(210.0)),
				BaseLevel + Params.MaxWaterRiseCm + 30.0 * Params.PostSubmersionRiseCmPerSecond, 1.e-6);
			const double Final = FloodSchedule::ExpeditionFinalRiseCm(Params);
			TestEqual(TEXT("final rise = 650 + 1500 with the default rules"), Final, Params.MaxWaterRiseCm + Params.PostSubmersionMaxRiseCm, 1.e-6);
			TestEqual(TEXT("capped long after"), Sea.Advance(ServerRise(5000.0)), BaseLevel + Final, 1.e-6);
			TestEqual(TEXT("stays at the cap"), Sea.Advance(ServerRise(9000.0)), BaseLevel + Final, 1.e-6);
		});

		It("stops at the end of the submersion when the rules turn the post-submersion rise off", [this]()
		{
			Params.PostSubmersionRiseCmPerSecond = 0.0;
			TestEqual(TEXT("final rise"), FloodSchedule::ExpeditionFinalRiseCm(Params), Params.MaxWaterRiseCm, 1.e-6);
			FFloodSeaRise Sea = MakeSea();
			TestEqual(TEXT("held at 650"), Sea.Advance(ServerRise(1000.0)), BaseLevel + Params.MaxWaterRiseCm, 1.e-6);
		});
	});

	Describe("The water never goes down (recette 16)", [this]()
	{
		It("never lowers the global sea, even when the clock says lower", [this]()
		{
			FFloodSeaRise Sea = MakeSea();
			double Previous = Sea.GetSeaLevelCm();
			for (double T = 100.0; T <= 400.0; T += 0.25)
			{
				const double Level = Sea.Advance(ServerRise(T));
				if (!TestTrue(*FString::Printf(TEXT("no drop at %.2f s"), T), Level >= Previous))
				{
					break;
				}
				Previous = Level;
			}

			// a client that sees a later deadline (bonus replicated late) or a phase not replicated yet reads 0
			const double Held = Sea.GetSeaLevelCm();
			TestEqual(TEXT("target 0"), Sea.Advance(0.0), Held);
			TestEqual(TEXT("lower target"), Sea.Advance(100.0), Held);
		});

		It("moves the sea surfaces by steps, never down, and always to the top", [this]()
		{
			const double Step = FloodRules->SeaLevelStepCm;
			TestTrue(TEXT("a positive step"), Step > 0.0);
			TestFalse(TEXT("below the step"), FloodSchedule::ShouldApplySeaLevel(100.0, 100.0 + Step * 0.5, Step, false));
			TestTrue(TEXT("at the step"), FloodSchedule::ShouldApplySeaLevel(100.0, 100.0 + Step, Step, false));
			TestTrue(TEXT("the top is always applied"), FloodSchedule::ShouldApplySeaLevel(100.0, 100.0 + Step * 0.1, Step, true));
			TestFalse(TEXT("never down"), FloodSchedule::ShouldApplySeaLevel(100.0, 50.0, Step, true));
			TestFalse(TEXT("nothing to do"), FloodSchedule::ShouldApplySeaLevel(100.0, 100.0, Step, true));

			// applied level over the whole rise: at most one step behind, never down, exactly at the top in the end
			FFloodSeaRise Sea = MakeSea();
			const double Final = FloodSchedule::ExpeditionFinalRiseCm(Params);
			double Applied = BaseLevel;
			int32 Moves = 0;
			for (double T = 100.0; T <= 400.0; T += 1.0 / 60.0)
			{
				const double Level = Sea.Advance(ServerRise(T));
				const bool bTop = Sea.HeldRiseCm >= Final - UE_KINDA_SMALL_NUMBER;
				if (FloodSchedule::ShouldApplySeaLevel(Applied, Level, Step, bTop))
				{
					Applied = Level;
					++Moves;
				}
				if (!TestTrue(*FString::Printf(TEXT("at most one step behind at %.2f s"), T), Level - Applied < Step + 1.e-6))
				{
					break;
				}
			}
			TestEqual(TEXT("at the top"), Applied, BaseLevel + Final, 1.e-6);
			TestTrue(*FString::Printf(TEXT("far fewer moves than frames (%d)"), Moves), Moves <= FMath::CeilToInt32(Final / Step) + 1);
		});
	});

	Describe("Settings", [this]()
	{
		It("raises the water on the fragment played, not the sea of the whole map, by default", [this]()
		{
			TestFalse(TEXT("global sea off: the spawn stays dry"), FloodRules->bRaiseGlobalSea);
			TestTrue(TEXT("grid on the fragment"), FloodRules->bLocalGridInExpedition);
			const double Side = FloodRules->ExpeditionGridCells * FloodRules->ExpeditionCellSizeCm;
			TestTrue(TEXT("the grid covers a 2 km fragment"), Side >= 250000.0);
			TestTrue(TEXT("the boat, 200 to 350 m off the beach, stays inside the grid"), FloodRules->ArrivalToGridCenterCm < Side * 0.5 - 35000.0);
		});
	});

	Describe("Cost of the local grid (measure)", [this]()
	{
		It("logs the cost of one step: legacy 512 x 512 grid, then the grid laid on the fragment", [this]()
		{
			auto Measure = [this](const TCHAR* Name, int32 Cells, float CellSizeCm)
			{
				FFloodGrid Grid;
				Grid.Init(Cells, Cells, CellSizeCm / 100.0f);
				for (int32 Y = 0; Y < Cells; ++Y)
				{
					for (int32 X = 0; X < Cells; ++X)
					{
						// a low island in the middle of the sea
						const float Distance = FVector2f(X - Cells * 0.5f, Y - Cells * 0.5f).Size() / (Cells * 0.5f);
						Grid.Ground[Grid.Index(X, Y)] = 3.0f - 8.0f * Distance;
					}
				}
				Grid.SetupSea(0.0f, FloodRules->OpenSeaDepthCm / 100.0f);
				const int32 Steps = 40;
				double Min = TNumericLimits<double>::Max();
				double Max = 0.0;
				double Sum = 0.0;
				for (int32 Step = 0; Step < Steps; ++Step)
				{
					const double Begin = FPlatformTime::Seconds();
					Grid.Step(FloodRules->StepSeconds, 4.0f * Step / Steps, FloodRules->Gravity, FloodRules->FluxDamping);
					const double Ms = (FPlatformTime::Seconds() - Begin) * 1000.0;
					Min = FMath::Min(Min, Ms);
					Max = FMath::Max(Max, Ms);
					Sum += Ms;
				}
				AddInfo(FString::Printf(TEXT("%s: grid %d x %d, one step: min %.2f ms, mean %.2f ms, max %.2f ms (up to %d steps per frame)"),
					Name, Cells, Cells, Min, Sum / Steps, Max, FloodRules->MaxStepsPerFrame));
				return Sum;
			};
			TestTrue(TEXT("legacy measured"), Measure(TEXT("Legacy"), 512, 600.0f) > 0.0);
			TestTrue(TEXT("fragment measured"), Measure(TEXT("Fragment"), FloodRules->ExpeditionGridCells, FloodRules->ExpeditionCellSizeCm) > 0.0);
		});
	});
}

#endif
