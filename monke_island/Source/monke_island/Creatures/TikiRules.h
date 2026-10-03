// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TikiRules.generated.h"

class UMaterialInterface;
class UStaticMesh;

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

	/** Speed given to a dying body along the killing blow when it goes limp (cm/s) */
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

	/** Speed given to the pieces of a body cut in two or of a limb cut off, along the blow (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dismember", meta=(ClampMin="0"))
	float CutFlingSpeed = 250.0f;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	TSubclassOf<AActor> ScatterItemClass;

	/** Variants of the scattered item (BP_Totem_A, B, C), one drawn at random per item; ScatterItemClass when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	TArray<TSubclassOf<AActor>> ScatterItemVariants;

	/** Items scattered at the start of a game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	int32 ScatterCount = 30;

	/** Two scattered items are never closer than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float ScatterMinSpacing = 30000.0f;

	/** No scattered item closer than this to a bush of the level (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float ScatterBushClearance = 250.0f;

	/** Height above the ground a scattered item appears at: it drops and settles (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float ScatterDropHeight = 15.0f;

	/** Seed of the draw of the spots; 0 = a new draw every game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	int32 ScatterSeed = 0;

	/** Candidate spots of the scattered items (UE cm, Z = approximate ground): dry sand without bush, out of the
	 *  villages (scripts/map/place_totems.py, collectibles/totem_spots.json) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	TArray<FVector> ScatterSpots;

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

	/** Deepest water a tiki wades into (cm): land creatures, they never swim nor chase a swimmer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature", meta=(ClampMin="0", Units="cm"))
	float MaxWadeDepth = 30.0f;

	/** How far ahead the water depth is checked before each step (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature", meta=(ClampMin="10", Units="cm"))
	float WaterProbeDistance = 90.0f;
};
