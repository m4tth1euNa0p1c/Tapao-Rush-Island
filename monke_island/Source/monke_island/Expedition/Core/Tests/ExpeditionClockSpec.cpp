// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Expedition.Clock
// Cahier §6.1, recette n°13 to n°16. Times are relative to an expedition started at StartTime.
BEGIN_DEFINE_SPEC(FExpeditionClockSpec, "monke_island.Expedition.Clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UExpeditionRules* Rules = nullptr;
	FExpeditionClockParams Params;
	FExpeditionClockState State;
	double StartTime = 0.0;

	/** Server time T seconds after the start */
	double At(double T) const { return StartTime + T; }
END_DEFINE_SPEC(FExpeditionClockSpec)

void FExpeditionClockSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UExpeditionRules>();
		Rules->AddToRoot();
		Params = FExpeditionClockParams::FromRules(*Rules);
		// Arbitrary non zero start: the rules must hold relative to the start, not to the world time
		StartTime = 1000.0;
		State = ExpeditionClock::Start(StartTime, Params);
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Cahier defaults", [this]()
	{
		It("uses 120 s before the flood, +120 s per zone, 60 s of submersion, 3 zones", [this]()
		{
			TestEqual(TEXT("initial delay"), Params.InitialFloodDelay, 120.0);
			TestEqual(TEXT("zone bonus"), Params.ZoneBonus, 120.0);
			TestEqual(TEXT("submersion"), Params.SubmersionDuration, 60.0);
			TestEqual(TEXT("zones"), Params.ZonesCount, 3);
		});

		It("is Preparing before the start", [this]()
		{
			const FExpeditionClockState NotStarted;
			TestEqual(TEXT("phase"), ExpeditionClock::PhaseAt(NotStarted, At(50.0), Params), EExpeditionPhase::Preparing);
			TestEqual(TEXT("water"), ExpeditionClock::WaterRiseCm(NotStarted, At(500.0), Params), 0.0);
			FExpeditionClockState Copy = NotStarted;
			TestEqual(TEXT("bonus"), ExpeditionClock::TryAddZoneBonus(Copy, TEXT("ZoneA"), At(10.0), Params), EZoneBonusResult::NotStarted);
		});
	});

	Describe("Recette 13: no pacification", [this]()
	{
		It("starts the flood at 120 s", [this]()
		{
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(120.0));
			TestEqual(TEXT("119.9 s"), ExpeditionClock::PhaseAt(State, At(119.9), Params), EExpeditionPhase::Exploring);
			TestEqual(TEXT("120 s"), ExpeditionClock::PhaseAt(State, At(120.0), Params), EExpeditionPhase::Submerging);
			TestEqual(TEXT("time left at 75 s"), ExpeditionClock::TimeBeforeSubmersion(State, At(75.0)), 45.0);
		});

		It("resolves at 180 s", [this]()
		{
			TestEqual(TEXT("end"), ExpeditionClock::SubmersionEndTime(State, Params), At(180.0));
			TestEqual(TEXT("179.9 s"), ExpeditionClock::PhaseAt(State, At(179.9), Params), EExpeditionPhase::Submerging);
			TestEqual(TEXT("180 s"), ExpeditionClock::PhaseAt(State, At(180.0), Params), EExpeditionPhase::Resolved);
		});

		It("keeps low water before 120 s, then rises along the curve to the maximum at 180 s", [this]()
		{
			TestEqual(TEXT("0 s"), ExpeditionClock::WaterRiseCm(State, At(0.0), Params), 0.0);
			TestEqual(TEXT("120 s"), ExpeditionClock::WaterRiseCm(State, At(120.0), Params), 0.0);
			TestEqual(TEXT("150 s"), ExpeditionClock::WaterRiseCm(State, At(150.0), Params),
				Params.MaxWaterRiseCm * FMath::Pow(0.5, Params.WaterRiseExponent), 0.001);
			TestEqual(TEXT("180 s"), ExpeditionClock::WaterRiseCm(State, At(180.0), Params), Params.MaxWaterRiseCm, 0.001);
			TestEqual(TEXT("300 s: still rising after the submersion"), ExpeditionClock::WaterRiseCm(State, At(300.0), Params),
				Params.MaxWaterRiseCm + FMath::Min(120.0 * Params.PostSubmersionRiseCmPerSecond, Params.PostSubmersionMaxRiseCm), 0.001);
		});

		It("keeps rising after the submersion until nobody can stand, then stops at the cap", [this]()
		{
			Params.PostSubmersionRiseCmPerSecond = 10.0;
			Params.PostSubmersionMaxRiseCm = 1500.0;
			TestEqual(TEXT("end of the submersion"), ExpeditionClock::WaterRiseCm(State, At(180.0), Params), Params.MaxWaterRiseCm, 0.001);
			TestEqual(TEXT("+30 s"), ExpeditionClock::WaterRiseCm(State, At(210.0), Params), Params.MaxWaterRiseCm + 300.0, 0.001);
			TestEqual(TEXT("capped"), ExpeditionClock::WaterRiseCm(State, At(1000.0), Params), Params.MaxWaterRiseCm + 1500.0, 0.001);
			Params.PostSubmersionRiseCmPerSecond = 0.0;
			TestEqual(TEXT("off"), ExpeditionClock::WaterRiseCm(State, At(1000.0), Params), Params.MaxWaterRiseCm, 0.001);
		});
	});

	Describe("Recette 14: one then two zones", [this]()
	{
		It("moves the flood to 240 s for a zone at 75 s, 165 s left", [this]()
		{
			TestEqual(TEXT("result"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(240.0));
			TestEqual(TEXT("time left"), ExpeditionClock::TimeBeforeSubmersion(State, At(75.0)), 165.0);
			TestEqual(TEXT("still exploring at 200 s"), ExpeditionClock::PhaseAt(State, At(200.0), Params), EExpeditionPhase::Exploring);
		});

		It("moves the flood to 360 s with a second zone", [this]()
		{
			TestEqual(TEXT("first"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("second"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(200.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(360.0));
			TestEqual(TEXT("resolution"), ExpeditionClock::SubmersionEndTime(State, Params), At(420.0));
		});

		It("caps at 8 minutes with the 3 zones of the first island", [this]()
		{
			TestEqual(TEXT("A"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(10.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("B"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(20.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("C"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneC"), At(30.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("D"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneD"), At(40.0), Params), EZoneBonusResult::ZoneLimitReached);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(480.0));
		});
	});

	Describe("Recette 15: duplicates and simultaneous zones", [this]()
	{
		It("grants a single bonus for a double validation of the same zone", [this]()
		{
			TestEqual(TEXT("first"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("same tick"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::AlreadyGranted);
			TestEqual(TEXT("later"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(100.0), Params), EZoneBonusResult::AlreadyGranted);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(240.0));
			TestEqual(TEXT("bonused zones"), State.BonusedZoneIds.Num(), 1);
		});

		It("grants +120 s to each of two zones validated in the same tick", [this]()
		{
			TestEqual(TEXT("A"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(119.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("B"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(119.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(360.0));
		});

		It("refuses a zone without identifier", [this]()
		{
			TestEqual(TEXT("none"), ExpeditionClock::TryAddZoneBonus(State, NAME_None, At(10.0), Params), EZoneBonusResult::InvalidZone);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(120.0));
		});

		It("keys the bonuses per expedition: a new expedition starts without any", [this]()
		{
			TestEqual(TEXT("A"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(10.0), Params), EZoneBonusResult::Granted);
			FExpeditionClockState Next = ExpeditionClock::Start(At(1000.0), Params);
			TestEqual(TEXT("A again"), ExpeditionClock::TryAddZoneBonus(Next, TEXT("ZoneA"), At(1010.0), Params), EZoneBonusResult::Granted);
		});
	});

	Describe("Recette 16: late validations", [this]()
	{
		It("refuses a validation exactly at the deadline: the flood wins", [this]()
		{
			TestEqual(TEXT("at 120 s"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(120.0), Params), EZoneBonusResult::TooLate);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(120.0));
			TestEqual(TEXT("bonused zones"), State.BonusedZoneIds.Num(), 0);
		});

		It("refuses a validation after the deadline, even after a bonus moved it", [this]()
		{
			TestEqual(TEXT("A"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(75.0), Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("B at 240 s"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneB"), At(240.0), Params), EZoneBonusResult::TooLate);
			TestEqual(TEXT("C at 250 s"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneC"), At(250.0), Params), EZoneBonusResult::TooLate);
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(240.0));
		});

		It("never lowers the water, whatever the validations", [this]()
		{
			double Previous = 0.0;
			for (double T = 0.0; T <= 600.0; T += 0.5)
			{
				// Validation attempts all along the way, legal or late
				if (FMath::IsNearlyEqual(FMath::Fmod(T, 50.0), 0.0))
				{
					(void)ExpeditionClock::TryAddZoneBonus(State, *FString::Printf(TEXT("Zone%d"), FMath::RoundToInt(T)), At(T), Params);
				}
				const double Rise = ExpeditionClock::WaterRiseCm(State, At(T), Params);
				if (!TestTrue(FString::Printf(TEXT("monotonic at %.1f s"), T), Rise >= Previous))
				{
					break;
				}
				Previous = Rise;
			}
			TestTrue(TEXT("fully risen at the end"), Previous >= Params.MaxWaterRiseCm - 0.001);
		});

		It("keeps the phase irreversible once the submersion started", [this]()
		{
			TestEqual(TEXT("submerging at 130 s"), ExpeditionClock::PhaseAt(State, At(130.0), Params), EExpeditionPhase::Submerging);
			TestEqual(TEXT("late bonus"), ExpeditionClock::TryAddZoneBonus(State, TEXT("ZoneA"), At(130.0), Params), EZoneBonusResult::TooLate);
			TestEqual(TEXT("still submerging"), ExpeditionClock::PhaseAt(State, At(130.0), Params), EExpeditionPhase::Submerging);
		});
	});

	Describe("Tuning", [this]()
	{
		It("follows the rules asset", [this]()
		{
			Rules->InitialFloodDelay = 30.0f;
			Rules->ZoneBonus = 45.0f;
			Rules->SubmersionDuration = 10.0f;
			Rules->ZonesCount = 0;
			Params = FExpeditionClockParams::FromRules(*Rules);
			State = ExpeditionClock::Start(StartTime, Params);
			for (int32 Index = 0; Index < 5; ++Index)
			{
				TestEqual(TEXT("no limit"), ExpeditionClock::TryAddZoneBonus(State, *FString::Printf(TEXT("Z%d"), Index), At(1.0), Params), EZoneBonusResult::Granted);
			}
			TestEqual(TEXT("flood start"), State.FloodStartTime, At(30.0 + 5.0 * 45.0));
			TestEqual(TEXT("end"), ExpeditionClock::SubmersionEndTime(State, Params), At(30.0 + 5.0 * 45.0 + 10.0));
		});

		It("handles a zero submersion duration as an instant flood", [this]()
		{
			Rules->SubmersionDuration = 0.0f;
			Params = FExpeditionClockParams::FromRules(*Rules);
			TestEqual(TEXT("before"), ExpeditionClock::WaterRiseCm(State, At(119.0), Params), 0.0);
			TestEqual(TEXT("at"), ExpeditionClock::WaterRiseCm(State, At(120.0), Params), Params.MaxWaterRiseCm, 0.001);
			TestEqual(TEXT("resolved"), ExpeditionClock::PhaseAt(State, At(120.0), Params), EExpeditionPhase::Resolved);
		});
	});
}

#endif
