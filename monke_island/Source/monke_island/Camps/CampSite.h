// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camps/SiteClearing.h"
#include "CampSite.generated.h"

class UCampRules;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  A fishermen camp (T-082), spawned by ACampSpawner on the server and replicated. On every machine it clears the
 *  vegetation of its zone (the instanced bushes, palms and grass are moved out of sight, their indices kept) and
 *  builds its decor from DA_CampRules; the server also spawns its items: tools worth stealing, fish to eat and common
 *  loot inside the hut. Its sphere, tagged TikiQuietZone, keeps the tiki, the totems and the scattered loot away.
 */
UCLASS()
class MONKE_ISLAND_API ACampSite : public AActor
{
	GENERATED_BODY()

public:

	ACampSite();

	/** Layout and items (set by the spawner before the camp finishes spawning, replicated with it) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Camp")
	TObjectPtr<UCampRules> Rules;

	/** Seed of the camp's draws (loot in the hut), set by the spawner */
	UPROPERTY(Replicated)
	int32 CampSeed = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Builds the decor pieces and the fire light (every machine) */
	void BuildDecor();

	/** Server: tools, fish and the loot of the hut */
	void SpawnItems();

	/** Ground under a point of the camp (world), or the point itself when no ground is found */
	FVector GroundAt(const FVector& World) const;

	/** Spawns one item (server) */
	AActor* SpawnItem(TSubclassOf<AActor> ItemClass, const FTransform& World);

	/** Zone of the camp, tagged TikiQuietZone */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> QuietZone;

	/** Decor pieces built at BeginPlay */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DecorPieces;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FireLight;

	/** Items spawned by this camp (server) */
	TArray<TWeakObjectPtr<AActor>> Items;

	/** Vegetation moved away: restored when the camp goes (editor sessions, level reload) */
	FSiteClearing Clearing;
};
