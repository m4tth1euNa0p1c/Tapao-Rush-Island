// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiCreature.h"
#include "TikiWarrior.generated.h"

/** Why a warrior is on the island (server) */
UENUM(BlueprintType)
enum class ETikiWarriorRole : uint8
{
	/** Came out at night, hunts the players nearby, goes back into the ground at dawn */
	Night,
	/** Called by a hit villager, hunts that player only */
	Defender
};

/**
 *  Tiki warrior (Guerrier Coco, Gardien Crabe, Veilleur des Cendres): leaps out of a bush, runs at its target and
 *  strikes in melee after a visible preparation (cahier §5). Server authoritative: the server moves it, sweeps
 *  the blow and applies the damage; every machine plays the clips.
 */
UCLASS()
class MONKE_ISLAND_API ATikiWarrior : public ATikiCreature
{
	GENERATED_BODY()

public:

	ATikiWarrior();

	/** WarriorRole and first target, set by the director on spawn (server) */
	void InitWarrior(ETikiWarriorRole InRole, APawn* InTarget, const FVector& InLeapDirection);

	/** WarriorRole of this warrior (server) */
	ETikiWarriorRole GetRole() const { return WarriorRole; }

	/** Current target (server) */
	APawn* GetTarget() const { return Target.Get(); }

	/** Goes back into the ground (server): at dawn, or when no player is left nearby */
	void Burrow();

protected:

	virtual void BeginPlay() override;

	virtual void ServerThink(float DeltaSeconds) override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual float GetRulesHealth() const override;

private:

	/** Server: sweeps in front and hits the players there */
	void StrikeNow();

	/** WarriorRole of this warrior */
	ETikiWarriorRole WarriorRole = ETikiWarriorRole::Night;

	/** Player hunted */
	TWeakObjectPtr<APawn> Target;

	/** Leap out of the bush, applied on the first think */
	FVector LeapDirection = FVector::ZeroVector;

	/** World time of the last blow start */
	double LastAttackTime = -1.0e6;

	/** World time the blow lands, 0 when no blow is pending */
	double PendingStrikeTime = 0.0;

	/** World time the warrior last had a target, for the burrow of an idle defender */
	double LastTargetTime = 0.0;

	/** Wander point of an idle warrior */
	FVector IdleGoal = FVector::ZeroVector;

	/** The warrior is going back into the ground */
	bool bBurrowing = false;
};
