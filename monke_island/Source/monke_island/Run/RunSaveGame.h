// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Run/RunContract.h"
#include "Run/RunLoadout.h"
#include "RunSaveGame.generated.h"

/**
 *  The run on disk (ADR-007, slot "Run"): zone, day, quota, treasury, and what each player carries. Written by
 *  URunSubsystem at every end of day, choice and purchase, through MonkeSaveSlot (envelope, checksum, transaction,
 *  last valid copy); read by the machine that keeps the run (never by a client, R-03), so JOUER goes on with the run.
 */
UCLASS()
class MONKE_ISLAND_API URunSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/**
	 *  Format written now: the schema version of the envelope and Version (a newer one is refused, never converted).
	 *  1: the run and the loadouts (T-302). 2: a loadout item keeps the loot value of a mob weapon (FRunLoadoutItem::BaseValue
	 *  and Value, T-314); a save of format 1 is read with no value (the defaults) and written again in format 2.
	 */
	static constexpr int32 CurrentVersion = 2;

	/** Bounds of a file, not tuning (security.md §3: values bounded after loading) */
	static constexpr int32 MaxLoadouts = 64;
	static constexpr int32 MaxLoadoutItems = 64;
	static constexpr int32 MaxPlayerKeyChars = 256;
	static constexpr int32 MaxItemCount = 9999;
	static constexpr int32 MaxRounds = 9999;
	static constexpr int32 MaxCrowns = 1000000000;
	static constexpr int32 MaxDays = 100000;

	/** A loadout item may only name an item definition of the game (security.md §3: asset ids from an allow-list) */
	static bool IsAllowedItemDefinition(const FSoftObjectPath& Path);

	/**
	 *  Checks what was read from disk: numbers clamped into their ranges, unknown enum values reset, loadout items that do
	 *  not name an item of the game dropped (OutFixes counts the changes). False when the file cannot be trusted (newer
	 *  format, more loadouts or items than a game can hold): it is then put aside like a damaged file.
	 */
	bool SanitizeLoaded(int32& OutFixes);

	/** Format of the save (migration) */
	UPROPERTY()
	int32 Version = CurrentVersion;

	/** The run */
	UPROPERTY()
	FRunContractState State;

	/** Last day */
	UPROPERTY()
	FRunDayReport LastDayReport;

	/** Equipment of each player, by player key (unique id, else name) */
	UPROPERTY()
	TMap<FString, FRunLoadout> Loadouts;

	/** When it was written */
	UPROPERTY()
	FDateTime Saved;
};
