// monke_island

#include "Misc/AutomationTest.h"
#include "Run/RunContract.h"
#include "Run/RunRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Run.Contract
BEGIN_DEFINE_SPEC(FRunContractSpec, "monke_island.Run.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** The zones of ADR-007 with plain test values */
	static FRunContractRules MakeRules()
	{
		FRunContractRules Rules;
		const int32 Quotas[] = { 1800, 3500, 6500, 10000, 18000 };
		for (int32 Index = 0; Index < 5; ++Index)
		{
			FRunZone& Zone = Rules.Zones.AddDefaulted_GetRef();
			Zone.Quota = Quotas[Index];
			Zone.SpawnScale = 1.0f + Index;
			Zone.LootValueScale = 1.0f + 0.5f * Index;
			Zone.FloodDelayScale = 1.0f - 0.1f * Index;
		}
		Rules.DaysPerContract = 3;
		Rules.MedicalFee = 50;
		return Rules;
	}

END_DEFINE_SPEC(FRunContractSpec)

void FRunContractSpec::Define()
{
	Describe("Days", [this]()
	{
		It("adds the days up until the quota, example of ADR-007 §9 (720 + 630 + 900 = 2250 / 1800)", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			TestEqual(TEXT("quota of zone 1"), State.Quota, 1800);
			TestEqual(TEXT("day 1"), FRunContractCalculator::EndDay(Rules, State, 720, 0, false).Verdict, ERunDayVerdict::NextDay);
			TestEqual(TEXT("day 2"), FRunContractCalculator::EndDay(Rules, State, 630, 0, false).Verdict, ERunDayVerdict::NextDay);
			TestEqual(TEXT("day 3 is the third"), State.DayInContract, 3);
			const FRunDayReport Last = FRunContractCalculator::EndDay(Rules, State, 900, 0, false);
			TestEqual(TEXT("day 3 completes"), Last.Verdict, ERunDayVerdict::ContractComplete);
			TestEqual(TEXT("excess paid"), Last.Payout, 450);
			TestEqual(TEXT("treasury"), State.Treasury, 450);
			TestEqual(TEXT("next zone"), State.ZoneIndex, 1);
			TestEqual(TEXT("new quota"), State.Quota, 3500);
			TestEqual(TEXT("day 1 again"), State.DayInContract, 1);
			TestEqual(TEXT("days played"), State.DaysPlayed, 3);
		});

		It("asks leave or stay when the quota is met with days left (greed, §16)", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			TestEqual(TEXT("verdict"), FRunContractCalculator::EndDay(Rules, State, 2000, 0, false).Verdict, ERunDayVerdict::QuotaMetChoose);
			TestTrue(TEXT("choice pending"), State.bAwaitingChoice);
			TestEqual(TEXT("no day counted while the choice is pending"), FRunContractCalculator::EndDay(Rules, State, 100, 0, false).Verdict, ERunDayVerdict::Invalid);

			const float Loot = FRunContractCalculator::LootValueScale(Rules, State);
			TestEqual(TEXT("stay"), FRunContractCalculator::Choose(Rules, State, true), ERunDayVerdict::NextDay);
			TestTrue(TEXT("greed day"), State.bGreedDay);
			TestEqual(TEXT("day 2"), State.DayInContract, 2);
			TestTrue(TEXT("more loot"), FRunContractCalculator::LootValueScale(Rules, State) > Loot);
			TestEqual(TEXT("asked again after the greed day"), FRunContractCalculator::EndDay(Rules, State, 500, 0, false).Verdict, ERunDayVerdict::QuotaMetChoose);

			int32 Payout = 0;
			TestEqual(TEXT("leave"), FRunContractCalculator::Choose(Rules, State, false, &Payout), ERunDayVerdict::ContractComplete);
			TestEqual(TEXT("excess of 2500 over 1800"), Payout, 700);
			TestEqual(TEXT("zone 2"), State.ZoneIndex, 1);
			TestFalse(TEXT("greed reset"), State.bGreedDay);
			TestEqual(TEXT("nothing to choose any more"), FRunContractCalculator::Choose(Rules, State, true), ERunDayVerdict::Invalid);
		});
	});

	Describe("Failure", [this]()
	{
		It("fails the contract after the last day: half the treasury lost, one zone back, run goes on (§14)", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			State.ZoneIndex = 2;
			State.Quota = 6500;
			State.Treasury = 1001;
			FRunContractCalculator::EndDay(Rules, State, 1000, 0, false);
			FRunContractCalculator::EndDay(Rules, State, 1000, 0, false);
			const FRunDayReport Report = FRunContractCalculator::EndDay(Rules, State, 1000, 0, false);
			TestEqual(TEXT("failed"), Report.Verdict, ERunDayVerdict::ContractFailed);
			TestEqual(TEXT("penalty"), Report.Penalty, 500);
			TestEqual(TEXT("treasury"), State.Treasury, 501);
			TestEqual(TEXT("one zone back"), State.ZoneIndex, 1);
			TestEqual(TEXT("its quota"), State.Quota, 3500);
			TestEqual(TEXT("still in progress"), State.Outcome, ERunOutcome::InProgress);
			TestEqual(TEXT("one failure"), State.FailedContracts, 1);
		});

		It("executes the crew on a failed contract when the rule is on: the run is lost, nothing more counted", [this]()
		{
			FRunContractRules Rules = MakeRules();
			Rules.bExecuteOnFailedContract = true;
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			State.Quota = 6500;
			State.Treasury = 1001;
			FRunContractCalculator::EndDay(Rules, State, 1000, 0, false);
			FRunContractCalculator::EndDay(Rules, State, 1000, 0, false);
			const FRunDayReport Report = FRunContractCalculator::EndDay(Rules, State, 1000, 0, false);
			TestEqual(TEXT("executed"), Report.Verdict, ERunDayVerdict::Executed);
			TestEqual(TEXT("run lost"), State.Outcome, ERunOutcome::Defeat);
			TestEqual(TEXT("no money penalty"), Report.Penalty, 0);
			TestEqual(TEXT("treasury untouched"), State.Treasury, 1001);
			TestEqual(TEXT("same zone"), State.ZoneIndex, 0);
			TestEqual(TEXT("one failure"), State.FailedContracts, 1);
			TestEqual(TEXT("nothing counted after"), FRunContractCalculator::EndDay(Rules, State, 1000, 0, false).Verdict, ERunDayVerdict::Invalid);
		});

		It("ships the execution rule on in DA_RunRules defaults (owner 2026-10-07)", [this]()
		{
			TestTrue(TEXT("execution on"), GetDefault<URunRules>()->MakeContractRules().bExecuteOnFailedContract);
			TestFalse(TEXT("off in the plain contract rules"), FRunContractRules().bExecuteOnFailedContract);
		});

		It("counts a wiped crew: nothing brought back, the day spent, medical fees (§14)", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			State.Treasury = 120;
			const FRunDayReport Report = FRunContractCalculator::EndDay(Rules, State, 900, 3, true);
			TestEqual(TEXT("verdict"), Report.Verdict, ERunDayVerdict::NextDay);
			TestEqual(TEXT("nothing brought back"), Report.DayValue, 0);
			TestEqual(TEXT("contract value"), State.ContractValue, 0);
			TestEqual(TEXT("fees capped by the treasury"), Report.MedicalFees, 120);
			TestEqual(TEXT("treasury never negative"), State.Treasury, 0);
			TestEqual(TEXT("the day is spent"), State.DayInContract, 2);
		});

		It("never drops below the first zone", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			for (int32 Day = 0; Day < 3; ++Day)
			{
				FRunContractCalculator::EndDay(Rules, State, 0, 0, false);
			}
			TestEqual(TEXT("zone 1"), State.ZoneIndex, 0);
			TestEqual(TEXT("contract 2"), State.ContractNumber, 2);
		});
	});

	Describe("Victory", [this]()
	{
		It("wins after the contract of the last zone and counts nothing more", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			State.ZoneIndex = 4;
			State.Quota = 18000;
			State.DayInContract = 3;
			TestEqual(TEXT("victory"), FRunContractCalculator::EndDay(Rules, State, 20000, 0, false).Verdict, ERunDayVerdict::Victory);
			TestEqual(TEXT("outcome"), State.Outcome, ERunOutcome::Victory);
			TestEqual(TEXT("nothing more"), FRunContractCalculator::EndDay(Rules, State, 1, 0, false).Verdict, ERunDayVerdict::Invalid);
		});
	});

	Describe("Scales", [this]()
	{
		It("grows the danger with the zone and the greed day", [this]()
		{
			const FRunContractRules Rules = MakeRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			TestEqual(TEXT("zone 1"), FRunContractCalculator::DangerScale(Rules, State), 1.0f);
			State.ZoneIndex = 2;
			TestEqual(TEXT("zone 3"), FRunContractCalculator::DangerScale(Rules, State), 3.0f);
			State.bGreedDay = true;
			TestEqual(TEXT("greed"), FRunContractCalculator::DangerScale(Rules, State), 3.0f * Rules.GreedDangerScale);
			TestEqual(TEXT("flood earlier"), FRunContractCalculator::FloodDelayScale(Rules, State), 0.8f, 1.0e-4f);
		});

		It("ships the three stages of the ten-day campaign in DA_RunRules defaults (5 500, 10 500, 20 500)", [this]()
		{
			const FRunContractRules Rules = GetDefault<URunRules>()->MakeContractRules();
			TestEqual(TEXT("three stages"), Rules.Zones.Num(), 3);
			if (Rules.Zones.Num() == 3)
			{
				TestEqual(TEXT("day 3"), Rules.Zones[0].Quota, 5500);
				TestEqual(TEXT("day 6"), Rules.Zones[1].Quota, 10500);
				TestEqual(TEXT("day 10"), Rules.Zones[2].Quota, 20500);
				int32 Days = 0;
				for (int32 Zone = 0; Zone < 3; ++Zone)
				{
					Days += FRunContractCalculator::DaysOfZone(Rules, Zone);
				}
				TestEqual(TEXT("ten days"), Days, 10);
				// since 2026-10-09 the difficulty curve by day carries the danger and the deluge (DA_RunRules.DifficultyByDay)
				TestEqual(TEXT("neutral danger"), Rules.Zones[2].SpawnScale, 1.0f);
				TestEqual(TEXT("neutral deluge"), Rules.Zones[2].FloodDelayScale, 1.0f);
				TestTrue(TEXT("richer loot at the end"), Rules.Zones[2].LootValueScale > Rules.Zones[0].LootValueScale);
			}
		});

		It("lets a zone last its own number of days", [this]()
		{
			FRunContractRules Rules = MakeRules();
			Rules.Zones[0].Days = 4;
			TestEqual(TEXT("own days"), FRunContractCalculator::DaysOfZone(Rules, 0), 4);
			TestEqual(TEXT("default days"), FRunContractCalculator::DaysOfZone(Rules, 1), 3);
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			for (int32 Day = 1; Day <= 3; ++Day)
			{
				TestEqual(TEXT("next day"), FRunContractCalculator::EndDay(Rules, State, 100, 0, false).Verdict, ERunDayVerdict::NextDay);
			}
			TestEqual(TEXT("day 4 of 4"), State.DayInContract, 4);
			TestEqual(TEXT("failed on day 4"), FRunContractCalculator::EndDay(Rules, State, 100, 0, false).Verdict, ERunDayVerdict::ContractFailed);
		});
	});
}

#endif
