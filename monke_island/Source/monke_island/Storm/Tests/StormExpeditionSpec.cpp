// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Storm/WindMath.h"
#include "Storm/WindRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Storm
// T-098, lot L3, cahier §6.1: the ultra violent wind sets in at the start of the submersion (or a lead before it),
// on the expedition clock, not on DeadlineDay.
BEGIN_DEFINE_SPEC(FStormExpeditionSpec, "monke_island.Storm.ExpeditionDeadline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UExpeditionRules* ExpeditionRules = nullptr;
	UWindRules* WindRules = nullptr;
	FExpeditionClockParams Params;
	FExpeditionClockState State;
	double StartTime = 0.0;

	/** Server time T seconds after the start */
	double At(double T) const { return StartTime + T; }

	/** Mean wind strength on the server at T with the given lead, as AWindStormActor computes it */
	float ServerStrength(double T, double Lead) const
	{
		const double Now = At(T);
		const double StormSeconds = WindMath::ExpeditionStormSeconds(State, Now, Lead, Now >= State.FloodStartTime);
		const double Elapsed = Now - State.StartTime;
		return WindMath::MeanStrengthAt(*WindRules, Elapsed, WindMath::DeadlineStartElapsed(Elapsed, StormSeconds));
	}
END_DEFINE_SPEC(FStormExpeditionSpec)

void FStormExpeditionSpec::Define()
{
	BeforeEach([this]()
	{
		ExpeditionRules = NewObject<UExpeditionRules>();
		ExpeditionRules->AddToRoot();
		// Explicit short custom preset; Standard v1.4 (540 s) is covered by Expedition.Clock.Standard v1.4.
		ExpeditionRules->InitialFloodDelay = 120.0f;
		WindRules = NewObject<UWindRules>();
		WindRules->AddToRoot();
		// Keep the gusts out of the way: only the ultra violent wind is under test
		WindRules->FirstGustSeconds = 100000.0f;
		Params = FExpeditionClockParams::FromRules(*ExpeditionRules);
		StartTime = 1000.0;
		State = ExpeditionClock::Start(StartTime, Params);
	});

	AfterEach([this]()
	{
		ExpeditionRules->RemoveFromRoot();
		ExpeditionRules = nullptr;
		WindRules->RemoveFromRoot();
		WindRules = nullptr;
	});

	It("defaults to no lead and the Auto clock", [this]()
	{
		TestEqual(TEXT("lead"), WindRules->ExpeditionStormLeadSeconds, 0.0f);
		TestEqual(TEXT("mode"), WindRules->ClockMode, EHazardClockMode::Auto);
	});

	It("sets in at the start of the submersion", [this]()
	{
		TestEqual(TEXT("no storm 1 s before"), WindMath::ExpeditionStormSeconds(State, At(119.0), 0.0, false), -1.0);
		TestEqual(TEXT("storm at the deadline"), WindMath::ExpeditionStormSeconds(State, At(120.0), 0.0, true), 0.0);
		TestEqual(TEXT("calm before"), ServerStrength(119.0, 0.0), WindRules->CalmStrength);
		TestEqual(TEXT("full strength after the ramp"), ServerStrength(120.0 + WindRules->DeadlineRampSeconds, 0.0), WindRules->DeadlineStrength, 0.001f);
	});

	It("sets in LeadSeconds before the submersion when the rules say so", [this]()
	{
		TestEqual(TEXT("no storm before the lead"), WindMath::ExpeditionStormSeconds(State, At(89.0), 30.0, false), -1.0);
		TestEqual(TEXT("storm 30 s before"), WindMath::ExpeditionStormSeconds(State, At(90.0), 30.0, false), 0.0);
		TestTrue(TEXT("blowing before the water"), ServerStrength(110.0, 30.0) > WindRules->CalmStrength);
		TestEqual(TEXT("never before the expedition start"), WindMath::ExpeditionStormSeconds(State, At(0.0), 500.0, false), 0.0);
	});

	It("moves back with a zone bonus and never stops once the submersion started", [this]()
	{
		TestEqual(TEXT("bonus"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::Granted);
		TestEqual(TEXT("no storm at the old deadline"), ServerStrength(150.0, 0.0), WindRules->CalmStrength);
		TestEqual(TEXT("storm at the new deadline"), WindMath::ExpeditionStormSeconds(State, At(240.0), 0.0, true), 0.0);
		TestEqual(TEXT("no bonus after it"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(250.0), Params), EZoneBonusResult::TooLate);
		for (double T = 240.0 + WindRules->DeadlineRampSeconds; T < 1200.0; T += 37.0)
		{
			if (!TestEqual(*FString::Printf(TEXT("ultra violent at %.0f s"), T), ServerStrength(T, 0.0), WindRules->DeadlineStrength, 0.001f))
			{
				break;
			}
		}
	});

	It("holds on a client until the replicated phase confirms the submersion", [this]()
	{
		TestEqual(TEXT("no lead: nothing before the phase"), WindMath::ExpeditionStormSeconds(State, At(125.0), 0.0, false), -1.0);
		TestEqual(TEXT("lead: held at the deadline"), WindMath::ExpeditionStormSeconds(State, At(125.0), 30.0, false), 30.0);
		TestEqual(TEXT("confirmed: catches up"), WindMath::ExpeditionStormSeconds(State, At(125.0), 30.0, true), 35.0);
	});

	It("ignores DeadlineDay on the expedition clock", [this]()
	{
		// The day counter is not an input any more: without a started clock there is no ultra violent wind at all
		const FExpeditionClockState NotStarted;
		TestEqual(TEXT("no clock, no storm"), WindMath::ExpeditionStormSeconds(NotStarted, At(100000.0), 0.0, true), -1.0);
		TestEqual(TEXT("no deadline elapsed"), WindMath::DeadlineStartElapsed(500.0, -1.0), -1.0);
		TestEqual(TEXT("deadline elapsed"), WindMath::DeadlineStartElapsed(500.0, 20.0), 480.0);
	});
}

#endif
