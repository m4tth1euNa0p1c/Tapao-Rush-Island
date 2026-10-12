// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GiantCrabTypes.generated.h"

/** What the giant crab is doing */
UENUM(BlueprintType)
enum class EGiantCrabState : uint8
{
	Idle,
	Approach,
	Attack,
	Throw,
	Taunt,
	Block,
	HitReact,
	Stagger,
	Flipped,
	Recover,
	Dead,
	/** Roar of a new phase (T-261): it takes no damage, a shock wave throws back those around it */
	Roar,
	/** Before a charge: it faces its prey and paws the sand */
	ChargeWindup,
	/** Charge across its arena, through everything in the way */
	Charge,
	/** Drops flat on the sand onto whoever stands under its shell */
	BellySlam
};

/** Its blows */
UENUM(BlueprintType)
enum class EGiantCrabAttack : uint8
{
	None,
	Swipe,
	Slam,
	Grab,
	/** Legs stamped on both flanks (from phase 2, T-261) */
	Stomp,
	/** Charge across the arena (from phase 2): drawn like a blow, carried out by the ChargeWindup and Charge states */
	Charge
};

/** Part of the crab a blow lands on: each has its multiplier (T-261) */
UENUM(BlueprintType)
enum class EGiantCrabZone : uint8
{
	/** Carapace, and anything that is not one of the others (body, antennae) */
	Shell,
	/** Walking legs */
	Leg,
	/** Claws: arm, hand and pincers (the hard chitin) */
	Claw,
	/** Claw joints (forearm, the elbow): weak point */
	Joint,
	/** Eyes and their stalks: weak point */
	Eye,
	/** Abdomen under the body: weak point */
	Belly,
	/** Mouth and mandibles: weak point */
	Mouth
};

/** Moments of the fight every machine plays (sound, camera shake, sand) */
UENUM(BlueprintType)
enum class EGiantCrabCue : uint8
{
	None,
	/** A light leg tip on the sand */
	Step,
	/** A hind leg on the sand */
	HeavyStep,
	/** Claw slam on the sand */
	Slam,
	/** Leg stamped on a flank */
	Stomp,
	/** Body dropped flat on the sand */
	BellySlam,
	/** Roar of a new phase */
	Roar,
	/** Shock wave of the roar */
	RoarWave,
	/** Screech at the start of a charge */
	Charge,
	/** A blow of the crab lands on someone */
	Hit,
	/** Claws clacked together */
	Clack,
	/** Hiss while it hunts */
	Hiss,
	/** Growl while it waits */
	Growl,
	/** Death cry */
	Death,
	/** The body hits the sand (turned over, dead) */
	BodyFall
};

/** A moment of an action of the crab (T-261): when it comes (s after the action began, play rate applied), the cue
 *  every machine plays, the flank of a stomp. The server does its damage at the same times */
struct FGiantCrabMoment
{
	float Time = 0.0f;
	EGiantCrabCue Cue = EGiantCrabCue::None;
	/** Flank of a stomp: -1 left, +1 right, 0 none */
	int8 Side = 0;
};
