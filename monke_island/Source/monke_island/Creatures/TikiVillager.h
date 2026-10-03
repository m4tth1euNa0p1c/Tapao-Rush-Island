// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiCreature.h"
#include "TikiVillager.generated.h"

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
	Flee
};

/**
 *  Worried villager (pnj-1): wanders in the middle of the island around its director, walks to the coconuts on
 *  the ground and gathers them. Hit by a player, it runs away and the director sends a tiki warrior out of the
 *  nearest bush against that player.
 */
UCLASS()
class MONKE_ISLAND_API ATikiVillager : public ATikiCreature
{
	GENERATED_BODY()

public:

	ATikiVillager();

	/** Centre and radius of the area the villager wanders in (server, set by the director) */
	void SetHome(const FVector& InCenter, float InRadius) { HomeCenter = InCenter; HomeRadius = InRadius; }

	/** Current state (server) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	ETikiVillagerState GetState() const { return State; }

	/** Coconuts this villager gathered (server) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	int32 GetCollectedCount() const { return CollectedCount; }

protected:

	virtual void ServerThink(float DeltaSeconds) override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual float GetRulesHealth() const override;

private:

	/** Picks the next goal: a free coconut nearby, else a random point of the area, else a pause */
	void ChooseNextGoal();

	/** Enters a state and records its start time */
	void SetState(ETikiVillagerState NewState);

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
};
