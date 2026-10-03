// monke_island

#include "Misc/AutomationTest.h"
#include "Economy/EconomyCalculator.h"
#include "Economy/EconomyRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Economy
BEGIN_DEFINE_SPEC(FEconomySpec, "monke_island.Economy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FEconomyRulesSnapshot Rules;
	FEconomyCampaignState State;

	/** Expedition with the given stowed stacks and a fresh id */
	FExpeditionInput MakeInput(const TArray<FExtractedResource>& Resources, int32 PlayerCount = 1)
	{
		FExpeditionInput Input;
		Input.ExpeditionId = FGuid::NewGuid();
		Input.PlayerCount = PlayerCount;
		Input.Resources = Resources;
		return Input;
	}

	/** Quota override for a player count */
	static FEconomyQuotaOverride MakeOverride(int32 MinPlayers, int32 Wood, int32 Food)
	{
		FEconomyQuotaOverride Override;
		Override.MinPlayers = MinPlayers;
		Override.Wood = Wood;
		Override.Food = Food;
		return Override;
	}

	/** Stacks meeting the default quotas (4 wood, 3 food) */
	TArray<FExtractedResource> QuotaStacks() const
	{
		return {
			FExtractedResource(EEconomyResourceCategory::Wood, 4, 0),
			FExtractedResource(EEconomyResourceCategory::Food, 3, 0)
		};
	}
END_DEFINE_SPEC(FEconomySpec)

void FEconomySpec::Define()
{
	BeforeEach([this]()
	{
		const UEconomyRules* Asset = NewObject<UEconomyRules>();
		Rules = Asset->MakeSnapshot();
		State = FEconomyCampaignState();
	});

	Describe("Rules", [this]()
	{
		It("defaults to the values of cahier section 7", [this]()
		{
			TestEqual(TEXT("wood quota"), Rules.BaseWoodQuota, 4);
			TestEqual(TEXT("food quota"), Rules.BaseFoodQuota, 3);
			TestEqual(TEXT("tax 35 %"), Rules.TaxRateBasisPoints, 3500);
			TestEqual(TEXT("bonus"), Rules.ContractBonus, 100);
			TestFalse(TEXT("bonus untaxed"), Rules.bTaxContractBonus);
			const int32* Axe = Rules.ShopPrices.Find(TEXT("Axe"));
			const int32* Lantern = Rules.ShopPrices.Find(TEXT("Lantern"));
			const int32* Slots = Rules.ShopPrices.Find(TEXT("BoatSlots"));
			TestTrue(TEXT("axe 100"), Axe != nullptr && *Axe == 100);
			TestTrue(TEXT("lantern 150"), Lantern != nullptr && *Lantern == 150);
			TestTrue(TEXT("boat slots 300"), Slots != nullptr && *Slots == 300);
		});

		It("adjusts the quotas by player count", [this]()
		{
			Rules.QuotaOverrides.Add(MakeOverride(3, 6, 5));
			Rules.QuotaOverrides.Add(MakeOverride(2, 5, 4));
			int32 Wood = 0;
			int32 Food = 0;
			FEconomyCalculator::GetQuotas(Rules, 1, Wood, Food);
			TestTrue(TEXT("1 player: base"), Wood == 4 && Food == 3);
			FEconomyCalculator::GetQuotas(Rules, 2, Wood, Food);
			TestTrue(TEXT("2 players"), Wood == 5 && Food == 4);
			FEconomyCalculator::GetQuotas(Rules, 4, Wood, Food);
			TestTrue(TEXT("4 players: highest override not above"), Wood == 6 && Food == 5);
		});
	});

	Describe("Tax", [this]()
	{
		It("gives the example of the cahier: 500 gold, quotas met => tax 175, bonus 100, net 425", [this]()
		{
			TArray<FExtractedResource> Stacks = QuotaStacks();
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, 500));
			const FExpeditionResolution Result = FEconomyCalculator::ResolveExpedition(Rules, MakeInput(Stacks), State);
			TestTrue(TEXT("ok"), Result.IsOk());
			TestEqual(TEXT("gross"), Result.Ledger.GrossSale, 500);
			TestEqual(TEXT("tax"), Result.Ledger.Tax, 175);
			TestEqual(TEXT("bonus"), Result.Ledger.ContractBonus, 100);
			TestEqual(TEXT("net"), Result.Ledger.Net, 425);
			TestTrue(TEXT("contract fulfilled"), Result.Ledger.bContractFulfilled);
			TestTrue(TEXT("no warning"), Result.Ledger.Warning == EExpeditionWarning::None);
			TestEqual(TEXT("gold credited"), State.Gold, 425);
		});

		It("rounds the tax down", [this]()
		{
			TestEqual(TEXT("1 -> 0"), FEconomyCalculator::ComputeTax(1, 3500), 0);
			TestEqual(TEXT("2 -> 0 (0.7)"), FEconomyCalculator::ComputeTax(2, 3500), 0);
			TestEqual(TEXT("3 -> 1 (1.05)"), FEconomyCalculator::ComputeTax(3, 3500), 1);
			TestEqual(TEXT("99 -> 34 (34.65)"), FEconomyCalculator::ComputeTax(99, 3500), 34);
			TestEqual(TEXT("101 -> 35 (35.35)"), FEconomyCalculator::ComputeTax(101, 3500), 35);
			TestEqual(TEXT("500 -> 175 exactly"), FEconomyCalculator::ComputeTax(500, 3500), 175);
			TestEqual(TEXT("0 -> 0"), FEconomyCalculator::ComputeTax(0, 3500), 0);
			TestEqual(TEXT("negative -> 0"), FEconomyCalculator::ComputeTax(-50, 3500), 0);
			TestEqual(TEXT("rate clamped to 100 %"), FEconomyCalculator::ComputeTax(80, 20000), 80);
			TestEqual(TEXT("no overflow"), FEconomyCalculator::ComputeTax(MAX_int32, 10000), MAX_int32);
		});

		It("taxes the bonus only when configured", [this]()
		{
			Rules.bTaxContractBonus = true;
			TArray<FExtractedResource> Stacks = QuotaStacks();
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, 500));
			FCommonReserve Reserve;
			const FExpeditionLedger Ledger = FEconomyCalculator::ComputeLedger(Rules, MakeInput(Stacks), Reserve);
			TestEqual(TEXT("tax on 600"), Ledger.Tax, 210);
			TestEqual(TEXT("net"), Ledger.Net, 390);
		});

		It("sums the sale of several valuables before taxing", [this]()
		{
			TArray<FExtractedResource> Stacks = QuotaStacks();
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, 1));
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, 2));
			FCommonReserve Reserve;
			const FExpeditionLedger Ledger = FEconomyCalculator::ComputeLedger(Rules, MakeInput(Stacks), Reserve);
			TestEqual(TEXT("gross 3"), Ledger.GrossSale, 3);
			TestEqual(TEXT("tax floor(1.05)"), Ledger.Tax, 1);
			TestEqual(TEXT("net"), Ledger.Net, 102);
		});
	});

	Describe("Quotas", [this]()
	{
		It("gives no bonus and a warning when a quota is missed", [this]()
		{
			const TArray<FExtractedResource> Stacks = {
				FExtractedResource(EEconomyResourceCategory::Wood, 4, 0),
				FExtractedResource(EEconomyResourceCategory::Food, 2, 0),
				FExtractedResource(EEconomyResourceCategory::Valuables, 1, 500)
			};
			const FExpeditionResolution Result = FEconomyCalculator::ResolveExpedition(Rules, MakeInput(Stacks), State);
			TestFalse(TEXT("contract failed"), Result.Ledger.bContractFulfilled);
			TestTrue(TEXT("warning"), Result.Ledger.Warning == EExpeditionWarning::QuotaMissed);
			TestEqual(TEXT("no bonus"), Result.Ledger.ContractBonus, 0);
			TestEqual(TEXT("tax still applies"), Result.Ledger.Tax, 175);
			TestEqual(TEXT("net"), Result.Ledger.Net, 325);
			TestTrue(TEXT("wood met"), Result.Ledger.WoodQuota.bMet);
			TestFalse(TEXT("food missed"), Result.Ledger.FoodQuota.bMet);
			TestEqual(TEXT("food required"), Result.Ledger.FoodQuota.Required, 3);
			TestEqual(TEXT("food delivered"), Result.Ledger.FoodQuota.Delivered, 2);
			TestEqual(TEXT("gold"), State.Gold, 325);
		});

		It("consumes the delivered units and moves only the surplus to the reserve", [this]()
		{
			State.Reserve.Wood = 2;
			State.Reserve.Food = 1;
			const TArray<FExtractedResource> Stacks = {
				FExtractedResource(EEconomyResourceCategory::Wood, 3, 0),
				FExtractedResource(EEconomyResourceCategory::Wood, 4, 0),
				FExtractedResource(EEconomyResourceCategory::Food, 3, 0)
			};
			const FExpeditionResolution Result = FEconomyCalculator::ResolveExpedition(Rules, MakeInput(Stacks), State);
			TestEqual(TEXT("wood brought"), Result.Ledger.WoodQuota.Brought, 7);
			TestEqual(TEXT("wood delivered"), Result.Ledger.WoodQuota.Delivered, 4);
			TestEqual(TEXT("wood surplus"), Result.Ledger.WoodQuota.Surplus, 3);
			TestEqual(TEXT("food surplus"), Result.Ledger.FoodQuota.Surplus, 0);
			TestEqual(TEXT("reserve wood 2 + 3"), State.Reserve.Wood, 5);
			TestEqual(TEXT("reserve food unchanged"), State.Reserve.Food, 1);
			TestEqual(TEXT("bonus only, nothing sold"), State.Gold, 100);
		});

		It("does not fill a quota from the reserve", [this]()
		{
			State.Reserve.Wood = 10;
			State.Reserve.Food = 10;
			const FExpeditionResolution Result = FEconomyCalculator::ResolveExpedition(Rules, MakeInput({}), State);
			TestFalse(TEXT("contract failed"), Result.Ledger.bContractFulfilled);
			TestEqual(TEXT("reserve wood kept"), State.Reserve.Wood, 10);
			TestEqual(TEXT("reserve food kept"), State.Reserve.Food, 10);
			TestEqual(TEXT("no gold"), State.Gold, 0);
		});

		It("counts only what was both extracted and stowed", [this]()
		{
			const TArray<FExtractedResource> Stacks = {
				FExtractedResource(EEconomyResourceCategory::Wood, 4, 0),
				FExtractedResource(EEconomyResourceCategory::Food, 3, 0),
				FExtractedResource(EEconomyResourceCategory::Wood, 5, 0, true, false),
				FExtractedResource(EEconomyResourceCategory::Food, 5, 0, false, true),
				FExtractedResource(EEconomyResourceCategory::Valuables, 1, 900, true, false),
				FExtractedResource(EEconomyResourceCategory::Valuables, 1, 100)
			};
			const FExpeditionResolution Result = FEconomyCalculator::ResolveExpedition(Rules, MakeInput(Stacks), State);
			TestEqual(TEXT("ignored stacks"), Result.Ledger.IgnoredStacks, 3);
			TestEqual(TEXT("wood brought"), Result.Ledger.WoodQuota.Brought, 4);
			TestEqual(TEXT("food brought"), Result.Ledger.FoodQuota.Brought, 3);
			TestEqual(TEXT("gross"), Result.Ledger.GrossSale, 100);
			TestEqual(TEXT("reserve wood"), State.Reserve.Wood, 0);
			TestEqual(TEXT("net 100 - 35 + 100"), Result.Ledger.Net, 165);
		});

		It("rejects negative stacks", [this]()
		{
			TArray<FExtractedResource> Stacks = QuotaStacks();
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Wood, -10, 0));
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, -500));
			FCommonReserve Reserve;
			const FExpeditionLedger Ledger = FEconomyCalculator::ComputeLedger(Rules, MakeInput(Stacks), Reserve);
			TestEqual(TEXT("invalid stacks"), Ledger.InvalidStacks, 2);
			TestTrue(TEXT("quota still met"), Ledger.bContractFulfilled);
			TestEqual(TEXT("gross"), Ledger.GrossSale, 0);
			TestEqual(TEXT("net"), Ledger.Net, 100);
		});
	});

	Describe("Resolution", [this]()
	{
		It("applies a balance once per expedition id", [this]()
		{
			TArray<FExtractedResource> Stacks = QuotaStacks();
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Wood, 2, 0));
			Stacks.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, 500));
			const FExpeditionInput Input = MakeInput(Stacks);
			const FExpeditionResolution First = FEconomyCalculator::ResolveExpedition(Rules, Input, State);
			const FExpeditionResolution Second = FEconomyCalculator::ResolveExpedition(Rules, Input, State);
			TestFalse(TEXT("first is new"), First.bAlreadyResolved);
			TestTrue(TEXT("second is a replay"), Second.bAlreadyResolved);
			TestTrue(TEXT("replay ok"), Second.IsOk());
			TestEqual(TEXT("same net"), Second.Ledger.Net, First.Ledger.Net);
			TestTrue(TEXT("same id"), Second.Ledger.ExpeditionId == Input.ExpeditionId);
			TestEqual(TEXT("credited once"), State.Gold, 425);
			TestEqual(TEXT("surplus added once"), State.Reserve.Wood, 2);
			TestEqual(TEXT("one entry"), State.ResolvedExpeditions.Num(), 1);
		});

		It("returns the stored ledger even if the replayed input differs", [this]()
		{
			FExpeditionInput Input = MakeInput(QuotaStacks());
			FEconomyCalculator::ResolveExpedition(Rules, Input, State);
			Input.Resources.Add(FExtractedResource(EEconomyResourceCategory::Valuables, 1, 10000));
			const FExpeditionResolution Replay = FEconomyCalculator::ResolveExpedition(Rules, Input, State);
			TestTrue(TEXT("replay"), Replay.bAlreadyResolved);
			TestEqual(TEXT("stored gross"), Replay.Ledger.GrossSale, 0);
			TestEqual(TEXT("gold unchanged"), State.Gold, 100);
		});

		It("resolves two different expeditions independently", [this]()
		{
			FEconomyCalculator::ResolveExpedition(Rules, MakeInput(QuotaStacks()), State);
			FEconomyCalculator::ResolveExpedition(Rules, MakeInput(QuotaStacks()), State);
			TestEqual(TEXT("two bonuses"), State.Gold, 200);
			TestEqual(TEXT("two entries"), State.ResolvedExpeditions.Num(), 2);
		});

		It("refuses an invalid expedition id without change", [this]()
		{
			FExpeditionInput Input = MakeInput(QuotaStacks());
			Input.ExpeditionId.Invalidate();
			const FExpeditionResolution Result = FEconomyCalculator::ResolveExpedition(Rules, Input, State);
			TestTrue(TEXT("error"), Result.Error == EExpeditionResolveError::InvalidExpeditionId);
			TestEqual(TEXT("no gold"), State.Gold, 0);
			TestEqual(TEXT("no entry"), State.ResolvedExpeditions.Num(), 0);
		});
	});

	Describe("Shop", [this]()
	{
		It("refuses a purchase the crew cannot afford", [this]()
		{
			State.Gold = 99;
			const FEconomyPurchaseResult Result = FEconomyCalculator::TryPurchase(Rules, TEXT("Axe"), State);
			TestTrue(TEXT("error"), Result.Error == EEconomyPurchaseError::InsufficientFunds);
			TestEqual(TEXT("price"), Result.Price, 100);
			TestEqual(TEXT("gold unchanged"), State.Gold, 99);
			TestEqual(TEXT("gold after"), Result.GoldAfter, 99);
		});

		It("accepts a purchase exactly funded", [this]()
		{
			State.Gold = 150;
			const FEconomyPurchaseResult Result = FEconomyCalculator::TryPurchase(Rules, TEXT("Lantern"), State);
			TestTrue(TEXT("ok"), Result.IsOk());
			TestEqual(TEXT("gold"), State.Gold, 0);
			TestEqual(TEXT("gold after"), Result.GoldAfter, 0);
		});

		It("refuses an unknown item", [this]()
		{
			State.Gold = 1000;
			const FEconomyPurchaseResult Result = FEconomyCalculator::TryPurchase(Rules, TEXT("Musket"), State);
			TestTrue(TEXT("error"), Result.Error == EEconomyPurchaseError::UnknownItem);
			TestEqual(TEXT("gold unchanged"), State.Gold, 1000);
		});

		It("refuses a negative price", [this]()
		{
			State.Gold = 1000;
			Rules.ShopPrices.Add(TEXT("Glitch"), -50);
			const FEconomyPurchaseResult Result = FEconomyCalculator::TryPurchase(Rules, TEXT("Glitch"), State);
			TestTrue(TEXT("error"), Result.Error == EEconomyPurchaseError::InvalidPrice);
			TestEqual(TEXT("gold unchanged"), State.Gold, 1000);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
