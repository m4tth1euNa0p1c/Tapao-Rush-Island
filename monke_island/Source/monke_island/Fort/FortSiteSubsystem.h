// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FortSiteSubsystem.generated.h"

class AFortSite;
class UFortSiteRules;

/**
 *  Server: the French forts of an expedition, built on the land of the atoll (owner 2026-10-08: "les forts français
 *  spawn sur la map de Raraka Tuamotu et pas sur une île personnalisée", "génère procéduralement", from day 1). In the
 *  fragment the world plays, from the forts' day of the campaign (DA_RunRules.Campaign.FortsFromDay; always without a
 *  run), DA_FortSiteRules.FortsPerExpedition sites of the fragment are drawn and an AFortSite set on the ground of each,
 *  before the camps, the marae and the tiki look at the land. The fragment comes from the travel URL; a PIE opened on
 *  the level takes the fragment whose sites are nearest to the players once they stand. Worlds that play no fragment
 *  (menus, test maps) get none.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UFortSiteSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Forts built in this world (server) */
	TArray<AFortSite*> GetForts() const;

private:

	/** Server: the forts of a fragment */
	void BuildForts(FName FragmentId);

	/** Server, a PIE without a fragment in the URL: the fragment nearest to the players, once they stand */
	void ResolveFromPlayers();

	/** DA_FortSiteRules (DefaultGame.ini) */
	UPROPERTY(Config)
	FSoftObjectPath RulesPath;

	UPROPERTY(Transient)
	TObjectPtr<UFortSiteRules> Rules;

	TArray<TWeakObjectPtr<AFortSite>> Forts;

	FTimerHandle ResolveTimer;
	double ResolveStart = 0.0;
};
