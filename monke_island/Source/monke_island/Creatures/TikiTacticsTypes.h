// monke_island

#pragma once

#include "CoreMinimal.h"
#include "TikiTacticsTypes.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/**
 *  Fighting role of a tiki warrior (T-275, owner request 2026-10-10: "rework tous les mécanismes"): each mob of the
 *  island has its own tactics, read from its class name unless a level instance forces one.
 */
UENUM(BlueprintType)
enum class ETikiArchetype : uint8
{
	/** Read from the class name (UTikiRules::Pack.Archetypes): BP_TikiCocoWarrior -> Stalker... */
	Auto,
	/** The behaviour before T-275: it runs at its foe and strikes */
	Brawler,
	/** Guerrier Coco: stalks from a distance, circles in a pack, feints then charges with a telegraphed heavy blow,
	 *  runs away when alone and wounded, keeps away from a lit torch */
	Stalker,
	/** Gardien Crabe: advances sideways behind a claw guard that blocks the blows from the front, seizes and throws,
	 *  stands between its foe and the other tiki */
	Bulwark,
	/** Veilleur des Cendres: keeps its distance behind a bush, blinds with a cloud of ash, leaves embers on the ground
	 *  as it backs away, howls to call the others */
	Watcher
};

/** What a warrior's tactics are doing, replicated for the clips and the sounds of every machine (T-275) */
UENUM(BlueprintType)
enum class ETikiTacticState : uint8
{
	None,
	/** Stalker: circles its foe at a distance, closer when it is not looked at */
	Stalk,
	/** Stalker: takes its place around the foe with the others of the pack */
	Encircle,
	/** Stalker: a false start, then a step back */
	Feint,
	/** Stalker: runs at its foe for the heavy blow */
	Charge,
	/** After a heavy blow or a grab: open, it stands a moment */
	Recover,
	/** Stalker: wounded and alone, it runs away to a bush */
	Flee,
	/** Stalker: hidden near its bush, it waits for its courage to come back */
	Cower,
	/** Bulwark: behind its claw guard */
	Guard,
	/** Bulwark: between its foe and an ally */
	Protect,
	/** Bulwark: the claw opens to seize */
	Grab,
	/** Bulwark: its guard broken, it staggers */
	Broken,
	/** Watcher: at its distance, behind a bush when there is one */
	Keep,
	/** Watcher: prepares a cloud of ash */
	Cast,
	/** Watcher: backs away from a foe who comes too close, leaving embers */
	Retreat,
	/** Watcher: howls to call the others */
	Howl
};

/** A cue of the tactics played at a place on every machine (ATikiWarrior::MulticastTacticCue, T-275) */
enum class ETikiTacticCue : uint8
{
	/** A blow blocked by the claw guard: the clang and the sparks */
	Block,
	/** The claw closing on a foe */
	Seize
};

/** Replicated state of the tactics (T-275): what it does, how it moves, whether its guard is up */
USTRUCT()
struct FTikiTacticRep
{
	GENERATED_BODY()

	/** What its tactics are doing */
	UPROPERTY()
	ETikiTacticState State = ETikiTacticState::None;

	/** It faces its foe while it moves (side steps, walk backward) */
	UPROPERTY()
	bool bStrafe = false;

	/** Its claw guard is up (Bulwark): the guard pose, the blows from the front blocked */
	UPROPERTY()
	bool bGuardUp = false;

	bool operator==(const FTikiTacticRep& Other) const
	{
		return State == Other.State && bStrafe == Other.bStrafe && bGuardUp == Other.bGuardUp;
	}

	bool operator!=(const FTikiTacticRep& Other) const
	{
		return !(*this == Other);
	}
};

/** A class name keyword and the role it gives (UTikiRules::Pack.Archetypes) */
USTRUCT(BlueprintType)
struct FTikiArchetypeMatch
{
	GENERATED_BODY()

	/** Part of the class name, case insensitive (CocoWarrior matches BP_TikiCocoWarrior_C) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactics")
	FString Keyword;

	/** Role of the classes whose name contains it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactics")
	ETikiArchetype Archetype = ETikiArchetype::Brawler;
};

/** Clips of a role (T-275): the mob anim library has them for each mob (A_<Mob>_Block, _Strafe_L/R, _Walk_Back), not
 *  bound in FTikiAnimSet; kept with the role because each role is one mob. A clip of another skeleton is never played */
USTRUCT(BlueprintType)
struct FTikiTacticClips
{
	GENERATED_BODY()

	/** Parry, one-shot (A_<Mob>_Block, 0.87 s): played each time the guard blocks a blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips")
	TObjectPtr<UAnimSequenceBase> Block;

	/** Side steps, looped (A_<Mob>_Strafe_L / _Strafe_R): it moves sideways, facing its foe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips")
	TObjectPtr<UAnimSequenceBase> StrafeLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips")
	TObjectPtr<UAnimSequenceBase> StrafeRight;

	/** Walking backward, looped (A_<Mob>_Walk_Back): it backs away, facing its foe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips")
	TObjectPtr<UAnimSequenceBase> WalkBack;

	/** Its signature move, one-shot: the heavy blow (Stalker), the grab (Bulwark), the throw of the cloud (Watcher);
	 *  empty: Anims.Attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips")
	TObjectPtr<UAnimSequenceBase> Signature;

	/** Speed the signature clip plays at: below 1 its preparation lasts longer and reads better */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips", meta=(ClampMin="0.2", ClampMax="2"))
	float SignatureRate = 1.0f;

	/** A feint (Stalker) or a howl (Watcher), one-shot; empty: Anims.Taunt, then Anims.Alert */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips")
	TObjectPtr<UAnimSequenceBase> Gesture;

	/** Speed the side steps do not slide at (cm/s, the animation manifest: 21 Coco, 20 Crab, 17 Ash); 0 = the walk of
	 *  the creature */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips", meta=(ClampMin="0", Units="cm/s"))
	float StrafeClipSpeed = 0.0f;

	/** Speed the walk backward does not slide at (cm/s: 35 Coco, 29 Crab, 22 Ash); 0 = the walk of the creature */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clips", meta=(ClampMin="0", Units="cm/s"))
	float WalkBackClipSpeed = 0.0f;
};

/** Sounds of a role (T-275), generated by scripts/audio/tiki_tactics_sounds.py; empty plays nothing */
USTRUCT(BlueprintType)
struct FTikiTacticSounds
{
	GENERATED_BODY()

	/** Spotting a foe, over the voice's growl: the clicks of the Coco, the clacks of the Crab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Alert;

	/** Now and then while it stalks (Stalker): breath and clicks, near the local camera only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Stalk;

	/** Start of its signature move: the growl of the heavy blow, the claws opening, the breath before the cloud */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Windup;

	/** A feint (Stalker) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Feint;

	/** Running away (Stalker) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Flee;

	/** A blow blocked by the guard (Bulwark), at the point of the blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Block;

	/** The guard broken (Bulwark) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> GuardBreak;

	/** The claw closing on a foe (Bulwark) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Seize;

	/** The howl that calls the others (Watcher), heard far */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> Howl;

	/** How far these sounds carry; none: the voice's (ATT_Tiki_Voice) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** How far the howl carries; none: Attenuation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundAttenuation> FarAttenuation;

	/** Volume of these sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0"))
	float Volume = 1.0f;

	/** Shortest and longest pause between two stalk sounds (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0.5", Units="s"))
	float StalkIntervalMin = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0.5", Units="s"))
	float StalkIntervalMax = 5.0f;

	/** Stalk sounds only within this distance of the local camera (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds", meta=(ClampMin="0", Units="cm"))
	float StalkSoundDistance = 2500.0f;
};

/** Rules shared by the roles (T-275): the pack, fairness, the torch, which class plays which role */
USTRUCT(BlueprintType)
struct FTikiPackRules
{
	GENERATED_BODY()

	/** Role of a warrior class: the first keyword its name contains; none: Brawler (the behaviour before T-275) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TArray<FTikiArchetypeMatch> Archetypes = {
		{ TEXT("CocoWarrior"), ETikiArchetype::Stalker },
		{ TEXT("CrabGuardian"), ETikiArchetype::Bulwark },
		{ TEXT("AshWatcher"), ETikiArchetype::Watcher } };

	/** Never more than this many warriors attack the same foe at once (blow, charge, grab); the others wait in their
	 *  place around it: tension, never a swarm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta=(ClampMin="1"))
	int32 MaxAttackersPerFoe = 2;

	/** The others hunting the same foe are counted this far (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta=(ClampMin="0", Units="cm"))
	float PackRadius = 3000.0f;

	/** Time between two looks at the pack and the ground around (s): the tactics replan at this pace */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta=(ClampMin="0.05", Units="s"))
	float ReplanSeconds = 0.3f;

	/** No blow starts on a player lying on the ground (knocked out, thrown): it waits around until the player is up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	bool bSpareDownedFoes = true;

	/** At night a standing player holding a lit torch is seen by the warriors (and the hidden monsters of the bushes)
	 *  up to TorchNightSightCm: the flame shows; crouched, the player stays hidden (T-262) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	bool bTorchRevealsAtNight = true;

	/** See bTorchRevealsAtNight (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch", meta=(ClampMin="0", Units="cm"))
	float TorchNightSightCm = 2500.0f;

	/** Turning speed of a warrior that faces its foe while it moves (deg/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta=(ClampMin="10", Units="DegreesPerSecond"))
	float StrafeTurnRate = 360.0f;
};

/** Guerrier Coco (T-275): stalks, circles, feints, charges with a telegraphed heavy blow, runs away alone and wounded */
USTRUCT(BlueprintType)
struct FTikiStalkerRules
{
	GENERATED_BODY()

	/** Health relative to UTikiRules::WarriorHealth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker", meta=(ClampMin="0.1"))
	float HealthScale = 1.0f;

	/** It circles its foe between these distances (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="100", Units="cm"))
	float StalkRadiusMin = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="100", Units="cm"))
	float StalkRadiusMax = 1100.0f;

	/** It stalks this long before it attacks, drawn between these (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0", Units="s"))
	float StalkSecondsMin = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0", Units="s"))
	float StalkSecondsMax = 8.0f;

	/** Speed while it circles (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0", Units="cm/s"))
	float StalkSpeed = 100.0f;

	/** Speed it closes in at while its foe does not look at it (cm/s); under about 170 the walk clip plays, not the run */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0", Units="cm/s"))
	float CreepSpeed = 160.0f;

	/** Its foe looks at it when it is within this angle of the foe's view (deg): it stops closing in, backs away */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="5", ClampMax="90", Units="Degrees"))
	float WatchedHalfAngle = 35.0f;

	/** Not looked at this long within FeintDistance, it charges from behind (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0", Units="s"))
	float UnwatchedRushSeconds = 2.5f;

	/** Each step around its foe turns this much (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="1", ClampMax="90", Units="Degrees"))
	float OrbitStepDegrees = 25.0f;

	/** It changes the way it circles every this many seconds, drawn between these (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0.5", Units="s"))
	float OrbitSwitchSecondsMin = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Stalk", meta=(ClampMin="0.5", Units="s"))
	float OrbitSwitchSecondsMax = 5.0f;

	/** With other stalkers on the same foe, each takes a place on this ring around it, the first behind it (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Encircle", meta=(ClampMin="100", Units="cm"))
	float EncircleRadius = 550.0f;

	/** It is in its place within this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Encircle", meta=(ClampMin="10", Units="cm"))
	float SlotReachedRadius = 150.0f;

	/** It attacks after this long even when not yet in its place (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Encircle", meta=(ClampMin="0", Units="s"))
	float EncircleMaxSeconds = 6.0f;

	/** Chance of a feint before a charge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Feint", meta=(ClampMin="0", ClampMax="1"))
	float FeintChance = 0.6f;

	/** A feint or a charge starts from this distance at most (cm); farther it closes in first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Feint", meta=(ClampMin="100", Units="cm"))
	float FeintDistance = 500.0f;

	/** The feint: a dash at this speed for this long, then a step back (cm/s, s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Feint", meta=(ClampMin="0", Units="cm/s"))
	float FeintDashSpeed = 560.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Feint", meta=(ClampMin="0", Units="s"))
	float FeintDashSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Feint", meta=(ClampMin="0", Units="s"))
	float FeintBackSeconds = 0.6f;

	/** The charge: speed, and how long before it gives up (cm/s, s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0", Units="cm/s"))
	float ChargeSpeed = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0.1", Units="s"))
	float ChargeMaxSeconds = 2.0f;

	/** The heavy blow starts from this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="50", Units="cm"))
	float HeavyRange = 230.0f;

	/** Its preparation, the growl and the slow clip that announce it (s): long enough to step aside */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0.2", Units="s"))
	float HeavyWindup = 0.85f;

	/** Reach and width of the heavy blow in front of it (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0", Units="cm"))
	float HeavyReach = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="1", Units="cm"))
	float HeavyRadius = 60.0f;

	/** Damage and push of the heavy blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0"))
	float HeavyDamage = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0", Units="cm/s"))
	float HeavyKnockback = 650.0f;

	/** After the heavy blow it stands open this long (s), then backs away to FadeDistance and stalks again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="0", Units="s"))
	float RecoverSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Charge", meta=(ClampMin="100", Units="cm"))
	float FadeDistance = 800.0f;

	/** It runs away under this share of its health when no other warrior is within FleeAllyRadius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0", ClampMax="1"))
	float FleeHealthFraction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0", Units="cm"))
	float FleeAllyRadius = 1500.0f;

	/** It runs to a bush within this distance, away from its foe; none: straight away (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0", Units="cm"))
	float FleeBushSearchRadius = 2500.0f;

	/** It gives up the flight after this long, and turns to fight when its foe is this close (s, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0.5", Units="s"))
	float FleeMaxSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0", Units="cm"))
	float CorneredDistance = 200.0f;

	/** At its bush a night warrior, an ambusher or a defender hides for good; a guardian waits this long, then comes
	 *  back (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0", Units="s"))
	float CowerSeconds = 12.0f;

	/** It does not run away again before this long (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Flee", meta=(ClampMin="0", Units="s"))
	float FleeCooldown = 25.0f;

	/** A lit torch in its foe's hand keeps it this far (cm), farther when raised */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Torch", meta=(ClampMin="0", Units="cm"))
	float TorchFearRadius = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Torch", meta=(ClampMin="0", Units="cm"))
	float TorchRaisedFearRadius = 600.0f;

	/** The fear lasts this long, or until this many stalkers hunt the foe together (s, count) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Torch", meta=(ClampMin="0", Units="s"))
	float TorchCourageSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker|Torch", meta=(ClampMin="1"))
	int32 TorchCouragePack = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker")
	FTikiTacticClips Clips;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stalker")
	FTikiTacticSounds Sounds;
};

/** Gardien Crabe (T-275): claw guard in front, sideways approach, seize and throw, stands for the others */
USTRUCT(BlueprintType)
struct FTikiBulwarkRules
{
	GENERATED_BODY()

	/** Health relative to UTikiRules::WarriorHealth: the tank */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark", meta=(ClampMin="0.1"))
	float HealthScale = 1.6f;

	/** Damage of its claw blow relative to UTikiRules::AttackDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark", meta=(ClampMin="0"))
	float StrikeDamageScale = 1.2f;

	/** Speed behind its guard (cm/s); farther than GuardDistance from its foe it runs, guard down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", Units="cm/s"))
	float GuardMoveSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="100", Units="cm"))
	float GuardDistance = 900.0f;

	/** The guard covers this half angle in front of it (deg): a blow from the side or the back gets through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float GuardHalfArc = 70.0f;

	/** Share of a melee blow (sabre, spear, club, torch, fist) and of a shot the guard lets through: none, the claw
	 *  takes it all (a musket ball kills at once, so a share would mean nothing); the heavy blows break the guard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", ClampMax="1"))
	float GuardMeleeDamageScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", ClampMax="1"))
	float GuardShotDamageScale = 0.0f;

	/** Turning speed behind its guard (deg/s): slow enough that a foe running around it gets at its side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="10", Units="DegreesPerSecond"))
	float GuardTurnRate = 110.0f;

	/** A blocked melee attacker is pushed back at this speed (cm/s) when it stands within GuardPushRange (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", Units="cm/s"))
	float GuardPushback = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", Units="cm"))
	float GuardPushRange = 350.0f;

	/** The guard breaks when the blows it blocked reach this much damage within GuardBreakWindow, or at one blow of
	 *  GuardBreakBlow or more; it stays down GuardBrokenSeconds (the stagger clip): the heavy blows open it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="1"))
	float GuardBreakDamage = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0.1", Units="s"))
	float GuardBreakWindow = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="1"))
	float GuardBreakBlow = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0", Units="s"))
	float GuardBrokenSeconds = 3.0f;

	/** It comes at its foe turned this much to one side, then the other (deg, s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Approach", meta=(ClampMin="0", ClampMax="80", Units="Degrees"))
	float SideApproachAngle = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Approach", meta=(ClampMin="0.5", Units="s"))
	float SideSwitchSeconds = 2.5f;

	/** The grab starts from this distance (cm); the claw closes this far in front, within this half angle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="50", Units="cm"))
	float GrabRange = 210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="50", Units="cm"))
	float GrabReach = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="5", ClampMax="180", Units="Degrees"))
	float GrabHalfArc = 50.0f;

	/** The claw opens this long before it closes: the clacks and the clip announce it, a step aside avoids it (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0.2", Units="s"))
	float GrabWindup = 0.8f;

	/** Damage of the claw closing, then the throw: the seized foe held this long, then thrown (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0"))
	float GrabDamage = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0", Units="s"))
	float GrabHoldSeconds = 0.2f;

	/** The throw: to the side by this angle from its front, at this speed and upward speed (deg, cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float ThrowSideAngle = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0", Units="cm/s"))
	float ThrowSpeed = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0", Units="cm/s"))
	float ThrowUpSpeed = 420.0f;

	/** Time between two grabs, and how long it stands open after one (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0", Units="s"))
	float GrabCooldown = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Grab", meta=(ClampMin="0", Units="s"))
	float GrabRecoverSeconds = 0.9f;

	/** It looks after the other warriors within this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Protect", meta=(ClampMin="0", Units="cm"))
	float ProtectRadius = 1500.0f;

	/** An ally needs it when its foe is within this distance of it (cm): a watcher always, another ally under this
	 *  share of its health */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Protect", meta=(ClampMin="0", Units="cm"))
	float WardThreatRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Protect", meta=(ClampMin="0", ClampMax="1"))
	float WardWoundedFraction = 0.5f;

	/** It stands this far from the ally, toward the foe (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Protect", meta=(ClampMin="50", Units="cm"))
	float InterposeDistance = 250.0f;

	/** Sparks of a blow blocked by the guard, at the point of the blow; none: only the sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard")
	TSoftObjectPtr<UNiagaraSystem> BlockSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark|Guard", meta=(ClampMin="0.01"))
	float BlockSystemScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark")
	FTikiTacticClips Clips;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bulwark")
	FTikiTacticSounds Sounds;
};

/** Cloud of ash and embers of the Veilleur (T-275, ATikiAshBurst): values, look and sounds */
USTRUCT(BlueprintType)
struct FTikiAshBurstRules
{
	GENERATED_BODY()

	/** The lump of ash flies at this speed, on an arc this high (cm/s, cm); at least MinFlightSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="100", Units="cm/s"))
	float FlightSpeed = 1300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="0", Units="cm"))
	float ArcHeight = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="0.05", Units="s"))
	float MinFlightSeconds = 0.4f;

	/** Radius and height of the cloud (cm), and how long it stays (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="50", Units="cm"))
	float CloudRadius = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="50", Units="cm"))
	float CloudHeight = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="0.5", Units="s"))
	float CloudSeconds = 6.0f;

	/** A player in the cloud is blinded this long (a veil on the screen), again at most every BlindRepeatSeconds
	 *  while it stays inside (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="0.1", Units="s"))
	float BlindSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="0.1", Units="s"))
	float BlindRepeatSeconds = 1.5f;

	/** The veil: colour, opacity, fade in and out (s); the HUD stays readable over it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil")
	FLinearColor VeilColor = FLinearColor(0.07f, 0.06f, 0.055f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil", meta=(ClampMin="0", ClampMax="1"))
	float VeilOpacity = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil", meta=(ClampMin="0.01", Units="s"))
	float VeilFadeIn = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil", meta=(ClampMin="0.01", Units="s"))
	float VeilFadeOut = 1.2f;

	/** Radius of an ember patch (cm), how long it burns (s), the damage it deals per second to the foes standing in
	 *  it, checked every EmberTickSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Embers", meta=(ClampMin="20", Units="cm"))
	float EmberRadius = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Embers", meta=(ClampMin="0.5", Units="s"))
	float EmberSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Embers", meta=(ClampMin="0"))
	float EmberDamagePerSecond = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Embers", meta=(ClampMin="0.1", Units="s"))
	float EmberTickSeconds = 0.5f;

	/** The cloud is checked for players this often (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cloud", meta=(ClampMin="0.05", Units="s"))
	float PulseSeconds = 0.2f;

	/** The lump in flight: mesh (a 1 m sphere), material, scale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> LumpMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UMaterialInterface> LumpMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0.01"))
	float LumpScale = 0.25f;

	/** The cloud: a 1 m sphere scaled to its size with a translucent material, and an effect spawned again every
	 *  CloudEffectSeconds while it lasts; either may be empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> CloudMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UMaterialInterface> CloudMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UNiagaraSystem> CloudEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0.01"))
	float CloudEffectScale = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0.1", Units="s"))
	float CloudEffectSeconds = 0.8f;

	/** The embers: a 1 m cylinder flattened to the patch with a glowing material, and an effect spawned again every
	 *  EmberEffectSeconds; either may be empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> EmberMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UMaterialInterface> EmberMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0.1", Units="cm"))
	float EmberThickness = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UNiagaraSystem> EmberEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0.01"))
	float EmberEffectScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0.1", Units="s"))
	float EmberEffectSeconds = 0.7f;

	/** The cloud bursting, the embers crackling (at the place), and the ringing of a blinded player (local, 2D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> BurstSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> EmberSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TArray<TObjectPtr<USoundBase>> VeilSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundAttenuation> Attenuation;
};

/** Veilleur des Cendres (T-275): keeps its distance at cover, blinds, leaves embers, backs away, howls */
USTRUCT(BlueprintType)
struct FTikiWatcherRules
{
	GENERATED_BODY()

	/** Health relative to UTikiRules::WarriorHealth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher", meta=(ClampMin="0.1"))
	float HealthScale = 0.85f;

	/** It keeps its foe between these distances (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Keep", meta=(ClampMin="100", Units="cm"))
	float KeepDistanceMin = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Keep", meta=(ClampMin="100", Units="cm"))
	float KeepDistanceMax = 1600.0f;

	/** It looks for a bush this far around the place it wants, and stands this far behind it from its foe (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Keep", meta=(ClampMin="0", Units="cm"))
	float CoverSearchRadius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Keep", meta=(ClampMin="0", Units="cm"))
	float CoverOffset = 130.0f;

	/** It looks for a new place this often (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Keep", meta=(ClampMin="0.2", Units="s"))
	float CoverReplanSeconds = 2.0f;

	/** A foe closer than this makes it back away, facing it (cm, cm/s); closer than TurnAndRunDistance it turns and runs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Retreat", meta=(ClampMin="0", Units="cm"))
	float RetreatDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Retreat", meta=(ClampMin="0", Units="cm/s"))
	float BackpedalSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Retreat", meta=(ClampMin="0", Units="cm"))
	float TurnAndRunDistance = 380.0f;

	/** It leaves an ember patch behind it while it backs away, at most this often (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Retreat", meta=(ClampMin="0", Units="s"))
	float EmberCooldown = 7.0f;

	/** The howl: the warriors within this distance come to look where it saw its foe, and up to HowlWakeAmbushers
	 *  monsters hidden in the bushes around leap out; once every HowlCooldown (cm, count, s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Howl", meta=(ClampMin="0", Units="cm"))
	float HowlRadius = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Howl", meta=(ClampMin="0"))
	int32 HowlWakeAmbushers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Howl", meta=(ClampMin="0", Units="s"))
	float HowlCooldown = 30.0f;

	/** It stands while it howls (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Howl", meta=(ClampMin="0", Units="s"))
	float HowlSeconds = 1.6f;

	/** The cloud: thrown at a foe between these distances, once every CloudCooldown (cm, s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0", Units="cm"))
	float CloudMinRange = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0", Units="cm"))
	float CloudMaxRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0", Units="s"))
	float CloudCooldown = 9.0f;

	/** It breathes the ash in this long before it throws (s): the clip and the sound announce it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0.2", Units="s"))
	float CastWindup = 0.9f;

	/** It aims where its foe will be this far ahead at its speed, the lead bounded (s, cm): never a homing cloud */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0", Units="s"))
	float CloudLeadSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0", Units="cm"))
	float CloudMaxLead = 300.0f;

	/** Height above its feet the lump leaves its hand from (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Cloud", meta=(ClampMin="0", Units="cm"))
	float ThrowHeight = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher")
	FTikiAshBurstRules Ash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher")
	FTikiTacticClips Clips;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher")
	FTikiTacticSounds Sounds;
};
