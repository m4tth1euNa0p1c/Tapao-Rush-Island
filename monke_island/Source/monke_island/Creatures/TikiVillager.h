// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiCreature.h"
#include "Stealth/StealthMath.h"
#include "TikiVillager.generated.h"

class AVillageSpawner;
class UFlagCeremonyRules;
struct FVillageHouseInfo;
enum class ETikiActivity : uint8;

/** What a villager is doing (server) */
UENUM(BlueprintType)
enum class ETikiVillagerState : uint8
{
	/** Standing still between two walks */
	Pause,
	/** Walking to a random point of the village area */
	Wander,
	/** Walking to a coconut on the ground */
	GoToCoconut,
	/** Picking the coconut up */
	Collect,
	/** Running away from the player who hit it */
	Flee,
	/** Walking to a point of interest of its village (well, shrine, engawa...) */
	GoToActivity,
	/** Performing the activity there, facing the decor */
	Activity,
	/** Running to its house (gate of the plot, then the door) after a villager of its village was killed */
	GoHome,
	/** Hidden in its house until the alarm is over */
	Hidden,
	/** In its house: walking along its waypoints to the next point of its routine */
	HomeWalk,
	/** In its house: doing the activity of a waypoint (resting by the hearth, praying at the tokonoma...) */
	HomeActivity,
	/** A player came into its house: it follows him at a distance, watching that nothing is taken */
	Watch,
	/** Robbed or hit: it screams and runs out of its house and away (or backs off into a far room) */
	Panic,
	/** Walking back into its house by the gate of its plot and its door */
	ReturnHome,
	/** The flag ceremony of its village: it takes the relic of its house, stands in the ring around the officiant,
	 *  lays the relic around him once the flag is up, cheers and goes home */
	Ceremony,
	/** Heard a player (running steps, a voice) or lost sight of the one it watched (T-262): walks there, looks around,
	 *  and goes back to its routine when nobody is found */
	Investigate
};

/** Step of a villager in a flag ceremony (server) */
UENUM(BlueprintType)
enum class ETikiCeremonyStep : uint8
{
	None,
	/** Walking to the relic of its house */
	FetchRelic,
	/** Taking it */
	TakeRelic,
	/** On its way out of its house and to its place in the ring */
	ToRing,
	/** In the ring, facing the officiant */
	InRing,
	/** Walking to the spot where it lays the relic */
	ToOffer,
	/** Laying it */
	Offer,
	/** Cheering, before it goes home */
	Celebrate,
	/** The ceremony was interrupted: bringing the relic back home */
	BringBack
};

/**
 *  Worried villager (pnj-1), living in one village. While a player is near the village it follows a routine:
 *  activities at the points of interest of the village (draws water at the well, reads the notice board, prays at
 *  the shrine, rests in front of an engawa, chats on the square...), strolls, gathers the coconuts on the ground.
 *  Hit by a player, it raises the alarm: the whole village runs away and tiki warriors come out of the bushes.
 *  When a villager is killed, the others run into their houses (the gate of the plot, then the door) and hide there
 *  for a while.
 *  Killed by a player, tiki warriors also leap out of the bushes around that player.
 *  A villager of a house (SetResidence, one per house of a kit with residents, owner request 2026-10-07) lives inside:
 *  it walks along the waypoints of the kit from one point of its routine to the next. A player coming in is followed
 *  and watched from a distance (a straight line when it sees him, the waypoints around the walls otherwise); an
 *  object of the house carried out of it, or a blow, and it screams, panics and runs away, then comes back later.
 *  At the flag ceremony of its village (ATikiDirector::StartGathering), every villager leaves its routine and stands
 *  in a ring around the officiant, coming round the outside of the ring; a villager of a house brings the Tidal Relic
 *  of its house, held in front of it, and lays it around the officiant once the flag is up, then cheers and goes home;
 *  an interrupted ceremony sends it back home with its relic, put back in its place.
 *  Stealth (T-262): it sees a crouched player, or any player at night, only at contact; a crouched thief is not seen.
 *  In its routine it listens: a shot or a blast makes it run away from the shooter, running steps or a voice send it
 *  to look (a villager of a house only in or at its house), and back to its routine when nobody is found.
 */
UCLASS()
class MONKE_ISLAND_API ATikiVillager : public ATikiCreature
{
	GENERATED_BODY()

public:

	ATikiVillager();

	/** Centre and radius of the area the villager wanders in (server, set by the director) */
	void SetHome(const FVector& InCenter, float InRadius) { HomeCenter = InCenter; HomeRadius = InRadius; }

	/** Index of its village in the director (server) */
	void SetVillage(int32 InIndex) { VillageIndex = InIndex; }

	/** Index of its village in the director, INDEX_NONE when none (server) */
	int32 GetVillage() const { return VillageIndex; }

	/** Runs away from the threat for Duration seconds, leaving what it was doing (server) */
	void Alarm(AActor* InThreat, float Duration);

	/** Its house: the door it goes in by and the gate of the plot in front of it (server, set by the director) */
	void SetHouse(const FVector& InDoor, const FVector& InGate) { HouseDoor = InDoor; HouseGate = InGate; bHasHouse = true; }

	/** Runs into its house and hides there for Duration seconds; without a house, runs away from the threat (server) */
	void HideAtHome(AActor* InThreat, float Duration);

	/** Lives in this house of the spawner, along the waypoints of its kit (server, set by the director at spawn) */
	void SetResidence(AVillageSpawner* InSpawner, int32 InHouse);

	/** House of the spawner it lives in, INDEX_NONE for a villager of the square */
	int32 GetResidence() const { return ResidenceIndex; }

	/** Server: robbed or hit: it screams, panics and runs away from the threat */
	void Panic(AActor* InThreat);

	/** Server: the flag ceremony of its village starts: it leaves its routine (a villager of a house with the relic of
	 *  its house) and stands at SlotDegrees in the ring around Center, coming round the outside of the ring */
	void JoinCeremony(APawn* InOfficiant, const FVector& InCenter, const FVector& InCairn, float SlotDegrees, const UFlagCeremonyRules* InRules);

	/** Server: the ceremony is over: with the flag up it lays its relic around the centre and cheers, otherwise it
	 *  brings its relic back home; then it goes back to its routine */
	void EndCeremony(bool bRaised);

	/** True while it takes part in a flag ceremony (server) */
	bool IsInCeremony() const { return State == ETikiVillagerState::Ceremony; }

	/** True while a villager of a house is at home and following its routine or watching (server): it sees thefts */
	bool IsWatchingHouse() const;

	/** Relic of its house it carries, or nullptr */
	UFUNCTION(BlueprintPure, Category="Tiki")
	AActor* GetCarriedRelic() const { return CarriedRelic; }

	/** True while it hides in its house */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsHiding() const { return bHiding; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Current state (server) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	ETikiVillagerState GetState() const { return State; }

	/** Coconuts this villager gathered (server) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	int32 GetCollectedCount() const { return CollectedCount; }

protected:

	virtual void ServerThink(float DeltaSeconds) override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual void OnKilled(AActor* Killer) override;

	virtual float GetRulesHealth() const override;

	/** The villagers' network rates: lower, and relevant nearer (T-312) */
	virtual const FTikiNetRules* GetNetRules() const override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Picks the next goal: a free coconut nearby, else a random point of the area, else a pause */
	void ChooseNextGoal();

	/** Enters a state and records its start time */
	void SetState(ETikiVillagerState NewState);

	/** Frees the point of interest and the coconut it holds */
	void LeaveActivity();

	/** Plays the clip of the current activity (none for read and watch: it stands facing the decor) */
	void PlayActivityClip();

	/** Current state */
	ETikiVillagerState State = ETikiVillagerState::Pause;

	/** World time the state started */
	double StateStartTime = 0.0;

	/** Pause length drawn when the pause started (s) */
	float PauseDuration = 1.0f;

	/** Point the villager walks to */
	FVector GoalLocation = FVector::ZeroVector;

	/** Coconut the villager walks to */
	TWeakObjectPtr<AActor> GoalCoconut;

	/** Player the villager runs away from */
	TWeakObjectPtr<AActor> Threat;

	/** Centre of the wander area */
	FVector HomeCenter = FVector::ZeroVector;

	/** Radius of the wander area (cm) */
	float HomeRadius = 3000.0f;

	/** Coconuts gathered */
	int32 CollectedCount = 0;

	/** Its village in the director */
	int32 VillageIndex = INDEX_NONE;

	/** Point of interest held, INDEX_NONE when none */
	int32 PoiIndex = INDEX_NONE;

	/** Activity being performed */
	ETikiActivity CurrentActivity{};

	/** Time drawn for the current activity (s) */
	float ActivityDuration = 0.0f;

	/** Time it runs away for (s) */
	float FleeTime = 0.0f;

	/** Door and gate of its house */
	FVector HouseDoor = FVector::ZeroVector;
	FVector HouseGate = FVector::ZeroVector;

	/** It has a house in its village */
	bool bHasHouse = false;

	/** On its way home, it is through the gate (or did not need it) */
	bool bPassedGate = false;

	/** Time it stays hidden (s) */
	float HideTime = 0.0f;

	/** World time its startle ends: it runs only then */
	double StartleEndTime = 0.0;

	/** World time it last waved at a player */
	double LastGreetTime = -1.0e6;

	/** It already did something with this pause (looked around) */
	bool bPauseActed = false;

	/** Startled by an alarm: the clip and the scream, and it waits StartleTime before it runs */
	void BeStartled();

	/** Hidden in its house: invisible and without collision on every machine */
	UPROPERTY(ReplicatedUsing=OnRep_Hiding)
	bool bHiding = false;

	/** Applies the hiding on every machine */
	UFUNCTION()
	void OnRep_Hiding();

	/** Shows or hides the villager and its collision */
	void ApplyHiding();

	// ---------------------------------------------------------------- villager of a house

	/** Spawner of its house */
	TWeakObjectPtr<AVillageSpawner> Spawner;

	/** Its house in the spawner, INDEX_NONE for a villager of the square */
	int32 ResidenceIndex = INDEX_NONE;

	/** Waypoints it walks through, in order (world), and the next one */
	TArray<FVector> Route;
	int32 RouteStep = 0;

	/** Hops over an obstacle on the way to the current waypoint of the ceremony (server) */
	int32 StuckHops = 0;

	/** Waypoint of its house it walks to (its routine), INDEX_NONE when none */
	int32 TargetNode = INDEX_NONE;

	/** Player it watches in its house */
	TWeakObjectPtr<APawn> Watched;

	/** World time it last planned a way toward the watched player */
	double LastWatchPlan = -1.0e6;

	/** Waypoint nearest to the watched player at the last plan */
	int32 WatchGoalNode = INDEX_NONE;

	/** Once the player is out: it stands at its door watching until then (world time, < 0: not watching the door) */
	double WatchDoorUntil = -1.0;

	/** World time of its next scream while it panics */
	double NextScreamTime = 0.0;

	/** Backed off into a far room of its house instead of running out (the threat stood at the door) */
	bool bCornered = false;

	/** House of its villager, nullptr for a villager of the square or a spawner gone */
	const FVillageHouseInfo* GetResidenceInfo() const;

	/** Waypoint of its house in the world */
	FVector NodeWorld(int32 Node) const;

	/** True when Location lies in its house (grown by Margin) */
	bool IsInResidence(const FVector& Location, float Margin) const;

	/** Waypoint of its house nearest to Location (world), INDEX_NONE without waypoints */
	int32 NodeNear(const FVector& Location) const;

	/** Way to a waypoint of its house: along the waypoints when inside, by the gate, the door and the doorway first
	 *  when outside; false when its house has no waypoints */
	bool RouteToNode(int32 Goal);

	/** Way out of its house: the waypoints to the door, then the doorway, the door and the gate of its plot (nothing
	 *  to add when already outside) */
	void RouteOut();

	/** Walks along Route; true once the last waypoint is reached (or skipped when stuck) */
	bool FollowRoute(float Speed);

	/** Next point of its routine in its house, walked to */
	void ChooseHomeGoal();

	/** Watches this player who came into its house */
	void StartWatching(APawn* Player);

	/** A living player inside its house it sees (a crouched one only at contact, T-262), or nullptr */
	APawn* FindPlayerInResidence() const;

	/** True when nothing blocks its sight of the pawn */
	bool CanSee(const APawn* Pawn) const;

	/** Turns smoothly toward a point (server) */
	void TurnToward(const FVector& Target, float DeltaSeconds);

	/** Server think of the states of a villager of a house and of the ceremony */
	void ThinkHome(float DeltaSeconds, double Elapsed);
	void ThinkCeremony(float DeltaSeconds, double Elapsed);

	/** Back to its routine: into its house, or the square for a villager of the square */
	void GoBackToRoutine();

	// ---------------------------------------------------------------- stealth (T-262)

	/** What it looks for: a noise heard, the player it watched gone out of sight */
	FStealthLead Lead;

	/** World time it looks around again while it searches */
	double NextSearchTurnTime = 0.0;

	/** Where and when it last saw the player it watches */
	FVector WatchSeenAt = FVector::ZeroVector;
	double WatchSeenTime = 0.0;

	/** True in the states of its routine, where a noise may call it away */
	bool IsListeningState() const;

	/** Server, in its routine: listens; a shot or a blast makes it run away from the shooter, running steps or a voice
	 *  send it to look (a villager of a house only in or at its house); true when it reacted */
	bool ListenInRoutine();

	/** Goes to the lead: straight for a villager of the square, along the waypoints of its house otherwise */
	void StartInvestigating();

	/** Server think of the Investigate state: there, it looks around; a player seen is watched (in its house) or
	 *  greeted (on the square); nobody, and it goes back to its routine once it forgot */
	void ThinkInvestigate(double Elapsed);

	// ---------------------------------------------------------------- flag ceremony

	/** Officiant of the ceremony, centre of the ring, middle of the stones, its slot and the rules */
	TWeakObjectPtr<APawn> Officiant;
	FVector CeremonyCenter = FVector::ZeroVector;
	FVector CeremonyCairn = FVector::ZeroVector;
	float CeremonySlot = 0.0f;
	TWeakObjectPtr<const UFlagCeremonyRules> CeremonyRules;

	/** Where it is in the ceremony, since when */
	ETikiCeremonyStep CeremonyStep = ETikiCeremonyStep::None;
	double CeremonyStepTime = 0.0;

	/** The flag is up: it lays its relic as soon as it stands in the ring */
	bool bOfferRelic = false;

	/** Enters a step of the ceremony */
	void SetCeremonyStep(ETikiCeremonyStep Step);

	/** Way round the outside of the ring to its slot */
	void RouteToRing();

	/** Relic of its house carried in front of it (every machine attaches it the same way) */
	UPROPERTY(ReplicatedUsing=OnRep_CarriedRelic)
	TObjectPtr<AActor> CarriedRelic;

	UFUNCTION()
	void OnRep_CarriedRelic(AActor* Previous);

	/** Server: takes the relic of its house in its hands; false when the house has none any more */
	bool TakeRelic();

	/** Server: lays the relic down here (a gift: it no longer belongs to the house) */
	void LayRelic(const FVector& Spot, float Yaw);

	/** Server: puts the relic back where it stood in the house */
	void PutBackRelic();

	/** Server: lets go of the relic where it is (it panics, it dies) */
	void DropRelic();

	/** Holds or lets go of a relic on this machine: physics off and attached in front, or detached */
	void AttachRelic(AActor* Relic, bool bAttach) const;
};
