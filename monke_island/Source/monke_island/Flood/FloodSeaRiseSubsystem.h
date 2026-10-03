// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Flood/FloodSchedule.h"
#include "FloodSeaRiseSubsystem.generated.h"

class AExpeditionGameState;
class UFloodRules;

/**
 *  Rise of the water of an expedition, seen from the whole world (T-103, cahier §6.1, recette n°16).
 *  Follows AExpeditionGameState::GetCurrentWaterRise (650 cm over the 60 s of the submersion, then +10 cm/s up to
 *  +15 m, DA_ExpeditionRules), held so it never goes down (FFloodSeaRise), and:
 *  - logs the frame times before the submersion, during it and during the post-submersion rise (perf of T-103);
 *  - with DA_FloodRules.bRaiseGlobalSea only (off by default: the owner wants the water to rise where the players
 *    are, not at the spawn), moves the sea level of the whole map through UIslandWaterSubsystem::SetSeaLevel.
 *  The water of the fragment played is the local grid of AFloodSimulationActor. Every machine computes the same rise
 *  from the replicated clock (server time, phase): a client waits for the replicated Submerging phase. Off in a map
 *  without expedition.
 */
UCLASS()
class MONKE_ISLAND_API UFloodSeaRiseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Flood rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	/** Returns the subsystem of the given world context, or nullptr */
	static UFloodSeaRiseSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

	virtual bool IsTickable() const override { return Mode == EMode::Undecided || Mode == EMode::Following; }

	/** Whether this world's global sea level follows the expedition clock (DA_FloodRules.bRaiseGlobalSea) */
	bool IsDrivingSea() const { return bRaiseGlobalSea && (Mode == EMode::Following || Mode == EMode::Done); }

	/** Rise of the global sea above its level before the expedition (cm) */
	double GetHeldRiseCm() const { return SeaRise.HeldRiseCm; }

private:

	/** What the subsystem does in this world, decided once the game state is known */
	enum class EMode : uint8
	{
		/** Waiting for the game state (a client receives it late) */
		Undecided,
		/** No expedition: nothing to follow */
		Off,
		/** Follows the expedition clock (and moves the global sea with bRaiseGlobalSea) */
		Following,
		/** The water reached its final rise and stays there */
		Done
	};

	/** Frame times of one window of the expedition (real time, not dilated) */
	struct FFrameStats
	{
		int32 Frames = 0;
		double Sum = 0.0;
		double Min = TNumericLimits<double>::Max();
		double Max = 0.0;

		void Add(double Seconds);
		void Log(const TCHAR* Window) const;
	};

	/** Decides the mode once the game state exists */
	void Decide();

	/** Records the frame time in its window and logs the windows as they close */
	void RecordFrame(const AExpeditionGameState& GameState);

	/** Settings (DA_FloodRules), defaults when the asset is missing */
	UPROPERTY(Transient)
	TObjectPtr<UFloodRules> Rules;

	/** Expedition followed */
	TWeakObjectPtr<AExpeditionGameState> Expedition;

	/** Sea level before the rise and highest rise reached */
	FFloodSeaRise SeaRise;

	/** Sea level last applied to UIslandWaterSubsystem (cm) */
	double AppliedSeaLevelCm = 0.0;

	/** Highest rise the expedition can reach (cm) */
	double FinalRiseCm = 0.0;

	/** Current mode */
	EMode Mode = EMode::Undecided;

	/** Whether the global sea level moves (DA_FloodRules.bRaiseGlobalSea, read once) */
	bool bRaiseGlobalSea = false;

	/** Frame times: last seconds before the submersion, the submersion, the post-submersion rise */
	FFrameStats BeforeStats;
	FFrameStats SubmersionStats;
	FFrameStats AfterStats;

	/** Index of the window being recorded: 0 before, 1 submersion, 2 after, 3 closed */
	int32 StatsWindow = 0;
};
