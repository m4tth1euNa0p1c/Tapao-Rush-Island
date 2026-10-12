// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camps/SiteClearing.h"
#include "MaraeSite.generated.h"

class UMaraeRules;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  A Marae des Perles built at runtime (T-075), spawned by ACampSpawner on the server and replicated. On every
 *  machine it clears the vegetation of its zone and builds its stones, scatter and (non replicated) torches from
 *  DA_MaraeRules; the server spawns its sacred objects, the pearl artifact among them. Tagged TikiQuietZone.
 */
UCLASS()
class MONKE_ISLAND_API AMaraeSite : public AActor
{
	GENERATED_BODY()

public:

	AMaraeSite();

	/** Layout (set by the spawner before the marae finishes spawning, replicated with it) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Marae")
	TObjectPtr<UMaraeRules> Rules;

	/** Server, before the site finishes spawning: the campaign dealt these artifacts of Rules->ArtifactPool to the site;
	 *  the pedestals of the others stay empty (not called: every pedestal is filled) */
	void DealArtifacts(const TArray<TSubclassOf<AActor>>& Artifacts);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Stones, scatter and torches (every machine) */
	void Build();

	/** Server: sacred objects; every machine: actors that do not replicate */
	void SpawnActors(const FVector& Ground);

	/** Zone of the marae, tagged TikiQuietZone */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> QuietZone;

	/** Pieces built at BeginPlay */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	/** Actors spawned by this marae */
	TArray<TWeakObjectPtr<AActor>> Spawned;

	/** Artifacts of the pool dealt to the site (server, DealArtifacts) */
	UPROPERTY(Transient)
	TArray<TSubclassOf<AActor>> DealtArtifacts;

	/** DealArtifacts was called: a pedestal whose artifact was not dealt stays empty */
	bool bArtifactsDealt = false;

	FSiteClearing Clearing;
};
