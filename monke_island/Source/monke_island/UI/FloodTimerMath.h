// monke_island

#pragma once

#include "CoreMinimal.h"
#include "FloodTimerMath.generated.h"

/** What the flood timer shows (cahier §6.1, §19) */
UENUM(BlueprintType)
enum class EFloodTimerMode : uint8
{
	/** No active flood source: nothing on screen */
	Hidden,
	/** Before the rise: "Montée des eaux dans 1:45" */
	Countdown,
	/** During the final rise: distinct evacuation countdown */
	Evacuation,
	/** The rise is over */
	Over,
	/** Every player is dead: countdown to the next expedition */
	GameOver
};

/** Times read from a flood source, all on the same clock (server time, or the elapsed time of the day / night clock) */
struct MONKE_ISLAND_API FFloodTimerInput
{
	/** Whether a flood clock runs */
	bool bActive = false;

	/** Current time on the clock of the source (s) */
	double Now = 0.0;

	/** Time the submersion starts (s) */
	double FloodStartTime = 0.0;

	/** Time the submersion ends (s) */
	double SubmersionEndTime = 0.0;

	/**
	 *  Whether the source confirms that the submersion started. For the expedition it is the replicated phase:
	 *  a client never shows the evacuation before the server decided it (a bonus may still be on its way).
	 */
	bool bSubmersionStarted = false;

	/** Whether the source confirms that the expedition resolved */
	bool bResolved = false;

	/** Whether every player is dead */
	bool bGameOver = false;

	/** Time the next expedition starts after a game over (s) */
	double RestartTime = 0.0;
};

/** What the timer displays at one moment */
struct MONKE_ISLAND_API FFloodTimerSnapshot
{
	/** Display mode */
	EFloodTimerMode Mode = EFloodTimerMode::Hidden;

	/** Seconds left in the current mode (to the rise, or to the end of the rise) */
	double SecondsLeft = 0.0;
};

/** Things that happened between two updates of the presenter */
struct MONKE_ISLAND_API FFloodTimerEvents
{
	/** Whether the mode changed */
	bool bModeChanged = false;

	/** Mode before the change */
	EFloodTimerMode PreviousMode = EFloodTimerMode::Hidden;

	/** Threshold that alerts now (s), negative when none */
	float AlertThreshold = -1.0f;

	/** Whether an alert fires */
	bool HasAlert() const { return AlertThreshold >= 0.0f; }
};

/**
 *  Pure logic of the flood timer, shared by UFloodTimerWidget and the tests: modes, formatting, single alerts.
 *  Never runs a clock of its own: every value comes from the times of the source.
 */
namespace FloodTimerMath
{
	/** Mode and time left given by the source */
	MONKE_ISLAND_API FFloodTimerSnapshot Evaluate(const FFloodTimerInput& Input);

	/** Whole seconds shown for a time left: rounded up, so 0:00 only shows once the time is really out */
	MONKE_ISLAND_API int32 DisplaySeconds(double Seconds);

	/** "m:ss" (minutes not capped: 8:00, 12:05) */
	MONKE_ISLAND_API FText FormatClock(double Seconds);

	/** Whether a deadline change is a zone bonus (the deadline moved later after the start), not the start itself */
	MONKE_ISLAND_API bool IsZoneBonus(double NewFloodStartTime, double PreviousFloodStartTime);
}

/**
 *  Turns successive snapshots into events: mode changes and single alerts at the thresholds (cahier §19:
 *  "Alerte unique à 60/30/10 s avant montée ; nouveau seuil après extension ne doit pas spammer").
 *  A threshold fires when the time left reaches it; thresholds already passed when the timer first sees the clock
 *  (late join) stay silent; several thresholds crossed at once (hitch, debug jump) give one alert, the most urgent.
 */
class MONKE_ISLAND_API FFloodTimerPresenter
{
public:

	/** Thresholds (s) and whether a deadline extension re-arms the thresholds above the new time left */
	void Configure(const TArray<float>& InThresholds, bool bInRearmAfterExtension);

	/** Forgets every fired alert and the last mode (new expedition or new source) */
	void Reset();

	/** Feeds a snapshot; returns what changed since the previous one */
	FFloodTimerEvents Update(const FFloodTimerSnapshot& Snapshot);

	/** Whether a threshold already alerted */
	bool HasFired(float Threshold) const;

	/** Last mode seen */
	EFloodTimerMode GetMode() const { return Mode; }

private:

	/** Thresholds, highest first */
	TArray<float> Thresholds;

	/** Fired flag of each threshold */
	TArray<bool> Fired;

	/** Re-arm after an extension */
	bool bRearmAfterExtension = false;

	/** Whether the countdown has been seen once (thresholds already passed then are silent) */
	bool bCountdownSeen = false;

	/** Time left of the previous countdown snapshot */
	double LastSecondsLeft = 0.0;

	/** Last mode */
	EFloodTimerMode Mode = EFloodTimerMode::Hidden;
};
