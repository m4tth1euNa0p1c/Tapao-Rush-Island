// monke_island

#include "Misc/AutomationTest.h"
#include "Run/RunContract.h"
#include "UI/Tapao/TapaoDayEndText.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** A day of the first contract (quota 500, small figures: no digit grouping in any culture) */
	FTapaoDayEndInput MakeInput(ERunDayVerdict Verdict, bool bWiped)
	{
		FTapaoDayEndInput Input;
		Input.Report.Verdict = Verdict;
		Input.Report.ZoneIndex = 0;
		Input.Report.Day = 2;
		Input.Report.Quota = 500;
		Input.Report.ContractValue = 300;
		Input.Report.DayValue = bWiped ? 0 : 120;
		Input.Report.bWiped = bWiped;
		Input.Report.Deaths = bWiped ? 2 : 0;
		Input.Report.MedicalFees = bWiped ? 100 : 0;
		Input.CampaignDay = 5;
		Input.ZoneName = FText::AsCultureInvariant(TEXT("Temples et forts"));
		Input.NextZoneName = FText::AsCultureInvariant(TEXT("Temples et forts"));
		Input.DeadlineDay = 6;
		Input.Treasury = 400;
		return Input;
	}

	bool AnyLineContains(const FTapaoDayEndView& View, const TCHAR* Part)
	{
		return View.Lines.ContainsByPredicate([Part](const FText& Line) { return Line.ToString().Contains(Part); });
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.UI.DayEnd
BEGIN_DEFINE_SPEC(FTapaoDayEndSpec, "monke_island.UI.DayEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTapaoDayEndSpec)

void FTapaoDayEndSpec::Define()
{
	// T-173 (ADR-007 §14): the whole crew dead loses the day, not the game
	Describe("Lost day", [this]()
	{
		It("titles the day with the campaign's count: JOUR 5 TERMINÉ, JOUR 5 PERDU", [this]()
		{
			TestEqual(TEXT("survived"), TapaoDayEndText::Build(MakeInput(ERunDayVerdict::NextDay, false)).Title.ToString(), FString(TEXT("JOUR 5 TERMINÉ")));
			TestEqual(TEXT("lost"), TapaoDayEndText::Build(MakeInput(ERunDayVerdict::NextDay, true)).Title.ToString(), FString(TEXT("JOUR 5 PERDU")));
			FTapaoDayEndInput NoCampaignDay = MakeInput(ERunDayVerdict::NextDay, false);
			NoCampaignDay.CampaignDay = 0;
			TestEqual(TEXT("the report's day without the campaign's"), TapaoDayEndText::Build(NoCampaignDay).Title.ToString(), FString(TEXT("JOUR 2 TERMINÉ")));
		});

		It("goes on to the next day: the loot stays on the island, the host may send the galleon on", [this]()
		{
			const FTapaoDayEndView View = TapaoDayEndText::Build(MakeInput(ERunDayVerdict::NextDay, true));
			TestEqual(TEXT("kind"), View.Kind, ETapaoDayEndKind::DayLost);
			TestTrue(TEXT("day lost"), View.bDayLost);
			TestTrue(TEXT("in red"), View.bAlarm);
			TestFalse(TEXT("the campaign goes on"), View.bCampaignLost);
			TestTrue(TEXT("JOUR SUIVANT for the host"), View.bOffersNextDay);
			TestFalse(TEXT("no greed question"), View.bAsksGreed);
			TestTrue(TEXT("the loot stays on the island"), AnyLineContains(View, TEXT("le butin reste sur l'île")));
			TestTrue(TEXT("the fees of the dead"), AnyLineContains(View, TEXT("100")));
			TestTrue(TEXT("the next day and the deadline"), View.Outcome.ToString().Contains(TEXT("jour 6")));
			TestTrue(TEXT("no Bureau header over a day"), View.Header.IsEmpty());
		});

		It("keeps the cause of the local player's death readable on a lost day only (T-135)", [this]()
		{
			FTapaoDayEndInput Lost = MakeInput(ERunDayVerdict::NextDay, true);
			Lost.DeathCause = FText::AsCultureInvariant(TEXT("Noyé"));
			TestTrue(TEXT("the cause"), AnyLineContains(TapaoDayEndText::Build(Lost), TEXT("Tu es mort : Noyé")));
			FTapaoDayEndInput Survived = MakeInput(ERunDayVerdict::NextDay, false);
			Survived.DeathCause = FText::AsCultureInvariant(TEXT("Noyé"));
			TestFalse(TEXT("not on a day survived"), AnyLineContains(TapaoDayEndText::Build(Survived), TEXT("Tu es mort")));
		});

		It("keeps the greed question open after a lost day, and offers no other button", [this]()
		{
			FTapaoDayEndInput Input = MakeInput(ERunDayVerdict::QuotaMetChoose, true);
			Input.bAwaitingChoice = true;
			Input.Report.ContractValue = 600;
			const FTapaoDayEndView View = TapaoDayEndText::Build(Input);
			TestTrue(TEXT("asks"), View.bAsksGreed);
			TestFalse(TEXT("no JOUR SUIVANT while the question is open"), View.bOffersNextDay);
			TestTrue(TEXT("quota met"), View.bQuotaMet);
		});

		It("counts down to the answer or to the next day; nothing on a day survived, nothing without a travel", [this]()
		{
			TestEqual(TEXT("answer"), TapaoDayEndText::Countdown(true, false, 12).ToString(), FString(TEXT("Réponse attendue dans 12 s")));
			TestEqual(TEXT("next day"), TapaoDayEndText::Countdown(false, true, 25).ToString(), FString(TEXT("Jour suivant dans 25 s")));
			TestTrue(TEXT("a day survived: the HUD counts the departure"), TapaoDayEndText::Countdown(false, false, 8).IsEmpty());
			TestTrue(TEXT("no travel"), TapaoDayEndText::Countdown(false, true, -1).IsEmpty());
		});

		It("keeps the campaign under way once the run state is filled: no NAUFRAGÉ after a wipe", [this]()
		{
			TestTrue(TEXT("a quota"), TapaoDayEndText::IsCampaignUnderWay(5500, 0, ERunDayVerdict::Invalid));
			TestTrue(TEXT("days played"), TapaoDayEndText::IsCampaignUnderWay(0, 3, ERunDayVerdict::Invalid));
			TestTrue(TEXT("a day counted"), TapaoDayEndText::IsCampaignUnderWay(0, 0, ERunDayVerdict::NextDay));
			TestFalse(TEXT("a map without a campaign"), TapaoDayEndText::IsCampaignUnderWay(0, 0, ERunDayVerdict::Invalid));
		});
	});

	Describe("Contract", [this]()
	{
		It("met: CONTRAT TERMINÉ in the Bureau's words, the excess paid, the next destination", [this]()
		{
			FTapaoDayEndInput Input = MakeInput(ERunDayVerdict::ContractComplete, false);
			Input.Report.ContractValue = 650;
			Input.Report.Payout = 150;
			Input.NextZoneName = FText::AsCultureInvariant(TEXT("Grand atoll"));
			const FTapaoDayEndView View = TapaoDayEndText::Build(Input);
			TestEqual(TEXT("kind"), View.Kind, ETapaoDayEndKind::ContractDone);
			TestEqual(TEXT("title"), View.Title.ToString(), FString(TEXT("CONTRAT TERMINÉ")));
			TestFalse(TEXT("the Bureau's header"), View.Header.IsEmpty());
			TestFalse(TEXT("good news"), View.bAlarm);
			TestTrue(TEXT("quota met"), View.bQuotaMet);
			TestTrue(TEXT("the excess"), AnyLineContains(View, TEXT("+150")));
			TestTrue(TEXT("the day in the subtitle"), View.Subtitle.ToString().Contains(TEXT("jour 5 terminé")));
			TestTrue(TEXT("the next destination"), View.Outcome.ToString().Contains(TEXT("Grand atoll")));
			TestFalse(TEXT("the Bureau never shouts"), View.Outcome.ToString().Contains(TEXT("!")));
		});

		It("met on a lost day: the contract notice, the day lost in its subtitle", [this]()
		{
			FTapaoDayEndInput Input = MakeInput(ERunDayVerdict::ContractComplete, true);
			Input.Report.ContractValue = 650;
			const FTapaoDayEndView View = TapaoDayEndText::Build(Input);
			TestEqual(TEXT("title"), View.Title.ToString(), FString(TEXT("CONTRAT TERMINÉ")));
			TestTrue(TEXT("subtitle"), View.Subtitle.ToString().Contains(TEXT("jour 5 perdu")));
			TestTrue(TEXT("the host may send the galleon on"), View.bOffersNextDay);
		});

		It("the last one met: CAMPAGNE HONORÉE", [this]()
		{
			const FTapaoDayEndView View = TapaoDayEndText::Build(MakeInput(ERunDayVerdict::Victory, false));
			TestEqual(TEXT("kind"), View.Kind, ETapaoDayEndKind::CampaignWon);
			TestEqual(TEXT("title"), View.Title.ToString(), FString(TEXT("CAMPAGNE HONORÉE")));
		});

		It("failed with the campaign going on: CONTRAT ÉCHOUÉ, the crowns missing, the penalty and its share", [this]()
		{
			FTapaoDayEndInput Input = MakeInput(ERunDayVerdict::ContractFailed, false);
			Input.Report.Penalty = 200;
			Input.FailedTreasuryLoss = 0.5f;
			const FTapaoDayEndView View = TapaoDayEndText::Build(Input);
			TestEqual(TEXT("kind"), View.Kind, ETapaoDayEndKind::ContractFailed);
			TestEqual(TEXT("title"), View.Title.ToString(), FString(TEXT("CONTRAT ÉCHOUÉ")));
			TestTrue(TEXT("in red"), View.bAlarm);
			TestFalse(TEXT("the campaign goes on"), View.bCampaignLost);
			TestTrue(TEXT("200 crowns missing"), AnyLineContains(View, TEXT("Il manque 200")));
			TestTrue(TEXT("the penalty and its share"), AnyLineContains(View, TEXT("(50 %)")));
			TestTrue(TEXT("the upgrades are kept"), View.Outcome.ToString().Contains(TEXT("améliorations")));
		});

		It("failed with the execution rule: the campaign lost, the squad for the living, posthumous for a wiped crew", [this]()
		{
			const FTapaoDayEndView Executed = TapaoDayEndText::Build(MakeInput(ERunDayVerdict::Executed, false));
			TestEqual(TEXT("kind"), Executed.Kind, ETapaoDayEndKind::CampaignLost);
			TestTrue(TEXT("campaign lost"), Executed.bCampaignLost);
			TestFalse(TEXT("no JOUR SUIVANT"), Executed.bOffersNextDay);
			TestTrue(TEXT("the French squad"), Executed.Outcome.ToString().Contains(TEXT("peloton")));
			const FTapaoDayEndView Wiped = TapaoDayEndText::Build(MakeInput(ERunDayVerdict::Executed, true));
			TestEqual(TEXT("title"), Wiped.Title.ToString(), FString(TEXT("CONTRAT ÉCHOUÉ")));
			TestTrue(TEXT("posthumous"), Wiped.Outcome.ToString().Contains(TEXT("posthume")));
			TestFalse(TEXT("no JOUR SUIVANT for a lost campaign"), Wiped.bOffersNextDay);
		});

		It("writes the end screen of a lost campaign: the zone, the crowns missing, the day it ends", [this]()
		{
			FRunDayReport Report;
			Report.Verdict = ERunDayVerdict::Executed;
			Report.Quota = 500;
			Report.ContractValue = 320;
			const TArray<FText> Lines = TapaoDayEndText::LostCampaignLines(Report, FText::AsCultureInvariant(TEXT("Villages et marae")), 3);
			TestEqual(TEXT("four lines"), Lines.Num(), 4);
			TestTrue(TEXT("the zone"), Lines[0].ToString().Contains(TEXT("Villages et marae")));
			TestTrue(TEXT("180 missing"), Lines[1].ToString().Contains(TEXT("180")));
			TestTrue(TEXT("the squad"), Lines[2].ToString().Contains(TEXT("peloton")));
			TestTrue(TEXT("day 3"), Lines[3].ToString().Contains(TEXT("jour 3")));
			Report.bWiped = true;
			TestTrue(TEXT("posthumous"), TapaoDayEndText::LostCampaignLines(Report, FText::GetEmpty(), 3)[2].ToString().Contains(TEXT("posthume")));
			TestEqual(TEXT("no report: the short line"), TapaoDayEndText::LostCampaignLines(FRunDayReport(), FText::GetEmpty(), 0).Num(), 1);
		});
	});

	// ADR-007 §16: the host decides, the answer reaches everybody with the run state
	Describe("Greed", [this]()
	{
		It("asks while the question is open, then tells what the answer led to", [this]()
		{
			TestEqual(TEXT("open"), TapaoDayEndText::EffectiveVerdict(ERunDayVerdict::QuotaMetChoose, true, ERunOutcome::InProgress, false), ERunDayVerdict::QuotaMetChoose);
			TestEqual(TEXT("stay"), TapaoDayEndText::EffectiveVerdict(ERunDayVerdict::QuotaMetChoose, false, ERunOutcome::InProgress, true), ERunDayVerdict::NextDay);
			TestEqual(TEXT("leave"), TapaoDayEndText::EffectiveVerdict(ERunDayVerdict::QuotaMetChoose, false, ERunOutcome::InProgress, false), ERunDayVerdict::ContractComplete);
			TestEqual(TEXT("leave the last zone"), TapaoDayEndText::EffectiveVerdict(ERunDayVerdict::QuotaMetChoose, false, ERunOutcome::Victory, false), ERunDayVerdict::Victory);
			TestEqual(TEXT("other verdicts as they are"), TapaoDayEndText::EffectiveVerdict(ERunDayVerdict::ContractFailed, false, ERunOutcome::InProgress, false), ERunDayVerdict::ContractFailed);
		});

		It("writes what one more day changes from DA_RunRules, and the answer when nobody answers", [this]()
		{
			const FString Leave = TapaoDayEndText::GreedHint(1.5f, 1.4f, false).ToString();
			TestTrue(TEXT("+50 % loot"), Leave.Contains(TEXT("+50 %")));
			TestTrue(TEXT("+40 % danger"), Leave.Contains(TEXT("+40 %")));
			TestTrue(TEXT("leaves by default"), Leave.Contains(TEXT("quitte la région")));
			const FString Stay = TapaoDayEndText::GreedHint(2.0f, 0.8f, true).ToString();
			TestTrue(TEXT("+100 %"), Stay.Contains(TEXT("+100 %")));
			TestTrue(TEXT("-20 %"), Stay.Contains(TEXT("-20 %")));
			TestTrue(TEXT("stays by default"), Stay.Contains(TEXT("reste")));
		});

		It("shows the stake to everybody: the hint comes with the question only", [this]()
		{
			FTapaoDayEndInput Input = MakeInput(ERunDayVerdict::QuotaMetChoose, false);
			Input.bAwaitingChoice = true;
			TestFalse(TEXT("hint while open"), TapaoDayEndText::Build(Input).GreedHint.IsEmpty());
			TestTrue(TEXT("no hint after"), TapaoDayEndText::Build(MakeInput(ERunDayVerdict::NextDay, false)).GreedHint.IsEmpty());
		});

		It("names the greed day after the answer stay", [this]()
		{
			FTapaoDayEndInput Input = MakeInput(ERunDayVerdict::NextDay, false);
			Input.bGreedDay = true;
			TestTrue(TEXT("one more day"), TapaoDayEndText::Build(Input).Outcome.ToString().Contains(TEXT("Un jour de plus")));
		});
	});

	// T-283: one count of the days, the campaign's, top left in small
	Describe("HUD contract line", [this]()
	{
		It("puts the deadline of the contract in days of the campaign (5 500 on day 3, 10 500 on day 6, 20 500 on day 10)", [this]()
		{
			TestEqual(TEXT("day 1, contract day 1/3"), TapaoDayEndText::ContractDeadlineDay(0, 1, 3), 3);
			TestEqual(TEXT("day 2, contract day 2/3"), TapaoDayEndText::ContractDeadlineDay(1, 2, 3), 3);
			TestEqual(TEXT("day 4, contract day 1/3"), TapaoDayEndText::ContractDeadlineDay(3, 1, 3), 6);
			TestEqual(TEXT("day 7, contract day 1/4"), TapaoDayEndText::ContractDeadlineDay(6, 1, 4), 10);
			TestEqual(TEXT("day 10, contract day 4/4"), TapaoDayEndText::ContractDeadlineDay(9, 4, 4), 10);
			TestEqual(TEXT("no run"), TapaoDayEndText::ContractDeadlineDay(0, 0, 3), 0);
		});

		It("writes Contrat : jour N, and the greed day", [this]()
		{
			TestEqual(TEXT("deadline"), TapaoDayEndText::ContractLine(1, 2, 3, false).ToString(), FString(TEXT("Contrat : jour 3")));
			TestTrue(TEXT("greed day"), TapaoDayEndText::ContractLine(1, 2, 3, true).ToString().EndsWith(TEXT("jour de cupidité")));
			TestTrue(TEXT("no run"), TapaoDayEndText::ContractLine(0, 1, 0, false).IsEmpty());
		});
	});
}

#endif
