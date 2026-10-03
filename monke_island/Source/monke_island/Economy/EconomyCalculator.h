// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Economy/EconomyTypes.h"

/**
 *  Pure economy math of cahier §7, with no UObject and no world: the server calls it, widgets only show its ledger.
 *  Order of the balance:
 *   1) keep only resources both extracted and stowed;
 *   2) hand wood / food to the quotas and consume the delivered units;
 *   3) move the surplus to the common reserve;
 *   4) sell the valuables, tax = floor(value x rate);
 *   5) credit value - tax + bonus (bonus only when every quota is met, untaxed by default);
 *   6) the ledger is stored once per ExpeditionId.
 */
struct MONKE_ISLAND_API FEconomyCalculator
{
	/** Quotas (wood, food) required for a player count */
	static void GetQuotas(const FEconomyRulesSnapshot& Rules, int32 PlayerCount, int32& OutWood, int32& OutFood);

	/** floor(Amount x BasisPoints / 10000) in integers; negative amounts give 0 */
	static int32 ComputeTax(int32 Amount, int32 TaxRateBasisPoints);

	/** Steps 1 to 5: computes the ledger and adds the surplus to InOutReserve. Credits no gold */
	static FExpeditionLedger ComputeLedger(const FEconomyRulesSnapshot& Rules, const FExpeditionInput& Input, FCommonReserve& InOutReserve);

	/**
	 *  Applies an expedition to the campaign once: reserve, gold and the register of resolved ids.
	 *  A second call with the same id returns the stored ledger and changes nothing.
	 */
	static FExpeditionResolution ResolveExpedition(const FEconomyRulesSnapshot& Rules, const FExpeditionInput& Input, FEconomyCampaignState& InOutState);

	/** Server-side shop purchase: refused, with no change, when the item is unknown or not covered by the gold */
	static FEconomyPurchaseResult TryPurchase(const FEconomyRulesSnapshot& Rules, FName ItemId, FEconomyCampaignState& InOutState);
};
