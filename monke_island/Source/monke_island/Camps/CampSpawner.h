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
 *  In a fragment of the expedition it first puts the Marae des Perles of the fragment (AMaraeSite) on one of its spots,
 *  then its reliquary temple (T-180, an AMaraeSite built from DA_TempleReliquaryRules) on the free spot nearest to the
 *  middle of the fragment's land, away from the marae, on level land above the beach; one temple at least is
 *  guaranteed, on the most central free spot when no spot passes the ground check.
 *  During a run the days of the campaign decide (DA_RunRules.Campaign, docs/features/campagne-dix-jours.md): no
 *  temple before its day, then TemplesPerFragment temples with the artifacts dealt to them, and the fishermen camps
 *  only with every building. Without a run (test maps) everything is put as authored.
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

	/** Reliquary temple built in the played fragment (DA_TempleReliquaryRules: a UMaraeRules); none when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temple")
	TObjectPtr<UMaraeRules> TempleRules;

	/** Temple class spawned (AMaraeSite or a blueprint of it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temple")
	TSubclassOf<AMaraeSite> TempleClass;

	/** Temples of this game (server) */
	UFUNCTION(BlueprintPure, Category="Temple")
	int32 GetTempleCount() const { return Temples.Num(); }

	/** Camps of this game (server) */
	UFUNCTION(BlueprintPure, Category="Camp")
	int32 GetCampCount() const { return Camps.Num(); }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: draws the spots and spawns the camps */
	void SpawnCamps();

	/** Server: Count temples of the fragment on its most central free spots (the indices used leave Order); Hands[i]
	 *  are the artifacts the campaign dealt to the i-th one (none: every pedestal filled) */
	void SpawnTemples(const TArray<FVector>& Spots, TArray<int32>& Order, const TArray<FVector>& Players, TArray<FVector>& Taken,
		TArray<FSphere>& Quiet, FRandomStream& Stream, int32 Count, const TArray<TArray<TSubclassOf<AActor>>>& Hands);

	/** Ground under Spot and at Radius around it (8 directions), the vegetation looked through; false when one has none */
	bool SampleGround(const FVector& Spot, float Radius, TArray<double>& OutZ) const;

	TArray<TWeakObjectPtr<ACampSite>> Camps;
	TArray<TWeakObjectPtr<AMaraeSite>> Marae;
	TArray<TWeakObjectPtr<AMaraeSite>> Temples;
	FTimerHandle SpawnTimer;
};
