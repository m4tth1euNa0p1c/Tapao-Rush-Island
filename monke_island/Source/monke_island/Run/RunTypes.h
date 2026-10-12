// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "RunTypes.generated.h"

/** What a signed register leads to (cahier §24.2, step 10) */
UENUM(BlueprintType)
enum class ERunRoundOutcome : uint8
{
	/** The quota is met: shop, then the next round */
	NextRound,
	/** The quota is missed: the Crown punishes the crew, the run is over */
	Defeat
};

/** State of a whole run (ADR-006): it ends on a missed quota or after the last period */
UENUM(BlueprintType)
enum class ERunOutcome : uint8
{
	/** Days and periods go on */
	InProgress,
	/** A quota was missed on its day: the crew is lost */
	Defeat,
	/** The last period was passed */
	Victory
};

/** Best run of the local profile, shown in the main menu (ADR-006); all zero = no record yet */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunRecord
{
	GENERATED_BODY()

	/** Highest period reached, from 1 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 BestPeriod = 0;

	/** Day reached in that period, from 1 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 BestDay = 0;

	/** Gross sales of that run (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 BestTotalSold = 0;

	/** That run passed the last period */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bVictory = false;

	/** When it was set */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FDateTime Date;
};

/** Answer of the server to a sale at the Balance of the Collector */
UENUM(BlueprintType)
enum class ERunSaleError : uint8
{
	/** Sold and counted */
	None,
	/** The item has no valid identifier */
	InvalidItem,
	/** This item was already sold: counted once (double click, two players) */
	AlreadySold,
	/** The register is signed: no more sale this round */
	RoundClosed
};

/** An order of the contract: a quantity of a kind of goods that pays a bonus, on top of the quota (§24.4) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunOrder
{
	GENERATED_BODY()

	/** Kind of goods (Wood, Food...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run")
	FName Category;

	/** Items of that kind to sell */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0"))
	int32 Quantity = 0;

	/** Paid when filled (crowns) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0"))
	int32 Bonus = 0;
};

/** An item sold at the Balance of the Collector */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunSale
{
	GENERATED_BODY()

	/** Stable identifier of the item: one sale per item */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FGuid ItemId;

	/** Kind of goods, for the orders */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FName Category;

	/** Value at the appraisal (crowns), damage included */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Value = 0;
};

/** The register of a round: the sales, once per item, and the orders of the contract */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunLedger
{
	GENERATED_BODY()

	/** Round of the register */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FGuid RoundId;

	/** Sales of the round */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	TArray<FRunSale> Sales;

	/** Orders of the contract */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	TArray<FRunOrder> Orders;

	/** Signed: no more sale */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bClosed = false;

	/** Sum of the sales (crowns), never negative */
	int32 GetGross() const;

	/** Items of a kind sold */
	int32 CountCategory(FName Category) const;

	/** True when this item was sold already */
	bool HasSold(const FGuid& ItemId) const;
};

/** What a round is resolved from when the register is signed */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunRoundInput
{
	GENERATED_BODY()

	/** Identifier of the round (the ExpeditionId of cahier §3, §17) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FGuid RoundId;

	/** Round number, from 1 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 RoundIndex = 1;

	/** Players in the crew when the round was launched (the quota scales with them) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 PlayerCount = 1;

	/** Players dead during the round, the abandoned ones included */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Deaths = 0;

	/** Sales and orders */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FRunLedger Ledger;
};

/** Lines of a signed register, shown to every player */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunRoundResult
{
	GENERATED_BODY()

	/** Round */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FGuid RoundId;

	/** Round number, from 1 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 RoundIndex = 1;

	/** Gross sales required (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Quota = 0;

	/** Gross sales (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Gross = 0;

	/** Tax rate applied (basis points) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 TaxRateBasisPoints = 0;

	/** Tax of the Crown (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Tax = 0;

	/** Contract bonus, paid when the quota is met (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 ContractBonus = 0;

	/** Bonus of the orders filled (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 OrdersBonus = 0;

	/** Orders filled */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 OrdersFilled = 0;

	/** Players dead during the round */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Deaths = 0;

	/** Recruitment fees of the dead (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 RecruitFees = 0;

	/** Gross - tax + bonuses - fees (crowns), may be negative; 0 on a defeat, which pays nothing */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Net = 0;

	/** Treasury before the signature (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 TreasuryBefore = 0;

	/** Treasury after it, never below zero (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 TreasuryAfter = 0;

	/** Gross sales reached the quota */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bQuotaMet = false;

	/** Next round or defeat */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	ERunRoundOutcome Outcome = ERunRoundOutcome::Defeat;
};

/** Progress of a run the calculator updates: treasury, rounds won, rounds already resolved */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunProgress
{
	GENERATED_BODY()

	/** Identifier of the run */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FGuid RunId;

	/** Common money of the crew (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Treasury = 0;

	/** Rounds whose quota was met */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 RoundsWon = 0;

	/** The run is lost: no round resolves any more */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bClosed = false;

	/** Registers already signed, by round: a second signature returns the first result */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	TMap<FGuid, FRunRoundResult> ResolvedRounds;
};

/** Answer to the signature of a register */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunResolution
{
	GENERATED_BODY()

	/** Lines of the register */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	FRunRoundResult Result;

	/** The round was resolved before: nothing changed */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bAlreadyResolved = false;

	/** False for an invalid round id or a run already lost: nothing changed */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bValid = true;
};

/** Plain copy of the rules for the pure calculator (no UObject, see URunRules::MakeSnapshot) */
struct MONKE_ISLAND_API FRunRulesSnapshot
{
	/** Quota of round n: Base + Step x (n-1) + Accel x (n-1)^2 (crowns) */
	int32 QuotaBase = 250;
	int32 QuotaStep = 120;
	int32 QuotaAccel = 30;

	/** Quota x (1 + Scale x (players - 1)) */
	float PlayerQuotaScale = 0.25f;

	/** Tax of the Crown on the sales (basis points) */
	int32 TaxRateBasisPoints = 3500;

	/** Contract bonus when the quota is met (crowns) */
	int32 ContractBonus = 100;

	/** The contract bonus is taxed too */
	bool bTaxContractBonus = false;

	/** Fee per player dead during the round (crowns) */
	int32 RecruitFee = 50;

	/** Difficulty of round n: 1 + Step x (n-1), at most Max */
	float DifficultyStep = 0.1f;
	float DifficultyMax = 1.5f;
};
