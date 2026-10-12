// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapons/FirearmTracer.h"
#include "FortSoldierRules.generated.h"

class UAnimSequence;
class UMaterialInterface;
class UMusketRules;
class UNiagaraSystem;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UTikiRules;

/** Kind of soldier of the garrison (maquette bean-francais, ue/BeanFR_EnemyManifest.json "roles") */
UENUM(BlueprintType)
enum class EFortSoldierRole : uint8
{
	/** Fusilier: musket, then the bayonet when pressed */
	Fusilier,
	/** Tirailleur (light infantry): musket, then the sabre */
	Skirmisher,
	/** Grenadier: musket, then charges with the bayonet */
	Grenadier,
	/** Sergent: musket, then the sabre; leads the patrol */
	Sergeant,
	/** Officier: musket and sabre, sturdier (mini-boss) */
	Officer
};

/** What a flinch of a soldier plays (GoreBox, owner 2026-10-08) */
UENUM(BlueprintType)
enum class EFortSoldierFlinch : uint8
{
	/** The clip of the zone the blow struck, or the plain flinch */
	Zone,
	/** Thrown on its back by a shot it survives, then up again (KnockbackMusket) */
	Knockback,
	/** Its headwear knocked off (HatKnockedOff) */
	HatOff
};

/** How a soldier dies: its clip and its look (GoreBox, owner 2026-10-08) */
UENUM(BlueprintType)
enum class EFortSoldierDeath : uint8
{
	Plain,
	/** A shot in the head (DeathHeadshot) */
	Headshot,
	/** The head taken off by a blade, or by a shot one time in DecapitationChance (DeathDecapitated, the head bone hidden, the blood of BloodRules) */
	Decapitated,
	/** A lethal shot from closer than ExplodeRangeCm: the body bursts (DeathExplode, then the ragdoll) */
	Exploded,
	/** A blade in the torso, one time in BisectChance: the body falls in two (DeathBisected; at BisectTime the upper
	 *  body is hidden and flies as the TorsoUpperMesh of the soldier, with its head and arms; owner 2026-10-09) */
	Bisected,
	/** Death of a soldier crawling without a leg, whatever the blow (LeglessCrawlDeath) */
	CrawlDeath
};

/** Blows of cold steel */
UENUM(BlueprintType)
enum class EFortSoldierBlow : uint8
{
	None,
	/** Bayonet thrust, musket in hand (A_BeanFR_Bayonet_Thrust) */
	Bayonet,
	/** Sabre cuts and thrust (A_Bean_Sabre_*) and the combo (A_BeanFR_Sabre_Combo) */
	SlashRight,
	SlashLeft,
	Thrust,
	Combo,
	/** The one-armed sabre blow of a soldier without its left arm (ArmlessFightOn, A_BeanFR_Armless_Fight_On; owner 2026-10-09) */
	OneArm
};

/** One blow of cold steel */
USTRUCT(BlueprintType)
struct FFortSoldierBlowRules
{
	GENERATED_BODY()

	/** Clip of the blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow")
	TObjectPtr<UAnimSequence> Clip;

	/** Reach, body to body (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0", Units="cm"))
	float ReachCm = 85.0f;

	/** Damage of each hit window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0"))
	float Damage = 15.0f;

	/** Moments of the clip the blow strikes (s): X start, Y end; one hit per window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow")
	TArray<FVector2D> HitWindows;

	/** Half angle of the cone in front of the soldier the blow covers (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float ConeDegrees = 50.0f;

	/** Wait after the blow before the next one (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0", Units="s"))
	float Recovery = 0.6f;

	/** Weight of the blow in the draw among the sabre blows (the bayonet is the only blow of the musket) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0"))
	float Weight = 1.0f;

	/** Speed the soldier lunges forward during the first hit window (cm/s), 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0"))
	float LungeSpeed = 0.0f;
};

/** What a kind of soldier changes */
USTRUCT(BlueprintType)
struct FFortSoldierRoleRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Role")
	EFortSoldierRole Role = EFortSoldierRole::Fusilier;

	/** Health of this kind relative to MaxHealth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Role", meta=(ClampMin="0.1"))
	float HealthScale = 1.0f;

	/** Damage of its blows and shots relative to the rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Role", meta=(ClampMin="0"))
	float DamageScale = 1.0f;

	/** Pressed while it still has shots: the bayonet (musket in hand) rather than the sabre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Role")
	bool bBayonet = true;
};

/**
 *  Rules of the soldiers of the French forts (DA_FortSoldierRules; campaign day 6, docs/features/campagne-dix-jours.md;
 *  maquette bean-francais of the second PC). Starting values from its manifest (ai: ranges, damage, cooldowns, speeds),
 *  to tune in play. The soldier patrols the posts of its fort, fires its musket (a few shots, a long reload), then
 *  fights with the bayonet or the sabre, and never leaves its fort (AFortGarrisonZone).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFortSoldierRules : public UDataAsset
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ health

	/** Health of a fusilier (the roles scale it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="1"))
	float MaxHealth = 100.0f;

	/** A blow of at least this much makes it flinch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="0"))
	float HitReactDamage = 10.0f;

	/** Time between two flinches (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="0", Units="s"))
	float HitReactCooldown = 1.5f;

	// ------------------------------------------------------------------ limbs (GoreBox dismemberment, owner 2026-10-09)

	/** Damage an arm takes before it is torn off: the blows on its bones (the pellets of a shot by bone, times the
	 *  LimbDamageScale of the weapon as for the tiki; a blade by the bone it struck), the soldier living or not. Torn,
	 *  the bone is hidden, the ArmLMesh / ArmRMesh of the soldier flies, the stump bleeds (BloodRules) and the
	 *  musket falls; without its left arm it fights on at the sabre (ArmlessFightOn), without its right one it panics */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Limbs", meta=(ClampMin="1"))
	float LimbHealthArm = 25.0f;

	/** The same for a leg: torn, the soldier falls and drags itself (LeglessCrawlLoop at CrawlSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Limbs", meta=(ClampMin="1"))
	float LimbHealthLeg = 30.0f;

	/** Speed a soldier without a leg drags itself at (cm/s), toward the one it hunts or its posts, no blow nor shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Limbs", meta=(ClampMin="0", Units="cm/s"))
	float CrawlSpeed = 40.0f;

	/** A crawling soldier bleeds out after this long on the ground (s; 0 = never): it dies its crawl death */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Limbs", meta=(ClampMin="0", Units="s"))
	float CrawlBleedOutSeconds = 6.0f;

	/** ...or as soon as its health falls to this (0 = never) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Limbs", meta=(ClampMin="0"))
	float CrawlDeathHealth = 15.0f;

	/** Chance a lethal blade in the torso (the player's cutlass when its move cuts, the sabre of another soldier) cuts
	 *  the body in two (DeathBisected) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Limbs", meta=(ClampMin="0", ClampMax="1"))
	float BisectChance = 0.5f;

	// ------------------------------------------------------------------ senses

	/** It sees a player up to this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0", Units="cm"))
	float SightRadiusCm = 2600.0f;

	/** Half angle of its view in front of it (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float SightHalfAngleDegrees = 75.0f;

	/** A player this close is noticed whatever the direction (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0", Units="cm"))
	float NoticeRadiusCm = 450.0f;

	/** Height of the eyes over the middle of the body (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(Units="cm"))
	float EyeHeightCm = 25.0f;

	/** Seconds a player out of sight is still hunted (to its last known place) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0", Units="s"))
	float LoseTargetSeconds = 10.0f;

	/** A soldier that spots a player (or is hurt) warns the others of the fort within this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0", Units="cm"))
	float AlarmRadiusCm = 4500.0f;

	/** Seconds between two looks around (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0.05", Units="s"))
	float SenseInterval = 0.25f;

	/** Time it takes to react once it spotted someone (turns, shouts) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senses", meta=(ClampMin="0", Units="s"))
	float ReactionSeconds = 0.6f;

	// ------------------------------------------------------------------ patrol

	/** Walking speed of the patrol (cm/s; the bean's legs play the clip faster) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="cm/s"))
	float PatrolSpeed = 140.0f;

	/** Running speed when it hunts (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="cm/s"))
	float RunSpeed = 340.0f;

	/** Turn rate when it faces someone (deg/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="deg/s"))
	float TurnRateDegrees = 400.0f;

	/** Time spent at a post before the next one (s, drawn between the two) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="s"))
	float PostSecondsMin = 4.0f;

	/** See PostSecondsMin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="s"))
	float PostSecondsMax = 10.0f;

	/** A point is reached this close (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="10", Units="cm"))
	float AcceptanceRadiusCm = 70.0f;

	/** Seconds between two paths toward someone it hunts (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0.05", Units="s"))
	float RepathInterval = 0.4f;

	/** Walking straight where there is no navigation mesh (T-319): it stops when a wall stands this close ahead (cm)... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="cm"))
	float DirectLookAheadCm = 60.0f;

	/** ...or when it got less than this much closer to its goal (cm) over DirectProgressSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0", Units="cm"))
	float DirectMinProgressCm = 25.0f;

	/** See DirectMinProgressCm (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0.1", Units="s"))
	float DirectProgressSeconds = 1.5f;

	// ------------------------------------------------------------------ garrison (T-332, owner 2026-10-11: "je veux plus de soldats dans les bâtiments")

	/** Guards a fort posts inside its buildings (barracks, guardhouse, governor's house, kitchen, chapel, prison, tower,
	 *  treasure vault: AFortGarrisonZone.BuildingPosts, scripts/editor/fort_building_posts_ue.py), on top of the day's
	 *  garrison of the court and the ramparts (DA_RunRules.DifficultyByDay.SoldiersPerFort): by campaign day (the first
	 *  entry is day 1, the last one stands for the days after it), one guard per post at most, spread over the buildings,
	 *  in the uniforms of the soldiers placed in the fort */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Garrison")
	TArray<int32> BuildingGuardsByDay = { 4, 4, 4, 5, 5, 5, 6, 6, 6, 6 };

	/** The same without a campaign (day 0: a test map without a run) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Garrison", meta=(ClampMin="0"))
	int32 BuildingGuardsWithoutRun = 4;

	/** Most soldiers alive in one fort once its guards are posted (each one is a full character: movement, skeleton,
	 *  musket, network): the guards stop there; 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Garrison", meta=(ClampMin="0"))
	int32 MaxSoldiersPerFort = 18;

	/** A guard of a building keeps within this distance of its post (cm, on the ground plane): it fights from its building
	 *  and its doorstep, then goes back to its post; 0 = the whole fort */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Garrison", meta=(ClampMin="0", Units="cm"))
	float BuildingHoldRadiusCm = 600.0f;

	/** ...and answers the alarm of the fort only for a foe seen this close to it (cm; 0 = every alarm): it hunts whoever
	 *  it sees, hears or hurts it like any soldier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Garrison", meta=(ClampMin="0", Units="cm"))
	float BuildingAlarmRadiusCm = 1500.0f;

	// ------------------------------------------------------------------ musket

	/** Shots it fires before it fights with steel only (owner: "un gun 6 coups") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0"))
	int32 Shots = 6;

	/** It fires from this distance on (body to body, cm); closer it fights hand to hand */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float MinFireCm = 300.0f;

	/** ...and up to this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float MaxFireCm = 1500.0f;

	/** Time it aims before the shot (s, drawn between the two) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="s"))
	float AimSecondsMin = 0.7f;

	/** See AimSecondsMin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="s"))
	float AimSecondsMax = 1.3f;

	/** Moment of the shot in the fire clip (s; AN_FireShot of A_Bean_Musket_Fire) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="s"))
	float FireShotTime = 0.542f;

	/** Damage of a ball that strikes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0"))
	float MusketDamage = 35.0f;

	/** Chance a ball strikes up to HitChanceNearCm, and from HitChanceFarCm (linear between) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", ClampMax="1"))
	float HitChanceNear = 0.55f;

	/** See HitChanceNear */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", ClampMax="1"))
	float HitChanceFar = 0.15f;

	/** See HitChanceNear */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float HitChanceNearCm = 500.0f;

	/** See HitChanceNear */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float HitChanceFarCm = 1500.0f;

	/** A missed ball lands this far beside the target at most (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float MissSpreadCm = 140.0f;

	/** Tracer rounds of the soldiers' musket shots, to what they struck or past their target (owner 2026-10-10) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FFirearmTracerLook MusketTracer;

	/** Damage of a ball of a firing squad (AExecutionScene): it kills */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0"))
	float SquadDamage = 10000.0f;

	/** The body's turn from the line to the target while it aims and fires (deg, + to the right); the musket points at
	 *  the target whatever the turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket|Poses", meta=(Units="deg"))
	float FireYawOffsetDeg = 0.0f;

	/** Aiming and firing (owner 2026-10-07: "fix la position de tir"): the musket, its grip in the right hand, points at
	 *  the target (replicated pitch), turned by this in its own frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket|Poses")
	FRotator AimMusketOffset = FRotator::ZeroRotator;

	/** Reloading ("fix animation rechargement"): the musket stands up, held by this hand at ReloadHoldPoint (a point of
	 *  the musket, cm from its grip along the barrel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket|Poses")
	FName ReloadHoldBone = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket|Poses")
	FVector ReloadHoldPoint = FVector(40.0, 0.0, 2.0);

	/** Reloading: the musket's turn in the soldier's frame (pitch up: the muzzle toward the sky) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket|Poses")
	FRotator ReloadMusketRotation = FRotator(75.0, 0.0, 0.0);

	/** Time the musket takes from a pose to the next (in the hand, shouldered, stood up) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket|Poses", meta=(ClampMin="0.01", Units="s"))
	float MusketPoseBlendSeconds = 0.25f;

	// ------------------------------------------------------------------ cold steel

	/** Pressed closer than this (body to body, cm), it fights with steel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel", meta=(ClampMin="0", Units="cm"))
	float MeleeSwitchCm = 140.0f;

	/** Bayonet thrust */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel")
	FFortSoldierBlowRules Bayonet;

	/** Sabre blows, drawn by their weights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel")
	FFortSoldierBlowRules SlashRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel")
	FFortSoldierBlowRules SlashLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel")
	FFortSoldierBlowRules Thrust;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel")
	FFortSoldierBlowRules Combo;

	/** Moment of the draw clip the sabre leaves the scabbard for the hand (and the musket goes to the back) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel", meta=(ClampMin="0", Units="s"))
	float DrawAttachTime = 0.3f;

	/** Seconds without anyone to fight before the sabre goes back to the scabbard (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Steel", meta=(ClampMin="0", Units="s"))
	float SheatheAfterSeconds = 12.0f;

	// ------------------------------------------------------------------ roles

	/** What each kind changes (a kind missing here plays the defaults) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roles")
	TArray<FFortSoldierRoleRules> Roles;

	// ------------------------------------------------------------------ animation (SK_Bean_Body_Skeleton)

	/** Standing at a post, musket at port arms (A_BeanFR_Idle_Guard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> IdleGuard;

	/** Patrol walk, musket at port arms (A_BeanFR_Patrol) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Patrol;

	/** Walk (A_Bean_Walk) and run (A_Bean_Run) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Run;

	/** Upper body while it runs with the musket (A_Bean_Musket_Hold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> MusketHold;

	/** Sabre in hand, standing (A_Bean_Sabre_Idle; also the upper body while it runs with the sabre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> SabreIdle;

	/** Musket: aim, fire, reload (A_Bean_Musket_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Aim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Reload;

	/** Sabre: draw and sheathe (A_Bean_Sabre_Draw / _Sheathe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> SabreDraw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> SabreSheathe;

	/** Flinch and death (A_Bean_Hit_React, A_Bean_Death) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> HitReact;

	/** Flinch by zone of the body (A_BeanFR_Hit_Head_Front / _Torso_Front / _Arm_L / _Arm_R / _Leg_L / _Leg_R, the gore
	 *  clips of the textured French beans, owner 2026-10-08): a blow the soldier survives plays the one of the bone it
	 *  struck; HitReact when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> HitHead;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> HitTorso;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> HitArmL;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> HitArmR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> HitLegL;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> HitLegR;

	/** Death by a blow in the head (A_BeanFR_Death_Headshot); Death for the others */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones")
	TObjectPtr<UAnimSequence> DeathHeadshot;

	/** A blow on the capsule is put on the bone its line passes nearest to, this near at most (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Zones", meta=(ClampMin="0", Units="cm"))
	float HitBoneRadiusCm = 20.0f;

	// ------------------------------------------------------------------ GoreBox deaths and reactions (owner 2026-10-08)

	/** Death by a blade in the head, or by a shot in the head one time in DecapitationChance: the head comes off
	 *  (A_BeanFR_Death_Decapitated; the head bone hidden at DecapitationTime, the HeadMesh of the soldier flies, the
	 *  blood of BloodRules at the neck) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> DeathDecapitated;

	/** Death by a lethal shot from closer than ExplodeRangeCm: the body bursts (A_BeanFR_Death_Explode, then at
	 *  ExplodeRagdollTime the ragdoll thrown back at ExplodeImpulse, the head flying) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> DeathExplode;

	/** A shot the soldier survives throws it on its back and it gets up (A_BeanFR_Knockback_Musket) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> KnockbackMusket;

	/** A blow on the headwear knocks it off and hurts nothing (A_BeanFR_Hat_Knocked_Off): at HatOffTime the hat bone is
	 *  hidden and the HatMesh of the soldier flies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> HatKnockedOff;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="s"))
	float HatOffTime = 0.42f;

	/** Bones of the headwear and of the head */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName HatBone = TEXT("hat");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName HeadBone = TEXT("head");

	/** The hat bone runs from the head joint up to the crown: only its upper share is the headwear, a blow lower than
	 *  that is a blow in the face (0.6: the top 40 % of the skull) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", ClampMax="1"))
	float HatBoneShare = 0.6f;

	/** Chance a lethal shot in the head takes it off rather than playing the headshot death */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", ClampMax="1"))
	float DecapitationChance = 0.4f;

	/** The head comes off this long into DeathDecapitated (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="s"))
	float DecapitationTime = 0.2f;

	/** A lethal shot from at most this far bursts the body (cm; 0 never) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="cm"))
	float ExplodeRangeCm = 120.0f;

	/** The ragdoll of the burst starts this long into DeathExplode (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="s"))
	float ExplodeRagdollTime = 0.3f;

	/** Speed the burst throws the ragdoll back with (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="cm/s"))
	float ExplodeImpulse = 700.0f;

	/** Speed a knocked off hat or head leaves with, up and away (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="cm/s"))
	float PieceSpeed = 350.0f;

	/** Where the HatMesh and HeadMesh of a soldier sit in their bone (cm from the unscaled bone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FVector HatMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FVector HeadMeshOffset = FVector::ZeroVector;

	/** Blood of the hits, the cuts and the bursts: the Blood category of a DA_TikiRules (colour, sprays, splats, pools) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UTikiRules> BloodRules;

	// ------------------------------------------------------------------ GoreBox wounds (limbs torn off, owner 2026-10-09)

	/** Locomotion of a soldier without a leg (A_BeanFR_Legless_Crawl_Loop, looped): it drags itself at CrawlSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> LeglessCrawlLoop;

	/** Its death, crawling, whatever the blow (A_BeanFR_Legless_Crawl_Death); Death when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> LeglessCrawlDeath;

	/** A crawling soldier that does not move writhes on the ground (A_BeanFR_Writhe_Ground_Loop, looped); the crawl
	 *  loop when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> WritheGroundLoop;

	/** A soldier without its right arm (or both), standing: it panics (A_BeanFR_Armless_Panic_Loop, looped) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> ArmlessPanicLoop;

	/** ...and running from its foes (A_BeanFR_Flee_Loop, looped; the feet keep pace at RunNoSlideSpeed); the panic
	 *  loop when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> FleeLoop;

	/** The one-armed sabre blow of a soldier without its left arm (A_BeanFR_Armless_Fight_On: hit windows 0.167-0.292,
	 *  0.583-0.708 and 1.0-1.125 s), played instead of the sabre blows; the sabre blows when its clip is empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FFortSoldierBlowRules ArmlessFightOn;

	/** Death cut in two at the waist (A_BeanFR_Death_Bisected): at BisectTime the BisectBone and what hangs from it
	 *  are hidden, the TorsoUpperMesh of the soldier flies with its head, its hat and the arms it still has, the waist
	 *  bleeds; the legs play the rest of the clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	TObjectPtr<UAnimSequence> DeathBisected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="s"))
	float BisectTime = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName BisectBone = TEXT("spine_02");

	/** First bones of the limbs: a blow on them or under them wears the limb, the tear hides them with their children */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName ArmLBone = TEXT("upperarm_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName ArmRBone = TEXT("upperarm_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName LegLBone = TEXT("thigh_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FName LegRBone = TEXT("thigh_r");

	/** Where the limb and upper torso pieces of a soldier (SM_BeanFR_<Skin>_Arm_L..., pivot at the socket
	 *  GoreLimb_<Module>) sit in their bone (cm from the unscaled bone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FVector ArmMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FVector LegMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore")
	FVector TorsoUpperMeshOffset = FVector::ZeroVector;

	/** Crawl stance: half height of the capsule (cm, the shots and blows pass over the body less) and the mesh raised
	 *  by this in it (cm; the crawl clips lie on the ground at the feet origin) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="10", Units="cm"))
	float CrawlCapsuleHalfHeight = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(Units="cm"))
	float CrawlMeshOffset = 0.0f;

	/** Ground speed the crawl loop does not slide at (cm/s; maquette: 18 cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="1", Units="cm/s"))
	float CrawlNoSlideSpeed = 18.0f;

	/** A panicking soldier runs this far from its foe at each turn (cm), held on its ground (the fort, the ring) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="cm"))
	float PanicFleeDistanceCm = 900.0f;

	/** With nobody to run from it runs round and round, each turn this far away (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Gore", meta=(ClampMin="0", Units="cm"))
	float PanicRingRadiusCm = 350.0f;

	/** Clip of a zone (HitZones::EZone), null when none is set */
	UAnimSequence* GetZoneHit(uint8 Zone) const
	{
		switch (Zone)
		{
		case 0: return HitHead;
		case 1: return HitTorso;
		case 2: return HitArmL;
		case 3: return HitArmR;
		case 4: return HitLegL;
		case 5: return HitLegR;
		default: return nullptr;
		}
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimSequence> Death;

	/** The player's musket (DA_MusketRules): its body clips of the shot and the reload, and the musket in the hand at each of
	 *  their frames (scripts/props/musket_tp_tracks.py): the soldier plays them and its musket follows the hands (owner,
	 *  2026-10-07: fix the shot and reload animations); none = Aim / Fire / Reload above, the musket held fixed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UMusketRules> MusketPoses;

	/** Ground speeds at which the walk (and patrol) and run clips do not slide (maquette: 49 and 112 cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="1", Units="cm/s"))
	float WalkNoSlideSpeed = 49.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="1", Units="cm/s"))
	float RunNoSlideSpeed = 112.0f;

	/** Faster than this it runs (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", Units="cm/s"))
	float RunAboveSpeed = 200.0f;

	/** Limits of the locomotion play rate (like the player's bean) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.1"))
	float MinPlayRate = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.1"))
	float MaxPlayRate = 3.2f;

	/** Blend of a change of clip (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.01", Units="s"))
	float BlendSeconds = 0.2f;

	/** First bone of the upper body layer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	FName UpperBodyRootBone = TEXT("spine_01");

	// ------------------------------------------------------------------ network (T-317, network review T-268 R-28)

	/** Updates per second of a living soldier while a player may see it (its movement, its state): the engine default
	 *  of 100 was 2 to 5 times the budget of 16 KB/s per client with a garrison in sight (docs/conventions/performance.md) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1", ClampMax="100"))
	float NetUpdateFrequency = 30.0f;

	/** Lowest rate while nothing of it changes, with net.UseAdaptiveNetUpdateFrequency 1 (the engine default is 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1", ClampMax="100"))
	float MinNetUpdateFrequency = 2.0f;

	/** A player farther than this no longer receives it (cm; the engine default is 15000) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1000", Units="cm"))
	float NetCullDistance = 15000.0f;

	/** Updates per second of a dead soldier until it goes dormant (its body falling when killed in the air) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1", ClampMax="100"))
	float DeadNetUpdateFrequency = 10.0f;

	/** A dead soldier stops replicating (dormancy) this long after its death, once its body lies on the ground: its last
	 *  state goes out first, a player who meets it later still receives it once, and a blow that tears a limb off the
	 *  body sends it again (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", Units="s"))
	float DeadDormancySeconds = 3.0f;

	/** The body of a dead soldier is removed this long after its death (s; 0 = it stays): fewer actors ticking and
	 *  kept on every machine */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", Units="s"))
	float DeadLifetimeSeconds = 180.0f;

	/** ...but never while a player stands this close to it (cm): it vanishes out of the crew's sight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", Units="cm"))
	float DeadRemoveClearanceCm = 3000.0f;

	/** How often the server looks again at a dead soldier still falling, or whose body waits for the crew to go (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.25", Units="s"))
	float DeadCheckSeconds = 2.0f;

	// ------------------------------------------------------------------ corpse (T-323)

	/** A dead soldier whose body lies still stops animating, on each machine: its death clip played out, on the ground,
	 *  its ragdoll asleep, no look of its death to come. Its skeleton keeps its last pose, no longer evaluated each frame
	 *  (nor its musket, its movement or its own tick); a look of the gore, a new detail level or a correction of its
	 *  place wakes it, and a ragdoll pushed still moves (the engine's physics blend). Off: it animates every frame until
	 *  its body goes (console switch for a measure: monke.Fort.CorpseFreeze) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse")
	bool bFreezeRestingCorpse = true;

	/** Seconds its body stays at rest before it stops animating, and again after each wake (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="s"))
	float CorpseRestSeconds = 1.0f;

	/** Below this speed the body counts as lying still (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="cm/s"))
	float CorpseStillSpeed = 5.0f;

	/** How often a body that stopped animating looks at the detail level the renderer wants for it (a player came closer
	 *  or went away): a change refreshes its skeleton once at the new level (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.05", ClampMax="5", Units="s"))
	float CorpseLODCheckSeconds = 0.25f;

	/** Bounds scale of a body lying on the ground (crawling without a leg, or dead: about a metre along the ground from
	 *  its feet), on every machine: the standing one of the blueprint (1.8: the hat and the musket) let a lying body that
	 *  reaches past its box vanish at the edge of the screen or behind a wall (T-332); 0 = the standing one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float LyingBoundsScale = 2.6f;

	// ------------------------------------------------------------------ gear (grips relative to unscaled bones, cm)

	/** Musket (SK_Bean_Musket, grip at the origin, muzzle along +X) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	TObjectPtr<USkeletalMesh> MusketMesh;

	/** Bone and grip of the musket in hand (DA_MusketRules.HandOffset) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FName HandBone = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FTransform MusketInHand;

	/** Bone and place of the musket slung on the back while the sabre is drawn (socket spine_02/musket_back) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FName BackBone = TEXT("spine_02");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FTransform MusketOnBack;

	/** Muzzle in the musket (cm, musket mesh space): the shot and the bayonet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FVector MuzzleOffset = FVector(62.6, 0.0, 4.5);

	/** The musket as a loose piece (SM_Bean_Musket, same pivot as MusketMesh): it flies off in physics when an arm is
	 *  torn off (owner 2026-10-09); none = the musket only vanishes with the arm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	TObjectPtr<UStaticMesh> MusketDropMesh;

	/** Bayonet (SM_Bean_Bayonet, pivot at the muzzle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	TObjectPtr<UStaticMesh> BayonetMesh;

	/** Bayonet relative to the muzzle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FTransform BayonetOnMuzzle;

	/** Sabre (SM_Bean_Cutlass) and its grip in hand (socket hand_r/sabre_grip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	TObjectPtr<UStaticMesh> SabreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FTransform SabreInHand;

	/** Bone of the scabbard and the sheathed sabre (sockets pelvis/scabbard, pelvis/sabre_sheathed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FName HipBone = TEXT("pelvis");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FTransform SabreSheathed;

	/** Scabbard (SM_Bean_Scabbard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	TObjectPtr<UStaticMesh> ScabbardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gear")
	FTransform ScabbardOnHip;

	// ------------------------------------------------------------------ effects and sounds

	/** Report of the musket (SW_Musket_Shot) and how far it carries */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundBase> ShotSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundAttenuation> ShotAttenuation;

	/** Whoosh of a blow and its impact (SW_Cutlass_Swing, SW_Cutlass_Hit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundBase> SwingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundBase> HitSound;

	/** Sabre leaving the scabbard (SW_Cutlass_Draw) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundBase> DrawSound;

	/** Muzzle flame (the player's musket: a sphere stretched along +X, unlit additive) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UStaticMesh> FlashMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UMaterialInterface> FlashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	FVector FlashScale = FVector(0.35f, 0.14f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta=(ClampMin="0.01", Units="s"))
	float FlashSeconds = 0.08f;

	/** Black powder smoke as a particle system (local +X along the shot): when set, it replaces the puff mesh below */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> SmokeSystem;

	/** Black powder smoke: a puff that swells and fades (opacity parameter SmokeOpacityParameter) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UStaticMesh> SmokeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TObjectPtr<UMaterialInterface> SmokeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	FName SmokeOpacityParameter = TEXT("Opacity");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta=(ClampMin="0.1", Units="s"))
	float SmokeSeconds = 2.5f;

	/** Size of the puff at the shot and when it is gone (scale of SmokeMesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	float SmokeStartScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	float SmokeEndScale = 1.4f;

	/** Rules of a role (the defaults when it is not listed) */
	const FFortSoldierRoleRules& GetRole(EFortSoldierRole Role) const
	{
		static const FFortSoldierRoleRules Default;
		const FFortSoldierRoleRules* Found = Roles.FindByPredicate([Role](const FFortSoldierRoleRules& Entry) { return Entry.Role == Role; });
		return Found ? *Found : Default;
	}

	/** Rules of a blow (none for None) */
	const FFortSoldierBlowRules* GetBlow(EFortSoldierBlow Blow) const
	{
		switch (Blow)
		{
		case EFortSoldierBlow::Bayonet: return &Bayonet;
		case EFortSoldierBlow::SlashRight: return &SlashRight;
		case EFortSoldierBlow::SlashLeft: return &SlashLeft;
		case EFortSoldierBlow::Thrust: return &Thrust;
		case EFortSoldierBlow::Combo: return &Combo;
		case EFortSoldierBlow::OneArm: return &ArmlessFightOn;
		default: return nullptr;
		}
	}
};
