// monke_island

#pragma once

#include "CoreMinimal.h"
#include "PacificationTypes.generated.h"

class APawn;

/** States of a pacifiable zone (cahier §6.2) */
UENUM(BlueprintType)
enum class EZoneState : uint8
{
	/** No player has found the zone yet: no objective running */
	Undiscovered,
	/** Objective running: designated threats are fixed, the flag is not planted yet */
	Contested,
	/** Flag planted: a player can recite the sermon */
	Securing,
	/** Sermon validated by the server: bonus granted once, nothing more to farm */
	Pacified,
	/** The zone went under water: no more pacification */
	Submerged
};

/** Why a sermon stopped before the end (the progress is reset to zero in every case) */
UENUM(BlueprintType)
enum class ESermonInterruptReason : uint8
{
	/** Not interrupted */
	None,
	/** The officiant released the action */
	Released,
	/** The officiant moved (or was pushed) farther than the tolerance from the start point */
	Moved,
	/** The officiant took damage */
	Damaged,
	/** The officiant died */
	OfficiantDied,
	/** The officiant pawn was destroyed or its player disconnected */
	OfficiantLost,
	/** A living designated threat is inside the perimeter */
	ThreatInPerimeter,
	/** The zone went under water */
	ZoneSubmerged
};

/** Answer of the server to a flag or sermon request: lets the caller show why it was refused */
UENUM(BlueprintType)
enum class EPacificationRequestResult : uint8
{
	/** The request was applied */
	Accepted,
	/** The pawn is already the officiant: nothing changes, the progress is kept */
	AlreadyOfficiant,
	/** Called on a client: only the server decides */
	NotAuthority,
	/** The zone has no rules asset */
	NoRules,
	/** The zone is not in a state that allows this request */
	WrongState,
	/** The sermon needs a planted flag */
	FlagNotPlanted,
	/** Another player is already reciting the sermon at this flag */
	OfficiantBusy,
	/** The pawn is missing, dead or being destroyed */
	InvalidPawn,
	/** The pawn is too far from the flag socket */
	OutOfReach,
	/** Designated threats are still alive (plant) or one is inside the perimeter (sermon) */
	ThreatsRemaining,
	/** The flag could not be spawned */
	SpawnFailed
};

/**
 *  Server-side sermon in progress at one flag, replicated so every client shows who officiates and the progress.
 *  Holds no rule: FPacificationLogic decides what it means.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FSermonState
{
	GENERATED_BODY()

	/** Whether a sermon is running */
	UPROPERTY(BlueprintReadOnly, Category="Sermon")
	bool bActive = false;

	/** Player reciting the sermon (one per flag) */
	UPROPERTY(BlueprintReadOnly, Category="Sermon")
	TObjectPtr<APawn> Officiant = nullptr;

	/** Server time when the sermon started (s) */
	UPROPERTY(BlueprintReadOnly, Category="Sermon")
	double StartServerTime = 0.0;

	/** Officiant location when the sermon started: the reference of the immobility check (server only) */
	UPROPERTY(NotReplicated)
	FVector StartLocation = FVector::ZeroVector;

	/** Whether the officiant had a controller at the start: losing it means a disconnection (server only) */
	UPROPERTY(NotReplicated)
	bool bHadControllerAtStart = false;

	/** Starts a sermon from zero: no time is ever carried over from an earlier attempt */
	void Begin(APawn* InOfficiant, const FVector& InStartLocation, double InServerTime, bool bInHasController);

	/** Stops the sermon and forgets the officiant */
	void Reset();

	/** Seconds recited so far (0 when no sermon runs) */
	double ElapsedSeconds(double ServerTime) const;

	/** Share of the sermon recited so far (0-1) */
	float Progress01(double ServerTime, float Duration) const;

	/** Whether the sermon has lasted its full duration */
	bool IsComplete(double ServerTime, float Duration) const;
};

/** What the server observes about the officiant at one check, fed to FPacificationLogic::EvaluateSermon */
struct FSermonObservation
{
	/** Officiant pawn still exists */
	bool bOfficiantValid = true;

	/** Officiant still alive */
	bool bOfficiantAlive = true;

	/** Officiant still connected (still has its controller, when it had one) */
	bool bOfficiantConnected = true;

	/** Officiant still holds the action */
	bool bHolding = true;

	/** Officiant took damage since the last check */
	bool bDamaged = false;

	/** A living designated threat is inside the perimeter */
	bool bThreatInPerimeter = false;

	/** Zone is under water */
	bool bZoneSubmerged = false;

	/** Current officiant location */
	FVector Location = FVector::ZeroVector;
};

/** Pure pacification rules, no world needed: used by APacificationZone and the specs */
struct MONKE_ISLAND_API FPacificationLogic
{
	/** Whether Current stays within the horizontal and vertical tolerances around Start (looking around never moves the pawn) */
	static bool IsWithinTolerance(const FVector& Start, const FVector& Current, float HorizontalToleranceCm, float VerticalToleranceCm);

	/** First reason that stops the running sermon, or None when it may go on */
	static ESermonInterruptReason EvaluateSermon(const FSermonState& State, const FSermonObservation& Observation, float HorizontalToleranceCm, float VerticalToleranceCm);

	/** Whether the flag can be planted: objective running and every designated threat dead */
	static bool CanPlantFlag(EZoneState ZoneState, int32 LivingDesignatedThreats);

	/** Whether a zone in this state accepts a new sermon */
	static bool AcceptsSermon(EZoneState ZoneState, bool bFlagPlanted);
};
