// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Village/VillageLayout.h"
#include "VillageRules.generated.h"

class AVillageAmbienceActor;
class UVillageAmbienceRules;

/** Kind of plant of the vegetation, for the clearing margins (scripts/map/village_clearing.py) */
UENUM(BlueprintType)
enum class EVillagePlant : uint8
{
	/** A coconut palm: its trunk */
	Palm,
	/** A young palm or pandanus: a wide crown low down */
	Young,
	/** A bush: naupaka, miki-miki, tahinu, pandanus */
	Bush,
	/** A fallen palm lying on the ground */
	Fallen,
	/** Ground cover and small props: ferns, grass, coconuts, coral, driftwood */
	Ground
};

/** A mesh name prefix and the plant it names (first match wins) */
USTRUCT(BlueprintType)
struct FVillagePlantRule
{
	GENERATED_BODY()

	/** Start of the mesh name (SM_CoconutPalm_Fallen) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	FString MeshPrefix;

	/** Plant it is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	EVillagePlant Plant = EVillagePlant::Ground;
};

/** Margin kept beyond a clearing area for each plant (cm): its crown radius and a gap */
USTRUCT(BlueprintType)
struct FVillageClearMargins
{
	GENERATED_BODY()

	/** Coconut palm (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float Palm = 0.0f;

	/** Young palm or pandanus (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float Young = 0.0f;

	/** Bush (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float Bush = 0.0f;

	/** Fallen palm (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float Fallen = 0.0f;

	/** Ground cover and small props (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village", meta=(ClampMin="0"))
	float Ground = 0.0f;

	/** Margin for one plant */
	float For(EVillagePlant Plant) const
	{
		switch (Plant)
		{
		case EVillagePlant::Palm: return Palm;
		case EVillagePlant::Young: return Young;
		case EVillagePlant::Bush: return Bush;
		case EVillagePlant::Fallen: return Fallen;
		default: return Ground;
		}
	}
};

/** Where the villages of a fragment are and which data asset holds them */
USTRUCT(BlueprintType)
struct FVillageFragmentEntry
{
	GENERATED_BODY()

	/** Fragment (FRG_Raraka_19) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	FName FragmentId;

	/** Arrival of the boat at the fragment (UE cm): a game started without the fragment in its URL (a PIE opened
	 *  directly on the level) plays the fragment whose arrival is nearest to a player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	FVector Arrival = FVector::ZeroVector;

	/** Its villages (DA_Villages_<FRG>), loaded only when played */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village")
	TSoftObjectPtr<UVillageSetDefinition> Set;
};

/**
 *  Villages of the expedition fragments built at runtime (DA_VillageRules, AVillageSpawner,
 *  docs/features/villages-japonais.md): which fragment is played, how the houses collide, the lights, and the
 *  vegetation cleared under the villages.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UVillageRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UVillageRules();

	/** Seconds after the start before the server looks for the players, when the URL names no fragment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fragment", meta=(ClampMin="0", Units="s"))
	float ResolveDelay = 0.5f;

	/** Seconds between two looks for the players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fragment", meta=(ClampMin="0.1", Units="s"))
	float ResolveInterval = 0.5f;

	/** Seconds after which the server gives up: no player near an arrival, the level plays no fragment villages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fragment", meta=(ClampMin="1", Units="s"))
	float ResolveTimeout = 20.0f;

	/** A player this close to the arrival of a fragment plays it (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fragment", meta=(ClampMin="1000"))
	float ArrivalMatchRadius = 100000.0f;

	/** The houses block the camera too, like the villages of the level (PCG_Raraka_Village, BlockAll): the camera comes
	 *  in front of a wall instead of looking through it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Houses")
	bool bHousesBlockCamera = true;

	/** Village ambience (lights that follow the day, butterflies, fireflies); no ambience, no light */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambience")
	TObjectPtr<UVillageAmbienceRules> AmbienceRules;

	/** Class of the ambience of each village */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambience")
	TSubclassOf<AVillageAmbienceActor> AmbienceClass;

	/** Colour of a house lamp */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights")
	FColor InteriorColor = FColor(255, 184, 107);

	/** Colour of a stone lantern */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights")
	FColor LanternColor = FColor(255, 160, 82);

	/** Size of a light source (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights", meta=(ClampMin="0"))
	float LightSourceRadius = 4.0f;

	/** A light farther than this from the camera is not drawn (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lights", meta=(ClampMin="0"))
	float LightMaxDrawDistance = 9000.0f;

	/** The vegetation cleared is moved this far down instead of removed: every instance keeps its index (the coconut
	 *  falls count nuts per instance), like the camps and marae of FSiteClearing (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing", meta=(ClampMin="1000"))
	float HiddenDepth = 100000.0f;

	/** Only the instanced meshes under these paths are cleared (the vegetation and the beach props) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	TArray<FString> ClearMeshPaths;

	/** Plant of a mesh, from its name (first match wins, Ground otherwise) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	TArray<FVillagePlantRule> PlantRules;

	/** Margins around a building (eaves included): a naupaka is up to 4.8 m wide */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	FVillageClearMargins BuildingMargins;

	/** Margins around a fence or a stone lantern: clean front yards */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	FVillageClearMargins SmallMargins;

	/** Margins around the square */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	FVillageClearMargins PlazaMargins;

	/** Margins along the road */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	FVillageClearMargins RoadMargins;

	/** Margins along the path to the shrine */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clearing")
	FVillageClearMargins PathMargins;

	/** Margin for a plant beyond an area of a kind (cm) */
	float GetMargin(EVillageClearKind Kind, EVillagePlant Plant) const;

	/** Largest margin of all (cm) */
	float GetMaxMargin() const;

	/** Plant of a mesh name */
	EVillagePlant GetPlant(const FString& MeshName) const;

	/** True when the instances of a mesh (its path name) may be cleared */
	bool IsClearable(const FString& MeshPath) const;
};
