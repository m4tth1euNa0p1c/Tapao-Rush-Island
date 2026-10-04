// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoatStartSubsystem.generated.h"

class ABoatActor;
class AController;
class AExpeditionArrivalPoint;
class AGameModeBase;
class APawn;
class APlayerController;
class UBoatStartRules;

/** Server: the boat start chose its arrival and the boat is on the water (fragment of the arrival, boat transform) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBoatArrivalResolved, FName /*FragmentId*/, const FTransform& /*BoatTransform*/);

/**
 *  Start in the boat (T-093, ADR-004): on the server, once the world has begun play, picks the arrival point
 *  of the fragment named by the ?Island= URL option (random and seeded without it), spawns the boat there on
 *  the water and seats every player as their pawn appears: the first one at the oars, the others on the free
 *  seats, standing aboard once the seats are taken. Each controller boards once: a respawn after death goes
 *  through the GameMode as before (PlayerStart). No arrival point, rules off or no water: nothing happens,
 *  the PlayerStart start is unchanged. Rules: DA_BoatStartRules (Config/DefaultGame.ini, StartRulesAsset).
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UBoatStartSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules of the start (DA_BoatStartRules); unset or missing: the class defaults */
	UPROPERTY(Config)
	TSoftObjectPtr<UBoatStartRules> StartRulesAsset;

	/** Returns the subsystem of the given world context, or nullptr */
	static UBoatStartSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	/**
	 *  Arrival given by the expedition data (mouillage Start of the island definition, T-087/T-094), used before
	 *  any AExpeditionArrivalPoint: priority override, then a point of the ?Island= fragment, then a random
	 *  point, else nothing. Boats/ stays independent from Expedition/: the caller injects the transform
	 *  (bow along +X). Call on the server before the first tick of the world (Initialize / OnWorldBeginPlay);
	 *  returns false, ignored, once the start is decided.
	 */
	bool SetArrivalOverride(const FTransform& BoatTransform, FName FragmentId);

	/** Removes the arrival override (before the start is decided) */
	void ClearArrivalOverride();

	/** True once the start has been decided (boat spawned or not); always true on clients */
	bool HasResolved() const { return bResolved; }

	/** Boat spawned at the arrival point, or nullptr */
	ABoatActor* GetArrivalBoat() const { return ArrivalBoat.Get(); }

	/** Fragment of the arrival point used, NAME_None when the start was not in the boat */
	FName GetArrivalFragmentId() const { return ArrivalFragmentId; }

	/**
	 *  Server, once: the arrival is resolved and the boat spawned there (T-141). Other domains (the expedition game state)
	 *  listen to learn the island really played without Boats/ depending on them. Not fired when the start is not in
	 *  the boat. Prefer CallOrRegisterOnArrivalResolved, which also serves a listener that comes after the event.
	 */
	FOnBoatArrivalResolved OnArrivalResolved;

	/** Calls Delegate now when the arrival is already resolved with a boat, else registers it on OnArrivalResolved */
	FDelegateHandle CallOrRegisterOnArrivalResolved(FOnBoatArrivalResolved::FDelegate&& Delegate);

private:

	friend class FBoatStartSpec;

	/** Records the resolved arrival and fires OnArrivalResolved, once */
	void NotifyArrivalResolved(FName FragmentId, const FTransform& BoatTransform);

	/** Transform of the boat at the resolved arrival */
	FTransform ArrivalBoatTransform = FTransform::Identity;

	/** OnArrivalResolved fired */
	bool bArrivalNotified = false;

	/** Chooses the point, spawns the boat and boards the players already there (server, once) */
	void Resolve();

	/** Arrival transform and fragment: the override, else an AExpeditionArrivalPoint; false when none */
	bool ChooseArrival(FTransform& OutPointTransform, FName& OutFragmentId, FString& OutSource) const;

	/** Spawns the boat on the water at the arrival and boards the players there; false when it cannot */
	bool SpawnArrivalBoat(const FTransform& PointTransform, FName FragmentId, const FString& Source);

	/** URL options of the world as the GameMode received them ("?Island=...?Seed=...") */
	FString GetWorldOptions() const;

	/** Deep enough water at the point or backwards from it (away from the island); false when none */
	bool FindWaterSpot(const FTransform& PointTransform, FTransform& OutBoatTransform) const;

	/** A player logged in (static GameMode event, filtered on this world) */
	void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	/** Boards the controller as soon as it has a pawn */
	void WatchController(AController* Controller);

	/** Stops watching the pawns of a controller */
	void StopWatching(AController* Controller);

	/** Seats (or stands) the pawn of a controller in the arrival boat, once per controller */
	void BoardController(AController* Controller);

	/** Rules in use */
	UPROPERTY(Transient)
	TObjectPtr<const UBoatStartRules> Rules;

	/** Boat spawned at the arrival point */
	TWeakObjectPtr<ABoatActor> ArrivalBoat;

	/** Fragment of the arrival point used */
	FName ArrivalFragmentId;

	/** Arrival injected by SetArrivalOverride */
	TOptional<FTransform> ArrivalOverride;

	/** Fragment of the injected arrival */
	FName ArrivalOverrideFragmentId;

	/** Controllers already boarded (or skipped): never boarded again, so a respawn is untouched */
	TSet<TWeakObjectPtr<AController>> HandledControllers;

	/** New pawn notifications of the controllers waiting for their pawn */
	TMap<TWeakObjectPtr<AController>, FDelegateHandle> PawnWatches;

	/** Handle of the PostLogin event */
	FDelegateHandle PostLoginHandle;

	/** Timer of Resolve (next tick after BeginPlay: the sea level and the actors are ready) */
	FTimerHandle ResolveTimer;

	/** Players standing aboard so far (picks the next standing spot) */
	int32 StandingCount = 0;

	/** The start has been decided */
	bool bResolved = false;
};
