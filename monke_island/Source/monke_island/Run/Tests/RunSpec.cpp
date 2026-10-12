// monke_island

#include "Misc/AutomationTest.h"
#include "Run/RunCalculator.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Run
BEGIN_DEFINE_SPEC(FRunCalculatorSpec, "monke_island.Run.Calculator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** A register of the round with these sale values, one item each */
	FRunRoundInput MakeRound(int32 RoundIndex, int32 Players, int32 Deaths, std::initializer_list<int32> Values)
	{
		FRunRoundInput Input;
		Input.RoundId = FGuid::NewGuid();
		Input.RoundIndex = RoundIndex;
		Input.PlayerCount = Players;
		Input.Deaths = Deaths;
		Input.Ledger.RoundId = Input.RoundId;
		for (const int32 Value : Values)
		{
			FRunCalculator::TrySell(Input.Ledger, FGuid::NewGuid(), NAME_None, Value);
		}
		return Input;
	}

END_DEFINE_SPEC(FRunCalculatorSpec)

void FRunCalculatorSpec::Define()
{
	const FRunRulesSnapshot Rules;

	Describe("Quota", [this, Rules]()
	{
		It("climbs 250, 400, 610, 880, 1210 for one player", [this, Rules]()
		{
			const int32 Expected[] = { 250, 400, 610, 880, 1210 };
			for (int32 Round = 1; Round <= 5; ++Round)
			{
				TestEqual(*FString::Printf(TEXT("round %d"), Round), FRunCalculator::QuotaForRound(Rules, Round, 1), Expected[Round - 1]);
			}
		});

		It("grows by a quarter per extra player, rounded", [this, Rules]()
		{
			TestEqual(TEXT("round 2, two players"), FRunCalculator::QuotaForRound(Rules, 2, 2), 500);
			TestEqual(TEXT("round 1, four players: 437.5"), FRunCalculator::QuotaForRound(Rules, 1, 4), 438);
			TestEqual(TEXT("round 0 reads as round 1"), FRunCalculator::QuotaForRound(Rules, 0, 1), 250);
		});

		It("raises the difficulty by a tenth per round, at most half", [this, Rules]()
		{
			TestEqual(TEXT("round 1"), FRunCalculator::DifficultyForRound(Rules, 1), 1.0f);
			TestEqual(TEXT("round 3"), FRunCalculator::DifficultyForRound(Rules, 3), 1.2f, 0.0001f);
			TestEqual(TEXT("round 9: capped"), FRunCalculator::DifficultyForRound(Rules, 9), 1.5f);
		});
	});

	Describe("Sales", [this]()
	{
		It("counts an item once, never after the signature", [this]()
		{
			FRunLedger Ledger;
			const FGuid Chest = FGuid::NewGuid();
			TestEqual(TEXT("sold"), FRunCalculator::TrySell(Ledger, Chest, NAME_None, 100), ERunSaleError::None);
			TestEqual(TEXT("double click"), FRunCalculator::TrySell(Ledger, Chest, NAME_None, 100), ERunSaleError::AlreadySold);
			TestEqual(TEXT("no id"), FRunCalculator::TrySell(Ledger, FGuid(), NAME_None, 50), ERunSaleError::InvalidItem);
			TestEqual(TEXT("gross"), Ledger.GetGross(), 100);
			Ledger.bClosed = true;
			TestEqual(TEXT("signed"), FRunCalculator::TrySell(Ledger, FGuid::NewGuid(), NAME_None, 30), ERunSaleError::RoundClosed);
		});
	});

	Describe("Register", [this, Rules]()
	{
		It("gives the worked example of cahier §24.9 (RUN14)", [this, Rules]()
		{
			const FRunRoundInput Input = MakeRound(2, 2, 1, { 300, 200, 120 });
			const FRunRoundResult Result = FRunCalculator::ComputeRound(Rules, Input, 0);
			TestEqual(TEXT("quota"), Result.Quota, 500);
			TestEqual(TEXT("gross"), Result.Gross, 620);
			TestTrue(TEXT("quota met"), Result.bQuotaMet);
			TestEqual(TEXT("tax"), Result.Tax, 217);
			TestEqual(TEXT("bonus"), Result.ContractBonus, 100);
			TestEqual(TEXT("fees"), Result.RecruitFees, 50);
			TestEqual(TEXT("net"), Result.Net, 453);
			TestEqual(TEXT("outcome"), Result.Outcome, ERunRoundOutcome::NextRound);
		});

		It("loses the run one crown short, with no bonus", [this, Rules]()
		{
			const FRunRoundResult Result = FRunCalculator::ComputeRound(Rules, MakeRound(2, 2, 0, { 499 }), 0);
			TestFalse(TEXT("quota missed"), Result.bQuotaMet);
			TestEqual(TEXT("no bonus"), Result.ContractBonus, 0);
			TestEqual(TEXT("defeat"), Result.Outcome, ERunRoundOutcome::Defeat);
		});

		It("pays nothing on a defeat, filled orders and deaths included", [this, Rules]()
		{
			FRunRoundInput Input = MakeRound(2, 2, 1, { 300 });
			for (int32 Log = 0; Log < 2; ++Log)
			{
				FRunCalculator::TrySell(Input.Ledger, FGuid::NewGuid(), TEXT("Wood"), 99);
			}
			FRunOrder Wood;
			Wood.Category = TEXT("Wood");
			Wood.Quantity = 2;
			Wood.Bonus = 40;
			Input.Ledger.Orders = { Wood };
			const FRunRoundResult Result = FRunCalculator::ComputeRound(Rules, Input, 200);
			TestEqual(TEXT("gross 498 for a quota of 500"), Result.Gross, 498);
			TestEqual(TEXT("defeat"), Result.Outcome, ERunRoundOutcome::Defeat);
			TestEqual(TEXT("the order is filled"), Result.OrdersFilled, 1);
			TestEqual(TEXT("but not paid"), Result.OrdersBonus, 0);
			TestEqual(TEXT("no tax"), Result.Tax, 0);
			TestEqual(TEXT("no fee"), Result.RecruitFees, 0);
			TestEqual(TEXT("one death still counted"), Result.Deaths, 1);
			TestEqual(TEXT("net"), Result.Net, 0);
			TestEqual(TEXT("treasury as it was"), Result.TreasuryAfter, 200);
		});

		It("pays the orders filled on top of the quota", [this, Rules]()
		{
			FRunRoundInput Input = MakeRound(1, 1, 0, {});
			for (int32 Log = 0; Log < 4; ++Log)
			{
				FRunCalculator::TrySell(Input.Ledger, FGuid::NewGuid(), TEXT("Wood"), 70);
			}
			FRunOrder Wood;
			Wood.Category = TEXT("Wood");
			Wood.Quantity = 4;
			Wood.Bonus = 40;
			FRunOrder Food;
			Food.Category = TEXT("Food");
			Food.Quantity = 3;
			Food.Bonus = 30;
			Input.Ledger.Orders = { Wood, Food };
			const FRunRoundResult Result = FRunCalculator::ComputeRound(Rules, Input, 0);
			TestEqual(TEXT("gross 280 for a quota of 250"), Result.Gross, 280);
			TestEqual(TEXT("one order filled"), Result.OrdersFilled, 1);
			TestEqual(TEXT("its bonus"), Result.OrdersBonus, 40);
			TestEqual(TEXT("net = 280 - 98 + 100 + 40"), Result.Net, 322);
		});

		It("never takes the treasury below zero", [this, Rules]()
		{
			const FRunRoundResult Result = FRunCalculator::ComputeRound(Rules, MakeRound(1, 1, 4, { 250 }), 10);
			TestEqual(TEXT("net = 250 - 87 + 100 - 200"), Result.Net, 63);
			FRunRulesSnapshot Costly = Rules;
			Costly.RecruitFee = 500;
			const FRunRoundResult Broke = FRunCalculator::ComputeRound(Costly, MakeRound(1, 1, 1, { 250 }), 10);
			TestEqual(TEXT("net = 250 - 87 + 100 - 500"), Broke.Net, -237);
			TestEqual(TEXT("treasury floored"), Broke.TreasuryAfter, 0);
		});
	});

	Describe("Run", [this, Rules]()
	{
		It("signs a register once and closes a lost run", [this, Rules]()
		{
			FRunProgress Progress;
			Progress.Treasury = 100;
			const FRunRoundInput Won = MakeRound(1, 1, 0, { 300 });
			const FRunResolution First = FRunCalculator::ResolveRound(Rules, Won, Progress);
			TestTrue(TEXT("valid"), First.bValid);
			TestEqual(TEXT("treasury 100 + 300 - 105 + 100"), Progress.Treasury, 395);
			const FRunResolution Again = FRunCalculator::ResolveRound(Rules, Won, Progress);
			TestTrue(TEXT("already resolved"), Again.bAlreadyResolved);
			TestEqual(TEXT("treasury unchanged"), Progress.Treasury, 395);
			TestEqual(TEXT("one round won"), Progress.RoundsWon, 1);

			const FRunResolution Lost = FRunCalculator::ResolveRound(Rules, MakeRound(2, 1, 0, { 100 }), Progress);
			TestEqual(TEXT("defeat"), Lost.Result.Outcome, ERunRoundOutcome::Defeat);
			TestTrue(TEXT("run closed"), Progress.bClosed);
			TestEqual(TEXT("a defeat credits nothing"), Progress.Treasury, 395);
			const FRunResolution After = FRunCalculator::ResolveRound(Rules, MakeRound(3, 1, 0, { 9999 }), Progress);
			TestFalse(TEXT("nothing after a defeat"), After.bValid);
			TestFalse(TEXT("an invalid round changes nothing"), FRunCalculator::ResolveRound(Rules, FRunRoundInput(), Progress).bValid);
		});
	});
}

#endif
