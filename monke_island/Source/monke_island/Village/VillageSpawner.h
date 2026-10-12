// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/VillageLayout.h"
#include "Village/VillageRules.h"
#include "VillageSpawner.generated.h"

class AFlagCeremonySite;
class AVillageSpawner;
class UInstancedStaticMeshComponent;
class ULightComponent;
class UVillageDoorsComponent;
class UVillageHouseKit;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVillagesBuilt, const AVillageSpawner*);

/** A house of the villages built from a kit, as every machine builds it; its contents on the server */
struct FVillageHouseInfo
{
	/** Its kit (held by DA_VillageRules) */
	const UVillageHouseKit* Kit = nullptr;

	/** Its placement (world) */
	FTransform Transform;

	/** Index of its village in GetVillages() */
	int32 Village = INDEX_NONE;

	/** In front of its main door, outside (world) */
	FVector Door = FVector::ZeroVector;

	/** Outside the fence of its plot (world), the door when it has none */
	FVector Gate = FVector::ZeroVector;

	/** Its Tidal Relic while it belongs to the house (server) */
	TWeakObjectPtr<AActor> Relic;

	/** Where the relic stood when the house was filled (world, server) */
	FTransform RelicHome;

	/** Its everyday objects while they belong to the house (server) */
	TArray<TWeakObjectPtr<AActor>> Items;
};

/**
 *  Builds the Japanese villages of the expedition fragment played by the level (docs/features/villages-japonais.md):
 *  one per pacifiable zone, generated offline by scripts/map/fragment_villages.py (DA_Villages_<FRG>).
 *  The server chooses the fragment once — the Island option of the travel URL (a game from L_Boot), otherwise the
 *  fragment of the arrival the boat start used (UBoatStartSubsystem, a PIE opened on the level), otherwise the
 *  fragment whose boat arrival is nearest to a player — and replicates it; every machine
 *  then builds the same villages: houses and props as instanced meshes with collision, the vegetation under them
 *  moved out of sight with the shapes of scripts/map/village_clearing.py, the lights and the ambience (not on a dedicated
 *  server). ATikiDirector peoples them on the server (OnVillagesBuilt). Nothing is placed in the level but this actor.
 *  A house whose mesh has a kit in DA_VillageRules (T-186: SM_Minka -> DA_MinkaKit) is built from it: its shell, solid
 *  all over, its sliding doors (UVillageDoorsComponent) and, on the server, its contents: a Tidal Relic and a few
 *  everyday objects resting on their slots, asleep, replicated. The server also puts a flag site (AFlagCeremonySite)
 *  on the square of every pacifiable village; once its flag is up, the sealed houses of the village open.
 */
UCLASS()
class MONKE_ISLAND_API AVillageSpawner : public AActor
{
	GENERATED_BODY()

public:

	AVillageSpawner();

	/** Rules of the fragment villages (DA_VillageRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	TObjectPtr<UVillageRules> Rules;

	/** The fragments the level can play and their villages (filled by scripts/map/fragment_villages_ue.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	TArray<FVillageFragmentEntry> Fragments;

	/** Fires on every machine once the villages of the fragment stand */
	FOnVillagesBuilt OnVillagesBuilt;

	/** First spawner of the world, or nullptr */
	static AVillageSpawner* Find(const UObject* WorldContextObject);

	/** True once the villages of the fragment stand */
	bool IsBuilt() const { return bBuilt; }

	/** Fragment played (None until the server chose it, or when the level plays none) */
	FName GetActiveFragment() const { return ActiveFragment; }

	/** The villages standing, empty before OnVillagesBuilt */
	const TArray<FVillageLayout>& GetVillages() const;

	/** Sliding doors of the houses built from a kit */
	UVillageDoorsComponent* GetDoors() const { return Doors; }

	/** Objects put in the houses this game, relics included (server) */
	int32 GetHouseContentCount() const { return HouseContents.Num(); }

	/** Houses built from a kit, in the order every machine builds them */
	int32 GetHouseCount() const { return Houses.Num(); }

	/** A house built from a kit, nullptr out of range */
	const FVillageHouseInfo* GetHouse(int32 Index) const { return Houses.IsValidIndex(Index) ? &Houses[Index] : nullptr; }

	/** House whose inside holds Location (its kit's Interior grown by Margin), INDEX_NONE when none */
	int32 FindHouseAt(const FVector& Location, float Margin) const;

	/** Server: an object no longer belongs to its house (given away by its villager, stolen and seen) */
	void ForgetHouseContent(int32 House, const AActor* Content);

	/** Server: an object comes back to its house (a relic put back after an interrupted ceremony) */
	void RestoreHouseRelic(int32 House, AActor* Relic);

	/** Server: the flag of the village is up: its sealed houses (the shrine) open for good */
	void OpenSealedHouses(int32 Village);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Sliding doors of the houses built from a kit (T-186) */
	UPROPERTY(VisibleAnywhere, Category="Village")
	TObjectPtr<UVillageDoorsComponent> Doors;

	/** Fragment chosen by the server, replicated to the clients that build the same villages */
	UPROPERTY(ReplicatedUsing=OnRep_ActiveFragment, VisibleInstanceOnly, Category="Village")
	FName ActiveFragment;

	/** Data asset of the villages standing */
	UPROPERTY(Transient)
	TObjectPtr<UVillageSetDefinition> ActiveSet;

	/** Instanced meshes of the villages, one per mesh and village */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> MeshComponents;

	/** Lights of the villages */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> LightComponents;

	/** Ambience actors of the villages, local to this machine */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> AmbienceActors;

	/** Retry of the server while no player is near an arrival yet */
	FTimerHandle ResolveTimer;

	/** World time the server started looking for the players */
	double ResolveStartTime = 0.0;

	/** The villages stand */
	bool bBuilt = false;

	/** An object waiting to be put in a house (server) */
	struct FHouseContent
	{
		/** Its class, held by the kit */
		TSubclassOf<AActor> Class;
		/** On its slot (world) */
		FTransform Transform;
		/** Gap under it */
		float Clearance = 0.0f;
		/** Its house in Houses */
		int32 House = INDEX_NONE;
		/** The relic of the house */
		bool bRelic = false;
	};

	/** Houses built from a kit */
	TArray<FVillageHouseInfo> Houses;

	/** Flag sites put on the squares (server) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AFlagCeremonySite>> FlagSites;

	/** Objects drawn for the houses, spawned a batch per step (server) */
	TArray<FHouseContent> PendingContents;

	/** Objects put in the houses (server) */
	TArray<TWeakObjectPtr<AActor>> HouseContents;

	/** Objects spawned per step */
	int32 ContentBatch = 40;

	/** Relics drawn for the houses (server, for the log) */
	int32 RelicsQueued = 0;

	FTimerHandle ContentTimer;

	UFUNCTION()
	void OnRep_ActiveFragment();

	/** Server: the fragment of the URL, otherwise the one whose arrival a player is near (retried until a timeout) */
	void ResolveFragment();

	/** Server: plays this fragment, replicates it and builds its villages */
	void SetActiveFragment(FName FragmentId);

	/** Builds the villages of a fragment on this machine */
	void Build(FName FragmentId);

	/** Moves the vegetation instances under the villages out of sight, their indices kept (before anything reads the bushes) */
	int32 ClearVegetation(const TArray<FVillageLayout>& Villages) const;

	/** Instanced meshes of a village, with collision, and the doors and contents of its houses built from a kit;
	 *  returns the instance count */
	int32 BuildMeshes(const FVillageLayout& Village, int32 VillageIndex, FRandomStream& ContentStream);

	/** Server: draws the relic and the everyday objects of a house built from a kit */
	void QueueHouseContents(const UVillageHouseKit& Kit, const FTransform& House, int32 HouseIndex, FRandomStream& Stream);

	/** Registers a house built from a kit, with its door and the gate of its plot from the layout */
	int32 AddHouseInfo(const UVillageHouseKit& Kit, const FTransform& House, const FVillageLayout& Village, int32 VillageIndex);

	/** Server: a flag site on the square of every pacifiable village that has none yet */
	void SpawnFlagSites();

	/** Server: puts the next batch of PendingContents in the houses */
	void SpawnContentBatch();

	/** Lights and ambience of a village; returns the light count */
	int32 BuildLights(const FVillageLayout& Village);
};
