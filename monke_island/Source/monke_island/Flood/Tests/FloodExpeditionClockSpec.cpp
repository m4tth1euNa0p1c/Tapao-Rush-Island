// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Flood/FloodRules.h"
#include "Flood/FloodSchedule.h"
#include "Flood/HazardClock.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Flood.ExpeditionClock
// T-098, lot L3, cahier §6.1, recette n°13 to n°18: the rising water follows the expedition clock.
// Times are relative to an expedition started at StartTime (server time).
BEGIN_DEFINE_SPEC(FFloodExpeditionClockSpec, "monke_island.Flood.ExpeditionClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UExpeditionRules* ExpeditionRules = nullptr;
	UFloodRules* FloodRules = nullptr;
	FExpeditionClockParams Params;
	FExpeditionClockState State;
	double StartTime = 0.0;

	/** Server time T seconds after the start */
	double At(double T) const { return StartTime + T; }

	/** Seconds of rising water seen by the server at T (its clock confirms the deadline by itself) */
	double ServerFloodSeconds(double T) const
	{
		return FloodSchedule::ExpeditionFloodSeconds(State, At(T), At(T) >= State.FloodStartTime);
	}
END_DEFINE_SPEC(FFloodExpeditionClockSpec)

void FFloodExpeditionClockSpec::Define()
{
	BeforeEach([this]()
	{
		ExpeditionRules = NewObject<UExpeditionRules>();
		ExpeditionRules->AddToRoot();
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

	Describe("Start of the rise", [this]()
	{
		It("starts at the expedition deadline (120 s), not at the day / night StartSeconds (290 s)", [this]()
		{
			TestEqual(TEXT("dry 1 ms before the deadline"), ServerFloodSeconds(119.999), -1.0);
			TestEqual(TEXT("rising at the deadline"), ServerFloodSeconds(120.0), 0.0);
			TestEqual(TEXT("10 s into the rise"), ServerFloodSeconds(130.0), 10.0, 1.e-6);
			TestTrue(TEXT("water above the low water at 150 s"), FloodSchedule::ExpeditionRiseCm(State, ServerFloodSeconds(150.0), Params) > 0.0);
			TestTrue(TEXT("the fallback timing is a different clock"), FloodRules->StartSeconds != Params.InitialFloodDelay);
		});

		It("lasts the 60 s of DA_ExpeditionRules and matches the game state curve", [this]()
		{
			TestEqual(TEXT("submersion"), Params.SubmersionDuration, 60.0);
			TestEqual(TEXT("full rise after 60 s"), FloodSchedule::ExpeditionRiseCm(State, 60.0, Params), Params.MaxWaterRiseCm, 1.e-6);
			for (double T = 0.0; T <= 70.0; T += 7.5)
			{
				TestEqual(*FString::Printf(TEXT("same height as ExpeditionClock at +%.1f s"), T),
					FloodSchedule::ExpeditionRiseCm(State, T, Params), ExpeditionClock::WaterRiseCm(State, State.FloodStartTime + T, Params), 1.e-6);
			}
		});

		It("waits for the replicated phase on a client", [this]()
		{
			TestEqual(TEXT("not confirmed: dry even past the deadline"), FloodSchedule::ExpeditionFloodSeconds(State, At(125.0), false), -1.0);
			TestEqual(TEXT("confirmed: catches up"), FloodSchedule::ExpeditionFloodSeconds(State, At(125.0), true), 5.0, 1.e-6);
			TestEqual(TEXT("confirmed a hair early: starts at 0"), FloodSchedule::ExpeditionFloodSeconds(State, At(119.99), true), 0.0);
			const FExpeditionClockState NotStarted;
			TestEqual(TEXT("no clock, no rise"), FloodSchedule::ExpeditionFloodSeconds(NotStarted, At(500.0), true), -1.0);
		});
	});

	Describe("Zone bonuses (recette 14)", [this]()
	{
		It("moves the rise to 240 s for a zone at 75 s, then 360 s for a second zone", [this]()
		{
			TestEqual(TEXT("first zone"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("dry at the old deadline"), ServerFloodSeconds(120.0), -1.0);
			TestEqual(TEXT("dry at 239 s"), ServerFloodSeconds(239.0), -1.0);
			TestEqual(TEXT("rising at 240 s"), ServerFloodSeconds(240.0), 0.0);

			TestEqual(TEXT("second zone"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(200.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("dry at 359 s"), ServerFloodSeconds(359.0), -1.0);
			TestEqual(TEXT("rising at 360 s"), ServerFloodSeconds(360.0), 0.0);
		});

		It("gives no bonus once the submersion started", [this]()
		{
			TestEqual(TEXT("at the deadline"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(120.0), Params), EZoneBonusResult::TooLate);
			TestEqual(TEXT("during the rise"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(150.0), Params), EZoneBonusResult::TooLate);
			TestEqual(TEXT("deadline unchanged"), State.FloodStartTime, At(120.0));
			TestEqual(TEXT("the rise goes on"), ServerFloodSeconds(150.0), 30.0, 1.e-6);
		});
	});

	Describe("The water never goes down", [this]()
	{
		It("rises monotonically during and after the submersion", [this]()
		{
			double Previous = 0.0;
			for (double T = 100.0; T <= 400.0; T += 0.5)
			{
				const double Rise = FloodSchedule::ExpeditionRiseCm(State, ServerFloodSeconds(T), Params);
				if (!TestTrue(*FString::Printf(TEXT("no drop at %.1f s"), T), Rise >= Previous))
				{
					break;
				}
				Previous = Rise;
			}
			TestTrue(TEXT("at least the full rise after the end (then the post-submersion rise)"), Previous >= Params.MaxWaterRiseCm - 1.e-6);
		});

		It("holds the sea even when the clock says lower (late replication)", [this]()
		{
			TestEqual(TEXT("hold"), FloodSchedule::HoldRise(500.0, 120.0), 500.0);
			TestEqual(TEXT("rise"), FloodSchedule::HoldRise(120.0, 500.0), 500.0);

			// A client that saw the old deadline then receives a later one: the held sea does not fall back
			double Held = FloodSchedule::HoldRise(0.0, FloodSchedule::ExpeditionRiseCm(State, 30.0, Params));
			FExpeditionClockState Moved = State;
			Moved.FloodStartTime += Params.ZoneBonus;
			const double Late = FloodSchedule::ExpeditionRiseCm(Moved, FloodSchedule::ExpeditionFloodSeconds(Moved, At(150.0), false), Params);
			Held = FloodSchedule::HoldRise(Held, Late);
			TestTrue(TEXT("still up"), Held > 0.0);
		});
	});

	Describe("Clock choice", [this]()
	{
		It("follows the expedition when the map has one, never both clocks", [this]()
		{
			TestEqual(TEXT("auto, expedition"), HazardClock::Resolve(EHazardClockMode::Auto, true, true), EHazardClockSource::Expedition);
			TestEqual(TEXT("auto, no expedition: fallback"), HazardClock::Resolve(EHazardClockMode::Auto, true, false), EHazardClockSource::DayNight);
			TestEqual(TEXT("auto, game state not replicated yet: wait"), HazardClock::Resolve(EHazardClockMode::Auto, false, false), EHazardClockSource::Undecided);
			TestEqual(TEXT("expedition only, none: off"), HazardClock::Resolve(EHazardClockMode::ExpeditionOnly, true, false), EHazardClockSource::Disabled);
			TestEqual(TEXT("expedition only, expedition"), HazardClock::Resolve(EHazardClockMode::ExpeditionOnly, true, true), EHazardClockSource::Expedition);
			TestEqual(TEXT("forced day / night"), HazardClock::Resolve(EHazardClockMode::DayNightOnly, true, true), EHazardClockSource::DayNight);
			TestEqual(TEXT("default mode"), FloodRules->ClockMode, EHazardClockMode::Auto);
		});

		It("keeps the legacy day / night timing for the fallback", [this]()
		{
			TestEqual(TEXT("dry before StartSeconds"), FloodSchedule::DayNightFloodSeconds(*FloodRules, FloodRules->StartSeconds - 1.0), -1.0);
			TestEqual(TEXT("rising at StartSeconds"), FloodSchedule::DayNightFloodSeconds(*FloodRules, FloodRules->StartSeconds), 0.0);
			TestEqual(TEXT("full rise"), FloodSchedule::DayNightRiseCm(*FloodRules, FloodRules->RiseSeconds), static_cast<double>(FloodRules->RiseCm), 0.1);
		});
	});
}

#endif
