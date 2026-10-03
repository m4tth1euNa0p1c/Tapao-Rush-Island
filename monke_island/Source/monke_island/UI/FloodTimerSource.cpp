// monke_island

#include "UI/FloodTimerSource.h"
#include "DayNight/DayNightCycleActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Flood/FloodRules.h"
#include "Flood/FloodSimulationActor.h"

namespace
{
	bool IsExpeditionRunning(const AExpeditionGameState* GameState)
	{
		return GameState && (GameState->GetClockState().bStarted || GameState->GetPhase() >= EExpeditionPhase::Exploring);
	}
}

void FFloodTimerSource::Resolve(UWorld* World, bool bInAllowFloodActor)
{
	bAllowFloodActor = bInAllowFloodActor;
	if (!World)
	{
		Expedition.Reset();
		FloodActor.Reset();
		return;
	}
	Expedition = World->GetGameState<AExpeditionGameState>();
	if (!FloodActor.IsValid())
	{
		TActorIterator<AFloodSimulationActor> It(World);
		FloodActor = It ? *It : nullptr;
	}
}

EFloodTimerSourceKind FFloodTimerSource::GetKind() const
{
	if (IsExpeditionRunning(Expedition.Get()))
	{
		return EFloodTimerSourceKind::Expedition;
	}
	if (bAllowFloodActor && FloodActor.IsValid() && FloodActor->Rules)
	{
		return EFloodTimerSourceKind::FloodActor;
	}
	return EFloodTimerSourceKind::None;
}

bool FFloodTimerSource::HasConflictingSources() const
{
	return IsExpeditionRunning(Expedition.Get()) && FloodActor.IsValid();
}

FFloodTimerInput FFloodTimerSource::Read() const
{
	FFloodTimerInput Input;
	switch (GetKind())
	{
	case EFloodTimerSourceKind::Expedition:
	{
		const AExpeditionGameState* GameState = Expedition.Get();
		Input.bActive = true;
		Input.Now = GameState->GetServerWorldTimeSeconds();
		Input.FloodStartTime = GameState->GetFloodStartTime();
		Input.SubmersionEndTime = GameState->GetSubmersionEndTime();
		// The replicated phase confirms the submersion: a client never anticipates the server
		Input.bSubmersionStarted = GameState->GetPhase() >= EExpeditionPhase::Submerging;
		Input.bResolved = GameState->GetPhase() == EExpeditionPhase::Resolved;
		break;
	}
	case EFloodTimerSourceKind::FloodActor:
	{
		// Same clock as AFloodSimulationActor::GetElapsedSeconds: the replicated day / night clock, else the world time
		const AFloodSimulationActor* Flood = FloodActor.Get();
		const UFloodRules* Rules = Flood->Rules;
		const ADayNightCycleActor* Clock = ADayNightCycleActor::Get(Flood);
		const UWorld* World = Flood->GetWorld();
		Input.bActive = true;
		Input.Now = Clock ? Clock->GetElapsedSeconds() : (World ? World->GetTimeSeconds() : 0.0);
		Input.FloodStartTime = Rules->StartSeconds;
		Input.SubmersionEndTime = static_cast<double>(Rules->StartSeconds) + Rules->RiseSeconds;
		// Deterministic on every machine: the actor starts rising exactly when its clock passes StartSeconds
		Input.bSubmersionStarted = Input.Now >= Input.FloodStartTime;
		break;
	}
	default:
		break;
	}
	return Input;
}
