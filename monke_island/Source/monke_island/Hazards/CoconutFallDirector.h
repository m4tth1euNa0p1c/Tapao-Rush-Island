// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoconutFallDirector.generated.h"

class AFallingCoconutActor;
class APawn;
class UCoconutFallRules;
class UInstancedStaticMeshComponent;

/**
 *  Drops coconuts from the palms around each player (server): the palms are the instanced meshes placed by PCG,
 *  found at begin play by their mesh name and height, so any new palm drops nuts without extra setup.
 *  Nuts fall more often under the storm wind (AWindStormActor), and often from the palm closest to the player.
 *  Debug (non shipping): monke.DropCoconut drops one above the first player.
 */
UCLASS()
class MONKE_ISLAND_API ACoconutFallDirector : public AActor
{
	GENERATED_BODY()

public:

	ACoconutFallDirector();

	/** Rules of the falls (DA_CoconutFallRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coconuts")
	TObjectPtr<UCoconutFallRules> Rules;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** How a palm is picked for a fall */
	enum class EPick : uint8
	{
		Random,
		Closest,
		AimedAtTarget
	};

	/** Drops a nut from a palm near Target (server). Returns the nut, or nullptr when no palm fits */
	AFallingCoconutActor* DropNear(const APawn* Target, EPick Pick);

private:

	/** Palm instance components found in the level */
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> PalmComponents;

	/** Nuts already dropped by each palm instance (component, instance index): a palm never regrows its nuts */
	TMap<TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>, int32> NutsDropped;

	/** Nuts still hanging on a palm instance */
	int32 NutsLeft(UInstancedStaticMeshComponent* Component, int32 Instance) const;

	/** Nuts alive, oldest first */
	TArray<TWeakObjectPtr<AFallingCoconutActor>> LiveCoconuts;

	/** Seconds until the next fall, per player controller */
	TMap<TWeakObjectPtr<AController>, float> Timers;

	/** Finds the palm components among the instanced meshes of the level */
	void FindPalms();
};
