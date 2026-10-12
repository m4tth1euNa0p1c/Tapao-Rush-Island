// monke_island

#include "Run/RunCalculator.h"
#include "Economy/EconomyCalculator.h"

namespace
{
	/** Crowns kept within int32, never negative */
	int32 ClampCrowns(int64 Amount)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Amount, 0, MAX_int32));
	}
}

int32 FRunLedger::GetGross() const
{
	int64 Sum = 0;
	for (const FRunSale& Sale : Sales)
	{
		Sum += FMath::Max(0, Sale.Value);
	}
	return ClampCrowns(Sum);
}

int32 FRunLedger::CountCategory(FName Category) const
{
	int32 Count = 0;
	for (const FRunSale& Sale : Sales)
	{
		Count += Sale.Category == Category ? 1 : 0;
	}
	return Count;
}

bool FRunLedger::HasSold(const FGuid& ItemId) const
{
	return Sales.ContainsByPredicate([&ItemId](const FRunSale& Sale) { return Sale.ItemId == ItemId; });
}

int32 FRunCalculator::QuotaForRound(const FRunRulesSnapshot& Rules, int32 RoundIndex, int32 PlayerCount)
{
	const int64 N = FMath::Max(1, RoundIndex) - 1;
	const int64 Base = static_cast<int64>(FMath::Max(0, Rules.QuotaBase)) + FMath::Max(0, Rules.QuotaStep) * N + FMath::Max(0, Rules.QuotaAccel) * N * N;
	const double Scale = 1.0 + FMath::Max(0.0f, Rules.PlayerQuotaScale) * FMath::Max(0, PlayerCount - 1);
	return ClampCrowns(FMath::RoundToInt64(static_cast<double>(Base) * Scale));
}

float FRunCalculator::DifficultyForRound(const FRunRulesSnapshot& Rules, int32 RoundIndex)
{
	const float Max = FMath::Max(1.0f, Rules.DifficultyMax);
	return FMath::Clamp(1.0f + FMath::Max(0.0f, Rules.DifficultyStep) * (FMath::Max(1, RoundIndex) - 1), 1.0f, Max);
}

ERunSaleError FRunCalculator::TrySell(FRunLedger& Ledger, const FGuid& ItemId, FName Category, int32 Value)
{
	if (Ledger.bClosed)
	{
		return ERunSaleError::RoundClosed;
	}
	if (!ItemId.IsValid())
	{
		return ERunSaleError::InvalidItem;
	}
	if (Ledger.HasSold(ItemId))
	{
		return ERunSaleError::AlreadySold;
	}
	FRunSale& Sale = Ledger.Sales.AddDefaulted_GetRef();
	Sale.ItemId = ItemId;
	Sale.Category = Category;
	Sale.Value = FMath::Max(0, Value);
	return ERunSaleError::None;
}

FRunRoundResult FRunCalculator::ComputeRound(const FRunRulesSnapshot& Rules, const FRunRoundInput& Input, int32 TreasuryBefore)
{
	FRunRoundResult Result;
	Result.RoundId = Input.RoundId;
	Result.RoundIndex = FMath::Max(1, Input.RoundIndex);
	Result.Quota = QuotaForRound(Rules, Result.RoundIndex, Input.PlayerCount);
	Result.Gross = Input.Ledger.GetGross();
	Result.bQuotaMet = Result.Gross >= Result.Quota;
	Result.Outcome = Result.bQuotaMet ? ERunRoundOutcome::NextRound : ERunRoundOutcome::Defeat;
	Result.TaxRateBasisPoints = FMath::Clamp(Rules.TaxRateBasisPoints, 0, 10000);
	Result.Deaths = FMath::Max(0, Input.Deaths);
	int64 OrdersBonus = 0;
	for (const FRunOrder& Order : Input.Ledger.Orders)
	{
		if (Order.Quantity > 0 && Input.Ledger.CountCategory(Order.Category) >= Order.Quantity)
		{
			OrdersBonus += FMath::Max(0, Order.Bonus);
			++Result.OrdersFilled;
		}
	}
	Result.TreasuryBefore = FMath::Max(0, TreasuryBefore);
	Result.TreasuryAfter = Result.TreasuryBefore;
	if (!Result.bQuotaMet)
	{
		// a broken contract pays nothing (§24.2, step 10): no tax, bonus, order or fee, net 0, treasury as it was
		return Result;
	}
	Result.ContractBonus = FMath::Max(0, Rules.ContractBonus);
	const int64 Taxed = static_cast<int64>(Result.Gross) + (Rules.bTaxContractBonus ? Result.ContractBonus : 0);
	Result.Tax = FEconomyCalculator::ComputeTax(ClampCrowns(Taxed), Result.TaxRateBasisPoints);
	// orders pay on top of the quota, never instead of it
	Result.OrdersBonus = ClampCrowns(OrdersBonus);
	Result.RecruitFees = ClampCrowns(static_cast<int64>(Result.Deaths) * FMath::Max(0, Rules.RecruitFee));
	const int64 Net = static_cast<int64>(Result.Gross) - Result.Tax + Result.ContractBonus + Result.OrdersBonus - Result.RecruitFees;
	Result.Net = static_cast<int32>(FMath::Clamp<int64>(Net, MIN_int32, MAX_int32));
	Result.TreasuryAfter = ClampCrowns(static_cast<int64>(Result.TreasuryBefore) + Net);
	return Result;
}

FRunResolution FRunCalculator::ResolveRound(const FRunRulesSnapshot& Rules, const FRunRoundInput& Input, FRunProgress& InOutProgress)
{
	FRunResolution Resolution;
	if (const FRunRoundResult* Stored = InOutProgress.ResolvedRounds.Find(Input.RoundId))
	{
		Resolution.Result = *Stored;
		Resolution.bAlreadyResolved = true;
		return Resolution;
	}
	if (!Input.RoundId.IsValid() || InOutProgress.bClosed)
	{
		Resolution.bValid = false;
		return Resolution;
	}
	Resolution.Result = ComputeRound(Rules, Input, InOutProgress.Treasury);
	InOutProgress.Treasury = Resolution.Result.TreasuryAfter;
	if (Resolution.Result.bQuotaMet)
	{
		++InOutProgress.RoundsWon;
	}
	else
	{
		InOutProgress.bClosed = true;
	}
	InOutProgress.ResolvedRounds.Add(Input.RoundId, Resolution.Result);
	return Resolution;
}
