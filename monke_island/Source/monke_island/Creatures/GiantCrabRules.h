// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/GiantCrabTypes.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Templates/SubclassOf.h"
#include "GiantCrabRules.generated.h"

class UAnimSequenceBase;
class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;

/** One blow of the giant crab: its clip, when it hurts and how */
USTRUCT(BlueprintType)
struct FGiantCrabAttackRules
{
	GENERATED_BODY()

	/** Clip of the blow (A_GiantCrab_Attack_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimSequenceBase> Clip;

	/** Damage to a living thing struck in front of the crab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float Damage = 25.0f;

	/** The blow starts when the target is this close to the crab's edge, and strikes this far (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm"))
	float ReachCm = 320.0f;

	/** Half angle of the struck cone in front of the crab (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float ConeDegrees = 70.0f;

	/** Window of the clip where it hurts (s from the start at play rate 1, notifies AN_HitWindow_Begin / End of the kit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float HitStart = 0.33f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float HitEnd = 0.8f;

	/** Lunge of the body during the blow (the kit's root motion, played by the movement: cm over the window below) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm"))
	float LungeCm = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float LungeStart = 0.33f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float LungeEnd = 0.8f;

	/** Speed the struck one is thrown back with, and up (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackUp = 250.0f;

	/** Shock wave around the impact (Slam, Stomp): radius (cm, 0 = none) and damage to everything inside */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm"))
	float ShockwaveRadiusCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float ShockwaveDamage = 15.0f;

	/** Chance of this blow among those in reach (relative weights; the stomp and the charge take theirs from the phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float Weight = 1.0f;

	/** Seconds before the crab strikes again after this blow (times the RecoveryScale of the phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float Recovery = 1.2f;

	/** The blow waits for the target to be in front of the claws (StrikeAngleDegrees); off for the stomp, which hits
	 *  the flanks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	bool bNeedsFacing = true;
};

/** A weak point of the shell: a socket of SK_GiantCrab, how close a hit must land and what it does */
USTRUCT(BlueprintType)
struct FGiantCrabWeakPoint
{
	GENERATED_BODY()

	/** Socket of the mesh (WeakPoint_Eye_L, WeakPoint_Belly...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point")
	FName Socket;

	/** Bones whose hit counts here (the bone of the hit body struck, or the bone a shot passes nearest to) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point")
	TArray<FName> Bones;

	/** A hit within this distance of the socket counts here (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point", meta=(ClampMin="0", Units="cm"))
	float RadiusCm = 30.0f;

	/** Damage multiplier of a hit here */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point", meta=(ClampMin="0"))
	float DamageScale = 2.0f;

	/** One hit of a firearm here turns the crab over at once (eyes, belly, kit brief) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point")
	bool bShotFlips = false;
};

/** One hit body of the crab (T-261): a capsule along its bone or a box, carried by the bone so that it follows the
 *  animation. The kit's PHAT hint (PHAT_GiantCrab_hint.json) gives the centres and sizes in the mesh's space at rest */
USTRUCT(BlueprintType)
struct FGiantCrabHitBody
{
	GENERATED_BODY()

	/** Bone it follows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body")
	FName Bone;

	/** A box (shell, belly, claw hands) rather than a capsule along the bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body")
	bool bBox = false;

	/** Centre in the mesh's space at rest (cm, the hint's center_cm_component) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body", meta=(Units="cm"))
	FVector CenterCm = FVector::ZeroVector;

	/** Capsule: its axis in the bone's space (the kit's bones run along their Y axis) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body")
	FVector BoneAxis = FVector(0.0f, 1.0f, 0.0f);

	/** Capsule: radius, and length of its straight part along the axis (cm; a length of 0 makes a sphere) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body", meta=(ClampMin="1", Units="cm"))
	float RadiusCm = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body", meta=(ClampMin="0", Units="cm"))
	float LengthCm = 60.0f;

	/** Box: size along the mesh's axes at rest (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body", meta=(Units="cm"))
	FVector BoxSizeCm = FVector(50.0f);

	/** What a blow there counts as */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Body")
	EGiantCrabZone Zone = EGiantCrabZone::Shell;
};

/** An upright pillar that stops the characters around the crab (T-328): where a limb stands at rest, from under the sand
 *  to above the tallest character. Upright and taller than anyone, it can only push a character aside, never down onto
 *  the sand: the bodies at head height did ("is stuck and failed to move", normal Z -0.98) */
USTRUCT(BlueprintType)
struct FGiantCrabPawnPillar
{
	GENERATED_BODY()

	/** Its name: the component is PawnPillar_<Name> (the "Component:" of the journal's "is stuck" lines) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawn Pillar")
	FName Name;

	/** Centre on the ground in the mesh's space at rest (cm, the X and Y of the hit bodies: the kit faces +Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawn Pillar", meta=(Units="cm"))
	FVector2D CenterCm = FVector2D::ZeroVector;

	/** Radius (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawn Pillar", meta=(ClampMin="5", Units="cm"))
	float RadiusCm = 25.0f;
};

/** The characters around the crab (T-328): their moves ignore its animated bodies, its pillars stop them, and the server
 *  frees one pinned in its shapes */
USTRUCT(BlueprintType)
struct FGiantCrabPawnRules
{
	GENERATED_BODY()

	/** The characters around it are looked at this often (s), this close to its centre (cm): past the reach of its limbs
	 *  by what a sprint and its scuttle cover between two looks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="0.05", Units="s"))
	float CheckSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="100", Units="cm"))
	float CheckRadiusCm = 1000.0f;

	/** Every machine: their moves ignore its animated bodies (AIslanderCharacter::AnimatedHitBodyMaskFilter added to their
	 *  capsule), its pillars stop them; the bodies stay for the blows and the shots. Off: the bodies stop whoever does not
	 *  ignore them already (the islanders of a network session) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns")
	bool bIgnoreAnimatedBodies = true;

	/** Server: frees a character pressed at least PinDepthCm into a shape that stops it for PinnedSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns")
	bool bFreePinned = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="0.5", Units="cm"))
	float PinDepthCm = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="0", Units="s"))
	float PinnedSeconds = 1.0f;

	/** Freed: first just out of the shape, level, its depth plus this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="0", Units="cm"))
	float NudgeMarginCm = 10.0f;

	/** ... else out of its footprint: its edge this far from the crab's centre (cm, past the leg tips), straight away from
	 *  the crab, else turned by AngleStepDegrees each side in turn, inside the arena */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="0", Units="cm"))
	float ClearRadiusCm = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="5", ClampMax="180", Units="deg"))
	float AngleStepDegrees = 30.0f;

	/** The ground of a spot is looked for from the character's head down to this far below its feet (cm); walkable, or
	 *  the spot is skipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pawns", meta=(ClampMin="10", Units="cm"))
	float GroundSearchCm = 150.0f;
};

/** A blow chained at once after another (T-261: swipe then slam in phase 2, grab after grab in phase 3) */
USTRUCT(BlueprintType)
struct FGiantCrabComboLink
{
	GENERATED_BODY()

	/** Blow just finished (Grab: the throw after a catch, or a grab that caught nobody) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combo")
	EGiantCrabAttack From = EGiantCrabAttack::Swipe;

	/** Blow that follows without a pause */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combo")
	EGiantCrabAttack To = EGiantCrabAttack::Slam;
};

/** One phase of the boss (T-261, owner 2026-10-09: "boss Dark Souls imbattable, avec des phases") */
USTRUCT(BlueprintType)
struct FGiantCrabPhaseRules
{
	GENERATED_BODY()

	/** The phase begins once the health fraction is at or below this (the first phase applies from full health) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase", meta=(ClampMin="0", ClampMax="1"))
	float HealthFraction = 1.0f;

	/** Roars when the phase begins: no damage meanwhile, shock wave, camera shake (Roar of the rules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
	bool bRoar = false;

	/** Frenzy: the eyes glow red (EyeMaterialSlots, EyeSockets of the rules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
	bool bFrenzy = false;

	/** Multipliers of its walking, scuttling and charging speeds, and of its turn rate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Speed", meta=(ClampMin="0.1"))
	float MoveSpeedScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Speed", meta=(ClampMin="0.1"))
	float TurnRateScale = 1.0f;

	/** Play rate of the clips of its blows, grab, throw, stomp, slam and charge wind-up: the wind-ups and the hit
	 *  windows come this much sooner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Speed", meta=(ClampMin="0.25", ClampMax="4"))
	float AttackPlayRate = 1.0f;

	/** Multiplier of the pause after a blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Speed", meta=(ClampMin="0"))
	float RecoveryScale = 1.0f;

	/** Multiplier of the damage of its blows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0"))
	float DamageScale = 1.0f;

	/** Resistance: share of the damage it takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0"))
	float DamageTakenScale = 1.0f;

	/** A blow counts for at most this before the multipliers (0: IncomingDamageCap of the rules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0"))
	float DamageCap = 0.0f;

	/** Chance to raise the claws against a blow from the front (negative: BlockChance of the rules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defence", meta=(ClampMax="1"))
	float BlockChance = -1.0f;

	/** Weights of the stomp and of the charge among its blows (0: never in this phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blows", meta=(ClampMin="0"))
	float StompWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blows", meta=(ClampMin="0"))
	float ChargeWeight = 0.0f;

	/** Chance that a blow chains at once into the next one of Combos */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blows", meta=(ClampMin="0", ClampMax="1"))
	float ComboChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blows")
	TArray<FGiantCrabComboLink> Combos;

	/** Seconds between two belly slams */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blows", meta=(ClampMin="0", Units="s"))
	float BellySlamCooldown = 4.0f;
};

/** Belly slam (T-261, owner: "lorsqu'on est sous lui il s'écrase contre le sol") */
USTRUCT(BlueprintType)
struct FGiantCrabBellySlamRules
{
	GENERATED_BODY()

	/** Clip (A_GiantCrab_BellySlam of T-260); none: the claw slam stands in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam")
	TObjectPtr<UAnimSequenceBase> Clip;

	/** Notify of the clip where the body hits the sand, and the time used without it (s at play rate 1: the impact of
	 *  the claw slam, AN_GroundPound_Impact) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam")
	FName ImpactNotify = TEXT("AN_BellySlam_Impact");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="s"))
	float FallbackImpactSeconds = 1.033f;

	/** A prey under the shell this long makes it drop (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="s"))
	float TriggerSeconds = 0.4f;

	/** Footprint of the shell on the ground: ellipse centre (crab frame, X ahead, Y right) and radii (cm), and how far
	 *  above or below its ground the feet of the one under it may be (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam")
	FVector2D FootprintCenterCm = FVector2D(25.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam")
	FVector2D FootprintRadiiCm = FVector2D(110.0f, 125.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="cm"))
	float MaxHeightCm = 120.0f;

	/** Damage to those under it (crushed, ragdoll for the players), and the speed they are thrown out with (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0"))
	float Damage = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="cm/s"))
	float CrushOutSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="cm/s"))
	float CrushUpSpeed = 250.0f;

	/** Shock wave around it: radius from its centre (cm), damage, push (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="cm"))
	float WaveRadiusCm = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0"))
	float WaveDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="cm/s"))
	float WaveKnockbackSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Belly Slam", meta=(ClampMin="0", Units="cm/s"))
	float WaveKnockbackUp = 300.0f;
};

/** Roar of a new phase (T-261) */
USTRUCT(BlueprintType)
struct FGiantCrabRoarRules
{
	GENERATED_BODY()

	/** Clip (A_GiantCrab_Roar of T-260); none: the claw clack stands in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar")
	TObjectPtr<UAnimSequenceBase> Clip;

	/** Notify of the clip where the shock wave leaves, and the time used without it (s at play rate 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar")
	FName ImpactNotify = TEXT("AN_Roar");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0", Units="s"))
	float FallbackImpactSeconds = 0.6f;

	/** The roar lasts at least this long, whatever the clip (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0", Units="s"))
	float MinSeconds = 2.2f;

	/** Takes no damage while it roars */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar")
	bool bInvulnerable = true;

	/** Shock wave: radius (cm), damage, push (cm/s); players closer than KnockoutRadiusCm fall in a ragdoll */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0", Units="cm"))
	float ShockwaveRadiusCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0"))
	float ShockwaveDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackUp = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roar", meta=(ClampMin="0", Units="cm"))
	float KnockoutRadiusCm = 350.0f;
};

/** Charge across the arena (T-261, from phase 2) */
USTRUCT(BlueprintType)
struct FGiantCrabChargeRules
{
	GENERATED_BODY()

	/** Clip while it paws the sand before (none: the roar, else the claw clack) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge")
	TObjectPtr<UAnimSequenceBase> WindupClip;

	/** Loops of the run, sideways like a crab: to its left (A_GiantCrab_Charge of T-260) and to its right
	 *  (A_GiantCrab_Charge_R); none: the sideways walks. Ground speed they are authored for (cm/s, 450 in v2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge")
	TObjectPtr<UAnimSequenceBase> Clip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge")
	TObjectPtr<UAnimSequenceBase> ClipRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="1", Units="cm/s"))
	float ClipSpeedCmps = 450.0f;

	/** Wind-up before the run (s, divided by the AttackPlayRate of the phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="s"))
	float WindupSeconds = 0.9f;

	/** Speed of the run (cm/s, times the MoveSpeedScale of the phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm/s"))
	float SpeedCmps = 1100.0f;

	/** Charges at a prey between these distances (cm, between the bodies) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm"))
	float MinDistanceCm = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm"))
	float MaxDistanceCm = 2600.0f;

	/** Runs on this far past where its prey stood (cm), for at most MaxSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm"))
	float OvershootCm = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0.1", Units="s"))
	float MaxSeconds = 3.0f;

	/** What it does to those in its way: damage, push ahead and up (cm/s), a ragdoll for the players */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0"))
	float Damage = 45.0f;

	/** Prey this close to its edge, within this half angle of its run, are struck (cm, deg): it runs sideways, its legs
	 *  reach some 2 m past its body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm"))
	float HitReachCm = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float HitConeDegrees = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackSpeed = 1300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackUp = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge")
	bool bKnockoutPlayers = true;

	/** It stands still, out of breath, this long after the run (s): the opening */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="s"))
	float RecoverySeconds = 1.2f;

	/** Seconds between two charges */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0", Units="s"))
	float Cooldown = 7.0f;
};

/** A moment every machine plays where it happens: a sound drawn among variations, a camera shake for the local
 *  players around, sand thrown up (T-261: "rends le crabe ultra terrifiant") */
USTRUCT(BlueprintType)
struct FGiantCrabCueRules
{
	GENERATED_BODY()

	/** Variations, one drawn at random each time (none: silent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue")
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue", meta=(ClampMin="0"))
	float Volume = 1.0f;

	/** Random pitch around 1 (+/-) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue", meta=(ClampMin="0", ClampMax="0.5"))
	float PitchJitter = 0.06f;

	/** Shake of the cameras of the local players (none: still): ShakeScale within the inner radius, fading to 0 at the
	 *  outer one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue")
	TSubclassOf<UCameraShakeBase> Shake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue", meta=(ClampMin="0"))
	float ShakeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue", meta=(ClampMin="0", Units="cm"))
	float ShakeInnerRadiusCm = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue", meta=(ClampMin="0", Units="cm"))
	float ShakeOuterRadiusCm = 2500.0f;

	/** Sand thrown up (DustSystem of the rules) at this scale (0: none) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cue", meta=(ClampMin="0"))
	float DustScale = 0.0f;
};

/** A leg whose tip makes a step sound on the sand (T-261, owner: "les bruits de pas des pattes du crabe géant") */
USTRUCT(BlueprintType)
struct FGiantCrabFoot
{
	GENERATED_BODY()

	/** Last bone of the leg (the dactyl: its tip touches the sand) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foot")
	FName Bone;

	/** A hind leg: the heavy step */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foot")
	bool bHeavy = false;

	/** Its tip from the bone (bone space, cm, the bone's rotation only: the kit's bones carry a scale of 100); v2: the
	 *  end of the dactyl, 84.6 / 92 / 92 / 78.2 cm along it for the pairs 1 to 4 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foot", meta=(Units="cm"))
	FVector TipOffsetCm = FVector(0.0f, -85.0f, 0.0f);
};

/**
 *  Tunable rules of the giant crab "Tupa Nui" (T-211, owner request 2026-10-07, kit crabe-geant of the second PC):
 *  it attacks every living thing in its arena (players, villagers, tiki warriors) and never leaves the circle.
 *  Values from the kit's GiantCrabManifest.json (gameplay, clips, notifies at 30 fps), tuned in DA_GiantCrabRules.
 *  Boss v2 (T-261, owner 2026-10-09): phases, hit bodies on its bones, belly slam, stomp, charge, roar, footsteps of
 *  the legs and the cues that make it terrifying (docs/features/crabe-geant.md, "Boss v2").
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UGiantCrabRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UGiantCrabRules();

	/** Health of the crab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="1"))
	float MaxHealth = 1500.0f;

	/** A blow counts for at most this much before the multipliers below (0 = no cap): the musket's 10 000 would end
	 *  the fight at the first shot. A phase may set its own (FGiantCrabPhaseRules::DamageCap) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="0"))
	float IncomingDamageCap = 150.0f;

	/** Share of a blow the shell lets through, and the legs (kit: musket balls glance off the shell, aim for the eyes,
	 *  the joints or the belly) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="0"))
	float ShellDamageScale = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="0"))
	float LegDamageScale = 0.5f;

	/** Share of a blow on a claw: arm, hand and pincers, the hard chitin (the elbow is a weak point) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="0"))
	float ClawDamageScale = 0.25f;

	/** Bones whose name holds this are legs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health")
	FString LegBoneToken = TEXT("leg");

	/** Takes no damage at all (tests, scripted fights); off: "quasi imbattable", never invincible */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health")
	bool bInvulnerable = false;

	/** Share of the pushes of the blows it takes (cutlass, shots): 0, an 850 kg boss does not budge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="0"))
	float KnockbackTakenScale = 0.0f;

	// ---------------------------------------------------------------- boss (T-261)

	/** Name on the boss bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss")
	FText BossName;

	/** Shows the boss bar at the bottom of the screen of the players it fights (else the bar above its shell) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss")
	bool bBossBar = true;

	/** A player inside its arena, or this close to it without one, is engaged while it fights (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0", Units="cm"))
	float BossBarRadiusCm = 3000.0f;

	/** The bar stays this long once it stops fighting, and after its death (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0", Units="s"))
	float BossBarLingerSeconds = 5.0f;

	/** Phases by health (the first applies from full health): speeds, wind-ups, recoveries, resistances, blows */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss")
	TArray<FGiantCrabPhaseRules> Phases;

	// ---------------------------------------------------------------- hit bodies (T-261)

	/** Bodies carried by its bones: the players bump into them and every part can be struck (kit PHAT hint) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies")
	TArray<FGiantCrabHitBody> HitBodies;

	/** The crab stops the players and the other characters: no walking through its legs and claws (the ground under its
	 *  shell, between the legs, stays free). T-328: by its upright pillars (PawnPillars), fixed on its root, the same on
	 *  every machine, alone or in a network session; the moves of the characters around it ignore its animated bodies
	 *  (Pawns), which pressed them down onto the sand from above. The bodies still block the Pawn channel: the second
	 *  look of the firearms finds them there (FirearmDamage::TraceHitBodies) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies")
	bool bHitBodiesBlockPawns = true;

	/** Upright pillars that stop the characters (T-328): the leg tips and the claws where they stand at rest. They
	 *  replace the fixed copies of every body of T-305, whose claws, legs and shell at head height pinned the characters */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies")
	TArray<FGiantCrabPawnPillar> PawnPillars;

	/** Bottom and top of the pillars above the sand of the mesh (cm): far under the ground and above the tallest
	 *  character (a tiki 1.35 m, a jump), so that a push out of them is always level */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies", meta=(Units="cm"))
	float PawnPillarBottomCm = -150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies", meta=(Units="cm"))
	float PawnPillarTopCm = 260.0f;

	/** The characters around it: their moves ignore its animated bodies, the pinned ones are freed (T-328) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pawns")
	FGiantCrabPawnRules Pawns;

	/** Trace channel of the shots of the firearms (FirearmRules / MusketRules TraceChannel: Camera today) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies")
	TEnumAsByte<ECollisionChannel> ShotTraceChannel = ECC_Camera;

	/** The legs and claws stop the shots too. Leave it off while the shots trace on the Camera channel: the third
	 *  person camera would bump into the moving legs. On once the weapons trace on a channel of their own */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies")
	bool bLimbsBlockShots = false;

	/** A blow that names neither a body nor a bone counts for the body nearest its point, this close (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Bodies", meta=(ClampMin="0", Units="cm"))
	float ZoneSnapDistanceCm = 80.0f;

	// ---------------------------------------------------------------- perception (T-261 with T-262)

	/** Sees its prey this far (UStealthSubsystem::CanSee: crouched, or at night, only at contact; T-207 bush rule) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="cm"))
	float SightRangeCm = 3000.0f;

	/** Multiplier of the radius of the noises it hears (UStealthSubsystem::FindHeardNoise) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0"))
	float HearingScale = 1.0f;

	/** It feels whatever touches its legs, seen or not: this close to its edge (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="cm"))
	float TouchDistanceCm = 160.0f;

	/** It keeps hunting a prey it no longer senses for this long (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="s"))
	float TargetMemorySeconds = 3.0f;

	/** Without an arena around it: the crab only goes after targets this close to its starting point (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="100", Units="cm"))
	float LeashRadiusCm = 2500.0f;

	/** Seconds between two choices of target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0.05", Units="s"))
	float RetargetInterval = 0.5f;

	/** A new target must be this much closer than the current one to take its place (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="cm"))
	float SwitchMarginCm = 300.0f;

	/** Without the stealth subsystem only: a player crouched in a bush (this close to a bush of the island) is unseen
	 *  by the crab (T-207, owner: "accroupi dans un buisson, le joueur est invisible pour eux"); 0: always seen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="cm"))
	float BushHideRadiusCm = 150.0f;

	/** ... unless the crab is this close to the player: it bumps into it (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="cm"))
	float BushSpotDistanceCm = 300.0f;

	/** Kept this far inside the edge of the arena (cm, on top of its own radius) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", Units="cm"))
	float EdgeMarginCm = 100.0f;

	// ---------------------------------------------------------------- movement

	/** Sideways walk (the crab's way), forward walk (cm/s, times the MoveSpeedScale of the phase) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0", Units="cm/s"))
	float SideSpeed = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0", Units="cm/s"))
	float ForwardSpeed = 100.0f;

	/** Prey farther than this (cm, between the bodies): the crab turns its side to it and scuttles there sideways, the
	 *  fast gait of a crab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0", Units="cm"))
	float ScuttleDistanceCm = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0", Units="cm/s"))
	float ScuttleSpeed = 480.0f;

	/** Speed the crab turns to face its target (deg/s, times the TurnRateScale of the phase) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1", Units="deg"))
	float TurnRateDegrees = 140.0f;

	/** Acceleration of its steps (cm/s2) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="100"))
	float MaxAcceleration = 3000.0f;

	/** Fastest play rate of the walk clips, which follow the body's speed (beyond it the feet slide) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1", ClampMax="8"))
	float MaxLocomotionPlayRate = 4.5f;

	/** Strikes only when the target is within this angle of its front (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float StrikeAngleDegrees = 25.0f;

	/** The crab circles its target sideways at this share of its side speed while it closes in */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0", ClampMax="1"))
	float CircleShare = 0.35f;

	// ---------------------------------------------------------------- blows

	/** Blows: claw swipe (left), claw slam with a shock wave (right), grab then throw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabAttackRules Swipe;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabAttackRules Slam;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabAttackRules Grab;

	/** Stomp of both flanks (A_GiantCrab_LegStomp of T-260; none: the hit reaction stands in): shock wave at each flank
	 *  (ShockwaveRadiusCm, ShockwaveDamage), no struck cone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabAttackRules Stomp;

	/** Notifies of the stomp clip (left, right flank), and the times used without them (s at play rate 1, flanks taken
	 *  in turn from the left) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FName StompNotifyLeft = TEXT("AN_Stomp_L");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FName StompNotifyRight = TEXT("AN_Stomp_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	TArray<float> StompImpactTimes;

	/** The flank waves are centred this far to its side (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm"))
	float StompSideOffsetCm = 170.0f;

	/** Charge across the arena */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabChargeRules Charge;

	/** Belly slam on whoever stands under its shell */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabBellySlamRules BellySlam;

	/** Roar of a new phase */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGiantCrabRoarRules Roar;

	/** A combo goes on only when its prey is within this share of the next blow's reach */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="1"))
	float ComboReachSlack = 1.4f;

	/** Throw after a grab that caught someone (clip, when the claw opens at play rate 1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimSequenceBase> ThrowClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float ThrowRelease = 0.5f;

	/** Speed of the throw (6 m/s at 35 deg in the kit) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm/s"))
	float ThrowSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", ClampMax="89", Units="deg"))
	float ThrowAngleDegrees = 35.0f;

	/** Speed of the throw of a player (cm/s): thrown much farther than a tiki, knocked out into a ragdoll for the whole
	 *  flight under real gravity (owner 2026-10-08: thrown "very far"). 24 m/s at 38 deg carry about 55 m on flat sand
	 *  without drag, less with the damping of the physics asset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="cm/s"))
	float ThrowSpeedPlayer = 2400.0f;

	/** Angle of the throw of a player above the ground (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", ClampMax="89", Units="deg"))
	float ThrowAngleDegreesPlayer = 38.0f;

	/** Damage of the throw to the one thrown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float ThrowDamage = 20.0f;

	/** After a slam the claw stays stuck in the sand (kit AN_ClawStuck_Begin / End, s at play rate 1): a blow on this
	 *  weak point meanwhile turns the crab over */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float SlamStuckStart = 1.033f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0", Units="s"))
	float SlamStuckEnd = 1.467f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FName SlamStuckWeakPoint = TEXT("WeakPoint_ClawJoint_R");

	/** Shortest and longest pause without a target before a claw clack (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Idle", meta=(ClampMin="1", Units="s"))
	float TauntIntervalMin = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Idle", meta=(ClampMin="1", Units="s"))
	float TauntIntervalMax = 14.0f;

	/** Blows from the front within this half angle may be blocked (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defence", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float FrontConeDegrees = 55.0f;

	/** Chance to raise the claws after a blow from the front (a phase may set its own), and how long they stay up (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defence", meta=(ClampMin="0", ClampMax="1"))
	float BlockChance = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defence", meta=(ClampMin="0", Units="s"))
	float BlockSeconds = 1.6f;

	/** Share of the damage a front blow does through raised claws (kit: -90 %) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defence", meta=(ClampMin="0", ClampMax="1"))
	float BlockedDamageScale = 0.1f;

	/** A blow at least this strong staggers it (s of reaction below; none while it strikes) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defence", meta=(ClampMin="0"))
	float HitReactDamage = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defence", meta=(ClampMin="0", Units="s"))
	float HitReactCooldown = 4.0f;

	/** Weak points (eyes, claw joints, belly, mouth) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weak Points")
	TArray<FGiantCrabWeakPoint> WeakPoints;

	/** Hits on weak points within the window that turn it over (kit: 3 in 6 s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weak Points", meta=(ClampMin="1"))
	int32 FlipHits = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weak Points", meta=(ClampMin="0", Units="s"))
	float FlipWindow = 6.0f;

	/** Seconds on its back, belly offered, and the damage multiplier meanwhile */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weak Points", meta=(ClampMin="0", Units="s"))
	float FlippedSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weak Points", meta=(ClampMin="0"))
	float FlippedDamageScale = 2.0f;

	/** Clips of the kit (A_GiantCrab_*, 30 fps) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Idle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> WalkSideLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> WalkSideRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> WalkForward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Taunt;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> BlockStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> BlockLoop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> BlockEnd;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> HitReact;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> StaggerFlip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> FlippedLoop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Recover;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Death;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> DeathFlipped;

	/** Crossfade between two clips (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims", meta=(ClampMin="0", Units="s"))
	float BlendSeconds = 0.2f;

	// ---------------------------------------------------------------- footsteps (T-261)

	/** Legs whose tip makes a step on the sand (the dactyls; the hind ones heavier) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps")
	TArray<FGiantCrabFoot> Feet;

	/** Notifies of the clips named <prefix><bone> (AN_Step_leg_l1_dactyl, T-260) play the step of that foot; a clip
	 *  without them gives its steps by the height of the tips */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps")
	FString StepNotifyPrefix = TEXT("AN_Step_");

	/** A tip lifted this high above its lowest point arms the step, which sounds when it comes back down this low (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps", meta=(ClampMin="0", Units="cm"))
	float StepLiftCm = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps", meta=(ClampMin="0", Units="cm"))
	float StepContactCm = 2.5f;

	/** The steps are worked out only when a local camera is this close (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps", meta=(ClampMin="0", Units="cm"))
	float StepAudibleDistanceCm = 4500.0f;

	/** At most this many steps a second, all feet together (a scuttle at full speed would give 40) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps", meta=(ClampMin="1"))
	float MaxStepsPerSecond = 16.0f;

	/** At most one camera shake of the heavy steps this often (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footsteps", meta=(ClampMin="0", Units="s"))
	float StepShakeInterval = 0.3f;

	// ---------------------------------------------------------------- cues (T-261)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules StepCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules HeavyStepCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules SlamCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules StompCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules BellySlamCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules RoarCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules RoarWaveCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules ChargeCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules HitCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules ClackCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules HissCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules GrowlCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules DeathCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FGiantCrabCueRules BodyFallCue;

	/** Notifies of the clips that play a cue (AN_Claw_Snap: a clack, AN_BodyFall: the body on the sand) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	TMap<FName, EGiantCrabCue> NotifyCues;

	/** Sand thrown up by the cues that have a DustScale (an effect of the project: NS_Firearm_Impact) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	TObjectPtr<UNiagaraSystem> DustSystem;

	/** Pause between two hisses while it hunts, two growls while it waits (s, drawn between X and Y) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FVector2D HissInterval = FVector2D(4.0f, 8.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cues")
	FVector2D GrowlInterval = FVector2D(8.0f, 15.0f);

	/** Sounds of the kit binding (optional, older rules): a blow lands, the claws clack, the shell hits the sand; used
	 *  when HitCue, ClackCue or SlamCue has no sound */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> ClackSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> SlamSound;

	// ---------------------------------------------------------------- frenzy look (T-261)

	/** Material slots of the eyes (M_GiantCrab_Eyes of T-260), their vector parameters given FrenzyEyeColor in a
	 *  frenzy, and scalar parameters set then (EmissiveStrength: the glow) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	TArray<FName> EyeMaterialSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	TArray<FName> FrenzyEyeColorParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	FLinearColor FrenzyEyeColor = FLinearColor(1.0f, 0.04f, 0.02f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	TMap<FName, float> FrenzyEyeScalarParams;

	/** Scalar parameter of the eye material that clouds the eyes (0 alive, 1 milky), set at the notify of the death
	 *  clip (AN_DeadEyes), else this long after the death (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	FName DeadEyesParam = TEXT("DeadEyes");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	FName DeadEyesNotify = TEXT("AN_DeadEyes");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy", meta=(ClampMin="0", Units="s"))
	float DeadEyesFallbackSeconds = 1.3f;

	/** Red lights at these sockets in a frenzy: intensity (cd, 0: none) and radius (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy")
	TArray<FName> EyeSockets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy", meta=(ClampMin="0"))
	float FrenzyEyeLightIntensity = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frenzy", meta=(ClampMin="0", Units="cm"))
	float FrenzyEyeLightRadiusCm = 300.0f;

	// ---------------------------------------------------------------- network (T-305, review T-268 R-25)

	/** A distant player's shot or blow is judged on the weak points (and on the claw stuck after a slam) where that
	 *  player saw them: at most this far back (s; 0: the server's pose only). It bounds what a high ping gains */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", ClampMax="1", Units="s"))
	float LagCompensationMaxSeconds = 0.3f;

	/** How much later than its ping a client shows the crab: the smoothing of its movement (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", ClampMax="0.5", Units="s"))
	float LagViewDelaySeconds = 0.05f;

	/** The weak points are looked for this long on each side of the moment the player saw them (s): the ping wavers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", ClampMax="0.5", Units="s"))
	float LagWindowSeconds = 0.05f;

	/** The server records where its weak points are this often (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.01", ClampMax="0.2", Units="s"))
	float WeakPointSampleSeconds = 0.033f;

	/** Rules of a phase, nullptr out of range (no phases: none) */
	const FGiantCrabPhaseRules* GetPhase(int32 Index) const;

	/** Health fractions where the phases begin, in their order */
	void GetPhaseThresholds(TArray<float>& OutThresholds) const;

	/** Rules of a cue, nullptr for None */
	const FGiantCrabCueRules* GetCue(EGiantCrabCue Cue) const;
};
