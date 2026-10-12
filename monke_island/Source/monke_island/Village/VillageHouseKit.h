// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VillageHouseKit.generated.h"

class ULootRules;
class UStaticMesh;

/** A sliding panel of a house (shoji, fusuma, board door), in the house frame (scripts/props/minka_doors.py) */
USTRUCT(BlueprintType)
struct FVillageHousePanel
{
	GENERATED_BODY()

	/** Mesh of the panel, its pivot at its bottom corner on its track (SM_Minka_Shoji_82x210) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	TObjectPtr<UStaticMesh> Mesh;

	/** Pivot when closed (house frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	FVector ClosedLocation = FVector::ZeroVector;

	/** Yaw (house frame, degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	float Yaw = 0.0f;

	/** Offset from closed to fully open, along its track (house frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	FVector Travel = FVector::ZeroVector;

	/** Turn about its pivot when fully open (degrees): a hinged leaf (the shrine doors); 0 for a sliding one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	float OpenYaw = 0.0f;

	/** Share of the travel it slides when its doorway opens: 1 for the leaf that slides behind the other, 0 for that other one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0", ClampMax="1"))
	float OpenFraction = 1.0f;

	/** Doorway it closes (index in the Doorways of the kit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0"))
	int32 Doorway = 0;
};

/** An opening of the house closed by its panels: it opens while a pawn is near it */
USTRUCT(BlueprintType)
struct FVillageHouseDoorway
{
	GENERATED_BODY()

	/** Name in the maquette (Minka_Shoji_X00_190) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	FName Name;

	/** Middle of the bay at the level of its track (house frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	FVector Center = FVector::ZeroVector;

	/** Direction of the track (house frame, degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	float Yaw = 0.0f;

	/** Half the width of the bay (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0", Units="cm"))
	float HalfWidth = 80.0f;

	/** Height of the bay above its track (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0", Units="cm"))
	float Height = 210.0f;
};

/** A spot where an object is put in the house, on the surface it rests on (house frame, cm) */
USTRUCT(BlueprintType)
struct FVillageHouseSlot
{
	GENERATED_BODY()

	/** On the floor, the tansu, the futon... (house frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	FVector Location = FVector::ZeroVector;

	/** Yaw of the object (house frame, degrees); the everyday objects turn up to ItemYawJitter either way */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	float Yaw = 0.0f;

	/** Room, for the logs and the level designers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	FName Room;
};

/** A waypoint inside the house its villager walks between (scripts/props/minka_nav.py): straight lines along its
 *  links stay clear of the walls and the furniture, the doorways of the links open as it comes */
USTRUCT(BlueprintType)
struct FVillageHouseNode
{
	GENERATED_BODY()

	/** On the floor (house frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	FVector Location = FVector::ZeroVector;

	/** Room, for the logs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	FName Room;

	/** Nodes reached in a straight line from this one (both ways) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	TArray<int32> Links;

	/** What the villager does here: work, read, pray, rest or watch (a point of its routine); none = a passage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	FName Activity;

	/** Direction it faces for the activity (house frame, degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	float Yaw = 0.0f;
};

/**
 *  A village house built from parts (T-186, DA_MinkaKit, docs/features/villages-japonais.md): the shell without its
 *  sliding panels, solid all over (roof, beams, furniture), its panels that slide open while someone is near them,
 *  and what lies inside: one Tidal Relic and a few everyday objects. AVillageSpawner builds every house of the
 *  fragment villages whose layout mesh is SourceMesh from this kit (scripts/props/minka_doors.py writes the parts).
 *  Its villagers (Residents) live inside along the waypoints of Nodes (scripts/props/minka_nav.py); a shrine kit keeps
 *  its doors shut until the flag of the village is raised (bSealedUntilFlag).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UVillageHouseKit : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Mesh of the village layouts this kit builds (SM_Minka) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House")
	TObjectPtr<UStaticMesh> SourceMesh;

	/** The house without its panels, in the same frame, every shape with its collision (SM_Minka_Shell) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House")
	TObjectPtr<UStaticMesh> ShellMesh;

	/** Its sliding panels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	TArray<FVillageHousePanel> Panels;

	/** Its openings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	TArray<FVillageHouseDoorway> Doorways;

	/** A pawn this close to the plane of a doorway, in front of it or behind it, opens it (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0", Units="cm"))
	float TriggerDepth = 150.0f;

	/** The doorway also opens for a pawn this far beyond the sides of its bay (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0", Units="cm"))
	float TriggerSideMargin = 30.0f;

	/** Seconds a panel takes to slide all the way */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0.05", Units="s"))
	float SlideSeconds = 0.35f;

	/** A doorway stays open this long after the last pawn left it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0", Units="s"))
	float CloseDelay = 1.2f;

	/** Seconds between two looks at the pawns near the doors (the server at every pawn, a client at its own) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors", meta=(ClampMin="0.02", Units="s"))
	float CheckInterval = 0.1f;

	/** A moving pawn opens a doorway before it reaches the trigger (T-301, review R-08): the panel has slid this share
	 *  of its way when the pawn gets there. The time ahead is this share of SlideSeconds, plus CheckInterval, plus
	 *  LeadExtraSeconds, plus half the round trip of a client's pawn on the server (it sees that pawn late) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors|Network", meta=(ClampMin="0", ClampMax="1"))
	float LeadSlideShare = 0.5f;

	/** Margin added to the time ahead (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors|Network", meta=(ClampMin="0", Units="s"))
	float LeadExtraSeconds = 0.05f;

	/** Most of a client's half round trip counted in the time ahead (s): a huge ping does not open every door it runs by */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors|Network", meta=(ClampMin="0", Units="s"))
	float MaxLeadPingSeconds = 0.2f;

	/** Farthest ahead of a pawn a doorway opens (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors|Network", meta=(ClampMin="0", Units="cm"))
	float MaxLeadDistance = 400.0f;

	/** Relic put in every house (BP_TidalRelic); none = no relic */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	TSubclassOf<AActor> RelicClass;

	/** Relics of which one is drawn per house (the legendary artifacts of a shrine); RelicClass when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	TArray<TSubclassOf<AActor>> RelicVariants;

	/** Where the relic may stand, one drawn per house (the tokonoma, facing the room) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	TArray<FVillageHouseSlot> RelicSlots;

	/** Kinds of everyday objects and their weights (DA_LootRules: its Items) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	TObjectPtr<ULootRules> ItemTable;

	/** Where the everyday objects may lie (free floor away from the passages, the tansu, the futon) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	TArray<FVillageHouseSlot> ItemSlots;

	/** Everyday objects put in a house, at least */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents", meta=(ClampMin="0"))
	int32 MinItemsPerHouse = 2;

	/** Everyday objects put in a house, at most (no more than the slots) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents", meta=(ClampMin="0"))
	int32 MaxItemsPerHouse = 3;

	/** Random turn of an everyday object around the yaw of its slot, either way (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float ItemYawJitter = 25.0f;

	/** Gap left under an object set on its slot, put to sleep (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents", meta=(ClampMin="0", Units="cm"))
	float RestClearance = 0.5f;

	/** Seed of the draw of the slots and kinds of the fragment; 0 = a new draw every game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents")
	int32 ContentSeed = 0;

	/** Objects spawned per step on the server: the contents are spread over a few frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contents", meta=(ClampMin="1", ClampMax="200"))
	int32 ContentSpawnBatch = 40;

	/** Its doors stay shut until the flag of its village is raised, then open for good (the shrine and its legendary
	 *  artifact, owner request 2026-10-07) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doors")
	bool bSealedUntilFlag = false;

	/** Villagers living in the house (ATikiDirector spawns them inside once a player comes near the village) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0", ClampMax="4"))
	int32 Residents = 0;

	/** Waypoints of the routine of its villagers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	TArray<FVillageHouseNode> Nodes;

	/** Node just inside the main door */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="0"))
	int32 DoorNode = 0;

	/** In front of the main door, outside (house frame, cm): between the house and its plot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	FVector OutsideLocation = FVector::ZeroVector;

	/** Node its villager stands at to take the relic of the house; INDEX_NONE: the node nearest to the relic */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents", meta=(ClampMin="-1"))
	int32 RelicNode = INDEX_NONE;

	/** Inside of the house, walls included (house frame, cm): a player there is watched, an object of the house
	 *  carried out of it is stolen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Residents")
	FBox Interior = FBox(ForceInit);
};
