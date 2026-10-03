// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"
#include "Save/SaveFileFormat.h"
#include "Save/SaveTypes.h"
#include "SaveSubsystem.generated.h"

class USG_Campaign;

/** Final result of a queued save operation (cahier §21 OnSaveCompleted) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveCompleted, const FSaveOperationResult&, Result);

/** Native form of OnSaveCompleted, for C++ listeners and tests */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSaveCompletedNative, const FSaveOperationResult& /*Result*/);

/** Save indicator change: Sauvegarde… / Sauvegardé / Échec */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveIndicatorChanged, ESaveIndicatorState, State);

/**
 *  Campaign saves of the host (cahier §17, §21 SaveSubsystem): list, create, load, migrate, save.
 *
 *  - Files <ProjectSavedDir>/SaveGames/Campaigns/<guid>.sav, named by identifier only, checked under the saved folder (SEC-04).
 *  - Writes are transactional (temp file, read back, then replacement keeping the last valid copy .bak) and serialised:
 *    one queue, one disk operation at a time on a worker thread, results back on the game thread (OnSaveCompleted).
 *  - Each disk operation has a maximum delay: past it the caller gets Timeout and the interface is unlocked,
 *    while the queue keeps waiting for the disk so writes never overlap.
 *  - ExpeditionInProgress: BeginExpedition writes the marker and removes the committed equipment; ApplyExpeditionResult
 *    applies the result, credits the returned equipment, clears the marker and records the ExpeditionId in one write.
 *    An ExpeditionId already in the ledger is never applied again. A marker found at load = expedition abandoned,
 *    committed equipment lost, no loot credited, summary in the load result (§17, UI10).
 *  - Transactions (create, begin, apply) are optimistic in memory and rolled back to the last written state
 *    when their write fails; the operations queued after them for the same campaign are cancelled.
 *
 *  Game thread only, except the worker lambdas that only touch copies of the data.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API USaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/**
	 *  Sets the folder of the campaign files (tests use a temporary folder). Must be under ProjectSavedDir.
	 *  @return false (and PathRejected logged) when refused; the previous folder is kept
	 */
	[[nodiscard]] bool InitializeWithRoot(const FString& RootDirectory);

	/** Waits for the queue, then stops the ticker. Called by Deinitialize; tests call it before dropping the object */
	void Shutdown();

	/** Campaigns found on disk, newest first (headers only: a damaged campaign never prevents listing the others) */
	UFUNCTION(BlueprintCallable, Category="Save")
	TArray<FCampaignSlotInfo> ListCampaigns() const;

	/**
	 *  Creates a campaign and writes it. It becomes the current campaign only once written (no fake slot, §22).
	 *  @param DisplayName  shown name, trimmed; never used as a path
	 */
	UFUNCTION(BlueprintCallable, Category="Save")
	FSaveOperationResult CreateCampaign(const FString& DisplayName);

	/** Reads, validates, migrates and loads a campaign; falls back to the last valid copy when the main file is damaged */
	UFUNCTION(BlueprintCallable, Category="Save")
	FSaveOperationResult LoadCampaign(const FGuid& CampaignId);

	/** Saves the current campaign as it is in memory (autosave, manual save at the hub) */
	UFUNCTION(BlueprintCallable, Category="Save")
	FSaveOperationResult SaveCurrentCampaign();

	/** Changes the display name of the current campaign and saves it */
	UFUNCTION(BlueprintCallable, Category="Save")
	FSaveOperationResult RenameCurrentCampaign(const FString& NewDisplayName);

	/**
	 *  Writes the ExpeditionInProgress marker before departure (server, at the hub) and takes the committed equipment
	 *  from the hub inventory. The departure is engaged only once OnSaveCompleted reports success for this request.
	 */
	UFUNCTION(BlueprintCallable, Category="Save|Expedition")
	FSaveOperationResult BeginExpedition(const FGuid& ExpeditionId, const TMap<FName, int32>& CommittedEquipment);

	/**
	 *  Applies the result of the expedition in progress and clears the marker in one write (server, at resolution).
	 *  Idempotent: an ExpeditionId already applied returns AlreadyApplied with the existing ledger entry, credits nothing.
	 */
	UFUNCTION(BlueprintCallable, Category="Save|Expedition")
	FSaveOperationResult ApplyExpeditionResult(const FGuid& ExpeditionId, const FExpeditionResultToApply& Result);

	/** Current campaign as known in memory (including writes still queued), or nullptr. Read it; change it only through this subsystem */
	UFUNCTION(BlueprintPure, Category="Save")
	USG_Campaign* GetCurrentCampaign() const { return WorkingCampaign; }

	/** Last campaign state confirmed on disk, or nullptr */
	USG_Campaign* GetCommittedCampaign() const { return CommittedCampaign; }

	/** Save indicator state */
	UFUNCTION(BlueprintPure, Category="Save")
	ESaveIndicatorState GetIndicatorState() const { return IndicatorState; }

	/** Save indicator text (String Table "MonkeSave") */
	UFUNCTION(BlueprintPure, Category="Save")
	FText GetIndicatorText() const;

	/** True while an operation is queued or running */
	UFUNCTION(BlueprintPure, Category="Save")
	bool HasPendingOperations() const { return Queue.Num() > 0; }

	/** Player message of an error (String Table "MonkeSave") */
	UFUNCTION(BlueprintPure, Category="Save")
	static FText GetErrorMessage(ESaveError Error);

	/** Folder of the campaign files */
	const FString& GetRootDirectory() const { return RootDirectory; }

	/** Advances the queue: finishes the completed disk operation, reports timeouts, starts the next one. Called by the ticker */
	void PumpQueue();

	/** Blocks the game thread until the queue is empty. Shutdown and tests only, never in gameplay */
	void WaitForIdle();

	/** Serialises the SaveGame properties of a campaign (game thread) */
	static TArray<uint8> SerializeCampaign(USG_Campaign* Campaign);

	/** Builds a campaign from serialised properties (game thread); nullptr when the archive reports an error */
	static USG_Campaign* DeserializeCampaign(const TArray<uint8>& Payload, UObject* Outer);

	/** Builds the complete file of a campaign, written as is (its SchemaVersion is kept: tests write old schemas with it) */
	static TArray<uint8> EncodeCampaignFile(USG_Campaign* Campaign);

	/** Final results of queued operations */
	UPROPERTY(BlueprintAssignable, Category="Save")
	FOnSaveCompleted OnSaveCompleted;

	/** Native form of OnSaveCompleted */
	FOnSaveCompletedNative OnSaveCompletedNative;

	/** Indicator changes */
	UPROPERTY(BlueprintAssignable, Category="Save")
	FOnSaveIndicatorChanged OnSaveIndicatorChanged;

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests: maximum delay of a disk operation */
	void SetOperationTimeoutForTests(float Seconds) { OperationTimeoutSeconds = Seconds; }
	/** Tests: artificial delay inside each disk operation (simulates a slow disk) */
	void SetWorkerDelayForTests(float Seconds) { DebugWorkerDelaySeconds = Seconds; }
	/** Tests: highest number of disk operations seen running at the same time (must stay 1) */
	int32 GetMaxConcurrentWorkersForTests() const;
#endif

protected:

	/** Maximum size of a campaign file (KiB): bigger files are refused before allocation */
	UPROPERTY(Config, EditDefaultsOnly, Category="Save|Limits", meta=(ClampMin="16"))
	int32 MaxFileKilobytes = 4096;

	/** Maximum delay of one disk operation (s) before the caller gets Timeout */
	UPROPERTY(Config, EditDefaultsOnly, Category="Save|Limits", meta=(ClampMin="0.01", Units="s"))
	float OperationTimeoutSeconds = 10.0f;

	/** Maximum display name length (characters) */
	UPROPERTY(Config, EditDefaultsOnly, Category="Save|Limits", meta=(ClampMin="1", ClampMax="128"))
	int32 MaxDisplayNameLength = 32;

	/** Maximum entries of a count map (reserve, equipment) read from disk or requested */
	UPROPERTY(Config, EditDefaultsOnly, Category="Save|Limits", meta=(ClampMin="1"))
	int32 MaxMapEntries = 256;

	/** Maximum ledger entries (expeditions) of a campaign */
	UPROPERTY(Config, EditDefaultsOnly, Category="Save|Limits", meta=(ClampMin="1"))
	int32 MaxLedgerEntries = 4096;

private:

	/** Counters shared with the worker threads */
	struct FWorkerStats;

	/** Output of a disk operation */
	struct FWorkerOutput
	{
		/** Write report (write operations) */
		MonkeSave::FWriteReport Write;
		/** Read report (load) */
		MonkeSave::FReadReport Read;
	};

	/** One queued disk operation */
	struct FQueuedOperation
	{
		/** Request identifier */
		int32 RequestId = 0;
		/** Operation */
		ESaveOperation Operation = ESaveOperation::None;
		/** Campaign */
		FGuid CampaignId;
		/** Read (load) rather than write */
		bool bIsRead = false;
		/** Failure rolls the in-memory state back and cancels the following operations of the campaign */
		bool bTransactional = false;
		/** Complete file to write */
		TArray<uint8> FileBytes;
		/** State that becomes the committed state on success */
		TStrongObjectPtr<USG_Campaign> StateToCommit;
		/** Ledger entry reported with the result */
		FExpeditionLedgerEntry LedgerEntry;
		/** Message reported with a successful result */
		FText SuccessMessage;
		/** Files of the campaign */
		MonkeSave::FCampaignPaths Paths;
		/** Started on a worker */
		bool bStarted = false;
		/** Start time (platform seconds) */
		double StartSeconds = 0.0;
		/** Timeout already reported to the caller: the late outcome is only logged */
		bool bTimeoutReported = false;
		/** Disk operation */
		TFuture<FWorkerOutput> Future;
	};

	/** Validates and trims a display name */
	[[nodiscard]] TValueOrError<FString, ESaveError> ValidateDisplayName(const FString& DisplayName) const;

	/** Result refused at once (nothing queued), logged as Warning */
	FSaveOperationResult Refuse(ESaveOperation Operation, const FGuid& CampaignId, ESaveError Error, const TCHAR* Reason) const;

	/** Snapshots a state and queues its write */
	FSaveOperationResult EnqueueWrite(ESaveOperation Operation, USG_Campaign* State, bool bTransactional, const FExpeditionLedgerEntry* LedgerEntry, const FText& SuccessMessage);

	/** Adds an operation and starts it when it is alone */
	void Enqueue(TSharedRef<FQueuedOperation> Operation);

	/** Starts the disk work of an operation on a worker thread */
	void StartOperation(FQueuedOperation& Operation);

	/** Game thread end of an operation */
	void FinishOperation(FQueuedOperation& Operation, FWorkerOutput&& Output);

	/** Game thread end of a load: deserialise, validate, migrate, recover an abandoned expedition */
	void FinishLoad(FQueuedOperation& Operation, const MonkeSave::FReadReport& Report, FSaveOperationResult& InOutResult);

	/** Rolls the working state back after a failed transaction and cancels the following operations of the campaign */
	void RollBackAfterFailure(const FQueuedOperation& Failed);

	/** Broadcasts a final result */
	void Broadcast(const FSaveOperationResult& Result);

	/** Updates the indicator */
	void SetIndicator(ESaveIndicatorState NewState);

	/** True while a write is queued or running */
	bool HasPendingWrite() const;

	/** Ticker body */
	bool TickQueue(float DeltaSeconds);

	/** Maximum file size in bytes */
	int64 GetMaxFileBytes() const { return static_cast<int64>(MaxFileKilobytes) * 1024; }

	/** Copy of a campaign owned by this subsystem */
	USG_Campaign* DuplicateCampaign(const USG_Campaign* Source);

	/** Campaign state including queued writes */
	UPROPERTY(Transient)
	TObjectPtr<USG_Campaign> WorkingCampaign;

	/** Last campaign state written successfully */
	UPROPERTY(Transient)
	TObjectPtr<USG_Campaign> CommittedCampaign;

	/** Folder of the campaign files (absolute, under ProjectSavedDir) */
	FString RootDirectory;

	/** Queued operations; the first one is running */
	TArray<TSharedRef<FQueuedOperation>> Queue;

	/** Last request identifier */
	int32 LastRequestId = 0;

	/** Indicator state */
	ESaveIndicatorState IndicatorState = ESaveIndicatorState::Idle;

	/** Ticker while the queue is not empty */
	FTSTicker::FDelegateHandle TickerHandle;

	/** Counters shared with the workers */
	TSharedPtr<FWorkerStats, ESPMode::ThreadSafe> WorkerStats;

	/** Artificial delay in each disk operation (tests) */
	float DebugWorkerDelaySeconds = 0.0f;
};
