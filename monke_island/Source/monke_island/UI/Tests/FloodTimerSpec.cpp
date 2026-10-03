// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "UI/FloodTimerMath.h"
#include "UI/HudTimerRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** Cahier §6.1 values (DA_ExpeditionRules defaults) */
	FExpeditionClockParams CahierParams()
	{
		FExpeditionClockParams Params;
		Params.InitialFloodDelay = 120.0;
		Params.ZoneBonus = 120.0;
		Params.SubmersionDuration = 60.0;
		Params.ZonesCount = 3;
		Params.MaxWaterRiseCm = 650.0;
		Params.WaterRiseExponent = 1.6;
		return Params;
	}

	/** Input of the expedition source at ServerTime, with the phase the server would have replicated */
	FFloodTimerInput ExpeditionInput(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params)
	{
		const EExpeditionPhase Phase = ExpeditionClock::PhaseAt(State, ServerTime, Params);
		FFloodTimerInput Input;
		Input.bActive = State.bStarted;
		Input.Now = ServerTime;
		Input.FloodStartTime = State.FloodStartTime;
		Input.SubmersionEndTime = ExpeditionClock::SubmersionEndTime(State, Params);
		Input.bSubmersionStarted = Phase >= EExpeditionPhase::Submerging;
		Input.bResolved = Phase == EExpeditionPhase::Resolved;
		return Input;
	}

	FFloodTimerSnapshot Countdown(double SecondsLeft)
	{
		FFloodTimerSnapshot Snapshot;
		Snapshot.Mode = EFloodTimerMode::Countdown;
		Snapshot.SecondsLeft = SecondsLeft;
		return Snapshot;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.UI.FloodTimer
BEGIN_DEFINE_SPEC(FFloodTimerSpec, "monke_island.UI.FloodTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FFloodTimerPresenter Presenter;
END_DEFINE_SPEC(FFloodTimerSpec)

void FFloodTimerSpec::Define()
{
	Describe("Format", [this]()
	{
		It("writes m:ss with two-digit seconds", [this]()
		{
			TestEqual(TEXT("105 s"), FloodTimerMath::FormatClock(105.0).ToString(), FString(TEXT("1:45")));
			TestEqual(TEXT("120 s"), FloodTimerMath::FormatClock(120.0).ToString(), FString(TEXT("2:00")));
			TestEqual(TEXT("9 s"), FloodTimerMath::FormatClock(9.0).ToString(), FString(TEXT("0:09")));
			TestEqual(TEXT("0 s"), FloodTimerMath::FormatClock(0.0).ToString(), FString(TEXT("0:00")));
			TestEqual(TEXT("480 s, minutes not capped"), FloodTimerMath::FormatClock(480.0).ToString(), FString(TEXT("8:00")));
			TestEqual(TEXT("725 s"), FloodTimerMath::FormatClock(725.0).ToString(), FString(TEXT("12:05")));
		});

		It("rounds up, so 0:00 only shows when the time is out, and never goes negative", [this]()
		{
			TestEqual(TEXT("104.2 s"), FloodTimerMath::FormatClock(104.2).ToString(), FString(TEXT("1:45")));
			TestEqual(TEXT("0.3 s"), FloodTimerMath::FormatClock(0.3).ToString(), FString(TEXT("0:01")));
			TestEqual(TEXT("float noise above 165"), FloodTimerMath::FormatClock(165.0000001).ToString(), FString(TEXT("2:45")));
			TestEqual(TEXT("negative"), FloodTimerMath::FormatClock(-4.0).ToString(), FString(TEXT("0:00")));
		});
	});

	Describe("Recette 14 (cahier §6.1, §23)", [this]()
	{
		It("zone at 75 s: deadline 240 s, 2:45 left; second zone: 360 s", [this]()
		{
			const FExpeditionClockParams Params = CahierParams();
			FExpeditionClockState State = ExpeditionClock::Start(0.0, Params);

			FFloodTimerSnapshot Before = FloodTimerMath::Evaluate(ExpeditionInput(State, 75.0, Params));
			TestEqual(TEXT("before the bonus: 0:45 left"), FloodTimerMath::FormatClock(Before.SecondsLeft).ToString(), FString(TEXT("0:45")));

			const double PreviousDeadline = State.FloodStartTime;
			TestEqual(TEXT("first zone granted"), ExpeditionClock::TryAddZoneBonus(State, TEXT("Zone_A"), 75.0, Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("deadline at 240 s"), State.FloodStartTime, 240.0);
			TestTrue(TEXT("seen as a zone bonus"), FloodTimerMath::IsZoneBonus(State.FloodStartTime, PreviousDeadline));
			TestEqual(TEXT("notification +2:00"), FloodTimerMath::FormatClock(State.FloodStartTime - PreviousDeadline).ToString(), FString(TEXT("2:00")));

			const FFloodTimerSnapshot After = FloodTimerMath::Evaluate(ExpeditionInput(State, 75.0, Params));
			TestEqual(TEXT("countdown"), After.Mode, EFloodTimerMode::Countdown);
			TestEqual(TEXT("165 s left"), After.SecondsLeft, 165.0);
			TestEqual(TEXT("shows 2:45"), FloodTimerMath::FormatClock(After.SecondsLeft).ToString(), FString(TEXT("2:45")));

			TestEqual(TEXT("second zone granted"), ExpeditionClock::TryAddZoneBonus(State, TEXT("Zone_B"), 100.0, Params), EZoneBonusResult::Granted);
			TestEqual(TEXT("deadline at 360 s"), State.FloodStartTime, 360.0);
			TestEqual(TEXT("shows 4:20 at 100 s"), FloodTimerMath::FormatClock(FloodTimerMath::Evaluate(ExpeditionInput(State, 100.0, Params)).SecondsLeft).ToString(),
				FString(TEXT("4:20")));
		});

		It("host and client compute the same text from the same replicated state and server time", [this]()
		{
			const FExpeditionClockParams Params = CahierParams();
			FExpeditionClockState Host = ExpeditionClock::Start(10.0, Params);
			(void)ExpeditionClock::TryAddZoneBonus(Host, TEXT("Zone_A"), 85.0, Params);
			const FExpeditionClockState Client = Host;   // what replication carries
			const double ServerTime = 131.4;              // GetServerWorldTimeSeconds on both machines
			TestEqual(TEXT("same text"),
				FloodTimerMath::FormatClock(FloodTimerMath::Evaluate(ExpeditionInput(Host, ServerTime, Params)).SecondsLeft).ToString(),
				FloodTimerMath::FormatClock(FloodTimerMath::Evaluate(ExpeditionInput(Client, ServerTime, Params)).SecondsLeft).ToString());
		});

		It("the start of the expedition is not a zone bonus", [this]()
		{
			TestFalse(TEXT("0 -> 120"), FloodTimerMath::IsZoneBonus(120.0, 0.0));
			TestFalse(TEXT("unchanged"), FloodTimerMath::IsZoneBonus(240.0, 240.0));
		});
	});

	Describe("Modes", [this]()
	{
		It("switches to the evacuation countdown at the deadline, then to over", [this]()
		{
			const FExpeditionClockParams Params = CahierParams();
			const FExpeditionClockState State = ExpeditionClock::Start(0.0, Params);

			const FFloodTimerSnapshot Early = FloodTimerMath::Evaluate(ExpeditionInput(State, 15.0, Params));
			TestEqual(TEXT("countdown"), Early.Mode, EFloodTimerMode::Countdown);
			TestEqual(TEXT("1:45"), FloodTimerMath::FormatClock(Early.SecondsLeft).ToString(), FString(TEXT("1:45")));

			const FFloodTimerSnapshot Rising = FloodTimerMath::Evaluate(ExpeditionInput(State, 138.0, Params));
			TestEqual(TEXT("evacuation"), Rising.Mode, EFloodTimerMode::Evacuation);
			TestEqual(TEXT("42 s to the end of the rise"), Rising.SecondsLeft, 42.0);

			const FFloodTimerSnapshot End = FloodTimerMath::Evaluate(ExpeditionInput(State, 180.0, Params));
			TestEqual(TEXT("over at 180 s"), End.Mode, EFloodTimerMode::Over);
		});

		It("a client never shows the evacuation before the server phase says so", [this]()
		{
			FFloodTimerInput Input;
			Input.bActive = true;
			Input.Now = 121.0;
			Input.FloodStartTime = 120.0;
			Input.SubmersionEndTime = 180.0;
			Input.bSubmersionStarted = false;   // phase not replicated yet (a bonus may be on its way)
			const FFloodTimerSnapshot Snapshot = FloodTimerMath::Evaluate(Input);
			TestEqual(TEXT("still the countdown"), Snapshot.Mode, EFloodTimerMode::Countdown);
			TestEqual(TEXT("held at 0:00"), Snapshot.SecondsLeft, 0.0);
		});

		It("no source: hidden", [this]()
		{
			TestEqual(TEXT("hidden"), FloodTimerMath::Evaluate(FFloodTimerInput()).Mode, EFloodTimerMode::Hidden);
		});

		It("reports the mode change to evacuation once", [this]()
		{
			Presenter.Configure({ 60.0f, 30.0f, 10.0f }, false);
			Presenter.Update(Countdown(5.0));
			FFloodTimerSnapshot Evacuation;
			Evacuation.Mode = EFloodTimerMode::Evacuation;
			Evacuation.SecondsLeft = 59.0;
			const FFloodTimerEvents First = Presenter.Update(Evacuation);
			TestTrue(TEXT("changed"), First.bModeChanged);
			TestEqual(TEXT("from the countdown"), First.PreviousMode, EFloodTimerMode::Countdown);
			TestFalse(TEXT("no threshold alert during the evacuation"), First.HasAlert());
			Evacuation.SecondsLeft = 30.0;
			TestFalse(TEXT("not twice"), Presenter.Update(Evacuation).bModeChanged);
		});
	});

	Describe("Alerts (cahier §19)", [this]()
	{
		BeforeEach([this]()
		{
			const UHudTimerRules* Rules = GetDefault<UHudTimerRules>();
			Presenter.Configure(Rules->AlertThresholds, Rules->bRearmAlertsAfterExtension);
		});

		It("defaults to 60 / 30 / 10 s, not re-armed by an extension", [this]()
		{
			const UHudTimerRules* Rules = GetDefault<UHudTimerRules>();
			TestEqual(TEXT("three thresholds"), Rules->AlertThresholds.Num(), 3);
			TestTrue(TEXT("60"), Rules->AlertThresholds.Contains(60.0f));
			TestTrue(TEXT("30"), Rules->AlertThresholds.Contains(30.0f));
			TestTrue(TEXT("10"), Rules->AlertThresholds.Contains(10.0f));
			TestFalse(TEXT("single alerts"), Rules->bRearmAlertsAfterExtension);
		});

		It("fires each threshold once, when it is reached", [this]()
		{
			TestFalse(TEXT("first look at 2:00"), Presenter.Update(Countdown(120.0)).HasAlert());
			TestFalse(TEXT("1:01"), Presenter.Update(Countdown(60.2)).HasAlert());
			TestEqual(TEXT("1:00 alerts"), Presenter.Update(Countdown(59.9)).AlertThreshold, 60.0f);
			TestFalse(TEXT("not twice"), Presenter.Update(Countdown(59.7)).HasAlert());
			TestEqual(TEXT("30"), Presenter.Update(Countdown(29.95)).AlertThreshold, 30.0f);
			TestEqual(TEXT("10"), Presenter.Update(Countdown(9.8)).AlertThreshold, 10.0f);
			TestFalse(TEXT("nothing after"), Presenter.Update(Countdown(2.0)).HasAlert());
		});

		It("does not replay a passed threshold after an extension (+2:00 at 0:50)", [this]()
		{
			Presenter.Update(Countdown(70.0));
			TestEqual(TEXT("60 fires"), Presenter.Update(Countdown(59.8)).AlertThreshold, 60.0f);
			// Zone bonus: 50 s left -> 170 s left
			TestFalse(TEXT("the extension itself is silent"), Presenter.Update(Countdown(170.0)).HasAlert());
			TestFalse(TEXT("60 s again: no second alert"), Presenter.Update(Countdown(59.5)).HasAlert());
			TestTrue(TEXT("60 stays spent"), Presenter.HasFired(60.0f));
			TestEqual(TEXT("30 still fires once"), Presenter.Update(Countdown(29.8)).AlertThreshold, 30.0f);
		});

		It("re-arms only when the rules ask, once per threshold", [this]()
		{
			Presenter.Configure({ 60.0f, 30.0f, 10.0f }, true);
			Presenter.Update(Countdown(70.0));
			TestEqual(TEXT("60"), Presenter.Update(Countdown(59.8)).AlertThreshold, 60.0f);
			Presenter.Update(Countdown(170.0));
			TestEqual(TEXT("60 again after the extension"), Presenter.Update(Countdown(59.8)).AlertThreshold, 60.0f);
			TestFalse(TEXT("but only once"), Presenter.Update(Countdown(59.0)).HasAlert());
		});

		It("joining late keeps the passed thresholds silent", [this]()
		{
			TestFalse(TEXT("first look at 0:25"), Presenter.Update(Countdown(25.0)).HasAlert());
			TestTrue(TEXT("60 spent"), Presenter.HasFired(60.0f));
			TestTrue(TEXT("30 spent"), Presenter.HasFired(30.0f));
			TestEqual(TEXT("10 still to come"), Presenter.Update(Countdown(9.9)).AlertThreshold, 10.0f);
		});

		It("a jump over several thresholds gives a single alert, the most urgent", [this]()
		{
			Presenter.Update(Countdown(90.0));
			TestEqual(TEXT("jump from 1:30 to 0:05"), Presenter.Update(Countdown(5.0)).AlertThreshold, 10.0f);
			TestTrue(TEXT("60 spent"), Presenter.HasFired(60.0f));
			TestTrue(TEXT("30 spent"), Presenter.HasFired(30.0f));
		});

		It("a new expedition starts its alerts afresh", [this]()
		{
			Presenter.Update(Countdown(70.0));
			Presenter.Update(Countdown(59.0));
			Presenter.Reset();
			Presenter.Update(Countdown(120.0));
			TestEqual(TEXT("60 fires in the new expedition"), Presenter.Update(Countdown(59.0)).AlertThreshold, 60.0f);
		});
	});
}

#endif
