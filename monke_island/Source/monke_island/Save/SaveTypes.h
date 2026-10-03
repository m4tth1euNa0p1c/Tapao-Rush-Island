// monke_island

#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.generated.h"

/** Save domain log (cahier §17, error-handling.md §4): writes, loads, recoveries, refusals */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeSave, Log, All);

/**
 *  Every expected failure of the save domain (error-handling.md §2).
 *  None means success. Each value has a short player message in the "MonkeSave" String Table (USaveSubsystem::GetErrorMessage).
 */
UENUM(BlueprintType)
enum class ESaveError : uint8
{
	/** Success */
	None,
	/** Display name empty, too long or with control characters */
	InvalidName,
	/** Request with impossible values (negative amount, unknown equipment, overflow) */
	InvalidRequest,
	/** Operation that needs a loaded campaign, with none loaded */
	NoCampaignLoaded,
	/** No file for this campaign identifier */
	CampaignNotFound,
	/** File unreadable: truncated, random, bad checksum, values out of bounds; no valid copy left */
	Corrupted,
	/** File written by a newer version of the game: refused, never modified */
	FutureVersion,
	/** The disk refused the read */
	ReadFailed,
	/** The disk refused the write or the replacement: the previous valid copy is kept */
	WriteFailed,
	/** The temporary file read back differs from what was written: nothing replaced */
	ValidationFailed,
	/** Built path outside the save folder (security.md SEC-04) */
	PathRejected,
	/** Disk operation longer than the maximum delay: interface unlocked, queue still serialised */
	Timeout,
	/** Queued operation dropped because an earlier transaction of the same campaign failed */
	Cancelled,
	/** An expedition marker is already present: one expedition at a time */
	ExpeditionAlreadyInProgress,
	/** Result for an expedition that is not the one in progress */
	ExpeditionMismatch,
	/** This ExpeditionId was already applied: the existing ledger entry is returned, nothing credited (idempotence) */
	AlreadyApplied,
	/** Equipment committed but not owned */
	InsufficientEquipment,
	/** Write requested on a client: only the host writes the campaign */
	NotHost
};

/** Operation that produced a result */
UENUM(BlueprintType)
enum class ESaveOperation : uint8
{
	/** Not an operation (default) */
	None,
	/** New campaign written for the first time */
	Create,
	/** Campaign read, validated and possibly migrated */
	Load,
	/** Plain save of the current campaign (autosave or manual, at the hub) */
	Save,
	/** Display name change */
	Rename,
	/** ExpeditionInProgress marker written before departure */
	BeginExpedition,
	/** Expedition result applied and marker cleared in one write */
	ApplyExpeditionResult,
	/** Marker found at load (crash / quit mid-expedition): expedition abandoned and written */
	RecoverAbandonedExpedition
};

/** Discreet save indicator of cahier §17 */
UENUM(BlueprintType)
enum class ESaveIndicatorState : uint8
{
	/** Nothing written yet in this session */
	Idle,
	/** "Sauvegarde…": at least one write queued or running */
	Saving,
	/** "Sauvegardé": last write succeeded and nothing pending */
	Saved,
	/** "Échec": last write failed */
	Failed
};

/**
 *  ExpeditionInProgress marker (§17): written before departure, with the equipment removed from the hub inventory.
 *  Cleared in the same write that applies the result. Found at load = expedition abandoned, equipment lost.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionInProgressMarker
{
	GENERATED_BODY()

	/** Expedition identifier (correlation id); invalid when no expedition is in progress */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	FGuid ExpeditionId;

	/** Equipment taken from the hub inventory for this expedition: item id -> count */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	TMap<FName, int32> CommittedEquipment;

	/** Departure time (UTC) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	FDateTime StartedUtc;

	/** True when an expedition is in progress */
	bool IsActive() const { return ExpeditionId.IsValid(); }
};

/**
 *  Expedition result to apply (minimal format owned by the save domain).
 *  The economy computes it on the server (§7) and only hands it here; nothing is computed by the save.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionResultToApply
{
	GENERATED_BODY()

	/** Gold change after quotas and tax (may be negative; the balance never goes below zero) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save|Expedition")
	int64 GoldDelta = 0;

	/** Reserve change per resource category (wood, food, chest…) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save|Expedition")
	TMap<FName, int32> ReserveDeltaByCategory;

	/** Committed equipment brought back alive: credited back to the hub inventory (at most what was committed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save|Expedition")
	TMap<FName, int32> ReturnedEquipment;
};

/** One line of the campaign ledger: an expedition applied or abandoned, never applied twice */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionLedgerEntry
{
	GENERATED_BODY()

	/** Expedition identifier */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	FGuid ExpeditionId;

	/** True when the expedition was abandoned (crash or quit): nothing credited */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	bool bAbandoned = false;

	/** Gold credited (0 when abandoned) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	int64 GoldDelta = 0;

	/** Reserve credited per category (empty when abandoned) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	TMap<FName, int32> ReserveDeltaByCategory;

	/** Committed equipment not brought back (all of it when abandoned) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	TMap<FName, int32> LostEquipment;

	/** Time the entry was written (UTC) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	FDateTime AppliedUtc;
};

/** One campaign file as listed by the load screen (read from the file header only) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FCampaignSlotInfo
{
	GENERATED_BODY()

	/** Stable internal identifier (file name) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FGuid CampaignId;

	/** Display name, editable, never used as a path */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FString DisplayName;

	/** Last successful write (UTC) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FDateTime LastSavedUtc;

	/** Schema version found in the file */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	int32 SchemaVersion = 0;

	/** False when the main file header is unreadable (the load may still recover the backup) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bReadable = false;

	/** A last valid copy (.bak) exists */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bHasBackup = false;

	/** The file holds an ExpeditionInProgress marker: loading it resumes at the hub with an abandoned expedition */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bHasExpeditionInProgress = false;
};

/**
 *  Typed result of every save operation (cahier §21, OnSaveCompleted): success or enumerated error, player message.
 *  A request function returns it at once: bQueued = the final result will come through OnSaveCompleted with the same RequestId,
 *  otherwise the request was refused (Error) or answered at once (AlreadyApplied with the existing ledger entry).
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FSaveOperationResult
{
	GENERATED_BODY()

	/** Request identifier (0 when nothing was queued) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	int32 RequestId = 0;

	/** Operation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	ESaveOperation Operation = ESaveOperation::None;

	/** Campaign concerned (correlation id) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FGuid CampaignId;

	/** None on success */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	ESaveError Error = ESaveError::None;

	/** True when the operation was queued and its final result is still to come */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bQueued = false;

	/** Final result of an operation already reported as Timeout (same RequestId): the state may have changed since */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bLateResult = false;

	/** Load only: the main file was damaged or missing, a last valid copy was used (the damaged file is kept aside, never erased silently) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bLoadedFromBackup = false;

	/** Load only: the file was migrated from an older schema */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bMigrated = false;

	/** Load only: an expedition marker was found, the expedition is abandoned (see LedgerEntry) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	bool bRecoveredAbandonedExpedition = false;

	/** Apply / AlreadyApplied / recovery: the ledger entry of the expedition */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FExpeditionLedgerEntry LedgerEntry;

	/** Short player message (String Table "MonkeSave"); empty on a plain success */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FText PlayerMessage;

	/** True when the operation succeeded (or is queued without error so far) */
	bool IsSuccess() const { return Error == ESaveError::None; }
};

/** Player messages of the save domain, from the code-defined String Table "MonkeSave" (error-handling.md §3, ERR-08) */
struct MONKE_ISLAND_API FSaveMessages
{
	/** Id of the String Table */
	static const TCHAR* TableId;

	/** Registers the String Table once (game thread) */
	static void EnsureRegistered();

	/** Message for an error (empty text for None) */
	static FText ForError(ESaveError Error);

	/** Message when a write reported as Timeout finally succeeded */
	static FText LateSaveDone();

	/** Message when a load used the last valid copy */
	static FText LoadedFromBackup();

	/** Summary of an abandoned expedition found at load (§17, UI10) */
	static FText AbandonedExpedition(int32 LostItemCount);

	/** Indicator text: Sauvegarde… / Sauvegardé / Échec */
	static FText ForIndicator(ESaveIndicatorState State);
};

/** Short readable name of an error for the logs */
MONKE_ISLAND_API FString LexToString(ESaveError Error);

/** Short readable name of an operation for the logs */
MONKE_ISLAND_API FString LexToString(ESaveOperation Operation);
