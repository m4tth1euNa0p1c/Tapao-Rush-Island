// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Creatures/TikiMath.h"
#include "TikiDirector.generated.h"

class AFlagCeremonySite;
class APawn;
enum class ETikiWarriorRole : uint8;
class ATikiVillager;
class ATikiWarrior;
class AVillageSpawner;
class UBillboardComponent;
class UInstancedStaticMeshComponent;
class UTikiRules;
enum class ETikiActivity : uint8;

/** A point of interest of a village (TargetPoint tagged VillagePOI, "Village.<Id>", "Activity.<name>"): a villager
 *  walks there, turns to Facing and performs the activity (server only) */
struct FTikiPoi
{
	/** Where the villager stands */
	FVector Location = FVector::ZeroVector;

	/** Direction it faces (the decor) */
	FVector Facing = FVector::ForwardVector;

	/** What it does there */
	ETikiActivity Activity{};

	/** Villager holding the point, if any */
	TWeakObjectPtr<ATikiVillager> Claimant;
};

/** A house of a village: the door the villagers go in by, the gate of its plot in front (marker VillageDoor, its
 *  forward vector towards the gate, tag "Gate.<cm>") */
struct FTikiDoor
{
	/** In front of the door */
	FVector Door = FVector::ZeroVector;

	/** Outside the fence of the plot */
	FVector Gate = FVector::ZeroVector;
};

/** A village of the level, read from its markers (scripts/map/place_villages.py): the zone marker tagged
 *  PacificationZone and "Village.<Id>" (a sphere gives the radius), and the NPC spawn points tagged NpcSpawn,
 *  "Village.<Id>" and "Role.<villager|elder|priest|guardian>"; or a village of the fragment played, built by
 *  AVillageSpawner (scripts/map/fragment_villages.py) (server only) */
struct FTikiVillage
{
	/** Id of the village (V01...), "Director" for the fallback village around the director */
	FName Id;

	/** Centre of the zone (the square, where the flag is planted) */
	FVector Center = FVector::ZeroVector;

	/** Radius of the zone (cm) */
	float Radius = 0.0f;

	/** Where its villagers appear (roles villager, elder, priest) */
	TArray<FVector> VillagerSpawns;

	/** Where its guardians come out of the nearest bush */
	TArray<FVector> GuardianSpawns;

	/** Villagers alive */
	TArray<TWeakObjectPtr<ATikiVillager>> Villagers;

	/** Guardians alive (never replaced) */
	TArray<TWeakObjectPtr<ATikiWarrior>> Guardians;

	/** Points of interest of its routine */
	TArray<FTikiPoi> Pois;

	/** A player is near: the villagers follow their routine */
	bool bActive = false;

	/** World time of the last alarm (tiki out of the bushes) */
	double LastAlarmTime = -1.0e6;

	/** Its houses */
	TArray<FTikiDoor> Doors;

	/** House of the next villager to appear */
	int32 NextDoor = 0;

	/** Its guardians came out (once per village) */
	bool bGuardiansSpawned = false;

	/** A pacifiable zone; a hamlet of a fragment is not, and has no guardian */
	bool bZone = true;

	/** Villagers living there, 0 for UTikiRules::VillagerCount */
	int32 VillagerCount = 0;

	/** Houses (AVillageSpawner) whose villager has not appeared yet, one entry per villager */
	TArray<int32> ResidentHouses;

	/** Villagers of its houses alive (never replaced) */
	TArray<TWeakObjectPtr<ATikiVillager>> Residents;
};

/** A bush where a monster lies in wait (server): it leaps out when a player comes near; every bush zone with enough
 *  bushes has at least one */
struct FTikiAmbushSlot
{
	/** The bush */
	FVector Bush = FVector::ZeroVector;

	/** World time the bush hides a monster again */
	double ArmedAt = 0.0;

	/** Monster out of the bush, if any */
	TWeakObjectPtr<ATikiWarrior> Monster;
};

/**
 *  Tiki life of an island, placed once in the level (server only, not replicated: the creatures replicate
 *  themselves).
 *  - Villagers appear only in the villages of the level (FTikiVillage, docs/features/villages-japonais.md), on
 *    their spawn points, and wander there; coconuts fall there for them to gather. A level without village
 *    marker keeps one village around this actor (VillageRadius).
 *  - The guardians of each village, its designated threats (cahier §6.2), leap out of the bushes nearest to its
 *    guardian points and wait there; they are not replaced once killed.
 *  - At night (ADayNightCycleActor), brings tiki warriors out of the bushes around the players; at dawn they go
 *    back into the ground.
 *  - When a player hits a villager, a tiki warrior leaps out of the bush nearest to the villager and attacks
 *    that player.
 *  - When a player kills a villager, tiki warriors leap out of the bushes around that player.
 *  - Every bush zone (AmbushZoneSize) with enough bushes hides monsters, out of the villages: one leaps out of its
 *    bush when a player comes near, and the bush hides another one some time after it went back or died.
 *  - At the start of the game, scatters a few rare items (the totems) on spots of dry sand far from the bushes.
 *  - The houses of the fragment villages built from a kit with residents (UVillageHouseKit::Residents) get their
 *    villager once a player comes near the village; it lives inside and watches its house. An object of the house
 *    carried out of it while its villager is home is a theft: the villager panics (owner request 2026-10-07).
 *  - The flag ceremony of a village (AFlagCeremonySite) gathers its villagers in a ring around the officiant; those
 *    of the houses bring their relic and lay it around him once the flag is up.
 *  Debug (non shipping): monke.TikiDefender spawns a defender against the local player; monke.TikiKillVillager
 *  [cut], monke.TikiCutArm and monke.TikiCutCorpse act on the villager or corpse nearest to the local player.
 */
UCLASS()
class MONKE_ISLAND_API ATikiDirector : public AActor
{
	GENERATED_BODY()

public:

	ATikiDirector();

	/** Returns the director of the given world, or nullptr */
	static ATikiDirector* Get(const UObject* WorldContextObject);

	/** Rules of the creatures (DA_TikiRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TObjectPtr<UTikiRules> Rules;

	/** Villager Blueprint (BP_TikiVillager) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TSubclassOf<ATikiVillager> VillagerClass;

	/** Warrior Blueprints, one drawn at random per spawn (BP_TikiCocoWarrior…) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TArray<TSubclassOf<ATikiWarrior>> WarriorClasses;

	/** Coconut dropped for the villagers (BP_CoconutPickup) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TSubclassOf<AActor> CoconutClass;

	/** Radius of the village around this actor when the level has no village marker (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki", meta=(ClampMin="100"))
	float VillageRadius = 4000.0f;

	/** Debug (non shipping): the villagers nearest to the first player to kill at the next thinks */
	UPROPERTY(EditAnywhere, Transient, Category="Debug")
	int32 DebugKillVillagers = 0;

	/** Debug (non shipping): the same, with a cut blow (the body falls in two halves) */
	UPROPERTY(EditAnywhere, Transient, Category="Debug")
	int32 DebugCutKillVillagers = 0;

	/** Debug (non shipping): arms to cut off the villagers nearest to the first player */
	UPROPERTY(EditAnywhere, Transient, Category="Debug")
	int32 DebugCutArms = 0;

	/** Debug (non shipping): whole corpses nearest to the first player to cut in two */
	UPROPERTY(EditAnywhere, Transient, Category="Debug")
	int32 DebugCutCorpses = 0;

	/** Debug (non shipping): the warriors nearest to the first player to kill at the next thinks (they drop their weapon) */
	UPROPERTY(EditAnywhere, Transient, Category="Debug")
	int32 DebugKillWarriors = 0;

	/** Server: reserves the free coconut nearest to Location within Radius for a villager; nullptr when none */
	AActor* ClaimCoconutNear(ATikiVillager* Villager, const FVector& Location, float Radius);

	/** Server: frees a reserved coconut */
	void ReleaseCoconut(AActor* Coconut);

	/** Server: the villager picks the coconut up; true when it was still there */
	bool CollectCoconut(ATikiVillager* Villager, AActor* Coconut);

	/** Server: a player hit a villager; a warrior leaps out of the nearest bush against that player */
	ATikiWarrior* CallDefender(ATikiVillager* Villager, APawn* Attacker);

	/** Server: a player hit a villager of a village: all its villagers run away, its guardians and tiki warriors out of
	 *  the bushes all around it go for the attacker (once per AlarmCooldown); without village, CallDefender */
	void RaiseAlarm(ATikiVillager* Villager, APawn* Attacker);

	/** Server: a player killed a villager: its village is alarmed as for a hit, and KillWarriorCount tiki warriors
	 *  leap out of the bushes around the killer (KillRingRadius), up to KillMaxAlivePerPlayer tiki after that player */
	void AvengeVillager(ATikiVillager* Villager, APawn* Killer);

	/** True when the village follows its routine: a player is within RoutineActivationRadius of it */
	bool IsVillageActive(int32 VillageIndex) const;

	/** Server: reserves a free point of interest of the village for the villager; its index, or INDEX_NONE */
	int32 ClaimPoi(ATikiVillager* Villager, int32 VillageIndex);

	/** Server: frees the point of interest held by the villager */
	void ReleasePoi(ATikiVillager* Villager);

	/** Copy of a point of interest; false when the indices are out of range */
	bool GetPoi(int32 VillageIndex, int32 PoiIndex, FTikiPoi& OutPoi) const;

	/** Ground point above the water within [MinRadius, MaxRadius] of Center, drawn at random (a few tries) */
	bool FindGroundPointNear(const FVector& Center, float MinRadius, float MaxRadius, FVector& OutPoint) const;

	/** Ground under a horizontal position, above the water; false over the sea or the lagoon */
	bool ProjectToGround(const FVector& Location, FVector& OutPoint) const;

	/** Bush instance nearest to Location within Radius; false when none */
	bool FindNearestBush(const FVector& Location, float Radius, FVector& OutBush);

	/** Spawns a warrior out of the bush nearest to Near (or out of the ground there) facing Target (server) */
	ATikiWarrior* SpawnWarriorFromBush(const FVector& Near, float BushRadius, APawn* Target, ETikiWarriorRole WarriorRole);

	/** Server: the howl of a watcher (T-275): up to MaxCount monsters hidden in the bushes within Radius of Where leap out,
	 *  nearest first, and run to look there (they attack only if they then see the foe), within the cap of monsters
	 *  around Suspect (AmbushMaxAlivePerPlayer); how many came out */
	int32 WakeAmbushers(const FVector& Where, APawn* Suspect, float Radius, int32 MaxCount);

	/** Server: the flag ceremony of a site starts: the villagers of its village (and of the villages near it, by its
	 *  rules) leave their routine and take their places in a ring around the officiant, in the order they come from */
	void StartGathering(AFlagCeremonySite* Site);

	/** Server: it ended: the flag up, they lay their relics and cheer; interrupted, they go home with them */
	void FinishGathering(AFlagCeremonySite* Site, bool bRaised);

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Editor icon */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBillboardComponent> Sprite;

	/** Villages of the level (at least the fallback one around the director once BeginPlay ran) */
	TArray<FTikiVillage> Villages;

	/** Night warriors alive */
	TArray<TWeakObjectPtr<ATikiWarrior>> NightWarriors;

	/** Defenders alive */
	TArray<TWeakObjectPtr<ATikiWarrior>> Defenders;

	/** Coconuts on the ground in the village area */
	TArray<TWeakObjectPtr<AActor>> Coconuts;

	/** Coconuts reserved by a villager */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<ATikiVillager>> Claims;

	/** Last defender call per player */
	TMap<TWeakObjectPtr<APawn>, double> LastDefenderTime;

	/** Bush positions of the level by grid cell (BushCellSize), filled once from the instanced meshes */
	TMap<FIntPoint, TArray<FVector>> BushCells;

	/** The bush search ran */
	bool bBushesCached = false;

	/** World time of the last coconut drop and night spawn */
	double LastCoconutDropTime = -1.0e6;
	double LastNightSpawnTime = -1.0e6;

	/** Timer of Think */
	FTimerHandle ThinkTimer;

	/** Periodic decisions (server) */
	void Think();

	/** Keeps the villagers and the coconuts of every village */
	void TickVillage(double Now);

	/** Reads the villages from their markers (or makes the fallback one around the director) */
	void FindVillages();

	/** The villages of the fragment played (AVillageSpawner) replace those of the markers, far from the game */
	void UseFragmentVillages(const AVillageSpawner* Spawner);

	/** The guardian delay is over: villages known later get their guardians at once */
	bool bGuardianTimeReached = false;

	/** Spawns one villager of the village on a free spawn point; false when none could appear */
	bool SpawnVillager(int32 VillageIndex);

	/** Spawns the villager of the next house of the village waiting for one, inside; false when none could appear */
	bool SpawnResident(int32 VillageIndex);

	/** An object of a house out of it while its villager is home: the villager panics at the thief */
	void CheckTheft();

	/** Player who took this object: the one holding it, else the nearest within ThiefSearchRadius; nullptr when none */
	APawn* FindThief(const AActor* Content) const;

	/** Spawner of the fragment villages, once they stand */
	TWeakObjectPtr<AVillageSpawner> FragmentSpawner;

	/** Villagers gathered by each flag site during its ceremony */
	TMap<TWeakObjectPtr<AFlagCeremonySite>, TArray<TWeakObjectPtr<ATikiVillager>>> Gatherings;

	/** Starts or stops the routine of each village from the distance of the players */
	void UpdateActivity();

	/** Brings the guardians of every village out of the bushes, once */
	void SpawnGuardians();

	/** Brings warriors out at night, sends them back at dawn */
	void TickNight(double Now);

	/** Finds the bush meshes among the instanced meshes of the level and files their instances in the grid */
	void CacheBushes();

	/** Grid cell of a world position */
	static FIntPoint BushCellOf(const FVector& Location);

	/** Warrior class drawn at random, or nullptr */
	TSubclassOf<ATikiWarrior> PickWarriorClass() const;

	/** Defenders alive hunting this player (alarm, kill and hit warriors) */
	int32 CountDefendersAgainst(const APawn* Player) const;

	/** Hidden monsters by bush zone (server) */
	TMap<FIntPoint, TArray<FTikiAmbushSlot>> AmbushZones;

	/** Slots whose monster is out of its bush: zone and index (server) */
	TArray<TPair<FIntPoint, int32>> OutAmbushSlots;

	/** World time a monster last leapt out around each player (server) */
	TMap<TWeakObjectPtr<APawn>, double> LastAmbushTime;

	/** Groups the bushes by zone and hides the monsters there, out of the villages (server, once the bushes are known) */
	void BuildAmbushZones();

	/** Brings the hidden monsters out when a player comes near, re-arms the bushes (server) */
	void TickAmbush(double Now);

	/** Bush zone of a world position */
	FIntPoint AmbushCellOf(const FVector& Location) const;

	/** Runs the debug requests (DebugKillVillagers...) (server, non shipping) */
	void RunDebugActions();

	/** Items scattered on the sand this game (server) */
	TArray<TWeakObjectPtr<AActor>> ScatteredItems;

	/** Initializes a finite, server-only Poisson/house placement job after the fragment villages stand */
	void ScatterItems();

	/** Processes a bounded number of candidates; only reschedules while initial placement is pending */
	void PlaceTotemBatch();

	/** Floor trace: below the roof indoors, vegetation skipped outdoors; reports a specific rejection */
	TikiMath::ETotemReject TraceTotemFloor(const TikiMath::FTotemRequest& Request, FVector& OutFloor) const;

	/** Spawns a replicated item, seats its bounds on the floor and immediately sleeps its physical bodies */
	bool SpawnRestingTotem(const TikiMath::FTotemRequest& Request, const FVector& Floor);

	/** Finishes or cancels initial placement and releases temporary buffers */
	void FinishTotemPlacement(bool bCancelled);

	/** Class references held for the finite placement job */
	UPROPERTY(Transient)
	TArray<TSubclassOf<AActor>> TotemClasses;

	TArray<TikiMath::FTotemRequest> TotemRequests;
	TArray<FVector> GroundTotemTaken;
	TArray<FVillageClearArea> TotemClearAreas;
	TArray<FSphere> TotemQuietZones;
	TMap<TikiMath::ETotemReject, int32> TotemRejects;
	FRandomStream TotemStream;
	FTimerHandle TotemTimer;
	int32 NextTotemRequest = 0;
	int32 HouseTotemsPlaced = 0;
	int32 HouseTotemsRequested = 0;
	int32 GroundTotemLimit = 0;
	float GroundTotemMinSpacing = 0.0f;
	/** Mean sea level frozen for this initial sample: waves and frame rate must not change the seeded draw */
	float TotemSeaLevel = 0.0f;
	double TotemWaitStarted = -1.0;
	double TotemPlacementStarted = 0.0;
	bool bTotemPlacementStarted = false;

	/** True when it is night on the island */
	bool IsNight() const;
};
