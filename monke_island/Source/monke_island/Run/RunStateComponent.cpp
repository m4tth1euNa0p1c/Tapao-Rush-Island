// monke_island

#include "Run/RunStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

URunStateComponent::URunStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

URunStateComponent* URunStateComponent::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<URunStateComponent>() : nullptr;
}

int32 URunStateComponent::CampaignDayOf(const UObject* WorldContext)
{
	const URunStateComponent* Run = Get(WorldContext);
	return Run ? Run->GetCampaignDay() : 0;
}

void URunStateComponent::SetState(int32 InDayIndex, int32 InPeriodIndex, int32 InDaysPerPeriod, int32 InPeriodCount, int32 InQuota,
	int32 InHoldValue, int32 InTreasury, ERunOutcome InOutcome)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	DaysPerPeriod = FMath::Max(1, InDaysPerPeriod);
	PeriodCount = FMath::Max(1, InPeriodCount);
	DayIndex = FMath::Clamp(InDayIndex, 1, DaysPerPeriod);
	PeriodIndex = FMath::Clamp(InPeriodIndex, 1, PeriodCount);
	Quota = FMath::Max(0, InQuota);
	HoldValue = FMath::Max(0, InHoldValue);
	Treasury = FMath::Max(0, InTreasury);
	Outcome = InOutcome;
	// the listen server does not get OnRep
	OnRep_State();
}

void URunStateComponent::SetFromContract(const FRunContractState& State, int32 InDaysPerContract, int32 InZoneCount,
	const FText& InZoneName, const FRunDayReport& InLastDayReport)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	ZoneName = InZoneName;
	bRunActive = State.bActive;
	bQuotaMet = State.bQuotaMet;
	bGreedDay = State.bGreedDay;
	bAwaitingChoice = State.bAwaitingChoice;
	DaysPlayed = State.DaysPlayed;
	LastDayReport = InLastDayReport;
	SetState(State.DayInContract, State.ZoneIndex + 1, InDaysPerContract, InZoneCount, State.Quota, State.ContractValue,
		State.Treasury, State.Outcome);
}

void URunStateComponent::OnRep_State()
{
	OnRunStateChanged.Broadcast();
}

void URunStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URunStateComponent, DayIndex);
	DOREPLIFETIME(URunStateComponent, PeriodIndex);
	DOREPLIFETIME(URunStateComponent, DaysPerPeriod);
	DOREPLIFETIME(URunStateComponent, PeriodCount);
	DOREPLIFETIME(URunStateComponent, Quota);
	DOREPLIFETIME(URunStateComponent, HoldValue);
	DOREPLIFETIME(URunStateComponent, Treasury);
	DOREPLIFETIME(URunStateComponent, Outcome);
	DOREPLIFETIME(URunStateComponent, ZoneName);
	DOREPLIFETIME(URunStateComponent, bQuotaMet);
	DOREPLIFETIME(URunStateComponent, bGreedDay);
	DOREPLIFETIME(URunStateComponent, bAwaitingChoice);
	DOREPLIFETIME(URunStateComponent, DaysPlayed);
	DOREPLIFETIME(URunStateComponent, bRunActive);
	DOREPLIFETIME(URunStateComponent, LastDayReport);
}
