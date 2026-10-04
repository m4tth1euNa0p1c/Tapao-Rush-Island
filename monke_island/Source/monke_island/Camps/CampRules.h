// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CampRules.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** Decor pieces of a fishermen camp */
UENUM(BlueprintType)
enum class ECampPieceKind : uint8
{
	Hut,
	Shelter,
	Campfire,
	LogSeat,
	DryingRack
};

/** One decor piece of the camp, placed in the camp frame (cm, +X forward of the camp, as in the maquette) */
USTRUCT(BlueprintType)
struct FCampPiece
{
	GENERATED_BODY()

	/** Mesh of the piece (see UCampRules meshes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	ECampPieceKind Kind = ECampPieceKind::Hut;

	/** Placement in the camp frame; Z is the height above the ground found under the piece */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	FTransform Transform;
};

/** An item spawned at the camp by the server (a tool worth stealing, a fish to eat) */
USTRUCT(BlueprintType)
struct FCampItem
{
	GENERATED_BODY()

	/** Item (BP_Camp_*: physics prop with a value, or consumable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	TSubclassOf<AActor> ItemClass;

	/** Placement in the camp frame; Z above the ground under it (or above the shelter table when bOnTable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	FTransform Transform;
};

/** Camp spots of one fragment of the expedition (one fragment is played per game) */
USTRUCT(BlueprintType)
struct FCampFragmentSpots
{
	GENERATED_BODY()

	/** Fragment (FRG_<Ile>_<NN>) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	FName FragmentId;

	/** Free spots inside its land, away from its village sites (UE cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	TArray<FVector> Spots;
};

/**
 *  Fishermen camps (maquette campement-pecheurs of the second PC, T-082): hut on stilts, work shelter, campfire, log
 *  seats, drying rack; tools that can be stolen (rod, net, trap, basket), fish to eat, common loot inside the hut.
 *  ACampSpawner puts a few camps at random free spots each game; each camp clears the vegetation around it.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UCampRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UCampRules();

	// ------------------------------------------------------------------ decor

	/** Hut on stilts (SM_Camp_Hut, sockets bed / basket_spot inside) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TObjectPtr<UStaticMesh> HutMesh;

	/** Work shelter (SM_Camp_WorkShelter, socket table) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TObjectPtr<UStaticMesh> ShelterMesh;

	/** Campfire (SM_Camp_Campfire, socket fire) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TObjectPtr<UStaticMesh> CampfireMesh;

	/** Log seat (SM_Camp_LogSeat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TObjectPtr<UStaticMesh> LogSeatMesh;

	/** Fish drying rack (SM_Camp_DryingRack) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TObjectPtr<UStaticMesh> DryingRackMesh;

	/** Layout of the decor (maquette: hut on the left, shelter on the right, fire in front) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TArray<FCampPiece> Pieces;

	/** Light of the campfire: intensity (cd), radius (cm) and flicker (light function) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor", meta=(ClampMin="0"))
	float FireLightIntensity = 3.0f;

	/** See FireLightIntensity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor", meta=(ClampMin="0"))
	float FireLightRadius = 600.0f;

	/** See FireLightIntensity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decor")
	TObjectPtr<UMaterialInterface> FireLightFunction;

	// ------------------------------------------------------------------ items

	/** Tools lying around the camp: worth stealing (physics props with a value) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TArray<FCampItem> Tools;

	/** Fish to eat (consumable, restores health) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TSubclassOf<AActor> FishClass;

	/** Where the fish lie (camp frame) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TArray<FTransform> FishSpots;

	/** Common loot found inside the hut (BP_Loot_*), drawn at random */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TArray<TSubclassOf<AActor>> HutLootClasses;

	/** Number of loot items inside the hut */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FInt32Interval HutLootCount = FInt32Interval(2, 4);

	/** Spots of the hut's floor where the loot lies (hut sockets), with a random spread (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TArray<FName> HutLootSockets = { TEXT("bed"), TEXT("basket_spot") };

	/** See HutLootSockets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items", meta=(ClampMin="0"))
	float HutLootSpreadCm = 35.0f;

	// ------------------------------------------------------------------ site

	/** Vegetation cleared around the camp centre (cm): bushes, palms and grass instances are taken away */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site", meta=(ClampMin="0"))
	float ClearRadius = 900.0f;

	/** Quiet zone of the camp (tag TikiQuietZone): no tiki out of its bushes, no totem, no scattered loot (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site", meta=(ClampMin="0"))
	float QuietRadius = 1100.0f;

	// ------------------------------------------------------------------ spawning

	/** Camps put on the island each game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 Count = 5;

	/** Two camps are never closer than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float MinSpacing = 30000.0f;

	/** Only spots within this distance of a PlayerStart (cm); 0 = the whole island */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float MaxDistanceFromStart = 250000.0f;

	/** No camp closer than this to a PlayerStart (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float MinDistanceFromStart = 4000.0f;

	/** Delay after the start before the camps appear (s): before the loot and the totems, which avoid their zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0", Units="s"))
	float SpawnDelay = 1.0f;

	/** Seed of the draw; 0 = a new draw every game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	int32 Seed = 0;

	/** Free flat spots of land with room for a camp (scripts/map/place_camps.py, collectibles/camp_spots.json) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TArray<FVector> Spots;

	/** Spots by fragment (camp_spots.json by_fragment): when the world plays a fragment, its camps go there, near
	 *  the players whatever the PlayerStarts; the island wide Spots stay for a map opened without expedition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TArray<FCampFragmentSpots> FragmentSpots;

	/** Camps put in the played fragment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 CountPerFragment = 3;

	/** Two camps of a fragment are never closer than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float FragmentMinSpacing = 12000.0f;

	/** Spots of a fragment, nullptr when it has none */
	const FCampFragmentSpots* FindFragment(FName FragmentId) const
	{
		return FragmentId.IsNone() ? nullptr : FragmentSpots.FindByPredicate([FragmentId](const FCampFragmentSpots& Entry) { return Entry.FragmentId == FragmentId; });
	}

	/** Mesh of a decor kind */
	UStaticMesh* GetPieceMesh(ECampPieceKind Kind) const;
};
