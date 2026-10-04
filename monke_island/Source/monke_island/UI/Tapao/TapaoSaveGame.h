// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TapaoSaveGame.generated.h"

/**
 *  Campaign save of the player (menu CONTINUER / NOUVELLE PARTIE): what the expeditions already gave. Written by
 *  UTapaoUISubsystem when an island starts and when the main menu shows the game; one slot per machine.
 */
UCLASS()
class MONKE_ISLAND_API UTapaoSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** Format of the save (migration) */
	UPROPERTY()
	int32 Version = 1;

	/** Expeditions started */
	UPROPERTY()
	int32 Expeditions = 0;

	/** Last island played (FRG_<Ile>_<NN>) */
	UPROPERTY()
	FString LastIsland;

	/** Deaths of the player (NAUFRAGÉ !) */
	UPROPERTY()
	int32 Wrecks = 0;

	/** Islands conquered (ÎLE CONQUISE !) */
	UPROPERTY()
	int32 Islands = 0;

	/** Time played on the islands (s, counted by the automatic saves) */
	UPROPERTY()
	float PlaySeconds = 0.0f;

	/** Money banked by the campaign */
	UPROPERTY()
	int32 Gold = 0;

	/** When the campaign was last played */
	UPROPERTY()
	FDateTime LastPlayed;
};
