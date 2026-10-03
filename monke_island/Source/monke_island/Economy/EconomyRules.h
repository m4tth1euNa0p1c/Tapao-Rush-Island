// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Economy/EconomyTypes.h"
#include "EconomyRules.generated.h"

/**
 *  Tunable rules of the expedition economy (DA_EconomyRules, cahier §7): quotas by player count,
 *  royal tax, contract bonus and shop prices. Defaults are the V1 values of the cahier.
 *  Gold is always an integer; the tax rate is stored in basis points so that floor(value x rate) is exact.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UEconomyRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UEconomyRules();

	/** Wood units required when no player-count override applies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quotas", meta=(ClampMin="0"))
	int32 BaseWoodQuota = 4;

	/** Food units required when no player-count override applies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quotas", meta=(ClampMin="0"))
	int32 BaseFoodQuota = 3;

	/** Quotas by player count: the override with the highest MinPlayers not above the player count wins */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quotas")
	TArray<FEconomyQuotaOverride> QuotaOverrides;

	/** Royal tax on the sale of valuables, in basis points (3500 = 35 %) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tax", meta=(ClampMin="0", ClampMax="10000"))
	int32 TaxRateBasisPoints = 3500;

	/** Gold granted when every quota is met */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0"))
	int32 ContractBonus = 100;

	/** The contract bonus is taxed with the sale (untaxed by default) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract")
	bool bTaxContractBonus = false;

	/** Shop prices in gold by item id (Axe, Lantern, BoatSlots) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shop")
	TMap<FName, int32> ShopPrices;

	/** Plain, clamped copy of the rules for the pure calculator */
	FEconomyRulesSnapshot MakeSnapshot() const;
};
