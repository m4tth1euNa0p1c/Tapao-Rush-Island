// monke_island

#include "Run/RunContract.h"

namespace
{
	const FRunZone* ZoneAt(const FRunContractRules& Rules, int32 ZoneIndex)
	{
		return Rules.Zones.IsEmpty() ? nullptr : &Rules.Zones[FMath::Clamp(ZoneIndex, 0, Rules.Zones.Num() - 1)];
	}
}

int32 FRunContractCalculator::QuotaOfZone(const FRunContractRules& Rules, int32 ZoneIndex)
{
	const FRunZone* Zone = ZoneAt(Rules, ZoneIndex);
	return Zone ? FMath::Max(0, Zone->Quota) : 0;
}

int32 FRunContractCalculator::DaysOfZone(const FRunContractRules& Rules, int32 ZoneIndex)
{
	const FRunZone* Zone = ZoneAt(Rules, ZoneIndex);
	return FMath::Max(1, Zone && Zone->Days > 0 ? Zone->Days : Rules.DaysPerContract);
}

FRunContractState FRunContractCalculator::StartRun(const FRunContractRules& Rules)
{
	FRunContractState State;
	State.bActive = true;
	State.ZoneIndex = 0;
	State.ContractNumber = 1;
	OpenContract(Rules, State);
	return State;
}

void FRunContractCalculator::JumpToDay(const FRunContractRules& Rules, FRunContractState& State, int32 CampaignDay)
{
	const int32 Treasury = State.Treasury;
	const int32 Day = FMath::Max(1, CampaignDay);
	State = StartRun(Rules);
	State.Treasury = Treasury;
	int32 DaysBefore = 0;
	while (State.ZoneIndex < Rules.Zones.Num() - 1 && Day > DaysBefore + DaysOfZone(Rules, State.ZoneIndex))
	{
		DaysBefore += DaysOfZone(Rules, State.ZoneIndex);
		++State.ZoneIndex;
		++State.ContractNumber;
	}
	OpenContract(Rules, State);
	State.DayInContract = FMath::Min(Day - DaysBefore, DaysOfZone(Rules, State.ZoneIndex));
	State.DaysPlayed = Day - 1;
}

void FRunContractCalculator::OpenContract(const FRunContractRules& Rules, FRunContractState& State)
{
	State.DayInContract = 1;
	State.ContractValue = 0;
	State.Quota = QuotaOfZone(Rules, State.ZoneIndex);
	State.bQuotaMet = false;
	State.bGreedDay = false;
	State.bAwaitingChoice = false;
}

FRunDayReport FRunContractCalculator::EndDay(const FRunContractRules& Rules, FRunContractState& State, int32 DayValue, int32 Deaths, bool bWiped)
{
	FRunDayReport Report;
	if (!State.bActive || State.Outcome != ERunOutcome::InProgress || State.bAwaitingChoice)
	{
		return Report;
	}

	Report.ZoneIndex = State.ZoneIndex;
	Report.Day = State.DayInContract;
	Report.bWiped = bWiped;
	Report.Deaths = FMath::Max(0, Deaths);
	// a wiped crew left everything on the island
	Report.DayValue = bWiped ? 0 : FMath::Max(0, DayValue);
	Report.MedicalFees = FMath::Min(State.Treasury, Report.Deaths * FMath::Max(0, Rules.MedicalFee));

	State.Treasury -= Report.MedicalFees;
	State.ContractValue += Report.DayValue;
	State.TotalValue += Report.DayValue;
	++State.DaysPlayed;
	State.bQuotaMet = State.ContractValue >= State.Quota;

	const bool bDaysLeft = State.DayInContract < DaysOfZone(Rules, State.ZoneIndex);
	Report.ContractValue = State.ContractValue;
	Report.Quota = State.Quota;

	if (State.bQuotaMet && bDaysLeft)
	{
		State.bAwaitingChoice = true;
		Report.Verdict = ERunDayVerdict::QuotaMetChoose;
	}
	else if (State.bQuotaMet)
	{
		Report.Verdict = CompleteContract(Rules, State, Report.Payout);
	}
	else if (bDaysLeft)
	{
		++State.DayInContract;
		Report.Verdict = ERunDayVerdict::NextDay;
	}
	else if (Rules.bExecuteOnFailedContract)
	{
		// the milestone day without the quota: the crew answers for it, the run is over
		++State.FailedContracts;
		State.Outcome = ERunOutcome::Defeat;
		Report.Verdict = ERunDayVerdict::Executed;
	}
	else
	{
		Report.Penalty = FMath::FloorToInt(State.Treasury * FMath::Clamp(Rules.FailedTreasuryLoss, 0.0f, 1.0f));
		State.Treasury -= Report.Penalty;
		++State.FailedContracts;
		State.ZoneIndex = FMath::Max(0, State.ZoneIndex - FMath::Max(0, Rules.FailedZoneDrop));
		++State.ContractNumber;
		OpenContract(Rules, State);
		Report.Verdict = ERunDayVerdict::ContractFailed;
	}
	return Report;
}

ERunDayVerdict FRunContractCalculator::Choose(const FRunContractRules& Rules, FRunContractState& State, bool bStay, int32* OutPayout)
{
	if (!State.bActive || !State.bAwaitingChoice || State.Outcome != ERunOutcome::InProgress)
	{
		return ERunDayVerdict::Invalid;
	}
	State.bAwaitingChoice = false;
	if (bStay)
	{
		++State.DayInContract;
		State.bGreedDay = true;
		return ERunDayVerdict::NextDay;
	}
	int32 Payout = 0;
	const ERunDayVerdict Verdict = CompleteContract(Rules, State, Payout);
	if (OutPayout)
	{
		*OutPayout = Payout;
	}
	return Verdict;
}

ERunDayVerdict FRunContractCalculator::CompleteContract(const FRunContractRules& Rules, FRunContractState& State, int32& OutPayout)
{
	const int32 Excess = FMath::Max(0, State.ContractValue - State.Quota);
	OutPayout = FMath::FloorToInt(Excess * FMath::Clamp(Rules.ExcessPayoutFraction, 0.0f, 1.0f));
	State.Treasury += OutPayout;
	State.bAwaitingChoice = false;

	const bool bLastZone = State.ZoneIndex >= Rules.Zones.Num() - 1;
	if (bLastZone)
	{
		State.Outcome = ERunOutcome::Victory;
		State.bQuotaMet = true;
		return ERunDayVerdict::Victory;
	}
	++State.ZoneIndex;
	++State.ContractNumber;
	OpenContract(Rules, State);
	return ERunDayVerdict::ContractComplete;
}

float FRunContractCalculator::DangerScale(const FRunContractRules& Rules, const FRunContractState& State)
{
	const FRunZone* Zone = ZoneAt(Rules, State.ZoneIndex);
	const float Base = Zone ? FMath::Max(0.0f, Zone->SpawnScale) : 1.0f;
	return State.bGreedDay ? Base * FMath::Max(0.0f, Rules.GreedDangerScale) : Base;
}

float FRunContractCalculator::LootValueScale(const FRunContractRules& Rules, const FRunContractState& State)
{
	const FRunZone* Zone = ZoneAt(Rules, State.ZoneIndex);
	const float Base = Zone ? FMath::Max(0.0f, Zone->LootValueScale) : 1.0f;
	return State.bGreedDay ? Base * FMath::Max(0.0f, Rules.GreedLootScale) : Base;
}

float FRunContractCalculator::FloodDelayScale(const FRunContractRules& Rules, const FRunContractState& State)
{
	const FRunZone* Zone = ZoneAt(Rules, State.ZoneIndex);
	return Zone ? FMath::Max(0.1f, Zone->FloodDelayScale) : 1.0f;
}
