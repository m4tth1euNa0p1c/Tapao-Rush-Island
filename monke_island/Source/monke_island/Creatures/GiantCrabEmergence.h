// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GiantCrabEmergence.generated.h"

class AGiantCrab;
class UGiantCrabEmergenceRules;

/**
 *  Server, in an expedition of the campaign from the giant crabs' day (DA_RunRules.Campaign.GiantCrabsFromDay): at each
 *  time of DA_GiantCrabEmergenceRules.EmergeSeconds of the day, a giant crab rises out of the sand some 50 m from a
 *  player (beach ground: a little over the sea, flat, no plant nor building on it), then charges. Nothing without a run
 *  or before its day (test maps keep their own crabs). Console: monke.CrabEmerge makes one rise now.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UGiantCrabEmergenceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	UGiantCrabEmergenceSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Server: a crab rises now near a player (console, tests); true when one did */
	bool EmergeNow();

private:

	/** Server: the day's emergences due */
	void Check();

	/** Server: a crab rising out of the sand near the player; true when a place was found */
	bool Emerge(const AActor* Player);

	/** DA_GiantCrabEmergenceRules (set in DefaultGame.ini or here) */
	UPROPERTY(Config)
	FSoftObjectPath EmergenceRulesPath;

	UPROPERTY(Transient)
	TObjectPtr<UGiantCrabEmergenceRules> Rules;

	/** Crabs rising, where they rise to and since when */
	struct FRising
	{
		TWeakObjectPtr<AGiantCrab> Crab;
		FVector Ground = FVector::ZeroVector;
		double Start = 0.0;
	};
	TArray<FRising> Rising;

	/** Emergences of the day already done, next look (world time) */
	int32 Done = 0;
	double NextCheck = 0.0;
	bool bServer = false;
};
