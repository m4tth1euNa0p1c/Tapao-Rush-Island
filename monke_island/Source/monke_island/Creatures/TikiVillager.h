// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiCreature.h"
#include "TikiVillager.generated.h"

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
	Hidden
};

/**
 *  Worried villager (pnj-1), living in one village. While a player is near the village it follows a routine:
 *  activities at the points of interest of the village (draws water at the well, reads the notice board, prays at
 *  the shrine, rests in front of an engawa, chats on the square...), strolls, gathers the coconuts on the ground.
 *  Hit by a player, it raises the alarm: the whole village runs away and tiki warriors come out of the bushes.
 *  When a villager is killed, the others run into their houses (the gate of the plot, then the door) and hide there
 *  for a while.
 *  Killed by a player, tiki warriors also leap out of the bushes around that player.
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
};
