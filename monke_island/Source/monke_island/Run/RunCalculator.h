// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Run/RunTypes.h"

/**
 *  Pure math of a run of rounds (cahier v1.3 §24.4, §24.6), with no UObject and no world: the server calls it, the
 *  widgets only show its register.
 *   - quota of round n = (Base + Step x (n-1) + Accel x (n-1)^2) x (1 + Scale x (players - 1)), rounded;
 *   - one sale per item; the quota is judged on the gross sales;
 *   - tax = floor(gross x rate) (FEconomyCalculator::ComputeTax), contract bonus when the quota is met, untaxed by default;
 *   - net = gross - tax + bonus + orders - recruitment fees; the treasury never goes below zero;
 *   - a missed quota pays nothing: no tax, bonus, order or fee, net 0, treasury unchanged (the orders filled still count);
 *   - a register is resolved once per RoundId; a lost run resolves nothing more.
 */
struct MONKE_ISLAND_API FRunCalculator
{
	/** Gross sales required in round RoundIndex (from 1) for PlayerCount players */
	static int32 QuotaForRound(const FRunRulesSnapshot& Rules, int32 RoundIndex, int32 PlayerCount);

	/** Difficulty factor of a round (enemies, guardians, ambushes), from 1 up to DifficultyMax */
	static float DifficultyForRound(const FRunRulesSnapshot& Rules, int32 RoundIndex);

	/** Sells an item into the register: once per item, never after the signature */
	static ERunSaleError TrySell(FRunLedger& Ledger, const FGuid& ItemId, FName Category, int32 Value);

	/** Lines of the register if it were signed now, with no side effect */
	static FRunRoundResult ComputeRound(const FRunRulesSnapshot& Rules, const FRunRoundInput& Input, int32 TreasuryBefore);

	/**
	 *  Signs the register once: treasury, rounds won, run closed on a defeat, register of the resolved rounds.
	 *  A second call with the same round returns the stored result and changes nothing.
	 */
	static FRunResolution ResolveRound(const FRunRulesSnapshot& Rules, const FRunRoundInput& Input, FRunProgress& InOutProgress);
};
