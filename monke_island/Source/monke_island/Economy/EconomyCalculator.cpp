// monke_island

#include "Economy/EconomyCalculator.h"

DEFINE_LOG_CATEGORY(LogMonkeEconomy);

namespace EconomyCalculatorPrivate
{
	/** Saturates a 64-bit gold amount into the int32 range */
	int32 ClampGold(int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, 0, MAX_int32));
	}

	/** Builds a quota line from the units brought; the delivered units are consumed, the rest is surplus */
	FExpeditionQuotaLine MakeQuotaLine(EEconomyResourceCategory Category, int32 Required, int64 Brought)
	{
		FExpeditionQuotaLine Line;
		Line.Category = Category;
		Line.Required = FMath::Max(0, Required);
		Line.Brought = static_cast<int32>(FMath::Clamp<int64>(Brought, 0, MAX_int32));
		Line.Delivered = FMath::Min(Line.Brought, Line.Required);
		Line.Surplus = Line.Brought - Line.Delivered;
		Line.bMet = Line.Delivered >= Line.Required;
		return Line;
	}
}

void FEconomyCalculator::GetQuotas(const FEconomyRulesSnapshot& Rules, int32 PlayerCount, int32& OutWood, int32& OutFood)
{
	OutWood = Rules.BaseWoodQuota;
	OutFood = Rules.BaseFoodQuota;
	int32 BestMinPlayers = 0;
	for (const FEconomyQuotaOverride& Override : Rules.QuotaOverrides)
	{
		if (Override.MinPlayers <= PlayerCount && Override.MinPlayers > BestMinPlayers)
		{
			BestMinPlayers = Override.MinPlayers;
			OutWood = Override.Wood;
			OutFood = Override.Food;
		}
	}
	OutWood = FMath::Max(0, OutWood);
	OutFood = FMath::Max(0, OutFood);
}

int32 FEconomyCalculator::ComputeTax(int32 Amount, int32 TaxRateBasisPoints)
{
	if (Amount <= 0)
	{
		return 0;
	}
	const int64 Rate = FMath::Clamp(TaxRateBasisPoints, 0, 10000);
	// Integer division of non-negative values is a floor: no float rounding (0.35f x 500 would give 174)
	return static_cast<int32>((static_cast<int64>(Amount) * Rate) / 10000);
}

FExpeditionLedger FEconomyCalculator::ComputeLedger(const FEconomyRulesSnapshot& Rules, const FExpeditionInput& Input)
{
	using namespace EconomyCalculatorPrivate;

	FExpeditionLedger Ledger;
	Ledger.ExpeditionId = Input.ExpeditionId;
	Ledger.TaxRateBasisPoints = FMath::Clamp(Rules.TaxRateBasisPoints, 0, 10000);

	// 1) Only what left the island stowed in the boat counts
	int64 WoodBrought = 0;
	int64 FoodBrought = 0;
	int64 GrossSale = 0;
	for (const FExtractedResource& Stack : Input.Resources)
	{
		if (Stack.Quantity < 0 || Stack.SaleValue < 0)
		{
			++Ledger.InvalidStacks;
			continue;
		}
		if (!Stack.bExtracted || !Stack.bStowed)
		{
			++Ledger.IgnoredStacks;
			continue;
		}
		switch (Stack.Category)
		{
		case EEconomyResourceCategory::Wood:
			WoodBrought += Stack.Quantity;
			break;
		case EEconomyResourceCategory::Food:
			FoodBrought += Stack.Quantity;
			break;
		case EEconomyResourceCategory::Valuables:
			GrossSale += Stack.SaleValue;
			break;
		default:
			++Ledger.InvalidStacks;
			break;
		}
	}

	if (Ledger.InvalidStacks > 0)
	{
		UE_LOG(LogMonkeEconomy, Warning, TEXT("Expedition %s: %d invalid stack(s) left out of the balance (negative quantity or value, or unknown category)"),
			*Input.ExpeditionId.ToString(), Ledger.InvalidStacks);
	}

	// 2) Quotas: delivered units are consumed by the contract
	int32 WoodRequired = 0;
	int32 FoodRequired = 0;
	GetQuotas(Rules, FMath::Max(1, Input.PlayerCount), WoodRequired, FoodRequired);
	Ledger.WoodQuota = MakeQuotaLine(EEconomyResourceCategory::Wood, WoodRequired, WoodBrought);
	Ledger.FoodQuota = MakeQuotaLine(EEconomyResourceCategory::Food, FoodRequired, FoodBrought);
	Ledger.bContractFulfilled = Ledger.WoodQuota.bMet && Ledger.FoodQuota.bMet;
	Ledger.Warning = Ledger.bContractFulfilled ? EExpeditionWarning::None : EExpeditionWarning::QuotaMissed;

	// 3) The surplus lines are in the ledger; ResolveExpedition moves them to the common reserve

	// 4) Sale of the valuables and royal tax
	Ledger.GrossSale = ClampGold(GrossSale);
	Ledger.ContractBonus = Ledger.bContractFulfilled ? FMath::Max(0, Rules.ContractBonus) : 0;
	const int64 TaxedAmount = static_cast<int64>(Ledger.GrossSale) + (Rules.bTaxContractBonus ? Ledger.ContractBonus : 0);
	Ledger.Tax = ComputeTax(ClampGold(TaxedAmount), Ledger.TaxRateBasisPoints);

	// 5) Net credit
	Ledger.Net = ClampGold(static_cast<int64>(Ledger.GrossSale) - Ledger.Tax + Ledger.ContractBonus);
	return Ledger;
}

FExpeditionResolution FEconomyCalculator::ResolveExpedition(const FEconomyRulesSnapshot& Rules, const FExpeditionInput& Input, FEconomyCampaignState& InOutState)
{
	FExpeditionResolution Resolution;
	if (!Input.ExpeditionId.IsValid())
	{
		Resolution.Error = EExpeditionResolveError::InvalidExpeditionId;
		UE_LOG(LogMonkeEconomy, Warning, TEXT("ResolveExpedition refused: invalid expedition id"));
		return Resolution;
	}

	// 6) One balance per expedition: a replay returns the stored ledger without crediting again
	if (const FExpeditionLedger* Existing = InOutState.ResolvedExpeditions.Find(Input.ExpeditionId))
	{
		Resolution.bAlreadyResolved = true;
		Resolution.Ledger = *Existing;
		UE_LOG(LogMonkeEconomy, Log, TEXT("Expedition %s already resolved: stored ledger returned, nothing credited"), *Input.ExpeditionId.ToString());
		return Resolution;
	}

	using namespace EconomyCalculatorPrivate;
	Resolution.Ledger = ComputeLedger(Rules, Input);
	// 3) Surplus to the common reserve, applied only here
	InOutState.Reserve.Wood = ClampGold(static_cast<int64>(InOutState.Reserve.Wood) + Resolution.Ledger.WoodQuota.Surplus);
	InOutState.Reserve.Food = ClampGold(static_cast<int64>(InOutState.Reserve.Food) + Resolution.Ledger.FoodQuota.Surplus);
	InOutState.Gold = ClampGold(static_cast<int64>(InOutState.Gold) + Resolution.Ledger.Net);
	InOutState.ResolvedExpeditions.Add(Input.ExpeditionId, Resolution.Ledger);
	UE_LOG(LogMonkeEconomy, Log, TEXT("Expedition %s resolved: sale %d, tax %d, bonus %d, net %d, contract %s"),
		*Input.ExpeditionId.ToString(), Resolution.Ledger.GrossSale, Resolution.Ledger.Tax, Resolution.Ledger.ContractBonus,
		Resolution.Ledger.Net, Resolution.Ledger.bContractFulfilled ? TEXT("fulfilled") : TEXT("failed"));
	return Resolution;
}

FEconomyPurchaseResult FEconomyCalculator::TryPurchase(const FEconomyRulesSnapshot& Rules, FName ItemId, FEconomyCampaignState& InOutState)
{
	FEconomyPurchaseResult Result;
	Result.ItemId = ItemId;
	Result.GoldAfter = InOutState.Gold;

	const int32* Price = Rules.ShopPrices.Find(ItemId);
	if (Price == nullptr)
	{
		Result.Error = EEconomyPurchaseError::UnknownItem;
		UE_LOG(LogMonkeEconomy, Warning, TEXT("Purchase refused: unknown item %s"), *ItemId.ToString());
		return Result;
	}
	Result.Price = *Price;
	if (*Price < 0)
	{
		Result.Error = EEconomyPurchaseError::InvalidPrice;
		UE_LOG(LogMonkeEconomy, Warning, TEXT("Purchase refused: negative price %d for %s"), *Price, *ItemId.ToString());
		return Result;
	}
	if (InOutState.Gold < *Price)
	{
		Result.Error = EEconomyPurchaseError::InsufficientFunds;
		UE_LOG(LogMonkeEconomy, Log, TEXT("Purchase refused: %s costs %d, crew has %d"), *ItemId.ToString(), *Price, InOutState.Gold);
		return Result;
	}

	InOutState.Gold -= *Price;
	Result.GoldAfter = InOutState.Gold;
	return Result;
}
