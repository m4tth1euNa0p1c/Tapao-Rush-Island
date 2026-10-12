// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiTactics.h"
#include "TikiStalkerTactics.generated.h"

/**
 *  Guerrier Coco (T-275): the stalker of the night. It circles its foe at a distance, facing it, closer when not looked
 *  at, farther when looked at, its breath and clicks heard nearby; several stalkers take their places around the foe,
 *  the first behind it. Then a feint (a false start and a step back) or straight a charge, ending in a heavy blow whose
 *  growl and slow preparation leave time to step aside; after it, open a moment, it slips away and stalks again. Never
 *  more attackers at once than the pack allows, no blow on a player lying down. Wounded with no other warrior near, it
 *  runs away to a bush (and hides there for good, a guardian only waits there); caught, it turns and fights. A lit
 *  torch keeps it at a distance a while (the courage comes back, or with enough of the pack).
 */
UCLASS()
class MONKE_ISLAND_API UTikiStalkerTactics : public UTikiTactics
{
	GENERATED_BODY()

public:

	virtual bool ThinkBusy(double Now, float DeltaSeconds) override;

	virtual bool ThinkEngaged(APawn* Foe, float Distance, double Now, float DeltaSeconds) override;

	virtual void OnEngaged(APawn* Foe) override;

	virtual void OnNoFoe() override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual bool IsCommitted() const override;

private:

	/** What it does with its foe */
	enum class EPhase : uint8
	{
		Stalk,
		Encircle,
		Feint,
		Charge,
		Heavy,
		Recover,
		Fade,
		Flee,
		Cower
	};

	/** Starts a phase now for Duration seconds */
	void Enter(EPhase InPhase, double Now, double Duration);

	/** Circles its foe for a drawn time (half of it when bShort: back from a blow) */
	void BeginStalk(double Now, bool bShort);

	/** A false start at Foe, then a step back */
	void BeginFeint(APawn* Foe, double Now);

	/** Runs at its foe for the heavy blow */
	void BeginCharge(double Now);

	/** Stops, growls and prepares the heavy blow toward Foe */
	void BeginHeavy(APawn* Foe, double Now);

	/** Runs away from Causer (its foe) to a bush, or straight away without one */
	void BeginFlee(AActor* Causer, double Now);

	bool ThinkStalk(APawn* Foe, float Distance, double Now, float DeltaSeconds);
	bool ThinkEncircle(APawn* Foe, float Distance, double Now, float DeltaSeconds);
	bool ThinkFeint(double Now, float DeltaSeconds);
	bool ThinkFlee(double Now);
	bool ThinkCower(double Now, float DeltaSeconds);

	/** Reads the pack and the torch of its foe again, at most every ReplanSeconds */
	void Replan(const APawn* Foe, double Now);

	EPhase Phase = EPhase::Stalk;
	double PhaseStart = 0.0;
	double PhaseEnd = 0.0;

	/** World time the heavy blow lands */
	double StrikeTime = 0.0;

	/** Way it circles (+1 / -1) and when it changes */
	int32 OrbitSide = 1;
	double NextOrbitSwitch = 0.0;

	/** World time its foe stopped looking at it, negative while looked at */
	double UnwatchedSince = -1.0;

	/** The pack around its foe, as last read */
	FTikiPackView Pack;
	double NextReplan = 0.0;

	/** Its foe lies on the ground this think */
	bool bFoeDowned = false;

	/** A lit torch holds it back, at this distance; since when the fear started (negative: no torch) */
	bool bTorchFear = false;
	float TorchFearDistance = 0.0f;
	double TorchHeldSince = -1.0;

	/** Where it runs away to, and whether that is a bush */
	FVector FleeGoal = FVector::ZeroVector;
	bool bFleeToBush = false;
	double NextFleeAllowed = 0.0;

	/** Foe of its last think, to stalk a new one from the start */
	TWeakObjectPtr<APawn> LastFoe;
};
