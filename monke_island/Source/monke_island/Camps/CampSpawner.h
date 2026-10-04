// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CampSpawner.generated.h"

class ACampSite;
class AMaraeSite;
class UCampRules;
class UMaraeRules;

/**
 *  Puts a few fishermen camps (ACampSite) at random free spots of the island at the start of each game (T-082),
 *  server only: spots near a PlayerStart, far from each other, out of the quiet zones (the marae), on solid ground.
 *  In a fragment of the expedition it first puts the Marae des Perles of the fragment (AMaraeSite) on one of its spots.
 */
UCLASS()
class MONKE_ISLAND_API ACampSpawner : public AActor
{
	GENERATED_BODY()

public:

	ACampSpawner();

	/** Camps, spots and counts (DA_CampRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	TObjectPtr<UCampRules> Rules;

	/** Camp class spawned (ACampSite or a blueprint of it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
	TSubclassOf<ACampSite> CampClass;

	/** Marae built in the played fragment (DA_MaraeRules); none when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	TObjectPtr<UMaraeRules> MaraeRules;

	/** Marae class spawned (AMaraeSite or a blueprint of it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Marae")
	TSubclassOf<AMaraeSite> MaraeClass;

	/** Camps of this game (server) */
	UFUNCTION(BlueprintPure, Category="Camp")
	int32 GetCampCount() const { return Camps.Num(); }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: draws the spots and spawns the camps */
	void SpawnCamps();

	TArray<TWeakObjectPtr<ACampSite>> Camps;
	TArray<TWeakObjectPtr<AMaraeSite>> Marae;
	FTimerHandle SpawnTimer;
};
