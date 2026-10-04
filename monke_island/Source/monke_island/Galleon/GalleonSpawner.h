// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GalleonSpawner.generated.h"

class ABoatActor;
class AGalleonActor;
class UGalleonRules;

/**
 *  Start of the game from the galleon (cahier §24.2, ADR-005), server only: once the barque of the start waits at
 *  the arrival point of the played fragment, anchors the galleon off it, in the nearest water deep enough, its
 *  ladder side toward the arrival; brings the barque to its dock and puts the crew on the deck.
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

	/** Water depth at a point (cm), the barque ignored */
	float DepthAt(const FVector2D& Point, float SeaLevel, const AActor* Ignored) const;

	FTimerHandle Timer;
	int32 Attempts = 0;

	/** Times the island catalog was waited for (a map played without the boot map loads it here) */
	int32 CatalogWaits = 0;
};
