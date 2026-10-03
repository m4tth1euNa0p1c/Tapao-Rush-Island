// monke_island

#include "Economy/EconomyRules.h"

UEconomyRules::UEconomyRules()
{
	ShopPrices.Add(TEXT("Axe"), 100);
	ShopPrices.Add(TEXT("Lantern"), 150);
	ShopPrices.Add(TEXT("BoatSlots"), 300);
}

FEconomyRulesSnapshot UEconomyRules::MakeSnapshot() const
{
	FEconomyRulesSnapshot Snapshot;
	Snapshot.BaseWoodQuota = FMath::Max(0, BaseWoodQuota);
	Snapshot.BaseFoodQuota = FMath::Max(0, BaseFoodQuota);
	Snapshot.QuotaOverrides = QuotaOverrides;
	Snapshot.TaxRateBasisPoints = FMath::Clamp(TaxRateBasisPoints, 0, 10000);
	Snapshot.ContractBonus = FMath::Max(0, ContractBonus);
	Snapshot.bTaxContractBonus = bTaxContractBonus;
	Snapshot.ShopPrices = ShopPrices;
	return Snapshot;
}
