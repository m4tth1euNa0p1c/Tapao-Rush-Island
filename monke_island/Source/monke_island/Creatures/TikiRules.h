// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Creatures/TikiLimbs.h"
#include "Creatures/TikiTacticsTypes.h"
#include "TikiRules.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/** How a dead tiki ends once its death clip has brought it to the ground (T-151) */
UENUM(BlueprintType)
enum class ETikiCorpseSettle : uint8
{
	/** The last pose of the clip is kept, laid on the ground and asleep (it wakes when a player grabs it) */
	FrozenPose,
	/** A calm ragdoll takes over: no push, high damping, joints locked, then laid on the ground */
	CalmRagdoll
};

/** How the fragments of a baked shredding demo end once they have lain on the ground for ShredDemoHoldTime (T-168) */
UENUM(BlueprintType)
enum class ETikiShredDemoEnd : uint8
{
	/** They sink into the ground and vanish over ShredDemoFadeTime (a Fade scalar parameter is written too) */
	Fade,
	/** They stay laid on the ground as pieces, ShredDemoKeepMax at most in the level (the oldest fades) */
	Keep
};

/**
 *  Musket of the armed tiki of the villages (owner 2026-10-07: "au bout de la 6e vague les mobs utilisent des fusils
 *  quand ils pop, dans les villages"; from DA_RunRules.Campaign.ArmedMobsFromDay, T-214): a share of the guardians and
 *  defenders of the villages come out of their bush with a musket, shoot from a distance (an aim, then the shot, then
 *  the reload) and strike in melee up close or once their balls are spent. The effects are those of the French
 *  soldiers (DA_FortSoldierRules).
 */
USTRUCT(BlueprintType)
struct FTikiMusketRules
{
	GENERATED_BODY()

	/** Share of the guardians and defenders of the villages who come out armed, once the campaign opened it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", ClampMax="1"))
	float ArmedShare = 0.6f;

	/** Balls in the pouch; then it fights in melee */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0"))
	int32 Shots = 3;

	/** It shoots at a target between these distances (cm); closer, it strikes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float MinRangeCm = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float MaxRangeCm = 2500.0f;

	/** Aim before the shot, the alert clip playing and the target followed (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="s"))
	float AimSeconds = 1.2f;

	/** Reload after a shot, standing (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="s"))
	float ReloadSeconds = 6.0f;

	/** Damage of a ball that hits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0"))
	float Damage = 25.0f;

	/** Chance to hit at HitChanceNearCm and closer, at HitChanceFarCm and farther, linear between */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", ClampMax="1"))
	float HitChanceNear = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", ClampMax="1"))
	float HitChanceFar = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float HitChanceNearCm = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float HitChanceFarCm = 2000.0f;

	/** A missed ball passes this far from the target (cm, the line of the flame) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0", Units="cm"))
	float MissSpreadCm = 120.0f;

	/** The musket (SM_Bean_Musket), the bone holding it and its place on it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FName Bone = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FTransform Grip;

	/** Muzzle in the musket mesh (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FVector MuzzleOffset = FVector(62.6, 0.0, 4.5);

	/** Bone of its own weapon, hidden while it carries the musket (None: kept) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FName HiddenWeaponBone = TEXT("weapon_r");

	/** Report of the shot and how far it carries */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<USoundBase> ShotSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<USoundAttenuation> ShotAttenuation;

	/** Flame at the muzzle (a sphere stretched along +X, unlit additive) and how long it shows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UStaticMesh> FlashMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UMaterialInterface> FlashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FVector FlashScale = FVector(0.35, 0.14, 0.14);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket", meta=(ClampMin="0.01", Units="s"))
	float FlashSeconds = 0.08f;
};

/**
 *  Network rates of a kind of creature (T-312, review R-28): the engine default of 100 updates per second, for every
 *  mob of a night village, was 2 to 5 times the budget of 16 KB/s per client (docs/conventions/performance.md)
 */
USTRUCT(BlueprintType)
struct FTikiNetRules
{
	GENERATED_BODY()

	/** Updates per second while it is relevant to a player (its movement, its state) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1", ClampMax="100"))
	float NetUpdateFrequency = 30.0f;

	/** Lowest rate while nothing of it changes, with net.UseAdaptiveNetUpdateFrequency 1 (the engine default is 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1", ClampMax="100"))
	float MinNetUpdateFrequency = 2.0f;

	/** A player farther than this no longer receives it (cm; the engine default is 15000) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1000", Units="cm"))
	float NetCullDistance = 15000.0f;

	static FTikiNetRules Make(float InFrequency, float InMinFrequency, float InCullDistance)
	{
		FTikiNetRules Rates;
		Rates.NetUpdateFrequency = InFrequency;
		Rates.MinNetUpdateFrequency = InMinFrequency;
		Rates.NetCullDistance = InCullDistance;
		return Rates;
	}
};

/**
 *  Tunable rules of the tiki creatures of an island (DA_TikiRules): villagers who wander in the middle of the
 *  island and gather coconuts, tiki warriors who come out of the bushes at night, and the defender a villager
 *  calls from the nearest bush when a player hits it.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTikiRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Villagers kept alive in each village of the level (around the director when the level has no village marker) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	int32 VillagerCount = 4;

	/** Health of a villager */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="1"))
	float VillagerHealth = 60.0f;

	/** Walking speed of a villager (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	float VillagerWalkSpeed = 110.0f;

	/** Running speed of a fleeing villager (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	float VillagerRunSpeed = 380.0f;

	/** Shortest pause between two walks (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float WanderPauseMin = 1.5f;

	/** Longest pause between two walks (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float WanderPauseMax = 4.0f;

	/** A villager notices a coconut up to this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	float CoconutSearchRadius = 2500.0f;

	/** Time spent picking a coconut up (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float CollectDuration = 2.0f;

	/** Time a hit villager runs away from its attacker (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float FleeDuration = 6.0f;

	/** Part of the village radius (zone marker) where its villagers wander and coconuts fall */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0.1", ClampMax="1"))
	float VillagerWanderFraction = 0.55f;

	/** A villager never appears closer than this to another living villager of its village (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float SpawnPointGap = 150.0f;

	/** Designated threats of each village (cahier §6.2): tiki warriors out of the bushes near the village; not replaced once killed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	int32 GuardiansPerVillage = 3;

	/** Hostile tiki by day: their counts and caps are multiplied by this when they come out (guardians, ambushes,
	 *  alarm, kill revenge, defenders), never below one when the base count is not zero. Villagers stay as they are */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float DaySpawnScale = 0.25f;

	/** The same at night, the night warriors included */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float NightSpawnScale = 1.25f;

	/** A guardian comes out of the bush nearest to its guardian point within this distance (cm); farther, out of the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float GuardianBushRadius = 2500.0f;

	/** An idle guardian wanders this far around the place it came out, and walks back beyond 1.5 times (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="100"))
	float GuardianPostRadius = 1200.0f;

	/** Delay after the start of the level before the guardians come out, once the bushes are known (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0", Units="s"))
	float GuardianSpawnDelay = 3.0f;

	/** Coconuts kept on the ground over the villages (around the director when the level has none) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coconut", meta=(ClampMin="0"))
	int32 CoconutCount = 10;

	/** Delay between two coconut drops when there are fewer than CoconutCount (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coconut", meta=(ClampMin="0.1", Units="s"))
	float CoconutDropInterval = 8.0f;

	/** Health of a tiki warrior */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="1"))
	float WarriorHealth = 80.0f;

	/** Walking speed of a tiki warrior (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float WarriorWalkSpeed = 140.0f;

	/** Running speed of a tiki warrior chasing a player (cm/s); the player sprints faster */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float WarriorRunSpeed = 400.0f;

	/** A night warrior notices a player up to this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AggroRadius = 2500.0f;

	/** A warrior gives up a target farther than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float LoseRadius = 5000.0f;

	/** Distance from which the warrior starts its blow (cm, centre to centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackRange = 160.0f;

	/** Visible preparation before the blow lands (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0", Units="s"))
	float AttackWindup = 0.45f;

	/** Time from the start of a blow to the next one (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0", Units="s"))
	float AttackCooldown = 1.6f;

	/** Damage of a blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackDamage = 15.0f;

	/** Reach of the blow in front of the warrior (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackReach = 180.0f;

	/** Radius of the blow sweep (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="1"))
	float AttackRadius = 50.0f;

	/** Horizontal speed given to a struck player (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackKnockback = 400.0f;

	/** Speed of the leap out of the bush (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float EmergeLeapSpeed = 450.0f;

	/** Time the dead body falls limp (ragdoll) before it becomes a corpse one can carry (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.1", Units="s"))
	float CorpseDelay = 2.5f;

	/** How a body dies (T-151). FrozenPose: the lethal reaction clip, else the death clip (Anims.Death), plays to the
	 *  ground, then the corpse is that last pose, rigid, its collision the physics asset bodies in that pose, laid on the
	 *  ground. CalmRagdoll: a calm ragdoll corpse (ATikiRagdollCorpse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse")
	ETikiCorpseSettle CorpseSettleMode = ETikiCorpseSettle::FrozenPose;

	/** Highest speed of a dead body (cm/s): bounds DeathImpulse, the speeds handed to the corpse and those after a
	 *  hit on a corpse (a musket ball) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float CorpseMaxSpeed = 300.0f;

	/** Linear damping of each body of a dead body (calm, it does not slide nor float) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float CorpseLinearDamping = 1.5f;

	/** Angular damping of each body of a dead body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float CorpseAngularDamping = 3.0f;

	/** Speed a body overlapping the ground or the capsule is pushed out at (cm/s): low, it does not jump */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float CorpseMaxDepenetrationSpeed = 50.0f;

	/** Highest spin of a body (degrees/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="DegreesPerSecond"))
	float CorpseMaxSpin = 720.0f;

	/** The joints of a dead body keep their bones together: linear limits locked and projection on (no stretch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse")
	bool bCorpseLockJoints = true;

	/** Distance a joint may open before the projection pulls it back (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="cm"))
	float CorpseJointLinearTolerance = 1.0f;

	/** Angle a joint may pass its limit before the projection pulls it back (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="Degrees"))
	float CorpseJointAngularTolerance = 10.0f;

	/** Time a ragdoll corpse moves before it is laid on the ground and put to sleep (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.1", Units="s"))
	float CorpseSettleTime = 3.0f;

	/** Gap between the lowest body and the ground below it that is left as is (cm); beyond, the corpse is moved
	 *  onto the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="cm"))
	float CorpseGroundTolerance = 4.0f;

	/** Height above and below the lowest body the ground is looked for (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="1", Units="cm"))
	float CorpseGroundTraceHeight = 200.0f;

	/** Speed given to a dying body along the killing blow when it goes limp (cm/s), at most CorpseMaxSpeed; none
	 *  after a lethal reaction clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float DeathImpulse = 350.0f;

	/** Time a corpse stays on the ground (s); 0 = until the rising water covers it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="s"))
	float CorpseLifetime = 0.0f;

	/** Water above the lowest point of a corpse before it is washed away (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0"))
	float CorpseFloodDepth = 30.0f;

	/** Period of the flood check of a corpse (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.2", Units="s"))
	float CorpseFloodCheckInterval = 2.0f;

	/** Mass of a whole corpse (kg): one player drags it, two lift it, and it weighs in the boat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.1"))
	float CorpseMassKg = 40.0f;

	/** Mass of each half of a corpse cut in two (kg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.1"))
	float CorpseHalfMassKg = 20.0f;

	/** Mass of a limb cut off (kg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0.1"))
	float CorpseLimbMassKg = 4.0f;

	/** Corpses kept in the level at most; beyond, the oldest one is washed away (performance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="1"))
	int32 CorpseMaxCount = 80;

	/** Margin of the physics box of a corpse around its joints (cm): the body is wider than its bones */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="1"))
	float CorpseBoxMargin = 22.0f;

	/** Time the last frame of the death clip is held before the frozen corpse replaces the body (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="s"))
	float CorpseFreezeDelay = 0.1f;

	/** Gap left between the collision of a frozen corpse and the ground when it is laid down (cm): it touches the
	 *  ground without overlapping it (no push out of the ground) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="cm"))
	float CorpseRestClearance = 0.5f;

	/** Gap between a frozen corpse and the ground left as is (cm): beyond, it goes down slowly; it is never lifted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="cm"))
	float CorpseSettleDeadZone = 5.0f;

	/** Speed a floating frozen corpse goes down to the ground at (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="1"))
	float CorpseSettleSpeed = 30.0f;

	/** Time the dead creature stays visible over its frozen corpse, same pose, before it goes (s): no pop while the
	 *  corpse appears on every machine */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse", meta=(ClampMin="0", Units="s"))
	float CorpseSwapTime = 0.3f;

	/** A corpse blocks the players and the creatures; off, they walk through it (no sticking on a corpse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corpse")
	bool bCorpseBlocksPawns = false;

	/** Bone a body is cut in two at: the upper half is this bone and what hangs from it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	FName HalfCutBone = TEXT("spine_02");

	/** Bones a cut blow can take off a living creature, each with what hangs from it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	TArray<FName> LimbBones = { TEXT("upperarm_r"), TEXT("upperarm_l") };

	/** Chance that a cut blow which does not kill takes a limb off (cutlass combo) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0", ClampMax="1"))
	float LimbCutChance = 0.6f;

	/** Damage of a warrior who lost the arm holding its weapon, relative to AttackDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0", ClampMax="1"))
	float MaimedDamageScale = 0.5f;

	/** Speed given to the pieces of a body cut in two or of a limb cut off, flat along the blow (cm/s), at most
	 *  CutPieceMaxSpeed (T-155: no more upward part) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0"))
	float CutFlingSpeed = 250.0f;

	/** Highest speed of a cut piece as it leaves the body, flat along the blow (cm/s, T-155): the piece is bounded to
	 *  this whatever CutFlingSpeed, the ragdoll or the shot give, then falls by its weight at the foot of the body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0"))
	float CutPieceMaxSpeed = 40.0f;

	/** Highest speed a blow (a sabre, a musket ball, a punch) gives a corpse or a piece lying on the ground (cm/s,
	 *  T-155); a throw by a player is not bounded */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0"))
	float CorpseBlowMaxSpeed = 40.0f;

	/** A fresh piece slower than this goes to sleep on the ground (cm/s, T-155) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0"))
	float CutPieceRestSpeed = 8.0f;

	/** A fresh piece is never put to sleep before this time: it falls and tips over first (s, T-155) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0", Units="s"))
	float CutPieceMinFallTime = 0.6f;

	/** The checks of a fresh piece stop after this time (s, T-155): it is left to the physics, which puts it to
	 *  sleep on its own once still */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0.5", Units="s"))
	float CutPieceSettleTimeout = 4.0f;

	/** Period of the checks of a fresh corpse: asleep once at rest, colliding with the other bodies once apart from
	 *  them (s, T-155) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0.05", Units="s"))
	float CorpseCheckInterval = 0.2f;

	/** Flat mesh closing a cut, scaled to the cut radius (a cylinder 100 cm wide and 100 cm high: the engine one);
	 *  none leaves the cut open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	TObjectPtr<UStaticMesh> CutCapMesh;

	/** Material of the cut face (bare wood) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember")
	TObjectPtr<UMaterialInterface> CutCapMaterial;

	/** Radius of the cut face of a body cut in two (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="1"))
	float BodyCutRadius = 24.0f;

	/** Radius of the cut face of a limb (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="1"))
	float LimbCutRadius = 6.0f;

	/** The bean's musket and sabre play the combat reaction clips of the struck creature (T-150); off: the hit clip
	 *  and the immediate ragdoll as before */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	bool bCombatReactions = true;

	/** Time in the reaction clip it starts at when the blow lands (s): its contact frame (12 at 30 fps), so the
	 *  preparation before the contact is not played after it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", Units="s"))
	float ReactionStartTime = 0.4f;

	/** Time of the contact in the reaction clips (s): a limb cut by the reaction comes off then */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", Units="s"))
	float ReactionContactTime = 0.4f;

	/** Time in a lethal reaction clip the ragdoll takes over at (s); 0 = at the end of the clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", Units="s"))
	float ReactionRagdollTime = 0.0f;

	/** Time in Musket_ArticulatedFall the held weapon leaves the hand (s): frame 27 at 30 fps. The other lethal
	 *  reactions let it go at the contact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", Units="s"))
	float ArticulatedFallDropTime = 0.9f;

	/** Time the creature's decisions stop during a reaction it survives (s); 0 = the rest of the clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", Units="s"))
	float ReactionAISuspendTime = 0.0f;

	/** A lethal shot dealing at least this damage bursts the body (Musket_Burst); 0 = never by damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0"))
	float MusketBurstDamage = 0.0f;

	/** A lethal shot from at most this distance bursts the body (cm); 0 = never by distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", Units="cm"))
	float MusketBurstDistance = 400.0f;

	/** Otherwise, chance a lethal shot bursts the body rather than falling (Musket_ArticulatedFall) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", ClampMax="1"))
	float MusketBurstChance = 0.25f;

	/** Limbs a burst takes off at the contact, drawn among LimbBones */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0"))
	int32 MusketBurstLimbCount = 1;

	/** A sabre blow aimed lower than this below the horizon is a blow at the legs (Sabre_Low, degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", ClampMax="90", Units="Degrees"))
	float SabreLowAimPitch = 25.0f;

	/** Swaps Sabre_LeftToRight and Sabre_RightToLeft, if the clips read the other way in game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	bool bMirrorSabreSides = false;

	/** Bone Sabre_LeftToRight takes off at the contact, with what hangs from it (when the blow cuts and the bone
	 *  exists); the same for the three bones below */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	FName SabreLeftToRightBone = TEXT("upperarm_l");

	/** Bone Sabre_RightToLeft takes off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	FName SabreRightToLeftBone = TEXT("upperarm_r");

	/** Bone Sabre_Down takes off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	FName SabreDownBone = TEXT("head");

	/** Bone Sabre_Low takes off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	FName SabreLowBone = TEXT("thigh_l");

	/** A body killed by a sabre reaction still falls in two halves when it becomes a corpse (T-072) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions")
	bool bSabreReactionCutsInTwo = false;

	/** A lethal sabre blow in the torso cuts the body in two one time in this (Combat.BisectedDeath: the waist bleeds at
	 *  the contact, the two halves come with the corpse; owner 2026-10-09); 0 = never */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CombatReactions", meta=(ClampMin="0", ClampMax="1"))
	float SabreBisectChance = 0.5f;

	/** A lethal Musket_Burst or sabre reaction with a baked demo in the mob's DA_<Mob>_ShredDemoSet replaces the mob
	 *  and its corpse by the demo (T-168): the fragments of the pack fly exactly as in Blender; off: T-150/T-155/T-167 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	bool bShredDemos = true;

	/** A lethal gun shot within the close range of its weapon (the shotgun under 5 m, DA_ShotgunRules) always takes the
	 *  mob's Musket_Burst demo when it has one, instead of the T-167 shredding; farther, the reaction picked by T-150 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	bool bCloseShotBurstDemo = true;

	/** Effect at the contact of a Musket_Burst demo (the burst, T-169), at the body's middle, along the shot; none:
	 *  only the blood. User parameters read: BloodColor, Direction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	TSoftObjectPtr<UNiagaraSystem> ShredDemoBurstSystem;

	/** A lethal sabre blow takes its demo only when it cuts (a cutlass move with bCut); off: every lethal sabre blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	bool bShredDemoSabreNeedsCut = false;

	/** Time the fragments lie still on the ground once the baked animation is over (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="0", Units="s"))
	float ShredDemoHoldTime = 8.0f;

	/** What the fragments become after the hold: a fade, or pieces kept on the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	ETikiShredDemoEnd ShredDemoEnd = ETikiShredDemoEnd::Fade;

	/** Length of the fade (s): the fragments sink into the ground, a Fade scalar parameter (0 to 1) goes to their
	 *  materials for a material that dithers them out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="0", Units="s"))
	float ShredDemoFadeTime = 2.0f;

	/** Depth the fragments sink to during the fade (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="0", Units="cm"))
	float ShredDemoSinkDepth = 30.0f;

	/** Demos kept as pieces in the level at most (ShredDemoEnd Keep); beyond, the oldest fades */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="1"))
	int32 ShredDemoKeepMax = 8;

	/** Rotation of the demo mesh in its actor, whose +X faces the attacker: the pack's victim faces Blender -Y, +Y in
	 *  Unreal after the FBX import, like the mob meshes (-90 yaw) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	FRotator ShredDemoMeshRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** The ground under the mob is looked for from this height above its feet (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="0", Units="cm"))
	float ShredDemoGroundTraceHeight = 60.0f;

	/** ... down to this depth below its feet (cm); none found: at the feet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="1", Units="cm"))
	float ShredDemoGroundTraceDepth = 200.0f;

	/** Height of the demo origin above the ground found (cm): the fragments land on the pack's ground plane, at 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(Units="cm"))
	float ShredDemoGroundClearance = 0.5f;

	/** Bounds scale of the fragments: they fly up to a few metres from the body, out of the mesh bounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="1"))
	float ShredDemoBoundsScale = 4.0f;

	/** Culling box of the body and of its body parts, times the box the engine computes from the physics bodies: the
	 *  reworked models (spikes, shells, crests) stick out of those bodies, and a mob whose box left the screen or went
	 *  behind cover vanished while still in view (owner 2026-10-10: "fix les bugs de dépop des mobs") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render", meta=(ClampMin="1", ClampMax="4"))
	float MeshBoundsScale = 1.8f;

	/** Culling box of the held weapon (spear, club, hammer), times its computed box: it takes the body's box, and a
	 *  spear of 1.4 m sticks far out of it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render", meta=(ClampMin="1", ClampMax="6"))
	float HeldItemBoundsScale = 2.6f;

	/** Limbs a shot can tear off (T-167): each has its own health, filled by the hits on its bones (gun shots: the
	 *  pistol, every pellet of the shotgun, the musket). Past it the limb is torn off with the T-155 modules, the
	 *  creature living or not */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth")
	TArray<FTikiLimb> Limbs = {
		{ TEXT("Head"), TEXT("head"), 25.0f, ETikiLimbLoss::Kills },
		{ TEXT("Arm_L"), TEXT("upperarm_l"), 25.0f, ETikiLimbLoss::Arm },
		{ TEXT("Arm_R"), TEXT("upperarm_r"), 25.0f, ETikiLimbLoss::Arm },
		{ TEXT("Leg_L"), TEXT("thigh_l"), 30.0f, ETikiLimbLoss::Leg },
		{ TEXT("Leg_R"), TEXT("thigh_r"), 30.0f, ETikiLimbLoss::Leg } };

	/** A shot on a bone counts for its limb only when the bone passes this near the shot (cm); farther, the body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth", meta=(ClampMin="1", Units="cm"))
	float LimbHitRadius = 18.0f;

	/** Speed left to a creature with one leg torn off (it limps), relative to its walk and run */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth", meta=(ClampMin="0.05", ClampMax="1"))
	float LegLossSpeedScale = 0.35f;

	/** Speed left with both legs torn off (it drags itself) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth", meta=(ClampMin="0.02", ClampMax="1"))
	float BothLegsLossSpeedScale = 0.12f;

	/** An arm torn off a living creature lets its weapon fall as a pickup (DroppedWeaponClass); off, the weapon
	 *  stays in the hand of the arm piece */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth")
	bool bArmLossDropsWeapon = true;

	/** A creature without both arms only runs in a panic (Anims.ArmlessPanic, owner 2026-10-09): its speed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth", meta=(ClampMin="0", Units="cm/s"))
	float ArmlessPanicSpeed = 380.0f;

	/** ...and how far from its nearest foe it runs at each turn (cm); without a foe it runs round and round, each turn
	 *  a fifth to a half of this away */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LimbHealth", meta=(ClampMin="0", Units="cm"))
	float ArmlessFleeDistance = 900.0f;

	/** Side of a bush zone (cm): the bushes are grouped by zone, and each zone with enough bushes hides at least one
	 *  monster that leaps out when a player comes near */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="1000"))
	float AmbushZoneSize = 10400.0f;

	/** Bushes a zone needs to hide a monster */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="1"))
	int32 AmbushMinBushes = 36;

	/** One more monster per this many bushes of a zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="1"))
	int32 AmbushBushesPerMonster = 240;

	/** Monsters hidden in one zone at most */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="1"))
	int32 AmbushMaxPerZone = 3;

	/** A hidden monster leaps out when a player comes this close to its bush (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0"))
	float AmbushTriggerRadius = 2500.0f;

	/** It stays hidden while the player is closer than this to its bush: it would pop at its feet (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0"))
	float AmbushMinDistance = 700.0f;

	/** Monsters out of their bushes at most around the same player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0"))
	int32 AmbushMaxAlivePerPlayer = 3;

	/** Shortest time between two monsters leaping out around the same player (s): one after the other, not a swarm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0", Units="s"))
	float AmbushPlayerInterval = 18.0f;

	/** No monster leaps out for a player moving faster than this (cm/s): rowing, thrown, flying past */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0"))
	float AmbushMaxPlayerSpeed = 900.0f;

	/** A monster with nobody to hunt for this long goes back into its bush (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="1", Units="s"))
	float AmbushIdleTime = 20.0f;

	/** A bush whose monster went back hides it again after this delay (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0", Units="s"))
	float AmbushRearmDelay = 240.0f;

	/** A bush whose monster was killed hides a new one after this delay (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="0", Units="s"))
	float AmbushKilledDelay = 900.0f;

	/** A monster out of its bush wanders this far around it and walks back beyond 1.5 times (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambush", meta=(ClampMin="100"))
	float AmbushPostRadius = 1000.0f;

	/** Rare items scattered on the sand at the start of each game (BP_Totem: a physics prop with a value, the players
	 *  carry it and stow it in the boat); none when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems")
	TSubclassOf<AActor> ScatterItemClass;

	/** Variants of the scattered item (BP_Totem_A, B, C), one drawn at random per item; ScatterItemClass when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems")
	TArray<TSubclassOf<AActor>> ScatterItemVariants;

	/** Items scattered at the start of a game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0"))
	int32 ScatterCount = 30;

	/** Two scattered items are never closer than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0"))
	float ScatterMinSpacing = 30000.0f;

	/** No scattered item closer than this to a bush of the level (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0"))
	float ScatterBushClearance = 250.0f;

	/** Height above the ground a scattered item appears at: it drops and settles (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0"))
	float ScatterDropHeight = 15.0f;

	/** Seed of the draw of the spots; 0 = a new draw every game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems")
	int32 ScatterSeed = 0;

	/** Candidate spots of the scattered items (UE cm, Z = approximate ground): dry sand without bush, out of the
	 *  villages (scripts/map/place_totems.py, collectibles/totem_spots.json) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totems")
	TArray<FVector> ScatterSpots;

	/** Use the old ScatterSpots draw instead of the ground Poisson sample; house totems are independent */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems")
	bool bUseLegacyScatterSpots = false;

	/** Minimum horizontal distance between ground totems (100 m); zero disables the ground sample */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", Units="cm"))
	float GroundTotemSpacing = 10000.0f;

	/** Maximum ground totems on the played fragment, excluding houses */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", ClampMax="1000"))
	int32 GroundTotemMax = 120;

	/** Reproducible placement and variant draw, including seed zero */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems")
	int32 GroundTotemSeed = 143;

	/** Jitter inside the sampling cells, as a fraction of their size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", ClampMax="1"))
	float GroundTotemJitter = 1.0f;

	/** Bounded dart-throwing budget, including water and clearance rejects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", ClampMax="20000"))
	int32 GroundTotemCandidateBudget = 6000;

	/** Runtime fragment definitions have no polygon: pad the active villages/arrival envelope by this distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", Units="cm"))
	float GroundTotemBoundsPadding = 10000.0f;

	/** Clearance beyond house footprints and plazas */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", Units="cm"))
	float GroundTotemBuildingMargin = 100.0f;

	/** Ground must be at least this far above the water surface (zero allows dry ground exactly at sea level) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", Units="cm"))
	float TotemWaterClearance = 0.0f;

	/** One totem in every matching house, including hamlets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems")
	bool bTotemInEveryHouse = true;

	/** Case-insensitive mesh-name patterns identifying whole houses */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems")
	TArray<FString> TotemHouseMeshKeywords = { TEXT("SM_Minka") };

	/** Offset from the mesh bounds centre in the house local XY frame; Z is added after the floor trace */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(Units="cm"))
	FVector HouseTotemLocalOffset = FVector(-200.0, 100.0, 0.0);

	/** Keep the requested house position within the inset mesh bounds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", Units="cm"))
	float HouseTotemInset = 100.0f;

	/** Random yaw relative to the house, in degrees (min, max) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems")
	FVector2D HouseTotemYawRange = FVector2D(-180.0, 180.0);

	/** Trace starts this far above the house base, inside the room below the roof */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", Units="cm"))
	float HouseTotemTraceHeight = 150.0f;

	/** Trace end below the house base */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", Units="cm"))
	float HouseTotemTraceDepth = 200.0f;

	/** Half height of the vertical ground trace around the village/arrival altitude */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", Units="cm"))
	float GroundTotemTraceHeight = 5000.0f;

	/** Maximum instanced vegetation components skipped by one ground trace */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", ClampMax="64"))
	int32 TotemTraceMaxPasses = 8;

	/** Gap under the mesh once settled on its bounds, rather than dropping it awake */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0", Units="cm"))
	float TotemRestClearance = 0.5f;

	/** Work budget per frame during initial placement only; capped at 50 even if the asset is edited */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", ClampMax="50"))
	int32 TotemPlacementsPerFrame = 32;

	/** Delay between retries while waiting for the fragment villages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="0.1", Units="s"))
	float TotemVillageRetryInterval = 0.5f;

	/** Maximum wait for the fragment villages; no placement on unrelated fallback villages on timeout */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Totems", meta=(ClampMin="1", Units="s"))
	float TotemVillageWaitTimeout = 30.0f;

	/** Night warriors kept alive while it is night */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0"))
	int32 NightWarriorCount = 4;

	/** Delay between two night spawns (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0.1", Units="s"))
	float NightSpawnInterval = 6.0f;

	/** Closest a night warrior appears from a player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0"))
	float NightSpawnMinDistance = 1500.0f;

	/** Farthest a night warrior appears from a player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0"))
	float NightSpawnMaxDistance = 3500.0f;

	/** A defender comes out of a bush up to this distance from the hit villager (cm); farther, it comes out of the ground next to it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender", meta=(ClampMin="0"))
	float BushSearchRadius = 3000.0f;

	/** Shortest time between two defenders called against the same player (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender", meta=(ClampMin="0", Units="s"))
	float DefenderCooldown = 5.0f;

	/** Defenders alive at most against the same player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender", meta=(ClampMin="1"))
	int32 MaxDefendersPerPlayer = 2;

	/** A static mesh whose name contains one of these words is a bush a tiki can hide in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender")
	TArray<FString> BushMeshKeywords = { TEXT("MikiMiki"), TEXT("Naupaka"), TEXT("Pandanus"), TEXT("Tahinu") };

	/** A bush lower than this is out of the game: the clearing of a village, a camp or a marae moved it away (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender")
	float BushMinZ = -10000.0f;

	/** Villagers follow their routine (activities at the points of interest of their village) while a player is this
	 *  close to the village centre; farther, they rest where they are (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Routine", meta=(ClampMin="0"))
	float RoutineActivationRadius = 15000.0f;

	/** Chance that the next goal of a villager is an activity at a point of interest rather than a stroll */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Routine", meta=(ClampMin="0", ClampMax="1"))
	float ActivityChance = 0.75f;

	/** Shortest time a villager spends at an activity (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Routine", meta=(ClampMin="0", Units="s"))
	float ActivityDurationMin = 6.0f;

	/** Longest time a villager spends at an activity (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Routine", meta=(ClampMin="0", Units="s"))
	float ActivityDurationMax = 14.0f;

	/** One take of an activity clip, replayed until the activity ends (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Routine", meta=(ClampMin="0.5", Units="s"))
	float ActivityClipDuration = 4.0f;

	/** Tiki warriors coming out of the bushes all around a village when one of its villagers is hit, against the attacker */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Alarm", meta=(ClampMin="0"))
	int32 AlarmWarriorCount = 4;

	/** The alarm warriors come out on a ring of this part of the village radius (the bushes around the houses) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Alarm", meta=(ClampMin="0.1", ClampMax="1.5"))
	float AlarmRingFraction = 0.7f;

	/** An alarm warrior comes out of the bush nearest to its point of the ring within this distance (cm); farther, out of the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Alarm", meta=(ClampMin="0"))
	float AlarmBushRadius = 2500.0f;

	/** Alarm warriors alive at most against the same player: an alarm only brings out what is missing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Alarm", meta=(ClampMin="0"))
	int32 AlarmMaxAlivePerPlayer = 6;

	/** Shortest time between two alarms of the same village (s); a hit meanwhile still makes its villagers run away */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Alarm", meta=(ClampMin="0", Units="s"))
	float AlarmCooldown = 45.0f;

	/** Time the villagers of an alarmed village run away from the attacker (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Alarm", meta=(ClampMin="0", Units="s"))
	float AlarmFleeDuration = 10.0f;

	/** Tiki warriors leaping out of the bushes around a player who kills a villager, against that player (every kill,
	 *  on top of the alarm of the village) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kill", meta=(ClampMin="0"))
	int32 KillWarriorCount = 3;

	/** The kill warriors come out of the bushes nearest to the points of a ring of this radius around the killer (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kill", meta=(ClampMin="100"))
	float KillRingRadius = 800.0f;

	/** A kill warrior comes out of the bush nearest to its point of the ring within this distance; farther, out of the
	 *  ground there (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kill", meta=(ClampMin="0"))
	float KillBushRadius = 1200.0f;

	/** Tiki alive at most against the same player once it kills, alarm and kill warriors together: a kill only brings
	 *  out what is missing. Above AlarmMaxAlivePerPlayer, so that a kill after an alarm still brings tiki out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kill", meta=(ClampMin="0"))
	int32 KillMaxAlivePerPlayer = 9;

	/** A static mesh whose name contains one of these words is a village building: no villager walks under it, no
	 *  coconut falls on it, no tiki comes out there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	TArray<FString> BuildingMeshKeywords = { TEXT("SM_Minka"), TEXT("SM_Village_") };

	/** Steepest ground a tiki point is taken on (minimal Z of the ground normal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0", ClampMax="1"))
	float MinGroundNormalZ = 0.7f;

	/** How fast a creature gets up to speed (cm/s2): a body leans into its first steps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1"))
	float CreatureAcceleration = 700.0f;

	/** How fast it stops when it stops walking (cm/s2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1"))
	float CreatureBraking = 900.0f;

	/** Turning speed of a creature (deg/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1"))
	float CreatureTurnRate = 320.0f;

	/** Crossfade between two locomotion clips: idle, walk, run (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", Units="s"))
	float LocomotionBlendTime = 0.25f;

	/** Crossfade into a one-shot clip: blow, hit, activity (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", Units="s"))
	float ActionBlendTime = 0.12f;

	/** Speed the walk clips were authored for (cm/s): they speed up or slow down with the body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="1"))
	float WalkClipSpeed = 110.0f;

	/** Speed the run clips were authored for (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="1"))
	float RunClipSpeed = 380.0f;

	/** Slowest and fastest a locomotion clip plays */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.1"))
	float MinClipRate = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.1"))
	float MaxClipRate = 4.0f;

	/** A blow at least this strong (or a cut) staggers a creature instead of a plain hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0"))
	float StaggerDamage = 35.0f;

	/** A warrior taunts a player out of its reach (in the water, in the boat) at most this often (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", Units="s"))
	float TauntInterval = 6.0f;

	/** Time a villager stays startled by an alarm before it runs (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", Units="s"))
	float StartleTime = 0.7f;

	/** Chance that a villager looks around when it stops (village with a player near) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", ClampMax="1"))
	float LookAroundChance = 0.3f;

	/** A villager waves at a player who comes this close (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0"))
	float GreetRadius = 450.0f;

	/** Time a villager waits before it waves again (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0", Units="s"))
	float GreetCooldown = 25.0f;

	/** Time a warrior waits before it growls again at a player it spots (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voice", meta=(ClampMin="0", Units="s"))
	float AggroVoiceCooldown = 8.0f;

	/** Time the villagers of a village where one of them was killed stay hidden in their houses (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0", Units="s"))
	float HideDuration = 60.0f;

	/** A villager who has not reached its door after this long hides where it is: it slipped into a house nearby (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="1", Units="s"))
	float HideReachTime = 25.0f;

	/** Period of the director decisions (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Director", meta=(ClampMin="0.1", Units="s"))
	float ThinkInterval = 1.0f;

	/** The villagers of the houses (UVillageHouseKit::Residents, owner request 2026-10-07) appear inside once a player
	 *  comes near the village: at most this many per director decision, over all the villages */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0"))
	int32 ResidentSpawnPerThink = 2;

	/** Time a villager stays at a point of its routine in its house (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="s"))
	float ResidentActivityMin = 8.0f;

	/** See ResidentActivityMin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="s"))
	float ResidentActivityMax = 20.0f;

	/** A villager reaches a waypoint of its house or of a way this close (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="10", Units="cm"))
	float WaypointRadius = 45.0f;

	/** At the flag ceremony a villager stuck on its way (a fence, a barrier: owner report 2026-10-07) hops over toward its
	 *  waypoint this many times (forward and up speeds, cm/s), then is set on the waypoint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0"))
	int32 CeremonyStuckHops = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="cm/s"))
	float StuckHopForward = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="cm/s"))
	float StuckHopUp = 540.0f;

	/** A villager follows a player in its house at this distance, watching that nothing is taken (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="50", Units="cm"))
	float WatchDistance = 170.0f;

	/** Closer than this it steps back: it never pushes the player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="cm"))
	float WatchMinDistance = 110.0f;

	/** Seconds between two new ways toward a player it cannot see (around a wall) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0.1", Units="s"))
	float WatchReplanInterval = 0.75f;

	/** Once the player is out, the villager stays at its door watching for this long (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="s"))
	float WatchLingerSeconds = 6.0f;

	/** An object of the house farther than this out of its inside is stolen (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="cm"))
	float TheftMargin = 40.0f;

	/** The thief is the player holding the object, or the nearest player within this distance of it (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="cm"))
	float ThiefSearchRadius = 800.0f;

	/** A robbed or hit villager screams and runs away for this long before it goes back home (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", Units="s"))
	float PanicSeconds = 12.0f;

	/** It screams again every this many seconds while it panics (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0.3", Units="s"))
	float ScreamInterval = 2.2f;

	/** Where a villager carries the relic of its house to the flag ceremony, held in front of it: the base of the
	 *  relic relative to the middle of its capsule, +X forward (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	FVector RelicCarryOffset = FVector(38.0f, 0.0f, -18.0f);

	/** Deepest water a tiki wades into (cm): land creatures, they never swim nor chase a swimmer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature", meta=(ClampMin="0", Units="cm"))
	float MaxWadeDepth = 30.0f;

	/** How far ahead the water depth is checked before each step (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature", meta=(ClampMin="10", Units="cm"))
	float WaterProbeDistance = 90.0f;
	/** Blood effects on every machine but a dedicated server (T-153): off, nothing is spawned nor loaded */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	bool bBloodEnabled = true;

	/** Musket ball entering the body: an exit spray along the shot, ballistic drops and a brief mist (< 1 s). User
	 *  parameters read: BloodColor (linear colour), Direction (unit vector, world) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> MusketImpactSystem;

	/** Pulsed jet out of a cut (stump of the body, cut end of a limb or of a half), attached to the cut bone for
	 *  SliceSprayDuration. User parameters read: BloodColor, Direction, Duration (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> SliceSpraySystem;

	/** A few small chunks and splashes at a cut or a musket burst. User parameters read: BloodColor, Direction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> ChunksSystem;

	/** Splats left on the ground under a hit or a cut, one drawn at random (MI_Blood_Splat_*, deferred decal
	 *  material). Parameter read: BloodColor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TArray<TSoftObjectPtr<UMaterialInterface>> SplatDecalMaterials;

	/** Pool growing under a corpse (MI_Blood_Pool). Parameters written per pool: BloodColor, GrowStartTime (world
	 *  time, s), GrowDuration (s); the material grows the pool from them, no tick */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UMaterialInterface> PoolDecalMaterial;

	/** Colour of the blood, written to the effects and decals: dark red by default, to be tinted by species later */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	FLinearColor BloodColor = FLinearColor(0.16f, 0.004f, 0.003f, 1.0f);

	/** Width of a splat on the ground, drawn between these (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="1", Units="cm"))
	float SplatSizeMin = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="1", Units="cm"))
	float SplatSizeMax = 55.0f;

	/** Width of a pool at its full size (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="1", Units="cm"))
	float PoolSize = 150.0f;

	/** Thickness of the decal box: the ground it projects on (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="1", Units="cm"))
	float DecalDepth = 40.0f;

	/** Time a splat stays before it fades (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", Units="s"))
	float SplatLifetime = 30.0f;

	/** Time a pool stays before it fades (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", Units="s"))
	float PoolLifetime = 60.0f;

	/** Length of the fade of a decal at the end of its life (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0.1", Units="s"))
	float DecalFadeDuration = 4.0f;

	/** Pause between the corpse laid on the ground and the pool starting to grow (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", Units="s"))
	float PoolGrowDelay = 0.5f;

	/** Time the pool takes to reach its full size (s): slow, it seeps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0.1", Units="s"))
	float PoolGrowDuration = 8.0f;

	/** Time the jet of a cut lasts (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0.1", ClampMax="5", Units="s"))
	float SliceSprayDuration = 1.3f;

	/** Ground looked for under a hit, down to this distance (cm): none, no splat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="10", Units="cm"))
	float GroundTraceDistance = 300.0f;

	/** Decals alive at once in the level (splats and pools): the oldest goes beyond (performance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0"))
	int32 MaxActiveDecals = 24;

	/** Blood effects and decals spawned in one frame at most (a musket burst, several hits at once; a shotgun shot
	 *  sprays once per pellet that struck, T-167) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="1"))
	int32 MaxBloodSpawnsPerFrame = 16;

	/** No blood farther than this from the local camera (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", Units="cm"))
	float BloodCullDistance = 6000.0f;

	/** A corpse piece or a cut seen this long after it happened gets no jet (a player joining late) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", Units="s"))
	float BloodFreshWindow = 1.0f;

	/** Network rates of the warriors (T-312): their blows and stalking need a steady picture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network")
	FTikiNetRules WarriorNet;

	/** Network rates of the villagers (T-312): they walk and work, a player far away does not need them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network")
	FTikiNetRules VillagerNet = FTikiNetRules::Make(15.0f, 2.0f, 10000.0f);

	/** Network rate of a corpse or a piece while it moves (falls, is dragged or carried); at rest it sleeps and stops
	 *  replicating altogether (dormancy, UNetRestComponent), and wakes when it moves again (T-312) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1", ClampMax="100"))
	float CorpseNetUpdateFrequency = 20.0f;

	/** A one-shot action received this late or later plays without its sounds (s): a creature coming back in
	 *  relevancy does not growl an old blow again; its clip starts that far in all the same (T-306) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0", Units="s"))
	float ActionLateSoundSeconds = 0.5f;

	/** Musket of the armed tiki of the villages (from the campaign day DA_RunRules.Campaign.ArmedMobsFromDay, T-214) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	FTikiMusketRules Musket;

	/** Tactics of the warriors (T-275, owner 2026-10-10): the role of each class, the pack (never more than a few
	 *  attackers at once, no blow on a player lying down), the torch at night */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactics")
	FTikiPackRules Pack;

	/** Guerrier Coco: stalks, circles in a pack, feints, charges with a telegraphed heavy blow, runs away alone and
	 *  wounded, keeps away from a lit torch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactics")
	FTikiStalkerRules Stalker;

	/** Gardien Crabe: claw guard in front, sideways approach, seize and throw, stands for the others */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactics")
	FTikiBulwarkRules Bulwark;

	/** Veilleur des Cendres: keeps its distance at cover, blinds with ash, leaves embers, backs away, howls */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactics")
	FTikiWatcherRules Watcher;

	/** Clips of a role, none for a Brawler */
	const FTikiTacticClips* GetTacticClips(ETikiArchetype Archetype) const
	{
		switch (Archetype)
		{
		case ETikiArchetype::Stalker: return &Stalker.Clips;
		case ETikiArchetype::Bulwark: return &Bulwark.Clips;
		case ETikiArchetype::Watcher: return &Watcher.Clips;
		default: return nullptr;
		}
	}

	/** Sounds of a role, none for a Brawler */
	const FTikiTacticSounds* GetTacticSounds(ETikiArchetype Archetype) const
	{
		switch (Archetype)
		{
		case ETikiArchetype::Stalker: return &Stalker.Sounds;
		case ETikiArchetype::Bulwark: return &Bulwark.Sounds;
		case ETikiArchetype::Watcher: return &Watcher.Sounds;
		default: return nullptr;
		}
	}

	/** Health of a warrior of a role relative to WarriorHealth */
	float GetHealthScale(ETikiArchetype Archetype) const
	{
		switch (Archetype)
		{
		case ETikiArchetype::Stalker: return Stalker.HealthScale;
		case ETikiArchetype::Bulwark: return Bulwark.HealthScale;
		case ETikiArchetype::Watcher: return Watcher.HealthScale;
		default: return 1.0f;
		}
	}
};
