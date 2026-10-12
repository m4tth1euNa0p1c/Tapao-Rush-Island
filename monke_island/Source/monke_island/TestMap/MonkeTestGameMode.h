// monke_island

#pragma once

#include "CoreMinimal.h"
#include "monke_islandGameMode.h"
#include "MonkeTestGameMode.generated.h"

class UInventoryItemDefinition;

/**
 *  Game mode of the test map L_Test_Flat (owner request 2026-10-06): the map stays apart from the game (no expedition,
 *  no flood, no permadeath: standard GameState, the dead player respawns on the same map) and the player can be
 *  invulnerable with full health. Parent of BP_TestGameMode.
 */
UCLASS()
class MONKE_ISLAND_API AMonkeTestGameMode : public Amonke_islandGameMode
{
	GENERATED_BODY()

public:

	/** The players take no damage at all and keep full health (CanBeDamaged off: hits, falls, coconuts, drowning) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test")
	bool bGodMode = true;

	/** Every firearm the players carry never runs dry: its pouch is filled again, the reloads still play (owner
	 *  2026-10-11: « munitions infinies sur le flat test pour chaque arme ») */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test")
	bool bInfiniteAmmo = true;

	/** Seconds between two refills of the pouches */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test", meta=(ClampMin="0.05", Units="s"))
	float AmmoRefillInterval = 0.25f;

	/** Items given to every player at each spawn on this map, after the inventory's own starting items: one per entry,
	 *  an item named again fills its stack (T-340: the bandages, DA_BandageRules.TestMapStartingCount of them) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test")
	TArray<TObjectPtr<UInventoryItemDefinition>> ExtraStartingItems;

	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

protected:

	virtual void BeginPlay() override;

private:

	/** Fills the pouch of every firearm of every player */
	void RefillAmmo();

	/** Gives ExtraStartingItems to a player that just spawned */
	void GiveExtraStartingItems(APawn* PlayerPawn) const;

	FTimerHandle AmmoTimer;
};
