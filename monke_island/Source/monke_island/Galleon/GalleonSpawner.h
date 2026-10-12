// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GalleonSpawner.generated.h"

class ABoatActor;
class AController;
class AGalleonActor;
class AGameModeBase;
class APlayerController;
class UGalleonRules;

/**
 *  Start of the game from the galleon (cahier §24.2, ADR-005), server only: once the barque of the start waits at
 *  the arrival point of the played fragment, anchors the galleon off it, in the nearest water deep enough, its
 *  ladder side toward the arrival; brings the barque to its dock and puts the crew on the deck. A player whose pawn
 *  comes after the anchoring (a client slower to load the island, network review T-268 R-02, T-292) is put on the
 *  deck too, once the boat start had its turn.
 */
UCLASS()
class MONKE_ISLAND_API AGalleonSpawner : public AActor
{
	GENERATED_BODY()

public:

	AGalleonSpawner();

	/** Ship, anchoring and start (DA_GalleonRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galleon")
	TObjectPtr<UGalleonRules> Rules;

	/** Galleon class spawned (AGalleonActor or a blueprint of it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galleon")
	TSubclassOf<AGalleonActor> GalleonClass;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Waits for the barque of the start, then anchors the ship */
	void TryAnchor();

	/** Anchoring spot around the arrival: centre and yaw; false when the water is nowhere deep enough */
	bool FindAnchorage(const ABoatActor* Boat, FVector& OutLocation, float& OutYaw) const;

	/** True when the ship's footprint at Centre, turned to Yaw (deg) and Margin cm wider all round, has MinDepthCm of
	 *  water everywhere */
	bool IsFootprintDeep(const FVector2D& Centre, float Yaw, float Margin, float MinDepth, float SeaLevel, const AActor* Ignored) const;

	/** Moves the mooring toward the nearest land as far as the footprint keeps its water (bApproachShore), the bow turned
	 *  away from the island (the departure sails off seaward); returns the distance moved (cm) */
	float ApproachShore(FVector& InOutLocation, float& InOutYaw, float SeaLevel, const AActor* Ignored) const;

	/** Water depth at a point (cm), the barque ignored */
	float DepthAt(const FVector2D& Point, float SeaLevel, const AActor* Ignored) const;

	/** Server, after the anchoring: a player logs in; his pawn is watched */
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	/** Server: the first pawn of this controller goes to the deck (now if it has one, else when it comes) */
	void WatchLateJoiner(AController* Controller);

	/** Server: the deck in two frames, after the boat start that boards a late joiner on the next one */
	void ScheduleLateJoiner(TWeakObjectPtr<AController> Controller);

	/** Server: puts a late joiner on the deck, once */
	void PlaceLateJoiner(TWeakObjectPtr<AController> Controller);

	/** Server: stops watching the pawns of a controller */
	void StopWatching(AController* Controller);

	FTimerHandle Timer;
	int32 Attempts = 0;

	/** Times the island catalog was waited for (a map played without the boot map loads it here) */
	int32 CatalogWaits = 0;

	/** The galleon anchored by this spawner */
	TWeakObjectPtr<AGalleonActor> Galleon;

	/** Controllers whose player was put on the deck: a respawn is not moved */
	TSet<TWeakObjectPtr<AController>> PlacedControllers;

	/** Pawn watches of the players who have none yet */
	TMap<TWeakObjectPtr<AController>, FDelegateHandle> PawnWatches;

	/** Logins after the anchoring */
	FDelegateHandle PostLoginHandle;

	/** Places of the deck given so far (PlaceOnDeck) */
	int32 DeckPlaces = 0;
};
