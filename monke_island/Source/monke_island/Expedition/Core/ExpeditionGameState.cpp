// monke_island

#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "monke_island.h"

AExpeditionGameState::AExpeditionGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

AExpeditionGameState* AExpeditionGameState::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetGameState<AExpeditionGameState>() : nullptr;
}

void AExpeditionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExpeditionGameState, ExpeditionId);
	DOREPLIFETIME(AExpeditionGameState, ClockState);
	DOREPLIFETIME(AExpeditionGameState, Phase);
	DOREPLIFETIME(AExpeditionGameState, Resolution);
}

void AExpeditionGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bStartOnBeginPlay)
	{
		StartExpedition();
	}
}

void AExpeditionGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
	}

	Super::EndPlay(EndPlayReason);
}

FExpeditionClockParams AExpeditionGameState::GetParams() const
{
	return Rules ? FExpeditionClockParams::FromRules(*Rules) : FExpeditionClockParams();
}

bool AExpeditionGameState::StartExpedition()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (ClockState.bStarted)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: already started, start ignored"), *ExpeditionId.ToString());
		return false;
	}
	if (!ensureMsgf(Rules, TEXT("%s has no ExpeditionRules (DA_ExpeditionRules): the expedition clock cannot start"), *GetNameSafe(this)))
	{
		return false;
	}

	const FExpeditionClockParams Params = GetParams();
	const double Now = GetServerWorldTimeSeconds();
	const double PreviousFloodStartTime = ClockState.FloodStartTime;

	ExpeditionId = FGuid::NewGuid();
	ClockState = ExpeditionClock::Start(Now, Params);
	Resolution = EExpeditionResolution::None;

	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: started at %.2f s, flood at %.2f s"),
		*ExpeditionId.ToString(), ClockState.StartTime, ClockState.FloodStartTime);

	OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousFloodStartTime);
	RefreshPhase();
	return true;
}

EZoneBonusResult AExpeditionGameState::AddZoneBonus(FName ZoneId)
{
	if (!HasAuthority())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: bonus of zone '%s' requested without authority (%s), ignored"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), *GetNameSafe(this));
		return EZoneBonusResult::NoAuthority;
	}

	// The rule "strictly before the deadline" uses the server clock, never a time given by the caller
	const double ServerTime = GetServerWorldTimeSeconds();

	// The flood may be due before its timer fired this frame: settle the phase first, it is irreversible
	RefreshPhase();

	const double PreviousFloodStartTime = ClockState.FloodStartTime;
	EZoneBonusResult Result = EZoneBonusResult::TooLate;
	if (Phase == EExpeditionPhase::Submerging || Phase == EExpeditionPhase::Resolved)
	{
		// The submersion started: whatever time the caller passes, it never moves the deadline back
		Result = HasZoneBonus(ZoneId) ? EZoneBonusResult::AlreadyGranted : EZoneBonusResult::TooLate;
	}
	else
	{
		Result = ExpeditionClock::TryAddZoneBonus(ClockState, ZoneId, ServerTime, GetParams());
	}

	switch (Result)
	{
	case EZoneBonusResult::Granted:
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: zone %s pacified at %.2f s, flood moved from %.2f s to %.2f s"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), ServerTime, PreviousFloodStartTime, ClockState.FloodStartTime);
		OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousFloodStartTime);
		RefreshPhase();
		break;

	case EZoneBonusResult::AlreadyGranted:
		UE_LOG(Logmonke_island, Verbose, TEXT("Expedition %s: zone %s already gave its bonus"), *ExpeditionId.ToString(), *ZoneId.ToString());
		break;

	case EZoneBonusResult::TooLate:
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: zone %s pacified at %.2f s, after the flood at %.2f s: no bonus"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), ServerTime, ClockState.FloodStartTime);
		break;

	default:
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: bonus of zone '%s' refused (%s)"), *ExpeditionId.ToString(), *ZoneId.ToString(),
			*UEnum::GetValueAsString(Result));
		break;
	}
	return Result;
}

double AExpeditionGameState::GetWaterRise(double ServerTime) const
{
	// Client: the replicated deadline may still move before the submersion; only the server clock decides it started
	if (!HasAuthority() && Phase < EExpeditionPhase::Submerging)
	{
		return 0.0;
	}
	return ExpeditionClock::WaterRiseCm(ClockState, ServerTime, GetParams());
}

double AExpeditionGameState::GetCurrentWaterRise() const
{
	return GetWaterRise(GetServerWorldTimeSeconds());
}

double AExpeditionGameState::GetTimeBeforeSubmersion() const
{
	return ExpeditionClock::TimeBeforeSubmersion(ClockState, GetServerWorldTimeSeconds());
}

double AExpeditionGameState::GetSubmersionEndTime() const
{
	return ExpeditionClock::SubmersionEndTime(ClockState, GetParams());
}

int32 AExpeditionGameState::GetRemainingZoneCount() const
{
	const int32 ZonesCount = GetParams().ZonesCount;
	return ZonesCount > 0 ? FMath::Max(0, ZonesCount - ClockState.BonusedZoneIds.Num()) : -1;
}

void AExpeditionGameState::RefreshPhase()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || Phase == EExpeditionPhase::Resolved)
	{
		return;
	}

	const FExpeditionClockParams Params = GetParams();
	const double Now = GetServerWorldTimeSeconds();
	const EExpeditionPhase Target = ExpeditionClock::PhaseAt(ClockState, Now, Params);

	// Phases only move forward, one step at a time so every server listener sees each of them
	// (clients only receive the latest replicated phase and may skip steps)
	while (Phase < Target)
	{
		SetPhase(static_cast<EExpeditionPhase>(static_cast<uint8>(Phase) + 1));
	}

	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(PhaseTimer);

	double NextTransition = -1.0;
	if (Phase == EExpeditionPhase::Exploring)
	{
		NextTransition = ClockState.FloodStartTime;
	}
	else if (Phase == EExpeditionPhase::Submerging)
	{
		NextTransition = ExpeditionClock::SubmersionEndTime(ClockState, Params);
	}

	if (NextTransition >= 0.0)
	{
		// A timer of rate 0 is cleared: keep a tiny delay, RefreshPhase reschedules if it fires a hair early
		const float Delay = static_cast<float>(FMath::Max(NextTransition - Now, static_cast<double>(UE_KINDA_SMALL_NUMBER)));
		Timers.SetTimer(PhaseTimer, this, &AExpeditionGameState::RefreshPhase, Delay, false);
	}
}

void AExpeditionGameState::SetPhase(EExpeditionPhase NewPhase)
{
	if (NewPhase == Phase)
	{
		return;
	}

	const EExpeditionPhase PreviousPhase = Phase;
	if (NewPhase == EExpeditionPhase::Resolved)
	{
		Resolution = EExpeditionResolution::Submerged;
	}
	Phase = NewPhase;

	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: phase %s -> %s at %.2f s"), *ExpeditionId.ToString(),
		*UEnum::GetValueAsString(PreviousPhase), *UEnum::GetValueAsString(NewPhase), GetServerWorldTimeSeconds());

	OnPhaseChanged.Broadcast(Phase, PreviousPhase);
	if (Phase == EExpeditionPhase::Resolved && Resolution == EExpeditionResolution::Submerged)
	{
		OnSubmersionEnded.Broadcast();
	}
}

void AExpeditionGameState::OnRep_ClockState(const FExpeditionClockState& PreviousClockState)
{
	if (ClockState.FloodStartTime != PreviousClockState.FloodStartTime)
	{
		OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousClockState.FloodStartTime);
	}
}

void AExpeditionGameState::OnRep_Phase(EExpeditionPhase PreviousPhase)
{
	OnPhaseChanged.Broadcast(Phase, PreviousPhase);
	if (Phase == EExpeditionPhase::Resolved && Resolution == EExpeditionResolution::Submerged)
	{
		OnSubmersionEnded.Broadcast();
	}
}
