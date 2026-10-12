// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DayTransitionSubsystem.generated.h"

class ACameraActor;
class AGalleonActor;
class UDayTransitionRules;

/**
 *  End and start of a day as the local player sees them (T-214; DA_DayTransitionRules):
 *  - departure: once the day is survived the view leaves the player for a camera watching the galleon sail off into
 *    the distance (moved here, on this machine: nobody plays any more), the crew hidden, the HUD counts down to the
 *    travel ("Départ dans N", STapaoHUD), then the screen fades to black;
 *  - arrival: on the next island the crew starts on the deck of the galleon at anchor (AGalleonSpawner, a few seconds
 *    after the boat start, in the black); the camera comes in from the open sea toward the galleon while the HUD shows
 *    "Jour N", stops outside the ship (never through the rigging) and the view cuts to the player's eyes through a short
 *    fade, with the controls back. Without a galleon, from behind the boat.
 *  Nothing on a dedicated server; the day, the travel and their times stay the server's (AExpeditionGameState).
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UDayTransitionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	UDayTransitionSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The subsystem of this world, or null */
	static UDayTransitionSubsystem* Get(const UObject* WorldContext);

	/** Opacity of the "Jour N" title of the arrival (0 when none) */
	float GetTitleAlpha() const;

	/** True while the departure scene plays */
	bool IsDeparting() const { return bDeparting; }

	/** A scene of the day is on screen (the arrival, its black wait, the departure, the execution): no player UI then */
	bool IsScenePlaying() const { return bDeparting || bArriving || bHoldingBlack || bExecuting; }

	/** True once the execution of the crew began on this machine (it never ends: the end screen follows) */
	bool IsExecuting() const { return bExecuting; }

	/** The execution scene played to its end */
	bool IsExecutionOver() const;

	virtual void Deinitialize() override;

private:

	void StartDeparture(AGalleonActor* Galleon);
	void UpdateDeparture(float DeltaTime);

	/** The execution of the crew (AExecutionScene): the title over the deck, black, the camera of the scene, black */
	void StartExecution();
	void UpdateExecution();
	void StartArrival();
	void UpdateArrival();

	/** Black screen while the galleon is awaited (on), or back from black (off) */
	void HoldBlack(bool bHold);

	/** True once the island's first heavy frames are over (or waited for long enough), latched */
	bool IsIslandSettled();

	/** The engine's on-screen messages (the VSM warnings of a Development build) hidden during a scene (owner
	 *  2026-10-07), shown again after it */
	void HideScreenMessages(bool bHide);

	bool bScreenMessagesHidden = false;
	bool bScreenMessagesWereShown = true;

	/** DA_DayTransitionRules (set in DefaultGame.ini or here) */
	UPROPERTY(Config)
	FSoftObjectPath RulesPath;

	UPROPERTY(Transient)
	TObjectPtr<UDayTransitionRules> Rules;

	/** Camera of the scene under way */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	TWeakObjectPtr<AGalleonActor> Galleon;
	FVector SailDirection = FVector::ForwardVector;
	float SailSpeedNow = 0.0f;
	bool bDeparting = false;
	bool bFaded = false;

	/** Arrival: done, under way, its start (real seconds) and the camera's start */
	bool bArrivalDone = false;
	bool bArriving = false;
	double ArrivalStart = -1.0;
	double WorldStart = 0.0;
	FTransform ArrivalFrom;
	FVector ArrivalTo = FVector::ZeroVector;
	bool bLocal = false;

	/** The cut to the player's eyes after the glide: under way, and its start (real seconds) */
	bool bCutting = false;
	double CutStart = 0.0;

	/** A galleon spawner is in the map: the crew will be put on its deck, the arrival waits for it in the black */
	bool bAwaitGalleon = false;
	bool bHoldingBlack = false;

	/** Smoothed length of a frame (real s), the island settled and when (real seconds) */
	float SmoothedFrameSeconds = 0.1f;
	bool bSettled = false;
	double SettledAt = 0.0;

	/** The execution: under way, faded out before the cut, on the scene's camera, faded out at the end */
	bool bExecuting = false;
	bool bExecutionFadedOut = false;
	bool bExecutionOnScene = false;
	bool bExecutionEndFade = false;
};
