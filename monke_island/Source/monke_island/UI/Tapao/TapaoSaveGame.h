// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TapaoSaveGame.generated.h"

/**
 *  Campaign save of the player (menu CONTINUER / NOUVELLE PARTIE): what the expeditions already gave. Written by
 *  UTapaoUISubsystem when an island starts and when the main menu shows the game, through MonkeSaveSlot (envelope,
 *  checksum, transaction, last valid copy, damaged file put aside); one slot per machine, never written by a client
 *  playing on a host (the campaign is the host's, cahier §16).
 */
UCLASS()
class MONKE_ISLAND_API UTapaoSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** Format written now: the schema version of the envelope and Version (a newer one is refused, never converted) */
	static constexpr int32 CurrentVersion = 1;

	/** Bounds of a file, not tuning (security.md §3: values bounded after loading) */
	static constexpr int32 MaxCount = 1000000000;
	static constexpr int32 MaxIslandNameChars = 128;

	/**
	 *  Checks what was read from disk: counters and time clamped into their ranges, an island name too long cleared, a
	 *  date out of range reset (OutFixes counts the changes). False when the file is of a newer format.
	 */
	bool SanitizeLoaded(int32& OutFixes);

	/** Format of the save (migration) */
	UPROPERTY()
	int32 Version = CurrentVersion;

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
