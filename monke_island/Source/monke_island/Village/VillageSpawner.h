// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/VillageLayout.h"
#include "Village/VillageRules.h"
#include "VillageSpawner.generated.h"

class AVillageSpawner;
class UInstancedStaticMeshComponent;
class ULightComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVillagesBuilt, const AVillageSpawner*);

/**
 *  Builds the Japanese villages of the expedition fragment played by the level (docs/features/villages-japonais.md):
 *  one per pacifiable zone, generated offline by scripts/map/fragment_villages.py (DA_Villages_<FRG>).
 *  The server chooses the fragment once — the Island option of the travel URL (a game from L_Boot), otherwise the
 *  fragment of the arrival the boat start used (UBoatStartSubsystem, a PIE opened on the level), otherwise the
 *  fragment whose boat arrival is nearest to a player — and replicates it; every machine
 *  then builds the same villages: houses and props as instanced meshes with collision, the vegetation under them
 *  moved out of sight with the shapes of scripts/map/village_clearing.py, the lights and the ambience (not on a dedicated
 *  server). ATikiDirector peoples them on the server (OnVillagesBuilt). Nothing is placed in the level but this actor.
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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

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

	/** Instanced meshes of a village, with collision; returns the instance count */
	int32 BuildMeshes(const FVillageLayout& Village);

	/** Lights and ambience of a village; returns the light count */
	int32 BuildLights(const FVillageLayout& Village);
};
