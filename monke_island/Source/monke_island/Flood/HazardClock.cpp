// monke_island

#include "Flood/HazardClock.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "monke_island.h"

namespace HazardClock
{
	EHazardClockSource Resolve(EHazardClockMode Mode, bool bGameStateKnown, bool bExpeditionAvailable)
	{
		if (Mode == EHazardClockMode::DayNightOnly)
		{
			return EHazardClockSource::DayNight;
		}
		if (!bGameStateKnown)
		{
			return EHazardClockSource::Undecided;
		}
		if (bExpeditionAvailable)
		{
			return EHazardClockSource::Expedition;
		}
		return Mode == EHazardClockMode::Auto ? EHazardClockSource::DayNight : EHazardClockSource::Disabled;
	}

	bool IsExpeditionAvailable(const AExpeditionGameState* GameState)
	{
		return GameState && GameState->Rules;
	}

	EHazardClockSource ResolveForWorld(EHazardClockMode Mode, const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return Resolve(Mode, GameState != nullptr, IsExpeditionAvailable(Cast<AExpeditionGameState>(GameState)));
	}

	void LogDecision(const AActor* Hazard, const TCHAR* What, EHazardClockMode Mode, EHazardClockSource Source)
	{
		const FString Name = GetNameSafe(Hazard);
		const FString ModeName = UEnum::GetValueAsString(Mode);
		switch (Source)
		{
		case EHazardClockSource::Expedition:
			UE_LOG(Logmonke_island, Log, TEXT("%s: %s follows the expedition clock (DA_ExpeditionRules), mode %s"), *Name, What, *ModeName);
			break;
		case EHazardClockSource::DayNight:
			UE_LOG(Logmonke_island, Log, TEXT("%s: %s follows the day / night clock (no expedition game state with rules in this map), mode %s"),
				*Name, What, *ModeName);
			break;
		case EHazardClockSource::Disabled:
			UE_LOG(Logmonke_island, Warning, TEXT("%s: %s is off: mode %s needs an expedition game state with rules (BP_ExpeditionGameState) and the map has none"),
				*Name, What, *ModeName);
			break;
		default:
			break;
		}
	}
}
