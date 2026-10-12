// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "GalleonRules.generated.h"

class USoundBase;
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

	/** No wind within this distance of the anchored ship (cm): on the decks and on the water around, the crew loads
	 *  the cargo without being pushed (AWindStormActor reads the WindCalmZone sphere); 0 = wind as elsewhere */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sway", meta=(ClampMin="0", Units="cm"))
	float WindCalmRadius = 4000.0f;

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

	/** Merchant standing on the deck (owner request 2026-10-06, ADR-007): spawned by the server, none when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	TSoftClassPtr<AActor> ShopkeeperClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/monke_island.ShopkeeperActor")));

	/** Where the merchant stands, from DeckSpawnSocket in the hull frame (cm; +Y towards the bow, Z to its capsule centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FVector ShopkeeperOffset = FVector(0.0, 260.0, 65.0);

	/** Yaw of the merchant relative to the hull (deg; -90 faces the stern, where the players start) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	float ShopkeeperYaw = -90.0f;

	/** Loot crate lying on the deck at each expedition (owner request 2026-10-09, T-258): spawned by the server beside the
	 *  merchant, to bring the loot in bulk from the land to the barque; none when empty. The bare class takes
	 *  DA_LootCrateRules and its meshes by path; BP_LootCrate may replace it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate")
	TSoftClassPtr<AActor> LootCrateClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/monke_island.LootCrateActor")));

	/** Where the crate goes, from DeckSpawnSocket in the hull frame (cm; +X away from the boarding ladder, +Y towards the
	 *  bow; Z unused: it lies on the deck found there): beside the merchant, off the crew's places and the ladder's way */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate")
	FVector LootCrateOffset = FVector(90.0, 230.0, 0.0);

	/** Yaw of the crate relative to the hull (deg; 90 lays its length along the keel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate")
	float LootCrateYaw = 90.0f;

	/** The crate takes the first free place around LootCrateOffset, on rings this far apart (cm), this many rings at most */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate", meta=(ClampMin="1", Units="cm"))
	float LootCrateSearchStep = 25.0f;

	/** See LootCrateSearchStep */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate", meta=(ClampMin="0"))
	int32 LootCrateSearchRings = 6;

	/** A free place lies on the crew's deck: at most this far above or below DeckSpawnSocket (cm; not on a castle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate", meta=(ClampMin="0", Units="cm"))
	float LootCrateMaxDeckStep = 25.0f;

	/** Gap kept around the crate at its place, and the height it is put down from onto the deck (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard|LootCrate", meta=(ClampMin="0", Units="cm"))
	float LootCrateClearanceCm = 3.0f;

	/** Lamp of the captain's cabin: socket of the hull, height above it (cm), intensity (cd, 0 = no lamp), radius (cm)
	 *  and colour temperature (K) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard")
	FName CabinLightSocket = TEXT("captain_cabin");

	/** See CabinLightSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard", meta=(Units="cm"))
	float CabinLightHeight = 95.0f;

	/** See CabinLightSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard", meta=(ClampMin="0"))
	float CabinLightIntensity = 5.0f;

	/** See CabinLightSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard", meta=(ClampMin="0", Units="cm"))
	float CabinLightRadius = 650.0f;

	/** See CabinLightSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aboard", meta=(ClampMin="1000"))
	float CabinLightTemperature = 2300.0f;

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

	/** Where the bow points, from the actor's +X (deg): the hull's length runs along its Y, the rudder and the flag at -Y,
	 *  the bow at +Y. The galleon sails off that way at the end of the day (owner 2026-10-07: "le bateau part sur le
	 *  côté alors qu'il est orienté de l'autre sens") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	float BowYawDeg = 90.0f;

	// ------------------------------------------------------------------ helm

	/** The wheel (socket of the hull, the wheel's place when the hull has none) and how near a player stands to use it
	 *  (cm, 2D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helm")
	FName HelmSocket = TEXT("helm");

	/** See HelmSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helm", meta=(ClampMin="0", Units="cm"))
	float HelmReach = 260.0f;

	/** Prompt of the wheel while nothing is under way (cahier §24.2 step 7: "Lever l'ancre", held) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helm")
	FText HelmPrompt = NSLOCTEXT("Galleon", "Helm", "Lever l'ancre (maintenir)");

	/** Prompt of the wheel while a vote or a notice runs: the key cancels it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helm")
	FText HelmCancelPrompt = NSLOCTEXT("Galleon", "HelmCancel", "Annuler le départ");

	// ------------------------------------------------------------------ departure (cahier §24.2 step 7, §6.5, §24.6)

	/** A living player aboard holds the wheel's key this long to raise the anchor (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float HelmHoldSeconds = 2.0f;

	/** Every living player aboard: the galleon sails this long after the anchor is raised (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureAllAboardNoticeSeconds = 5.0f;

	/** Notice once a vote passed (s): bell, foghorn, message and countdown for the whole crew, cancellable at the wheel;
	 *  whoever is ashore at its end is left behind and dies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureVoteNoticeSeconds = 10.0f;

	/** A vote passes with strictly more than this share of the living players voting for it (0.5: the majority of the
	 *  cahier, 2 of 2 or 3, 3 of 4; 0: the one who raised the anchor decides alone; 0.99: unanimity) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", ClampMax="0.99"))
	float DepartureMajorityFraction = 0.5f;

	/** A vote without majority lapses after this long (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="1", Units="s"))
	float DepartureVoteSeconds = 30.0f;

	/** After a vote or a notice failed (cancelled, lapsed, nobody aboard), no new vote opens for this long (s): the bell
	 *  does not ring again and again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureVoteCooldownSeconds = 10.0f;

	/** Keys of the wheel: held to raise the anchor, pressed to cancel a vote or a notice (active only at the wheel, over
	 *  the interaction key there) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure")
	TArray<FKey> HelmKeys = { EKeys::F };

	/** Keys of the vote for the departure, offered by the HUD to every living player during a vote (O for "oui") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure")
	TArray<FKey> VoteKeys = { EKeys::O };

	/** Priority of the wheel's and the vote's key mappings: above the character's (the wheel's key is the interaction
	 *  key, taken over at the wheel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure")
	int32 CrewInputPriority = 20;

	/** A player's presses of the wheel's key and votes reach the server at most this often (s, rate limit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureRequestMinInterval = 0.25f;

	/** The server checks the hold, the vote and the notice this often (s): who is alive, who is aboard, the wheel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0.05", Units="s"))
	float DepartureCheckSeconds = 0.25f;

	/** The HUD tells why the last vote or notice failed for this long (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureMessageSeconds = 5.0f;

	/** Ship's bell: a vote opens, a notice starts (heard by the whole crew, 2D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound")
	TSoftObjectPtr<USoundBase> DepartureBellSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Audio/Expedition/SW_Exp_Bell.SW_Exp_Bell")));

	/** Foghorn of a notice; the project has none yet: the evacuation sound of the flood counter stands in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound")
	TSoftObjectPtr<USoundBase> DepartureHornSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Sounds/S_HudTimer_Evacuation.S_HudTimer_Evacuation")));

	/** Tick of the notice's last seconds (the countdown sound of the expedition, SW_UI_Click) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound")
	TSoftObjectPtr<USoundBase> DepartureTickSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Tapao/Audio/SW_UI_Click.SW_UI_Click")));

	/** Volume of the bell and the foghorn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound", meta=(ClampMin="0", ClampMax="2"))
	float DepartureSoundVolume = 0.6f;

	/** Volume of the ticks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound", meta=(ClampMin="0", ClampMax="2"))
	float DepartureTickVolume = 0.3f;

	/** The notice ticks its last seconds from this one down to 1 (0 = no tick) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound", meta=(ClampMin="0"))
	int32 DepartureTickFromSeconds = 5;

	/** A sound of the departure is not played for a stage older than this (s): a player who arrives during a notice
	 *  does not hear its bell */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure|Sound", meta=(ClampMin="0", Units="s"))
	float DepartureSoundGraceSeconds = 1.5f;

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

	/** The galleon comes as near the island as its draft allows (owner 2026-10-07: "faire apparaître le galion moins loin
	 *  des côtes"): from its mooring it moves toward the nearest land while its whole footprint, ShoreMarginCm wider all
	 *  round, keeps MinDepthCm of water */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring")
	bool bApproachShore = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="cm"))
	float ShoreMarginCm = 300.0f;

	/** Water the galleon needs under its whole footprint as it comes near the island (cm; its keel is about 190 cm deep,
	 *  the lagoon of Raraka is 2.5 m deep 100 m off the beach) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="cm"))
	float ApproachMinDepthCm = 160.0f;

	/** How far the island is looked for around the mooring (cm; the moorings of the data lie up to 325 m off) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Anchoring", meta=(ClampMin="0", Units="cm"))
	float ShoreSearchCm = 150000.0f;
};
