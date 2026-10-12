// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TikiBlood.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USkeleton;
class UTikiBloodSpeciesRules;
class UTikiRules;
struct FStreamableHandle;

/**
 *  Blood of the tiki creatures (T-153), purely cosmetic and local: every machine spawns it from what it already
 *  receives (a hit multicast, the replicated cut bones, the corpse pieces), a dedicated server never does. The
 *  assets and the look are in the Blood category of DA_TikiRules; this subsystem loads them asynchronously, bounds
 *  what spawns in a frame and how many decals stay, and culls far from the local camera.
 *  Species (T-162): a body bleeds the blood of its species (DA_BloodSpecies, by the skeleton of its mesh: amber sap
 *  of the coconut warrior, blue hemolymph of the crab guardian, glowing lava of the ash watcher...), else the blood of
 *  its rules (the dark red of the humans and beans). A spawn without a body (chunks) takes the species of the wound
 *  bled at the same place in the same frame.
 *
 *  Parameters written to the assets (documented on the rules): Niagara user parameters BloodColor, Direction,
 *  Duration; decal material parameters BloodColor, GrowStartTime, GrowDuration.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UTikiBloodSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UTikiBloodSubsystem();

	/** Subsystem of the world of an object, nullptr on a dedicated server or without a world */
	static UTikiBloodSubsystem* Get(const UObject* WorldContext);

	/** Starts loading the blood assets of these rules in the background (once per rules asset), and the species table */
	void Preload(const UTikiRules* Rules);

	/** A musket ball or a blade entering the body at Location, travelling along Direction: the impact spray and a
	 *  splat on the ground under it (bSplat false: the spray only, for the other pellets of a shotgun shot, T-167).
	 *  Ignore: the body hit (left out of the ground trace, its species gives the blood) */
	void SpawnHit(const UTikiRules* Rules, const FVector& Location, const FVector& Direction, const AActor* Ignore, bool bSplat = true);

	/** Pulsed jet out of a cut, attached to a bone of Body (kept at Location, in the world), along Direction, for
	 *  SliceSprayDuration; Body's skeleton gives the species */
	void SpawnSliceSpray(const UTikiRules* Rules, USceneComponent* Body, FName Bone, const FVector& Location, const FVector& Direction);

	/** Chunks and splashes of a cut or a burst; Source gives the species (none: the wound bled there this frame) */
	void SpawnChunks(const UTikiRules* Rules, const FVector& Location, const FVector& Direction, const AActor* Source = nullptr);

	/** A splat on the ground under Location (vertical trace, Ignore left out; its species gives the blood) */
	void SpawnSplat(const UTikiRules* Rules, const FVector& Location, const AActor* Ignore);

	/** A pool growing on the ground under Location, AgeSeconds after its corpse appeared (a player joining late sees
	 *  it already grown); Ignore: the corpse */
	void SpawnPool(const UTikiRules* Rules, const FVector& Location, const AActor* Ignore, float AgeSeconds);

	virtual void Deinitialize() override;

private:

	/** DA_BloodSpecies (DefaultGame.ini [/Script/monke_island.TikiBloodSubsystem] SpeciesRulesPath; this default otherwise) */
	UPROPERTY(Config)
	FSoftObjectPath SpeciesRulesPath;

	/** Species table, once loaded (nullptr before, or when there is none: every body bleeds the blood of its rules) */
	UPROPERTY(Transient)
	TObjectPtr<UTikiBloodSpeciesRules> SpeciesRules;

	/** True once the species table was asked for (it is loaded once per world) */
	bool bSpeciesRequested = false;

	/** Background load of the species table */
	TSharedPtr<FStreamableHandle> SpeciesLoad;

	/** Background load of the systems of the species, once the table is in (keeps them loaded) */
	TSharedPtr<FStreamableHandle> SpeciesSystemsLoad;

	/** Species of the skeletons met so far (INDEX_NONE: none) */
	TMap<TWeakObjectPtr<const USkeleton>, int32> SpeciesBySkeleton;

	/** Last wound bled: its frame, place and species, for a spawn without a body */
	uint64 WoundFrame = 0;
	FVector WoundLocation = FVector::ZeroVector;
	int32 WoundSpecies = INDEX_NONE;

	/** Loads the species table and its systems in the background (once) */
	void RequestSpecies();

	/** Species of a body: its mesh's skeleton (Body first, else the character mesh or the first skinned mesh of the
	 *  actor), else the wound bled at Location this frame; remembers the wound */
	int32 ResolveSpecies(const AActor* Source, const USceneComponent* Body, const FVector& Location);

	/** Colour of the blood of a species (the rules' own when INDEX_NONE) */
	FLinearColor ColorOf(const UTikiRules* Rules, int32 Species) const;

	/** Blood system of a kind (TikiBloodSystems in the .cpp: impact, slice spray, chunks) for a species: its own copy
	 *  when it has one (nullptr while it loads), else the rules' system */
	UNiagaraSystem* SystemOf(const UTikiRules* Rules, int32 Species, int32 Kind) const;

	/** True when blood may spawn at Location this frame: rules on, not a dedicated server, near the local camera,
	 *  within the frame budget (counts it) */
	bool ConsumeBudget(const UTikiRules* Rules, const FVector& Location);

	/** Niagara system at a point, oriented along Direction, with the colour and direction parameters */
	UNiagaraComponent* SpawnSystem(UNiagaraSystem* System, const FLinearColor& Color, const FVector& Location, const FVector& Direction);

	/** Ground point under Location; false when none */
	bool TraceGround(const UTikiRules* Rules, const FVector& Location, const AActor* Ignore, FVector& OutPoint, FVector& OutNormal) const;

	/** Decal on the ground, faded after Lifetime; the oldest goes beyond MaxActiveDecals */
	UDecalComponent* SpawnDecal(const UTikiRules* Rules, UMaterialInterface* Material, const FLinearColor& Color, const FVector& Point,
		const FVector& Normal, float Size, float Lifetime);

	/** Decals spawned by this subsystem, oldest first (the engine destroys them at the end of their fade) */
	TArray<TWeakObjectPtr<UDecalComponent>> Decals;

	/** Frame the budget counts, and what it spawned in it */
	uint64 BudgetFrame = 0;
	int32 BudgetUsed = 0;

	/** Background loads of the blood assets, one per rules asset */
	TMap<TWeakObjectPtr<const UTikiRules>, TSharedPtr<FStreamableHandle>> Loads;
};
