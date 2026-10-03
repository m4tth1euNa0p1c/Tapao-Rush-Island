// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Templates/ValueOrError.h"
#include "Save/SaveTypes.h"

/**
 *  Campaign file format and disk operations (cahier §17, security.md §3, error-handling.md §2).
 *  Pure functions without UObject: safe on a worker thread, testable alone.
 *
 *  File = envelope header + payload (the USG_Campaign properties serialised on the game thread):
 *    uint32 Magic 'MKSV' | uint16 EnvelopeVersion | uint16 Flags | int32 SchemaVersion | FGuid CampaignId (4 x uint32)
 *    | int64 LastSavedTicks | int32 NameBytes + UTF-8 display name | int32 PayloadBytes | uint32 PayloadCrc32 | payload
 *  Every size is bounded before any allocation; the file size must match exactly; the checksum covers the payload.
 *  A truncated, random or hostile file is refused by these checks, never parsed further (SEC-05, ERR-01).
 */
namespace MonkeSave
{
	/** 'MKSV' */
	inline constexpr uint32 FileMagic = 0x4D4B5356u;

	/** Version of the envelope layout (not of the campaign schema) */
	inline constexpr uint16 EnvelopeVersion = 1;

	/** Envelope flag: the campaign holds an ExpeditionInProgress marker */
	inline constexpr uint16 FlagExpeditionInProgress = 1u << 0;

	/** Upper bound of the display name in the header, in UTF-8 bytes */
	inline constexpr int32 MaxHeaderNameBytes = 512;

	/** Fixed part of the header before the name */
	inline constexpr int32 FixedHeaderBytes = 4 + 2 + 2 + 4 + 16 + 8 + 4;

	/** Header read from or written to a file */
	struct FEnvelopeHeader
	{
		/** Flags (FlagExpeditionInProgress…) */
		uint16 Flags = 0;
		/** Campaign schema version of the payload */
		int32 SchemaVersion = 0;
		/** Stable campaign identifier */
		FGuid CampaignId;
		/** Last save time (UTC ticks) */
		int64 LastSavedTicks = 0;
		/** Display name, for the load screen */
		FString DisplayName;
		/** Payload size */
		int32 PayloadBytes = 0;
		/** CRC32 of the payload */
		uint32 PayloadCrc = 0;
	};

	/** Decoded file */
	struct FDecodedFile
	{
		/** Header */
		FEnvelopeHeader Header;
		/** Payload bytes (checksum verified) */
		TArray<uint8> Payload;
	};

	/** Where a valid file came from */
	enum class EFileSource : uint8
	{
		Main,
		Temp,
		Backup
	};

	/** Valid candidate found by ReadCandidates, in priority order */
	struct FFileCandidate
	{
		/** Source file */
		EFileSource Source = EFileSource::Main;
		/** Decoded content */
		FDecodedFile File;
	};

	/** Result of ReadCandidates */
	struct FReadReport
	{
		/** Valid files, best first (main, then leftover temp, then backup) */
		TArray<FFileCandidate> Candidates;
		/** Error when there is no candidate (CampaignNotFound, Corrupted, FutureVersion, ReadFailed) */
		ESaveError Error = ESaveError::None;
		/** The main file exists but is not valid */
		bool bMainDamaged = false;
	};

	/** Result of WriteAtomic */
	struct FWriteReport
	{
		/** None on success */
		ESaveError Error = ESaveError::None;
		/** A damaged main file was moved aside (<id>.corrupt) instead of becoming the backup */
		bool bQuarantinedDamagedMain = false;
		/** Short technical detail for the logs (no user path, security.md §5) */
		FString Detail;
	};

	/** Files of one campaign */
	struct FCampaignPaths
	{
		/** <root>/<guid>.sav */
		FString Main;
		/** <root>/<guid>.sav.tmp */
		FString Temp;
		/** <root>/<guid>.sav.bak: last valid copy */
		FString Backup;
		/** <root>/<guid>.sav.corrupt: damaged main file kept aside for diagnosis */
		FString Corrupt;
	};

	/** Full absolute form of a directory, with a trailing slash */
	MONKE_ISLAND_API FString NormaliseDirectory(const FString& Directory);

	/** True when Directory is the saved folder or below it (security.md: no write outside ProjectSavedDir) */
	[[nodiscard]] MONKE_ISLAND_API bool IsAllowedRoot(const FString& Directory);

	/** Paths of a campaign, named by its identifier only; PathRejected when the result would leave the root (SEC-04) */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<FCampaignPaths, ESaveError> MakeCampaignPaths(const FString& RootDirectory, const FGuid& CampaignId);

	/** Builds a complete file */
	[[nodiscard]] MONKE_ISLAND_API TArray<uint8> Encode(const FEnvelopeHeader& Header, const TArray<uint8>& Payload);

	/**
	 *  Decodes a complete file with every bound checked.
	 *  Errors: Corrupted (bad magic, size, checksum, bounds), FutureVersion (schema newer than CurrentSchemaVersion).
	 */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<FDecodedFile, ESaveError> Decode(const TArray<uint8>& Bytes, int32 CurrentSchemaVersion, int64 MaxFileBytes);

	/** Reads only the header of a file (load screen); false when unreadable */
	[[nodiscard]] MONKE_ISLAND_API bool ReadHeaderOnly(const FString& FilePath, int64 MaxFileBytes, FEnvelopeHeader& OutHeader);

	/** Reads a file whole, refusing it above MaxFileBytes */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<TArray<uint8>, ESaveError> ReadBoundedFile(const FString& FilePath, int64 MaxFileBytes);

	/** Reads main, leftover temp and backup, and keeps the valid ones (worker thread) */
	[[nodiscard]] MONKE_ISLAND_API FReadReport ReadCandidates(const FCampaignPaths& Paths, const FGuid& ExpectedId, int32 CurrentSchemaVersion, int64 MaxFileBytes);

	/**
	 *  Transactional write (worker thread):
	 *  1. write <id>.sav.tmp; 2. read it back, it must decode and equal the bytes written;
	 *  3. the current main file becomes the backup if it is valid, otherwise it is moved aside to .corrupt (the backup is kept);
	 *  4. the temp file becomes the main file. Any failure keeps the last valid copy and reports an error.
	 *  A crash between 3 and 4 leaves a valid temp file that ReadCandidates picks up.
	 */
	[[nodiscard]] MONKE_ISLAND_API FWriteReport WriteAtomic(const FCampaignPaths& Paths, const TArray<uint8>& FileBytes, const FGuid& ExpectedId, int32 CurrentSchemaVersion, int64 MaxFileBytes);
}
