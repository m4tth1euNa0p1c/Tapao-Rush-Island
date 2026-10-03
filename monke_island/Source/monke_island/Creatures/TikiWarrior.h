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
	Defender,
	/** Designated threat of a village (cahier §6.2): waits by the bush it came out of, hunts the players who come
	 *  close, walks back to its post; stays at dawn and is not replaced once killed */
	Guardian,
	/** Lay in wait in a bush of a bush zone: leaps out when a player comes near, hunts the players nearby around its
	 *  bush and goes back into it when nobody is left */
	Ambusher
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

	/** Hunts this player now (server): a guardian of an alarmed village */
	void Engage(APawn* InTarget);

	/** Place a guardian keeps when it has no target, and how far it wanders around it (server) */
	void SetPost(const FVector& InPost, float InRadius) { Post = InPost; PostRadius = InRadius; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void ServerThink(float DeltaSeconds) override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual float GetRulesHealth() const override;

	virtual UAnimSequenceBase* GetIdleClip() const override { return bInCombat && Anims.IdleCombat ? Anims.IdleCombat.Get() : Anims.Idle.Get(); }

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

	/** Post of a guardian (where it came out of its bush) */
	FVector Post = FVector::ZeroVector;

	/** Wander radius around the post, 0 when the warrior has no post */
	float PostRadius = 0.0f;

	/** The warrior is going back into the ground */
	bool bBurrowing = false;

	/** World time of its last growl at a player it spotted */
	double LastRoarTime = -1.0e6;

	/** World time of its last taunt */
	double LastTauntTime = -1.0e6;

	/** It has a target: it stands on guard (replicated for the clip) */
	UPROPERTY(Replicated)
	bool bInCombat = false;
};
