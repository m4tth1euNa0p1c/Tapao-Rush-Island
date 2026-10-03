// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "EconomyTypes.generated.h"

MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeEconomy, Log, All);

/** Category of an extracted resource for the expedition balance (cahier §7) */
UENUM(BlueprintType)
enum class EEconomyResourceCategory : uint8
{
	/** Quota resource, counted in units */
	Wood,
	/** Quota resource, counted in units */
	Food,
	/** Sold at the end of the expedition, taxed on its sale value */
	Valuables
};

/** Warning raised by an expedition balance */
UENUM(BlueprintType)
enum class EExpeditionWarning : uint8
{
	/** Every quota is met */
	None,
	/** At least one quota is not met: no contract bonus */
	QuotaMissed
};

/** Why an expedition could not be resolved */
UENUM(BlueprintType)
enum class EExpeditionResolveError : uint8
{
	/** Resolved (or already resolved: the stored balance is returned) */
	None,
	/** The expedition id is not a valid guid */
	InvalidExpeditionId
};

/** Why a shop purchase was refused */
UENUM(BlueprintType)
enum class EEconomyPurchaseError : uint8
{
	/** Purchase accepted */
	None,
	/** The item is not sold by the shop */
	UnknownItem,
	/** The configured price is negative */
	InvalidPrice,
	/** The crew gold does not cover the price */
	InsufficientFunds
};

/** One stack of resources at the end of an expedition, as reported by the server */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExtractedResource
{
	GENERATED_BODY()

	/** Category of the stack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	EEconomyResourceCategory Category = EEconomyResourceCategory::Wood;

	/** Units in the stack (wood and food count toward the quotas) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy", meta=(ClampMin="0"))
	int32 Quantity = 0;

	/** Total sale value of the stack in gold (valuables only, ignored for wood and food) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy", meta=(ClampMin="0"))
	int32 SaleValue = 0;

	/** The stack left the island with the boat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	bool bExtracted = false;

	/** The stack was stowed in the boat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	bool bStowed = false;

	FExtractedResource() = default;

	FExtractedResource(EEconomyResourceCategory InCategory, int32 InQuantity, int32 InSaleValue, bool bInExtracted = true, bool bInStowed = true)
		: Category(InCategory), Quantity(InQuantity), SaleValue(InSaleValue), bExtracted(bInExtracted), bStowed(bInStowed)
	{
	}
};

/** What the server hands to the balance at the end of an expedition */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionInput
{
	GENERATED_BODY()

	/** Unique id of the expedition: a balance is applied once per id */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	FGuid ExpeditionId;

	/** Number of players of the expedition (selects the quotas) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy", meta=(ClampMin="1"))
	int32 PlayerCount = 1;

	/** Every resource stack known at the end of the expedition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	TArray<FExtractedResource> Resources;
};

/** Common stock of the crew (surplus of wood and food) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCommonReserve
{
	GENERATED_BODY()

	/** Wood units in stock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy", meta=(ClampMin="0"))
	int32 Wood = 0;

	/** Food units in stock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy", meta=(ClampMin="0"))
	int32 Food = 0;
};

/** One quota line of the balance */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionQuotaLine
{
	GENERATED_BODY()

	/** Quota resource of the line */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	EEconomyResourceCategory Category = EEconomyResourceCategory::Wood;

	/** Units required by the contract */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Required = 0;

	/** Units extracted and stowed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Brought = 0;

	/** Units handed to the contract and consumed (at most Required) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Delivered = 0;

	/** Units beyond the quota, moved to the common reserve */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Surplus = 0;

	/** Delivered reaches Required */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	bool bMet = false;
};

/** Line-by-line balance of an expedition, shown to every player and saved once */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionLedger
{
	GENERATED_BODY()

	/** Expedition the balance belongs to */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	FGuid ExpeditionId;

	/** Wood quota line */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	FExpeditionQuotaLine WoodQuota;

	/** Food quota line */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	FExpeditionQuotaLine FoodQuota;

	/** Stacks left out because they were not both extracted and stowed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 IgnoredStacks = 0;

	/** Stacks left out because of a negative quantity or value */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 InvalidStacks = 0;

	/** Gross sale value of the valuables (gold) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 GrossSale = 0;

	/** Tax rate applied, in basis points (3500 = 35 %) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 TaxRateBasisPoints = 0;

	/** Royal tax: floor(taxed amount x rate) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Tax = 0;

	/** Contract bonus (0 when a quota is missed) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 ContractBonus = 0;

	/** Gold credited to the crew: gross sale - tax + bonus */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Net = 0;

	/** Every quota is met */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	bool bContractFulfilled = false;

	/** Warning to show with the balance */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	EExpeditionWarning Warning = EExpeditionWarning::None;
};

/** Persistent economy of the crew: gold, reserve and the register of resolved expeditions */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FEconomyCampaignState
{
	GENERATED_BODY()

	/** Common gold of the crew */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy", meta=(ClampMin="0"))
	int32 Gold = 0;

	/** Common stock of wood and food */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	FCommonReserve Reserve;

	/** Balance of every expedition already applied, by expedition id */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	TMap<FGuid, FExpeditionLedger> ResolvedExpeditions;
};

/** Outcome of applying an expedition to the campaign */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionResolution
{
	GENERATED_BODY()

	/** Why the resolution failed (None on success) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	EExpeditionResolveError Error = EExpeditionResolveError::None;

	/** The expedition was already applied: the stored balance is returned and nothing is credited */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	bool bAlreadyResolved = false;

	/** Balance of the expedition */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	FExpeditionLedger Ledger;

	/** The resolution succeeded (first time or replay) */
	bool IsOk() const { return Error == EExpeditionResolveError::None; }
};

/** Outcome of a shop purchase request */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FEconomyPurchaseResult
{
	GENERATED_BODY()

	/** Why the purchase was refused (None when accepted) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	EEconomyPurchaseError Error = EEconomyPurchaseError::None;

	/** Item requested */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	FName ItemId;

	/** Price of the item (0 when unknown) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Price = 0;

	/** Gold of the crew after the request (unchanged when refused) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy")
	int32 GoldAfter = 0;

	/** The purchase was accepted */
	bool IsOk() const { return Error == EEconomyPurchaseError::None; }
};

/** Quotas for a given player count (overrides the base quotas) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FEconomyQuotaOverride
{
	GENERATED_BODY()

	/** Applies from this player count up to the next override */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quotas", meta=(ClampMin="1"))
	int32 MinPlayers = 1;

	/** Wood units required */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quotas", meta=(ClampMin="0"))
	int32 Wood = 4;

	/** Food units required */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quotas", meta=(ClampMin="0"))
	int32 Food = 3;
};

/** Plain copy of the economy rules: the pure calculator needs no UObject. Neutral defaults: the cahier values live in UEconomyRules only */
struct MONKE_ISLAND_API FEconomyRulesSnapshot
{
	/** Wood units required when no override applies */
	int32 BaseWoodQuota = 0;

	/** Food units required when no override applies */
	int32 BaseFoodQuota = 0;

	/** Quotas by player count */
	TArray<FEconomyQuotaOverride> QuotaOverrides;

	/** Tax rate in basis points (3500 = 35 %) */
	int32 TaxRateBasisPoints = 0;

	/** Contract bonus when every quota is met */
	int32 ContractBonus = 0;

	/** The contract bonus is taxed with the sale (false by default, cahier §7) */
	bool bTaxContractBonus = false;

	/** Shop prices by item id */
	TMap<FName, int32> ShopPrices;
};
