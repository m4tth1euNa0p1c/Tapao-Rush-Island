// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExpeditionArrivalPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;

/**
 *  Where the boat of the players appears at the start of an expedition (T-093): on the water, a few hundred
 *  metres off a beach of the fragment, the arrow (actor +X) is the bow and points to the island.
 *  UBoatStartSubsystem picks the point whose FragmentId matches the ?Island= URL option (T-094), or a random
 *  one; a map without any point keeps the PlayerStart start. Placed by T-095 from fragments.json.
 *  Server side marker only: not replicated, no tick, no collision.
 */
UCLASS(ClassGroup=(Boat), HideCategories=(Collision, Physics, Input, Replication, Rendering, HLOD, Cooking))
class MONKE_ISLAND_API AExpeditionArrivalPoint : public AActor
{
	GENERATED_BODY()

public:

	AExpeditionArrivalPoint();

	/** Fragment this point belongs to (fragments.json id, e.g. FRG_Raraka_01); several points per fragment allowed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival")
	FName FragmentId;

	/** Transform of the boat at this point: location, yaw only (bow along the arrow) */
	UFUNCTION(BlueprintPure, Category="Arrival")
	FTransform GetBoatTransform() const;

private:

	/** Root of the point */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	/** Bow direction, editor only */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> Arrow;

	/** Icon, editor only */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
