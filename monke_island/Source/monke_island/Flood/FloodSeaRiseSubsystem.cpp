// monke_island

#include "Flood/FloodSeaRiseSubsystem.h"
#include "Flood/FloodRules.h"
#include "Flood/HazardClock.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

const TCHAR* UFloodSeaRiseSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Environment/Storm/DA_FloodRules.DA_FloodRules");

UFloodSeaRiseSubsystem* UFloodSeaRiseSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFloodSeaRiseSubsystem>() : nullptr;
}

bool UFloodSeaRiseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// same worlds as UIslandWaterSubsystem: only game worlds have a playable sea
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UFloodSeaRiseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Rules = LoadObject<UFloodRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("SeaRise: %s missing, default flood rules"), DefaultRulesPath);
		Rules = NewObject<UFloodRules>(this);
	}
	Mode = EMode::Undecided;
	SeaRise = FFloodSeaRise();
	StatsWindow = 0;
}

void UFloodSeaRiseSubsystem::Deinitialize()
{
	// the expedition may end (travel, end of PIE) before the sea reached its top: keep the measures of the open windows
	if (Mode == EMode::Following)
	{
		if (StatsWindow == 1)
		{
			SubmersionStats.Log(TEXT("submersion (interrupted)"));
		}
		else if (StatsWindow == 2)
		{
			AfterStats.Log(TEXT("rise after the submersion (interrupted)"));
		}
	}
	Expedition.Reset();
	Super::Deinitialize();
}

TStatId UFloodSeaRiseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFloodSeaRiseSubsystem, STATGROUP_Tickables);
}

void UFloodSeaRiseSubsystem::Decide()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameStateBase = World ? World->GetGameState() : nullptr;
	if (!GameStateBase || !Rules)
	{
		// a client waits for the replicated game state
		return;
	}

	AExpeditionGameState* GameState = AExpeditionGameState::Get(this);
	if (!HazardClock::IsExpeditionAvailable(GameState) || Rules->ClockMode == EHazardClockMode::DayNightOnly)
	{
		Mode = EMode::Off;
		UE_LOG(Logmonke_island, Log, TEXT("SeaRise: no expedition clock in %s (or DA_FloodRules forces the day / night clock): the global sea stays put"),
			*GetNameSafe(World));
		return;
	}
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!ensureMsgf(Water, TEXT("SeaRise: no UIslandWaterSubsystem in a game world")))
	{
		Mode = EMode::Off;
		return;
	}

	Expedition = GameState;
	// Only this subsystem moves the sea: its level is still the one of the map, even for a late joiner
	SeaRise.BaseSeaLevelCm = Water->GetSeaLevel();
	SeaRise.HeldRiseCm = 0.0;
	AppliedSeaLevelCm = SeaRise.BaseSeaLevelCm;
	FinalRiseCm = FloodSchedule::ExpeditionFinalRiseCm(FExpeditionClockParams::FromRules(*GameState->Rules));
	bRaiseGlobalSea = Rules->bRaiseGlobalSea;
	Mode = EMode::Following;
	if (bRaiseGlobalSea)
	{
		UE_LOG(Logmonke_island, Log, TEXT("SeaRise: the global sea (%.0f cm) follows the expedition clock, up to +%.0f cm, in steps of %.1f cm"),
			SeaRise.BaseSeaLevelCm, FinalRiseCm, Rules->SeaLevelStepCm);
	}
	else
	{
		UE_LOG(Logmonke_island, Log, TEXT("SeaRise: the global sea stays at %.0f cm (DA_FloodRules.bRaiseGlobalSea off): the water rises on the fragment played only"),
			SeaRise.BaseSeaLevelCm);
	}
}

void UFloodSeaRiseSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFloodSeaRiseSubsystem::Tick);

	if (Mode == EMode::Undecided)
	{
		Decide();
		if (Mode != EMode::Following)
		{
			return;
		}
	}

	const AExpeditionGameState* GameState = Expedition.Get();
	UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!GameState || !Water || !Rules)
	{
		return;
	}

	RecordFrame(*GameState);

	// 0 before the deadline; a client stays at 0 until the replicated phase reaches Submerging
	const double PreviousRise = SeaRise.HeldRiseCm;
	const double SeaLevel = SeaRise.Advance(GameState->GetCurrentWaterRise());
	if (PreviousRise <= 0.0 && SeaRise.HeldRiseCm > 0.0)
	{
		UE_LOG(Logmonke_island, Log, TEXT("SeaRise: the water starts rising (%.2f s server time, deadline %.2f s)"),
			GameState->GetServerWorldTimeSeconds(), GameState->GetFloodStartTime());
	}

	const bool bTop = SeaRise.HeldRiseCm >= FinalRiseCm - UE_KINDA_SMALL_NUMBER;
	if (bRaiseGlobalSea && FloodSchedule::ShouldApplySeaLevel(AppliedSeaLevelCm, SeaLevel, Rules->SeaLevelStepCm, bTop))
	{
		AppliedSeaLevelCm = SeaLevel;
		Water->SetSeaLevel(static_cast<float>(SeaLevel));
	}

	if (bTop)
	{
		Mode = EMode::Done;
		if (StatsWindow == 2)
		{
			AfterStats.Log(TEXT("rise after the submersion"));
			StatsWindow = 3;
		}
		UE_LOG(Logmonke_island, Log, TEXT("SeaRise: the water reached its top, +%.0f cm (%s)"), SeaRise.HeldRiseCm,
			bRaiseGlobalSea ? TEXT("global sea") : TEXT("fragment grid"));
	}
}

void UFloodSeaRiseSubsystem::RecordFrame(const AExpeditionGameState& GameState)
{
	const double FrameSeconds = FApp::GetDeltaTime();
	const EExpeditionPhase Phase = GameState.GetPhase();
	const double Now = GameState.GetServerWorldTimeSeconds();
	const bool bSubmerging = SeaRise.HeldRiseCm > 0.0 || Phase >= EExpeditionPhase::Submerging;
	const bool bAfter = bSubmerging && Now >= GameState.GetSubmersionEndTime();

	if (StatsWindow == 0)
	{
		if (!bSubmerging)
		{
			if (GameState.GetClockState().bStarted && GameState.GetTimeBeforeSubmersion() <= Rules->FrameStatsLeadSeconds)
			{
				BeforeStats.Add(FrameSeconds);
			}
			return;
		}
		BeforeStats.Log(TEXT("before the submersion"));
		StatsWindow = 1;
	}
	if (StatsWindow == 1)
	{
		if (!bAfter)
		{
			SubmersionStats.Add(FrameSeconds);
			return;
		}
		SubmersionStats.Log(TEXT("submersion"));
		StatsWindow = 2;
	}
	if (StatsWindow == 2)
	{
		AfterStats.Add(FrameSeconds);
	}
}

void UFloodSeaRiseSubsystem::FFrameStats::Add(double Seconds)
{
	++Frames;
	Sum += Seconds;
	Min = FMath::Min(Min, Seconds);
	Max = FMath::Max(Max, Seconds);
}

void UFloodSeaRiseSubsystem::FFrameStats::Log(const TCHAR* Window) const
{
	if (Frames == 0)
	{
		UE_LOG(Logmonke_island, Log, TEXT("SeaRise frame times, %s: no frame recorded"), Window);
		return;
	}
	UE_LOG(Logmonke_island, Log, TEXT("SeaRise frame times, %s: %d frames, min %.2f ms, mean %.2f ms, max %.2f ms"),
		Window, Frames, Min * 1000.0, Sum / Frames * 1000.0, Max * 1000.0);
}
