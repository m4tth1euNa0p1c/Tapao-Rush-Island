// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiTactics.h"
#include "Creatures/TikiTacticsMath.h"
#include "TikiBulwarkTactics.generated.h"

/**
 *  Gardien Crabe (T-275): the tank. It comes at its foe sideways, behind a claw guard that blocks the blows from the
 *  front (the sabre, the spear, the club, the torch, the fist and the shots: a clang and sparks, a melee attacker pushed
 *  back); a blow from the side or the back gets through, and the heavy blows break the guard (a crack, it staggers, open
 *  a few seconds). It turns slowly behind its guard: a foe running around it reaches its side. In reach, its claw opens
 *  (the clacks and the clip announce it, a step aside avoids it) and seizes: damage, then the foe is thrown to the side,
 *  away from the ally it covers. It stands between its foe and a watcher or a wounded ally. Never more attackers at
 *  once than the pack allows, no blow on a player lying down.
 */
UCLASS()
class MONKE_ISLAND_API UTikiBulwarkTactics : public UTikiTactics
{
	GENERATED_BODY()

public:

	virtual bool ThinkBusy(double Now, float DeltaSeconds) override;

	virtual bool ThinkEngaged(APawn* Foe, float Distance, double Now, float DeltaSeconds) override;

	virtual void OnNoFoe() override;

	virtual bool FilterDamage(float& InOutDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual bool IsCommitted() const override;

	virtual float GetStrikeDamageScale() const override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** What it does with its foe */
	enum class EPhase : uint8
	{
		Guard,
		Grab,
		Recover,
		Broken
	};

	/** The claw opens toward Foe */
	void BeginGrab(APawn* Foe, double Now);

	/** The claw closes: the foe in front is seized and thrown */
	void CloseClaw(double Now);

	/** The seized foe flies away at Velocity (a ragdoll for a player) */
	void Throw(TWeakObjectPtr<APawn> Victim, FVector Velocity);

	/** Its guard breaks: it staggers, open */
	void BreakGuard(double Now);

	/** The ally its foe threatens most: a watcher, or a wounded ally; none when nobody needs it */
	ATikiWarrior* PickWard(const APawn* Foe) const;

	EPhase Phase = EPhase::Guard;
	double PhaseEnd = 0.0;

	/** World time the claw closes, and the next grab allowed */
	double SnapTime = 0.0;
	double NextGrab = 0.0;

	/** Side it comes at its foe from (+1 / -1), and when it changes */
	int32 Side = 1;
	double NextSideSwitch = 0.0;

	/** Blows its guard took lately: past a limit it breaks */
	TikiTactics::FGuardPoise Poise;

	/** Ally it covers, read again every ReplanSeconds */
	TWeakObjectPtr<ATikiWarrior> Ward;
	double NextReplan = 0.0;

	/** The throw after the hold */
	FTimerHandle ThrowTimer;
};
