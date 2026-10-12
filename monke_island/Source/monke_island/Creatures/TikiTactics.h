// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Creatures/TikiTacticsTypes.h"
#include "TikiTactics.generated.h"

class AActor;
class AController;
class APawn;
class ATikiDirector;
class ATikiWarrior;
class UTikiRules;
enum class ETikiAction : uint8;
struct FDamageEvent;

/** Tactics of the tiki warriors (T-275): phases, guard, howls, flights */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeTactics, Log, All);

/** The others of the pack around the same foe, as a warrior reads them (server, T-275) */
struct FTikiPackView
{
	/** Warriors hunting the same foe, this one included */
	int32 Hunters = 1;

	/** Hunters of the same role, this one included, and the rank of this one among them (0 = the first): its place */
	int32 SameRole = 1;
	int32 SameRoleRank = 0;

	/** Others attacking that foe now: a blow, a charge, a grab, a cast */
	int32 Attacking = 0;
};

/**
 *  Server brain of a warrior's role (T-275, owner request 2026-10-10: distinct roles, readable tactics, horror and never
 *  unfair). ATikiWarrior creates it at begin play from its archetype, on the server only, never replicated: the
 *  warrior replicates FTikiTacticRep for the clips and the sounds of every machine. The warrior keeps the target and the
 *  stealth (the foe seen, lost, heard: T-262), the leap out of the bush and the musket; the tactics decide how it fights
 *  the foe it hunts. No tick of its own: the warrior's think calls it. Subclasses: UTikiStalkerTactics (Guerrier Coco),
 *  UTikiBulwarkTactics (Gardien Crabe), UTikiWatcherTactics (Veilleur des Cendres).
 */
UCLASS(Abstract, ClassGroup=(monke_island))
class MONKE_ISLAND_API UTikiTactics : public UActorComponent
{
	GENERATED_BODY()

public:

	UTikiTactics();

	/** Binds the warrior it thinks for (server, once, before its first think) */
	void Bind(ATikiWarrior* InWarrior);

	/** Server, at each think before the target is kept or chosen: a move that runs to its end (a heavy blow, a grab,
	 *  a cast, a howl, a flight); true when it ran this think */
	virtual bool ThinkBusy(double Now, float DeltaSeconds) { return false; }

	/** Server: the warrior hunts Foe, Distance away (2D); true when the tactics moved it, false for the classic melee */
	virtual bool ThinkEngaged(APawn* Foe, float Distance, double Now, float DeltaSeconds) { return false; }

	/** Server: the warrior has a foe again after a time without one (spotted, hit by it, called against it) */
	virtual void OnEngaged(APawn* Foe) {}

	/** Server: the warrior has no foe at this think */
	virtual void OnNoFoe();

	/** Server: a blow reaches the warrior; true when its defence took part (the guard), InOutDamage then what is left */
	virtual bool FilterDamage(float& InOutDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) { return false; }

	/** Server: the warrior took Damage (after its defence) from Causer */
	virtual void OnDamaged(AActor* Causer, float Damage) {}

	/** True while it attacks its foe (a charge, a heavy blow, a grab, a cast): the pack counts it */
	virtual bool IsCommitted() const { return false; }

	/** Damage of its classic blow relative to UTikiRules::AttackDamage */
	virtual float GetStrikeDamageScale() const { return 1.0f; }

protected:

	/** Warrior it thinks for */
	ATikiWarrior* GetWarrior() const { return Warrior; }

	/** Rules of the warrior (set before begin play by the director, or on the level instance) */
	const UTikiRules* GetRules() const;

	/** World time (s) */
	double GetNow() const;

	/** The warrior walks or runs toward a point; the distance left (2D, cm) */
	float MoveToward(const FVector& Target, float Speed);

	/** The warrior stops */
	void StopMoving();

	/** The warrior turns toward a point at once */
	void FaceToward(const FVector& Target);

	/** The warrior turns toward a point at RateDeg per second at most (the pack's StrafeTurnRate when 0) */
	void TurnToward(const FVector& Target, float DeltaSeconds, float RateDeg = 0.0f);

	/** Replicates what the tactics do; bStrafe: the body faces its foe while it moves (no turn to its movement) */
	void SetState(ETikiTacticState State, bool bStrafe, bool bGuardUp);

	/** Plays an action on every machine */
	void PlayAction(ETikiAction Action, uint8 Variant = 0);

	/** Plays a cue at a place on every machine (the clang of a blocked blow, the claw closing) */
	void PlayCue(ETikiTacticCue Cue, const FVector& Location);

	/** True when it did not move 30 cm toward its goal in the last 2 s */
	bool UpdateStuck(bool bWantsToMove);

	/** True when it sees its foe at this think (the stealth of the warrior, T-262) */
	bool SeesFoe() const;

	/** Where its foe is: there when seen, else where it was seen or heard last */
	FVector KnownFoeLocation(const APawn* Foe) const;

	/** Where its foe looks from and toward (its camera for a player): the stalker closes in when not looked at */
	void FoeView(const APawn* Foe, FVector& OutLocation, FVector& OutDirection) const;

	/** The others of the pack around Foe, read now */
	FTikiPackView ReadPack(const APawn* Foe) const;

	/** Other living warriors within Radius (2D), those running away left out */
	int32 CountAlliesNear(float Radius) const;

	/** True when one more attacker may commit to Foe now (UTikiRules::Pack.MaxAttackersPerFoe), read now */
	bool TryTakeAttackToken(const APawn* Foe) const;

	/** True when Foe lies on the ground (a player knocked out or thrown) and the rules spare it */
	bool IsFoeDowned(const APawn* Foe) const;

	/** Share of its health the warrior has left */
	float GetHealthFraction() const;

	/** Director of the island, none in a level without one */
	ATikiDirector* GetDirector() const;

	/** Bush nearest to Location within Radius (the bush grid of the director); false when none or no director */
	bool FindBush(const FVector& Location, float Radius, FVector& OutBush) const;

	/** Ground under a point (a trace down from Above cm over it, Depth cm under it); the point itself when none */
	FVector GroundUnder(const FVector& Point, float Above = 200.0f, float Depth = 600.0f) const;

	/** Server: the classic blow of the warrior at Victim (its preparation, then the sweep) */
	void BeginStrike(APawn* Victim, double Now);

	/** True when the classic blow may start now (its cooldown is over) */
	bool CanStartStrike(double Now) const;

	/** Server: a sweep in front of the warrior with these values; the foes struck */
	int32 StrikeWith(float Reach, float Radius, float Damage, float Knockback);

private:

	/** Warrior it thinks for */
	UPROPERTY(Transient)
	TObjectPtr<ATikiWarrior> Warrior;
};
