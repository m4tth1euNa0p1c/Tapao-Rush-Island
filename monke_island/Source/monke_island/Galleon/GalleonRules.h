// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GalleonRules.generated.h"

class UStaticMesh;

/**
 *  English galleon at the bean's scale (maquette galion-anglais of the second PC, cahier §24, ADR-005): the ship the
 *  crew starts from, anchored off the played fragment with the barque moored at its flank. V1 is kinematic: it
 *  follows the sea level with a cosmetic sway, its decks are walkable, the boarding ladder takes the players up and
 *  down. Every value here, none in the code.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UGalleonRules : public UDataAsset
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ meshes (origin on the waterline, bow +Y)

	/** Hull, decks, castles, furnished cabin, masts (SM_Galleon, complex collision, sockets) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> HullMesh;

	/** Sails (SM_Galleon_Sails, no collision) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> SailsMesh;

	/** Shrouds and stays (SM_Galleon_Rigging, no collision) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> RiggingMesh;

	/** Flag (SM_Galleon_Flag), at FlagPivot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> FlagMesh;

	/** Steering wheel (SM_Galleon_Wheel), at WheelPivot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> WheelMesh;

	/** Rudder (SM_Galleon_Rudder), at RudderPivot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> RudderMesh;

	/** Pivots of the separate parts in the hull frame (cm, galleon-report.json) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FVector FlagPivot = FVector(0.0, -1143.0, 750.0);

	/** See FlagPivot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FVector WheelPivot = FVector(0.0, -500.0, 385.0);

	/** See FlagPivot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FVector RudderPivot = FVector(0.0, -1105.0, 0.0);

	// ------------------------------------------------------------------ sway (cosmetic, same on every machine)

	/** Roll and pitch of the anchored ship (deg) and heave (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sway", meta=(ClampMin="0"))
	float RollDeg = 1.0f;

	/** See RollDeg */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sway", meta=(ClampMin="0"))
	float PitchDeg = 0.4f;

	/** See RollDeg */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sway", meta=(ClampMin="0", Units="cm"))
	float HeaveCm = 6.0f;

	/** Periods of the roll, the pitch and the heave (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sway")
	FVector SwayPeriods = FVector(7.0, 5.3, 6.1);

	/** Fastest the anchored ship follows the water up or down (cm/s): above the fastest rise of the flood */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sway", meta=(ClampMin="1", Units="cm/s"))
	float FloodFollowSpeed = 60.0f;

	// ------------------------------------------------------------------ aboard

	/** Volume of the players aboard: decks, cabin, top of the ladder (hull frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FVector AboardCentre = FVector(0.0, 0.0, 420.0);

	/** See AboardCentre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FVector AboardExtent = FVector(330.0, 1150.0, 300.0);

	/** Volume of the cargo aboard: objects lying on the decks (hull frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FVector CargoCentre = FVector(0.0, 0.0, 330.0);

	/** See CargoCentre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FVector CargoExtent = FVector(310.0, 1120.0, 220.0);

	/** Where the crew appears on the deck (socket of the hull) and the gap between two players (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FName DeckSpawnSocket = TEXT("bean_spawn");

	/** See DeckSpawnSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard", meta=(ClampMin="0", Units="cm"))
	float DeckSpawnSpacing = 90.0f;

	/** The players start the game on the deck (cahier §24.2); false: they stay in the barque, moored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	bool bPlayersStartOnDeck = true;

	// ------------------------------------------------------------------ boarding ladder

	/** Foot of the boarding ladder on the flank (socket of the hull) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	FName LadderSocket = TEXT("boarding_ladder");

	/** Where a player stands on the deck at the top of the ladder (hull frame, cm, deck level) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	FVector LadderDeckPoint = FVector(-215.0, -90.0, 162.0);

	/** Reach of the ladder: from its foot (water, barque) and from its top on the deck (cm, 2D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder", meta=(ClampMin="0", Units="cm"))
	float LadderReach = 420.0f;

	/** Prompts of the interaction key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	FText BoardPrompt = NSLOCTEXT("Galleon", "Board", "Monter à bord");

	/** See BoardPrompt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	FText ToBoatPrompt = NSLOCTEXT("Galleon", "ToBoat", "Descendre dans la barque");

	/** See BoardPrompt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	FText ToWaterPrompt = NSLOCTEXT("Galleon", "ToWater", "Descendre à l'eau");

	// ------------------------------------------------------------------ barque

	/** Where the barque lies along the flank (socket of the hull) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barque")
	FName DockSocket = TEXT("barque_dock");

	/** Yaw of the moored barque against the ship (deg): alongside, bow forward */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barque")
	float DockYawOffset = 90.0f;

	/** The barque is moored when it lies this close to the dock (cm) and this slow (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barque", meta=(ClampMin="0", Units="cm"))
	float MooringRadius = 450.0f;

	/** See MooringRadius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barque", meta=(ClampMin="0", Units="cm/s"))
	float MooringMaxSpeed = 40.0f;

	/** The barque of the start is brought to the dock when the ship anchors */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barque")
	bool bMoorBoatAtStart = true;

	// ------------------------------------------------------------------ anchoring (AGalleonSpawner)

	/** Delay after the start before the ship anchors (s): the island builds itself from the arrival first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="s"))
	float SpawnDelay = 2.0f;

	/** Water under the whole hull, at least (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="cm"))
	float MinDepthCm = 400.0f;

	/** Searched from this distance of the arrival point, outward, by steps, up to the maximum (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="cm"))
	float MinDistanceFromArrival = 2500.0f;

	/** See MinDistanceFromArrival */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="100", Units="cm"))
	float SearchStep = 500.0f;

	/** See MinDistanceFromArrival */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="cm"))
	float MaxDistanceFromArrival = 30000.0f;

	/** Directions tried around the arrival point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="4"))
	int32 SearchDirections = 24;

	/** Half length and half beam of the hull checked for depth (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring")
	FVector2D FootprintHalfSize = FVector2D(1150.0, 330.0);
};
