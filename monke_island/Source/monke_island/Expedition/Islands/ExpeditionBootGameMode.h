// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Expedition/Islands/ExpeditionIslandTypes.h"
#include "ExpeditionBootGameMode.generated.h"

class UExpeditionIslandDefinition;
class UWorld;

/**
 *  Game mode of the boot map (L_Boot, T-094), until the hub of L4: loads DA_IslandCatalog, draws a fragment
 *  (never the previous one twice in a row) and travels to <map>?Island=<id>, where the boat of T-093 starts with
 *  the team seated. No pawn on the boot map. When no island can be drawn, travels to FallbackMap so the player is
 *  never stuck on an empty map.
 */
UCLASS()
class MONKE_ISLAND_API AExpeditionBootGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AExpeditionBootGameMode();

	/** Draws with FixedSeed (reproducible runs, tests) instead of a clock seed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boot")
	bool bUseFixedSeed = false;

	/** Seed of the draw when bUseFixedSeed is on */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boot", meta=(EditCondition="bUseFixedSeed"))
	int32 FixedSeed = 0;

	/** Map opened when no island can be drawn (missing or empty catalog); empty = stay on the boot map */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boot")
	TSoftObjectPtr<UWorld> FallbackMap;

	/** Error of the boot (None while it runs or once the travel started) */
	UFUNCTION(BlueprintPure, Category="Boot")
	EFragmentError GetBootError() const { return BootError; }

protected:

	virtual void BeginPlay() override;

	/** No pawn on the boot map: the players only wait for the travel */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:

	/** The catalog load ended: draw and travel, or fall back */
	void HandleCatalogLoaded(EFragmentError Error, const TArray<UExpeditionIslandDefinition*>& Definitions);

	/** Travels to the drawn island (next tick, out of the load callback) */
	void TravelToIsland();

	/** Reports the error and opens the fallback map when there is one */
	void Fail(EFragmentError Error);

	/** Error of the boot */
	EFragmentError BootError = EFragmentError::None;

	/** The travel was requested: the boot runs once */
	bool bTravelRequested = false;
};
