// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiTactics.h"
#include "TikiWatcherTactics.generated.h"

/**
 *  Veilleur des Cendres (T-275): the caller and the blinder. When it engages a foe it howls (far heard): the warriors
 *  around come to look where it saw the foe, and a monster hidden in a bush nearby may leap out; they attack only if they
 *  then see the foe. It keeps the foe at a distance, behind a bush when there is one, and throws a lump of ash on an
 *  arc (a long breath and a gesture announce it, it lands where it was aimed: a step aside avoids it): the cloud blinds
 *  the players inside (a veil on their screen). A foe coming close makes it back away facing it, leaving embers between
 *  them that burn; closer still it turns and runs; caught, it strikes with its club. Never more attackers at once than
 *  the pack allows, no throw at a player lying down.
 */
UCLASS()
class MONKE_ISLAND_API UTikiWatcherTactics : public UTikiTactics
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
		Keep,
		Howl,
		Cast
	};

	/** Howls at Foe and calls the others */
	void BeginHowl(APawn* Foe, double Now);

	/** The warriors around come to look, a hidden monster may leap out */
	void CallPack(APawn* Foe);

	/** Breathes the ash in, then throws it at Foe */
	void BeginCast(APawn* Foe, double Now);

	/** The lump leaves its hand toward where its foe will be */
	void ReleaseCloud();

	/** Embers on the ground between it and Foe */
	void DropEmbers(const APawn* Foe);

	/** Place it wants: at its distance from Foe, behind a bush when there is one */
	FVector PickCover(const APawn* Foe) const;

	EPhase Phase = EPhase::Keep;
	double PhaseEnd = 0.0;

	/** Next howl, cloud and embers allowed */
	double NextHowl = 0.0;
	double NextCloud = 0.0;
	double NextEmbers = 0.0;

	/** Place it keeps, and when it looks for a new one */
	FVector CoverGoal = FVector::ZeroVector;
	bool bHasCover = false;
	double NextCoverReplan = 0.0;

	/** Way aside it takes for a moment when its retreat is blocked (a wall, the sea) */
	FVector Detour = FVector::ZeroVector;
	double DetourUntil = 0.0;
};
