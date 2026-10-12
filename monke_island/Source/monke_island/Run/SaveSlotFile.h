// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Templates/ValueOrError.h"

class UClass;
class USaveGame;

/** Save slot file layer (cahier §17): reads, checks, writes, files put aside */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeSaveSlot, Log, All);

/** Why a save slot could not be read or written (cahier §16, §17; security.md §3; error-handling.md §2) */
enum class ESaveSlotError : uint8
{
	/** Done */
	None,
	/** No file in the slot */
	NotFound,
	/** Empty, truncated, random, too large, bad checksum, unreadable properties or values that cannot be trusted */
	Corrupted,
	/** The file holds a save of another kind (another slot, a foreign file) */
	WrongClass,
	/** Written by a newer version of the game: refused, never converted */
	FutureVersion,
	/** The file is there but cannot be read (locked, no access) */
	ReadFailed,
	/** The temporary file could not be written, read back or put in place */
	WriteFailed,
	/** The slot name or its folder is not allowed (SEC-04) */
	PathRejected
};

/** Short name for the logs */
MONKE_ISLAND_API const TCHAR* LexToString(ESaveSlotError Error);

/**
 *  Defensive file layer of the save slots (Run, TapaoCampaign): cahier §16, §17; security.md §3 (SEC-04, SEC-05);
 *  error-handling.md §2, §3 (ERR-01, ERR-06). Free functions without state, tested alone (monke_island.Save.*).
 *
 *  A file is an envelope and a payload. The payload is what UGameplayStatics::SaveGameToMemory writes (engine header
 *  "SAVG", path of the class, tagged properties). The envelope takes 24 bytes:
 *    int32 0 | uint32 'MKSV' | uint16 envelope version | uint16 flags | int32 schema version | int32 payload bytes | uint32 CRC32
 *  The leading zero makes an older build, which reads the file as a bare engine save, see an empty class name and give
 *  up instead of allocating from the magic. A bare engine save (the format before the envelope) is read as a legacy file.
 *
 *  Reading never trusts the file: size bounded before reading; envelope and checksum checked; engine header parsed
 *  with every count and string length bounded; class path compared with the expected class before any object exists
 *  (the class named in the file is never loaded); properties read with a bounded string size and no object loading.
 *  Writing is a transaction: temporary file, read back and checked, current file kept as the last valid copy (.bak) or
 *  put aside when damaged, then the temporary file takes its place. A damaged file is renamed aside, never overwritten.
 *  Every call happens on the game thread, one after the other: the writes are serialised.
 */
namespace MonkeSaveSlot
{
	/** Largest file read or written (bytes) */
	inline constexpr int64 MaxFileBytes = 1024 * 1024;

	/** Longest string read from a save, in characters (names, keys, paths; an FName stops at 1023) */
	inline constexpr int32 MaxStringChars = 1000;

	/** Where a loaded save came from */
	enum class ESource : uint8
	{
		/** Nothing valid: a fresh save starts */
		None,
		/** The slot file */
		Main,
		/** A checked temporary file left by a write cut short before it took its place */
		Temp,
		/** The last valid copy */
		Backup
	};

	/** Files of a slot */
	struct MONKE_ISLAND_API FSlotPaths
	{
		/** <folder>/<Slot>.sav: the very file UGameplayStatics uses for the slot */
		FString Main;

		/** <folder>/<Slot>.sav.tmp */
		FString Temp;

		/** <folder>/<Slot>.sav.bak: last valid copy */
		FString Backup;

		/** <folder>/<Slot>.sav.<yyyymmdd-hhmmss>[-n].<Reason>: a name no file has yet, for a file put aside */
		FString MakeAsidePath(const TCHAR* Reason, const FDateTime& When) const;
	};

	/** Result of Read */
	struct MONKE_ISLAND_API FReadResult
	{
		/** The save read (transient object: the caller keeps it in a UPROPERTY or copies it), null for a fresh start */
		USaveGame* Object = nullptr;

		/** Where it came from */
		ESource Source = ESource::None;

		/** What was wrong with the slot file: None when it was read, NotFound when there was none */
		ESaveSlotError MainError = ESaveSlotError::None;

		/** File name (no folder: no user path in messages, security.md §6) of the slot file put aside, empty if none */
		FString AsideFileName;

		/** The save was a bare engine save (the format before the envelope): the next write converts it, the old file
		 *  stays as the last valid copy */
		bool bLegacy = false;

		/** False when the slot file could not be read nor put aside: nothing may be written over it this session */
		bool bWritable = true;
	};

	/** Checks a loaded save and fixes its values in place; false when it cannot be trusted (put aside like a damaged file) */
	using FAcceptSave = TFunctionRef<bool(USaveGame&)>;

	/** Slot names are identifiers (ASCII letters, digits, underscore; 1 to 64), never a name typed by a player (§17, SEC-04) */
	[[nodiscard]] MONKE_ISLAND_API bool IsValidSlotName(const FString& SlotName);

	/** Folder of the save slots (Saved/SaveGames, as the engine's generic save system) */
	[[nodiscard]] MONKE_ISLAND_API FString DefaultDirectory();

	/** Files of a slot in Directory, which must lie under the Saved folder (PathRejected otherwise) */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<FSlotPaths, ESaveSlotError> MakePaths(const FString& Directory, const FString& SlotName);

	/** A whole file for this save (WriteFailed when it cannot be serialised or is too large) */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<TArray<uint8>, ESaveSlotError> Encode(USaveGame& Object, int32 SchemaVersion);

	/** Checks a whole file without reading its properties: size, envelope, checksum, engine header and class */
	[[nodiscard]] MONKE_ISLAND_API ESaveSlotError Check(TConstArrayView<uint8> Bytes, const UClass* ExpectedClass, int32 CurrentSchemaVersion);

	/** Reads a whole file into a new transient object of ExpectedClass; bOutLegacy tells a bare engine save */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<USaveGame*, ESaveSlotError> Decode(TConstArrayView<uint8> Bytes, UClass* ExpectedClass,
		int32 CurrentSchemaVersion, bool& bOutLegacy);

	/** Reads a file of at most MaxFileBytes: NotFound, Corrupted (empty or too large) or ReadFailed (locked) otherwise */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<TArray<uint8>, ESaveSlotError> ReadBoundedFile(const FString& Path);

	/**
	 *  Reads the slot: the slot file, else a checked temporary file, else the last valid copy (copied back as the slot
	 *  file). A damaged or too new slot file is renamed aside first, untouched; a locked one is left alone (bWritable false).
	 */
	[[nodiscard]] MONKE_ISLAND_API FReadResult Read(const FSlotPaths& Paths, UClass* ExpectedClass, int32 CurrentSchemaVersion,
		FAcceptSave Accept, const FDateTime& Now);

	/** Writes the slot as a transaction (see above); None on success, the slot file untouched on failure */
	[[nodiscard]] MONKE_ISLAND_API ESaveSlotError Write(const FSlotPaths& Paths, USaveGame& Object, int32 SchemaVersion, const FDateTime& Now);

	/**
	 *  What the player is told after Read (cahier §16, error-handling.md §3), empty when all went well
	 *  @param SaveName    subject of the sentence: "La sauvegarde de la partie"
	 *  @param FreshStart  end of the sentence when nothing valid is left: "une nouvelle partie commencera"
	 */
	[[nodiscard]] MONKE_ISLAND_API FText DescribeForPlayer(const FReadResult& Result, const FText& SaveName, const FText& FreshStart);

	/** What the player is told when a write failed: the last valid copy is kept */
	[[nodiscard]] MONKE_ISLAND_API FText DescribeWriteFailureForPlayer(const FText& SaveName);
}
