// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MaraeRules.generated.h"

class UStaticMesh;

/** One static piece of the marae (stone, altar, coral, bush, palm), in the site frame (cm) */
USTRUCT(BlueprintType)
struct FMaraePiece
{
	GENERATED_BODY()

	/** Mesh of the piece */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	TObjectPtr<UStaticMesh> Mesh;

	/** Placement in the site frame; Z above the ground of the site centre, or of the piece when bFollowGround */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	FTransform Transform;

	/** Set on the ground found under the piece (scatter around the pyramid) instead of the site's ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	bool bFollowGround = false;

	/** Blocks the players (stones, palms); grass, ferns and rubble do not */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	bool bCollision = true;
};

/** One actor of the marae: torch (spawned on every machine when it does not replicate) or sacred object (server) */
USTRUCT(BlueprintType)
struct FMaraeActorSpot
{
	GENERATED_BODY()

	/** Actor (BP_Marae_Torch, BP_Marae_PearlArtifact, BP_Marae_Offerings, BP_Marae_Conch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	TSubclassOf<AActor> ActorClass;

	/** Placement in the site frame; Z above the ground of the site centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	FTransform Transform;
};

/**
 *  Marae des Perles built at runtime (T-075, owner request 2026-10-03: more marae): the layout of the hand placed
 *  marae of Lvl_Raraka (art/export/Props/Marae/marae_layout.json), rebuilt by AMaraeSite in the played fragment.
 *  ACampSpawner draws its spot among the free spots of the fragment before the camps.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UMaraeRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Static pieces (scripts/props/set_marae_rules.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	TArray<FMaraePiece> Pieces;

	/** Torches and sacred objects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	TArray<FMaraeActorSpot> Actors;

	/** Height of the structure over the ground of the site centre (cm, the pyramid is sunk a little) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float StructureOffsetZ = -5.0f;

	/** Vegetation cleared around the centre (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site", meta=(ClampMin="0"))
	float ClearRadius = 1400.0f;

	/** Quiet zone of the marae (tag TikiQuietZone): no tiki, no totem, no camp, no scattered loot (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site", meta=(ClampMin="0"))
	float QuietRadius = 1500.0f;

	/** Marae put in the played fragment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 CountPerFragment = 12;

	/** Marae guaranteed near the arrival (owner feedback 2026-10-04: see one or two from the barque, like the
	 *  villages): the free spots nearest to the players between NearMinDistance and NearMaxDistance, placed first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 GuaranteedNearCount = 2;

	/** See GuaranteedNearCount (cm, from the players at the start) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0", Units="cm"))
	float NearMinDistance = 3000.0f;

	/** See GuaranteedNearCount */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0", Units="cm"))
	float NearMaxDistance = 40000.0f;

	/** No other marae closer than this to the players at the start (cm): they are found by exploring */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float MinDistanceFromPlayers = 8000.0f;
};
