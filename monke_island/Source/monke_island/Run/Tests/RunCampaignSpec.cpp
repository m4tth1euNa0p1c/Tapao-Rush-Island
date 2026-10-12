// monke_island

#include "Misc/AutomationTest.h"
#include "Run/RunCampaign.h"
#include "Run/RunContract.h"
#include "Run/RunDifficulty.h"
#include "Run/RunRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Run.Campaign
BEGIN_DEFINE_SPEC(FRunCampaignSpec, "monke_island.Run.Campaign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRunCampaignSpec)

void FRunCampaignSpec::Define()
{
	Describe("Unlocks", [this]()
	{
		It("opens the content on the days asked by the owner (temple and crabs from day 1, forts and every artifact temple from day 3, the camps 7)", [this]()
		{
			const FRunCampaignUnlocks Unlocks;
			TestEqual(TEXT("temples"), RunCampaign::UnlockDay(Unlocks, ERunUnlock::Temples), 1);
			TestEqual(TEXT("forts"), RunCampaign::UnlockDay(Unlocks, ERunUnlock::Forts), 3);
			TestEqual(TEXT("buildings"), RunCampaign::UnlockDay(Unlocks, ERunUnlock::AllBuildings), 7);
			TestEqual(TEXT("artifacts"), RunCampaign::UnlockDay(Unlocks, ERunUnlock::AllArtifacts), 3);
			TestEqual(TEXT("crabs"), RunCampaign::UnlockDay(Unlocks, ERunUnlock::GiantCrabs), 1);
			TestTrue(TEXT("temple on day 1"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::Temples, 1));
			TestTrue(TEXT("crabs on day 1"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::GiantCrabs, 1));
			TestFalse(TEXT("no fort on day 2"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::Forts, 2));
			TestTrue(TEXT("forts on day 3"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::Forts, 3));
			TestFalse(TEXT("one temple on day 2"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::AllArtifacts, 2));
			TestTrue(TEXT("every artifact temple on day 3"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::AllArtifacts, 3));
			TestFalse(TEXT("no camp on day 6"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::AllBuildings, 6));
			TestTrue(TEXT("camps on day 7"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::AllBuildings, 7));
		});

		It("still holds a content back until its day when the rules ask for it", [this]()
		{
			FRunCampaignUnlocks Unlocks;
			Unlocks.FortsFromDay = 6;
			TestFalse(TEXT("no fort on day 5"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::Forts, 5));
			TestTrue(TEXT("forts on day 6"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::Forts, 6));
		});

		It("opens everything without a campaign (day 0: test maps)", [this]()
		{
			const FRunCampaignUnlocks Unlocks;
			TestTrue(TEXT("temples"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::Temples, 0));
			TestTrue(TEXT("buildings"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::AllBuildings, 0));
			TestTrue(TEXT("crabs"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::GiantCrabs, 0));
		});

		It("puts one temple on days 1 and 2, then every artifact temple from day 3 (owner 2026-10-09)", [this]()
		{
			const FRunCampaignUnlocks Unlocks;
			TestEqual(TEXT("day 1"), RunCampaign::TemplesPerFragment(Unlocks, 1), 1);
			TestEqual(TEXT("day 2"), RunCampaign::TemplesPerFragment(Unlocks, 2), 1);
			TestEqual(TEXT("day 3"), RunCampaign::TemplesPerFragment(Unlocks, 3), Unlocks.TemplesPerFragmentAll);
			TestEqual(TEXT("day 7"), RunCampaign::TemplesPerFragment(Unlocks, 7), Unlocks.TemplesPerFragmentAll);
			TestEqual(TEXT("no campaign: every temple"), RunCampaign::TemplesPerFragment(Unlocks, 0), Unlocks.TemplesPerFragmentAll);
			FRunCampaignUnlocks Later;
			Later.TemplesFromDay = 3;
			TestEqual(TEXT("none before its day"), RunCampaign::TemplesPerFragment(Later, 2), 0);
		});

		It("arms the tiki of the villages from day 6 (the 6th wave of the owner)", [this]()
		{
			const FRunCampaignUnlocks Unlocks;
			TestEqual(TEXT("unlock day"), RunCampaign::UnlockDay(Unlocks, ERunUnlock::ArmedMobs), 6);
			TestFalse(TEXT("no gun on day 5"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::ArmedMobs, 5));
			TestTrue(TEXT("guns on day 6"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::ArmedMobs, 6));
			TestTrue(TEXT("guns without a campaign"), RunCampaign::IsUnlocked(Unlocks, ERunUnlock::ArmedMobs, 0));
		});

		It("adds the extra tiki from day 2", [this]()
		{
			const FRunCampaignUnlocks Unlocks;
			TestEqual(TEXT("day 1"), RunCampaign::MobScale(Unlocks, 1), 1.0f);
			TestEqual(TEXT("day 2"), RunCampaign::MobScale(Unlocks, 2), Unlocks.MobSurplusScale);
			TestEqual(TEXT("no campaign"), RunCampaign::MobScale(Unlocks, 0), 1.0f);
		});

		It("ships the unlock days in DA_RunRules defaults (forts and every artifact temple on day 3)", [this]()
		{
			const FRunCampaignUnlocks& Campaign = GetDefault<URunRules>()->Campaign;
			TestEqual(TEXT("temples"), Campaign.TemplesFromDay, 1);
			TestEqual(TEXT("forts"), Campaign.FortsFromDay, 3);
			TestEqual(TEXT("every artifact temple"), Campaign.AllArtifactsFromDay, 3);
			TestEqual(TEXT("crabs"), Campaign.GiantCrabsFromDay, 1);
			TestEqual(TEXT("one artifact per temple"), Campaign.ArtifactsPerTemple, 1);
		});
	});

	Describe("Difficulty by day (owner 2026-10-09: tout devient plus dur)", [this]()
	{
		It("plays the rules as they are without a campaign (day 0) or without a curve", [this]()
		{
			const FRunDayDifficulty Neutral;
			TestEqual(TEXT("mobs"), Neutral.MobCountScale, 1.0f);
			TestEqual(TEXT("health"), Neutral.MobHealthScale, 1.0f);
			TestEqual(TEXT("damage"), Neutral.MobDamageScale, 1.0f);
			TestEqual(TEXT("soldiers: as placed"), Neutral.SoldiersPerFort, 0);
			TestEqual(TEXT("forts: the rules"), Neutral.FortsPerExpedition, 0);
			TestEqual(TEXT("crab: the rules"), Neutral.CrabEmergenceMinute, 0.0f);
			TestEqual(TEXT("flood"), Neutral.FloodDelayScale, 1.0f);
			TestEqual(TEXT("night: the campaign's rule"), Neutral.NightShare, 0.0f);
			TArray<FRunDayDifficulty> Curve;
			FRunDayDifficulty& Hard = Curve.AddDefaulted_GetRef();
			Hard.MobCountScale = 3.0f;
			TestEqual(TEXT("day 0 with a curve"), RunDifficulty::Of(Curve, 0).MobCountScale, 1.0f);
			TestEqual(TEXT("day 5 without a curve"), RunDifficulty::Of(TArray<FRunDayDifficulty>(), 5).MobCountScale, 1.0f);
			TestEqual(TEXT("the rules' count"), RunDifficulty::CountOr(0, 7), 7);
			TestEqual(TEXT("the day's count"), RunDifficulty::CountOr(9, 7), 9);
		});

		It("reads the row of the day and keeps the last one for the days after the table", [this]()
		{
			TArray<FRunDayDifficulty> Curve;
			for (int32 Day = 1; Day <= 3; ++Day)
			{
				Curve.AddDefaulted_GetRef().MobCountScale = static_cast<float>(Day);
			}
			TestEqual(TEXT("day 1"), RunDifficulty::Of(Curve, 1).MobCountScale, 1.0f);
			TestEqual(TEXT("day 2"), RunDifficulty::Of(Curve, 2).MobCountScale, 2.0f);
			TestEqual(TEXT("day 3"), RunDifficulty::Of(Curve, 3).MobCountScale, 3.0f);
			TestEqual(TEXT("day 12: the last row"), RunDifficulty::Of(Curve, 12).MobCountScale, 3.0f);
		});

		It("holds a hand-edited row above its floors", [this]()
		{
			TArray<FRunDayDifficulty> Curve;
			FRunDayDifficulty& Broken = Curve.AddDefaulted_GetRef();
			Broken.MobCountScale = -1.0f;
			Broken.MobHealthScale = 0.0f;
			Broken.CrabHealthScale = -2.0f;
			Broken.FloodDelayScale = 0.0f;
			Broken.NightShare = 4.0f;
			Broken.SoldiersPerFort = -3;
			const FRunDayDifficulty Day = RunDifficulty::Of(Curve, 1);
			TestEqual(TEXT("no negative count"), Day.MobCountScale, 0.0f);
			TestEqual(TEXT("some health"), Day.MobHealthScale, 0.1f);
			TestEqual(TEXT("some crab health"), Day.CrabHealthScale, 0.1f);
			TestEqual(TEXT("a deluge still coming"), Day.FloodDelayScale, 0.1f);
			TestEqual(TEXT("a whole night at most"), Day.NightShare, 1.0f);
			TestEqual(TEXT("no negative garrison"), Day.SoldiersPerFort, 0);
			TestEqual(TEXT("health never under 1"), RunDifficulty::ScaledMaxHealth(100.0f, 0.0f), 1.0f);
			TestEqual(TEXT("health x2"), RunDifficulty::ScaledMaxHealth(100.0f, 2.0f), 200.0f);
		});

		It("ships a curve of ten days in DA_RunRules defaults that only gets harder", [this]()
		{
			const URunRules* Rules = GetDefault<URunRules>();
			const TArray<FRunDayDifficulty>& Curve = Rules->DifficultyByDay;
			TestEqual(TEXT("ten days"), Curve.Num(), 10);
			if (Curve.Num() != 10)
			{
				return;
			}
			TestTrue(TEXT("day 1 is gentle: fewer mobs"), Curve[0].MobCountScale < 1.0f);
			TestTrue(TEXT("day 1 is gentle: weaker mobs"), Curve[0].MobHealthScale < 1.0f);
			TestEqual(TEXT("day 2: the rules"), Curve[1].MobCountScale, 1.0f);
			for (int32 Day = 2; Day <= 10; ++Day)
			{
				const FRunDayDifficulty& Before = Curve[Day - 2];
				const FRunDayDifficulty& Now = Curve[Day - 1];
				TestTrue(*FString::Printf(TEXT("day %d: no fewer mobs"), Day), Now.MobCountScale >= Before.MobCountScale);
				TestTrue(*FString::Printf(TEXT("day %d: no weaker mobs"), Day), Now.MobHealthScale >= Before.MobHealthScale);
				TestTrue(*FString::Printf(TEXT("day %d: no softer blows"), Day), Now.MobDamageScale >= Before.MobDamageScale);
				TestTrue(*FString::Printf(TEXT("day %d: no smaller garrison"), Day), Now.SoldiersPerFort >= Before.SoldiersPerFort);
				TestTrue(*FString::Printf(TEXT("day %d: no worse aim"), Day), Now.SoldierHitChanceScale >= Before.SoldierHitChanceScale);
				TestTrue(*FString::Printf(TEXT("day %d: no fewer forts"), Day), Now.FortsPerExpedition >= Before.FortsPerExpedition);
				TestTrue(*FString::Printf(TEXT("day %d: no weaker crab"), Day), Now.CrabHealthScale >= Before.CrabHealthScale);
				TestTrue(*FString::Printf(TEXT("day %d: no later deluge"), Day), Now.FloodDelayScale <= Before.FloodDelayScale);
				TestTrue(*FString::Printf(TEXT("day %d: no shorter night"), Day), Now.NightShare >= Before.NightShare);
				if (Before.CrabEmergenceMinute > 0.0f)
				{
					TestTrue(*FString::Printf(TEXT("day %d: no later crab"), Day), Now.CrabEmergenceMinute > 0.0f && Now.CrabEmergenceMinute <= Before.CrabEmergenceMinute);
				}
			}
			// the days the owner named (2026-10-09)
			TestEqual(TEXT("day 3: one fort"), Rules->GetDayDifficulty(3).FortsPerExpedition, 1);
			TestEqual(TEXT("day 3: the garrison of the level"), Rules->GetDayDifficulty(3).SoldiersPerFort, 7);
			TestEqual(TEXT("day 6: two forts"), Rules->GetDayDifficulty(6).FortsPerExpedition, 2);
			TestEqual(TEXT("day 10: twice the mobs"), Rules->GetDayDifficulty(10).MobCountScale, 2.0f);
			TestEqual(TEXT("day 10: twice their health"), Rules->GetDayDifficulty(10).MobHealthScale, 2.0f);
			TestEqual(TEXT("day 10: three forts"), Rules->GetDayDifficulty(10).FortsPerExpedition, 3);
			TestEqual(TEXT("day 10: the crab at 4:00"), Rules->GetDayDifficulty(10).CrabEmergenceMinute, 4.0f);
			TestTrue(TEXT("day 10: the deluge at about 7 min of the 9 (540 s)"), FMath::Abs(540.0f * Rules->GetDayDifficulty(10).FloodDelayScale - 420.0f) <= 15.0f);
			TestEqual(TEXT("day 12 plays day 10"), Rules->GetDayDifficulty(12).MobCountScale, Rules->GetDayDifficulty(10).MobCountScale);
			TestEqual(TEXT("test maps play the rules"), Rules->GetDayDifficulty(0).MobCountScale, 1.0f);
		});

		It("leaves the danger and the deluge of the stages to the curve (the zones of DA_RunRules are neutral)", [this]()
		{
			const URunRules* Rules = GetDefault<URunRules>();
			for (const FRunZone& Zone : Rules->Zones)
			{
				TestEqual(*FString::Printf(TEXT("%s: spawns"), *Zone.ZoneId.ToString()), Zone.SpawnScale, 1.0f);
				TestEqual(*FString::Printf(TEXT("%s: deluge"), *Zone.ZoneId.ToString()), Zone.FloodDelayScale, 1.0f);
			}
		});

		It("starts the day late enough for the night share asked", [this]()
		{
			// the night falls at 18 h 30, an expedition lasts 8.5 solar hours up to the deluge
			TestEqual(TEXT("half the day at night: start at 14 h 15"), RunDifficulty::NightShareStartHour(18.5f, 8.5f, 0.5f), 14.25f, 1.0e-4f);
			TestEqual(TEXT("all night: start at the night fall"), RunDifficulty::NightShareStartHour(18.5f, 8.5f, 1.0f), 18.5f, 1.0e-4f);
			TestEqual(TEXT("no night: start the whole expedition before"), RunDifficulty::NightShareStartHour(18.5f, 8.5f, 0.0f), 10.0f, 1.0e-4f);
			TestEqual(TEXT("a shorter day (deluge x0.78) starts later"), RunDifficulty::NightShareStartHour(18.5f, 8.5f * 0.78f, 0.8f), 18.5f - 0.2f * 8.5f * 0.78f, 1.0e-3f);
			TestEqual(TEXT("never before midnight"), RunDifficulty::NightShareStartHour(2.0f, 30.0f, 0.0f), 0.0f, 1.0e-4f);
			TestTrue(TEXT("the defaults tell the night fall and the day's length"), GetDefault<URunRules>()->NightFallHour > 18.0f && GetDefault<URunRules>()->ExpeditionSolarHours > 0.0f);
		});
	});

	Describe("Artifacts", [this]()
	{
		It("deals one of the six to the first temple", [this]()
		{
			FRandomStream Stream(7);
			const TArray<TArray<int32>> Hands = RunCampaign::DealArtifacts(6, 1, 1, Stream);
			TestEqual(TEXT("one temple"), Hands.Num(), 1);
			if (Hands.Num() == 1)
			{
				TestEqual(TEXT("one artifact"), Hands[0].Num(), 1);
				TestTrue(TEXT("in the pool"), Hands[0].Num() == 1 && Hands[0][0] >= 0 && Hands[0][0] < 6);
			}
		});

		It("never deals the same artifact twice and leaves the temples past the pool empty", [this]()
		{
			FRandomStream Stream(11);
			const TArray<TArray<int32>> Hands = RunCampaign::DealArtifacts(2, 3, 1, Stream);
			TestEqual(TEXT("three temples"), Hands.Num(), 3);
			if (Hands.Num() == 3)
			{
				TestEqual(TEXT("first"), Hands[0].Num(), 1);
				TestEqual(TEXT("second"), Hands[1].Num(), 1);
				TestEqual(TEXT("third is empty"), Hands[2].Num(), 0);
				TestTrue(TEXT("different"), Hands[0].Num() == 1 && Hands[1].Num() == 1 && Hands[0][0] != Hands[1][0]);
			}
			FRandomStream Wide(3);
			TSet<int32> Seen;
			for (const TArray<int32>& Hand : RunCampaign::DealArtifacts(6, 3, 2, Wide))
			{
				for (const int32 Index : Hand)
				{
					TestFalse(TEXT("no repeat"), Seen.Contains(Index));
					Seen.Add(Index);
				}
			}
			TestEqual(TEXT("six dealt"), Seen.Num(), 6);
		});

		It("deals the same hands for the same seed", [this]()
		{
			FRandomStream First(42);
			FRandomStream Second(42);
			TestTrue(TEXT("same"), RunCampaign::DealArtifacts(6, 3, 1, First) == RunCampaign::DealArtifacts(6, 3, 1, Second));
		});
	});

	Describe("Night and day (owner 2026-10-07)", [this]()
	{
		It("brings more tiki out of the bushes by day from day 2, never without a campaign", [this]()
		{
			FRunCampaignUnlocks Unlocks;
			Unlocks.DayBushMobsFromDay = 2;
			Unlocks.DayBushMobScale = 2.5f;
			TestEqual(TEXT("day 1"), RunCampaign::DayBushScale(Unlocks, 1), 1.0f);
			TestEqual(TEXT("day 2"), RunCampaign::DayBushScale(Unlocks, 2), 2.5f);
			TestEqual(TEXT("day 9"), RunCampaign::DayBushScale(Unlocks, 9), 2.5f);
			TestEqual(TEXT("no campaign"), RunCampaign::DayBushScale(Unlocks, 0), 1.0f);
		});

		It("makes the night fall early from day 3: the expedition starts late in the day", [this]()
		{
			FRunCampaignUnlocks Unlocks;
			Unlocks.EarlyNightFromDay = 3;
			Unlocks.EarlyNightStartHours = FFloatInterval(13.0f, 15.5f);
			FFloatInterval Hours(6.5f, 18.0f);
			TestFalse(TEXT("day 2: the usual draw"), RunCampaign::EarlyNightStartHours(Unlocks, 2, Hours));
			TestEqual(TEXT("untouched"), Hours.Min, 6.5f);
			TestTrue(TEXT("day 3"), RunCampaign::EarlyNightStartHours(Unlocks, 3, Hours));
			TestEqual(TEXT("from 13 h"), Hours.Min, 13.0f);
			TestEqual(TEXT("to 15 h 30"), Hours.Max, 15.5f);
			TestFalse(TEXT("no campaign"), RunCampaign::EarlyNightStartHours(Unlocks, 0, Hours));
		});

		It("ships these days in DA_RunRules defaults (night on day 3, bushes on day 2)", [this]()
		{
			const FRunCampaignUnlocks& Campaign = GetDefault<URunRules>()->Campaign;
			TestEqual(TEXT("early night"), RunCampaign::UnlockDay(Campaign, ERunUnlock::EarlyNight), 3);
			TestEqual(TEXT("day bush mobs"), RunCampaign::UnlockDay(Campaign, ERunUnlock::DayBushMobs), 2);
			TestTrue(TEXT("more by day"), Campaign.DayBushMobScale > 1.0f);
		});
	});

	Describe("Days", [this]()
	{
		It("jumps to a day of the campaign: its stage, its day in the contract, the treasury kept", [this]()
		{
			const FRunContractRules Rules = GetDefault<URunRules>()->MakeContractRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			State.Treasury = 300;
			FRunContractCalculator::JumpToDay(Rules, State, 3);
			TestEqual(TEXT("day 3: first stage"), State.ZoneIndex, 0);
			TestEqual(TEXT("day 3 of 3"), State.DayInContract, 3);
			TestEqual(TEXT("two days played"), State.DaysPlayed, 2);
			FRunContractCalculator::JumpToDay(Rules, State, 4);
			TestEqual(TEXT("day 4: second stage"), State.ZoneIndex, 1);
			TestEqual(TEXT("day 1 of the contract"), State.DayInContract, 1);
			TestEqual(TEXT("quota 10 500"), State.Quota, 10500);
			FRunContractCalculator::JumpToDay(Rules, State, 10);
			TestEqual(TEXT("day 10: last stage"), State.ZoneIndex, 2);
			TestEqual(TEXT("day 4 of 4"), State.DayInContract, 4);
			TestEqual(TEXT("quota 20 500"), State.Quota, 20500);
			TestEqual(TEXT("third contract"), State.ContractNumber, 3);
			TestEqual(TEXT("treasury kept"), State.Treasury, 300);
			TestEqual(TEXT("nothing brought back"), State.ContractValue, 0);
			FRunContractCalculator::JumpToDay(Rules, State, 12);
			TestEqual(TEXT("past the end: last day"), State.DayInContract, 4);
			TestEqual(TEXT("eleven days played"), State.DaysPlayed, 11);
		});

		It("counts the campaign across the contracts (day 4 follows the third day of the first stage)", [this]()
		{
			const FRunContractRules Rules = GetDefault<URunRules>()->MakeContractRules();
			FRunContractState State = FRunContractCalculator::StartRun(Rules);
			FRunContractCalculator::EndDay(Rules, State, 2000, 0, false);
			FRunContractCalculator::EndDay(Rules, State, 2000, 0, false);
			FRunContractCalculator::EndDay(Rules, State, 2000, 0, false);
			TestEqual(TEXT("second stage"), State.ZoneIndex, 1);
			TestEqual(TEXT("three days played: day 4"), State.DaysPlayed + 1, 4);
		});
	});
}

#endif
