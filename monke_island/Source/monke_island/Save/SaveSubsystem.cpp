// monke_island

#include "Save/SaveSubsystem.h"
#include "Save/SG_Campaign.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Logging/StructuredLog.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include <atomic>

struct USaveSubsystem::FWorkerStats
{
	/** Disk operations running now */
	std::atomic<int32> Active { 0 };
	/** Highest value of Active seen */
	std::atomic<int32> MaxActive { 0 };
};

namespace
{
	/** Default folder of the campaign files */
	FString DefaultCampaignRoot()
	{
		return FPaths::ProjectSavedDir() / TEXT("SaveGames") / TEXT("Campaigns");
	}

	/** Adds a delta to a count, false on overflow or negative result */
	bool AddCount(int32 Current, int64 Delta, int32& OutValue)
	{
		const int64 Value = static_cast<int64>(Current) + Delta;
		if (Value < 0 || Value > MAX_int32)
		{
			return false;
		}
		OutValue = static_cast<int32>(Value);
		return true;
	}

	/** Memory reader whose strings and names can never be longer than the payload itself (SEC-05) */
	class FBoundedMemoryReader : public FMemoryReader
	{
	public:
		explicit FBoundedMemoryReader(const TArray<uint8>& InBytes)
			: FMemoryReader(InBytes, true)
		{
			ArMaxSerializeSize = InBytes.Num();
		}
	};

	/** Total items of a count map */
	int32 CountItems(const TMap<FName, int32>& Map)
	{
		int64 Total = 0;
		for (const TPair<FName, int32>& Pair : Map)
		{
			Total += FMath::Max(Pair.Value, 0);
		}
		return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifetime

void USaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FSaveMessages::EnsureRegistered();
	if (!InitializeWithRoot(DefaultCampaignRoot()))
	{
		UE_LOGFMT(LogMonkeSave, Error, "Default campaign folder refused: saves disabled for this session");
	}
}

void USaveSubsystem::Deinitialize()
{
	Shutdown();
	Super::Deinitialize();
}

bool USaveSubsystem::InitializeWithRoot(const FString& InRootDirectory)
{
	if (!WorkerStats.IsValid())
	{
		WorkerStats = MakeShared<FWorkerStats, ESPMode::ThreadSafe>();
	}
	if (!MonkeSave::IsAllowedRoot(InRootDirectory))
	{
		UE_LOGFMT(LogMonkeSave, Warning, "Campaign folder refused: outside the saved folder (SEC-04)");
		return false;
	}
	RootDirectory = MonkeSave::NormaliseDirectory(InRootDirectory);
	return true;
}

void USaveSubsystem::Shutdown()
{
	WaitForIdle();
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries

TArray<FCampaignSlotInfo> USaveSubsystem::ListCampaigns() const
{
	TArray<FCampaignSlotInfo> Slots;
	if (RootDirectory.IsEmpty())
	{
		return Slots;
	}

	// Every file of the folder, filtered by suffix: a campaign may only have its temp or backup file after a crash
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(RootDirectory / TEXT("*")), true, false);
	TSet<FGuid> Ids;
	for (const FString& File : Files)
	{
		FString Base = File;
		if (!Base.RemoveFromEnd(TEXT(".sav")) && !Base.RemoveFromEnd(TEXT(".sav.bak")) && !Base.RemoveFromEnd(TEXT(".sav.tmp")))
		{
			continue;
		}
		FGuid Id;
		if (FGuid::ParseExact(Base, EGuidFormats::Digits, Id) && Id.IsValid())
		{
			Ids.Add(Id);
		}
	}

	const int64 MaxBytes = GetMaxFileBytes();
	for (const FGuid& Id : Ids)
	{
		TValueOrError<MonkeSave::FCampaignPaths, ESaveError> Paths = MonkeSave::MakeCampaignPaths(RootDirectory, Id);
		if (Paths.HasError())
		{
			continue;
		}
		FCampaignSlotInfo& Slot = Slots.AddDefaulted_GetRef();
		Slot.CampaignId = Id;
		Slot.bHasBackup = IFileManager::Get().FileExists(*Paths.GetValue().Backup);

		MonkeSave::FEnvelopeHeader Header;
		Slot.bReadable = MonkeSave::ReadHeaderOnly(Paths.GetValue().Main, MaxBytes, Header);
		if (Slot.bReadable
			|| MonkeSave::ReadHeaderOnly(Paths.GetValue().Temp, MaxBytes, Header)
			|| MonkeSave::ReadHeaderOnly(Paths.GetValue().Backup, MaxBytes, Header))
		{
			Slot.DisplayName = Header.DisplayName.Left(MaxDisplayNameLength);
			Slot.LastSavedUtc = FDateTime(Header.LastSavedTicks);
			Slot.SchemaVersion = Header.SchemaVersion;
			Slot.bHasExpeditionInProgress = (Header.Flags & MonkeSave::FlagExpeditionInProgress) != 0;
		}
	}
	Slots.Sort([](const FCampaignSlotInfo& A, const FCampaignSlotInfo& B)
	{
		return A.LastSavedUtc > B.LastSavedUtc;
	});
	return Slots;
}

FText USaveSubsystem::GetIndicatorText() const
{
	return FSaveMessages::ForIndicator(IndicatorState);
}

FText USaveSubsystem::GetErrorMessage(ESaveError Error)
{
	return FSaveMessages::ForError(Error);
}

// ---------------------------------------------------------------------------------------------------------------------
// Requests

FSaveOperationResult USaveSubsystem::CreateCampaign(const FString& DisplayName)
{
	TValueOrError<FString, ESaveError> Name = ValidateDisplayName(DisplayName);
	if (Name.HasError())
	{
		return Refuse(ESaveOperation::Create, FGuid(), Name.GetError(), TEXT("invalid display name"));
	}
	if (!IsWriteAllowed())
	{
		return Refuse(ESaveOperation::Create, FGuid(), ESaveError::NotHost, TEXT("write requested on a client"));
	}
	if (RootDirectory.IsEmpty())
	{
		return Refuse(ESaveOperation::Create, FGuid(), ESaveError::PathRejected, TEXT("no campaign folder"));
	}

	USG_Campaign* Campaign = NewObject<USG_Campaign>(this);
	Campaign->SchemaVersion = USG_Campaign::CurrentSchemaVersion;
	Campaign->CampaignId = FGuid::NewGuid();
	Campaign->DisplayName = Name.StealValue();
	Campaign->CreatedUtc = FDateTime::UtcNow();
	Campaign->LastSavedUtc = Campaign->CreatedUtc;

	// Not current until written: a failed creation leaves no playable slot (§22)
	return EnqueueWrite(ESaveOperation::Create, Campaign, true, nullptr, FText::GetEmpty());
}

FSaveOperationResult USaveSubsystem::LoadCampaign(const FGuid& CampaignId)
{
	TValueOrError<MonkeSave::FCampaignPaths, ESaveError> Paths = MonkeSave::MakeCampaignPaths(RootDirectory, CampaignId);
	if (Paths.HasError())
	{
		return Refuse(ESaveOperation::Load, CampaignId, CampaignId.IsValid() ? Paths.GetError() : ESaveError::CampaignNotFound, TEXT("invalid campaign id or folder"));
	}

	TSharedRef<FQueuedOperation> Operation = MakeShared<FQueuedOperation>();
	Operation->RequestId = ++LastRequestId;
	Operation->Operation = ESaveOperation::Load;
	Operation->CampaignId = CampaignId;
	Operation->bIsRead = true;
	Operation->Paths = Paths.StealValue();
	Enqueue(Operation);

	FSaveOperationResult Result;
	Result.RequestId = Operation->RequestId;
	Result.Operation = ESaveOperation::Load;
	Result.CampaignId = CampaignId;
	Result.bQueued = true;
	return Result;
}

FSaveOperationResult USaveSubsystem::SaveCurrentCampaign()
{
	if (!WorkingCampaign)
	{
		return Refuse(ESaveOperation::Save, FGuid(), ESaveError::NoCampaignLoaded, TEXT("no campaign loaded"));
	}
	if (!IsWriteAllowed())
	{
		return Refuse(ESaveOperation::Save, WorkingCampaign->CampaignId, ESaveError::NotHost, TEXT("write requested on a client"));
	}
	return EnqueueWrite(ESaveOperation::Save, WorkingCampaign, false, nullptr, FText::GetEmpty());
}

FSaveOperationResult USaveSubsystem::RenameCurrentCampaign(const FString& NewDisplayName)
{
	if (!WorkingCampaign)
	{
		return Refuse(ESaveOperation::Rename, FGuid(), ESaveError::NoCampaignLoaded, TEXT("no campaign loaded"));
	}
	if (!IsWriteAllowed())
	{
		return Refuse(ESaveOperation::Rename, WorkingCampaign->CampaignId, ESaveError::NotHost, TEXT("write requested on a client"));
	}
	TValueOrError<FString, ESaveError> Name = ValidateDisplayName(NewDisplayName);
	if (Name.HasError())
	{
		return Refuse(ESaveOperation::Rename, WorkingCampaign->CampaignId, Name.GetError(), TEXT("invalid display name"));
	}
	WorkingCampaign->DisplayName = Name.StealValue();
	return EnqueueWrite(ESaveOperation::Rename, WorkingCampaign, false, nullptr, FText::GetEmpty());
}

FSaveOperationResult USaveSubsystem::BeginExpedition(const FGuid& ExpeditionId, const TMap<FName, int32>& CommittedEquipment)
{
	if (!WorkingCampaign)
	{
		return Refuse(ESaveOperation::BeginExpedition, FGuid(), ESaveError::NoCampaignLoaded, TEXT("no campaign loaded"));
	}
	const FGuid CampaignId = WorkingCampaign->CampaignId;
	if (!IsWriteAllowed())
	{
		return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::NotHost, TEXT("write requested on a client"));
	}
	if (!ExpeditionId.IsValid() || CommittedEquipment.Num() > MaxMapEntries)
	{
		return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::InvalidRequest, TEXT("invalid expedition id or equipment list"));
	}
	if (const FExpeditionLedgerEntry* Existing = WorkingCampaign->FindLedgerEntry(ExpeditionId))
	{
		FSaveOperationResult Result = Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::AlreadyApplied, TEXT("expedition id already resolved"));
		Result.LedgerEntry = *Existing;
		return Result;
	}
	if (WorkingCampaign->HasExpeditionInProgress() && WorkingCampaign->ExpeditionInProgress.ExpeditionId == ExpeditionId)
	{
		// Idempotent: same departure requested again (retry after a Timeout). Answer with the write still queued, or at once if written
		FSaveOperationResult Same;
		Same.Operation = ESaveOperation::BeginExpedition;
		Same.CampaignId = CampaignId;
		for (const TSharedRef<FQueuedOperation>& Pending : Queue)
		{
			if (Pending->Operation == ESaveOperation::BeginExpedition && Pending->CampaignId == CampaignId && Pending->StateToCommit.IsValid()
				&& Pending->StateToCommit->ExpeditionInProgress.ExpeditionId == ExpeditionId)
			{
				Same.RequestId = Pending->RequestId;
				Same.bQueued = true;
				break;
			}
		}
		UE_LOGFMT(LogMonkeSave, Log, "Campaign {CampaignId}: departure of expedition {ExpeditionId} requested again ({State})",
			CampaignId.ToString(), ExpeditionId.ToString(), Same.bQueued ? TEXT("write pending") : TEXT("already written"));
		return Same;
	}
	if (WorkingCampaign->HasExpeditionInProgress())
	{
		return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::ExpeditionAlreadyInProgress, TEXT("marker already set"));
	}
	if (WorkingCampaign->ExpeditionLedger.Num() >= MaxLedgerEntries)
	{
		UE_LOGFMT(LogMonkeSave, Error, "Campaign {CampaignId}: ledger full ({Count}), expedition refused", CampaignId.ToString(), WorkingCampaign->ExpeditionLedger.Num());
		return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::InvalidRequest, TEXT("ledger full"));
	}
	for (const TPair<FName, int32>& Pair : CommittedEquipment)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0)
		{
			return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::InvalidRequest, TEXT("invalid committed equipment entry"));
		}
		const int32* Owned = WorkingCampaign->HubEquipment.Find(Pair.Key);
		if (!Owned || *Owned < Pair.Value)
		{
			return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::InsufficientEquipment, TEXT("equipment not owned"));
		}
	}

	USG_Campaign* Next = DuplicateCampaign(WorkingCampaign);
	for (const TPair<FName, int32>& Pair : CommittedEquipment)
	{
		int32* Owned = Next->HubEquipment.Find(Pair.Key);
		if (!ensureMsgf(Owned && *Owned >= Pair.Value, TEXT("Committed equipment changed between validation and copy")))
		{
			return Refuse(ESaveOperation::BeginExpedition, CampaignId, ESaveError::InvalidRequest, TEXT("equipment changed during the request"));
		}
		*Owned -= Pair.Value;
		if (*Owned == 0)
		{
			Next->HubEquipment.Remove(Pair.Key);
		}
	}
	Next->ExpeditionInProgress.ExpeditionId = ExpeditionId;
	Next->ExpeditionInProgress.CommittedEquipment = CommittedEquipment;
	Next->ExpeditionInProgress.StartedUtc = FDateTime::UtcNow();

	WorkingCampaign = Next;
	UE_LOGFMT(LogMonkeSave, Display, "Campaign {CampaignId}: expedition {ExpeditionId} marker queued ({Items} items committed)",
		CampaignId.ToString(), ExpeditionId.ToString(), CountItems(CommittedEquipment));
	return EnqueueWrite(ESaveOperation::BeginExpedition, Next, true, nullptr, FText::GetEmpty());
}

FSaveOperationResult USaveSubsystem::ApplyExpeditionResult(const FGuid& ExpeditionId, const FExpeditionResultToApply& Result)
{
	if (!WorkingCampaign)
	{
		return Refuse(ESaveOperation::ApplyExpeditionResult, FGuid(), ESaveError::NoCampaignLoaded, TEXT("no campaign loaded"));
	}
	const FGuid CampaignId = WorkingCampaign->CampaignId;
	if (!IsWriteAllowed())
	{
		return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::NotHost, TEXT("write requested on a client"));
	}
	if (!ExpeditionId.IsValid())
	{
		return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("invalid expedition id"));
	}

	// Idempotence first: a resolved id returns the existing entry and credits nothing (§17, §22)
	if (const FExpeditionLedgerEntry* Existing = WorkingCampaign->FindLedgerEntry(ExpeditionId))
	{
		FSaveOperationResult Refused = Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::AlreadyApplied, TEXT("expedition already applied, nothing credited"));
		Refused.LedgerEntry = *Existing;
		return Refused;
	}
	const FExpeditionInProgressMarker& Marker = WorkingCampaign->ExpeditionInProgress;
	if (!Marker.IsActive() || Marker.ExpeditionId != ExpeditionId)
	{
		return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::ExpeditionMismatch, TEXT("no marker for this expedition"));
	}
	if (Result.ReserveDeltaByCategory.Num() > MaxMapEntries || Result.ReturnedEquipment.Num() > MaxMapEntries)
	{
		return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("result too large"));
	}

	// Validate everything before touching anything
	const int64 Gold = WorkingCampaign->Gold;
	if ((Result.GoldDelta > 0 && Gold > MAX_int64 - Result.GoldDelta) || Gold + Result.GoldDelta < 0)
	{
		return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("gold out of bounds"));
	}
	for (const TPair<FName, int32>& Pair : Result.ReturnedEquipment)
	{
		const int32* Committed = Marker.CommittedEquipment.Find(Pair.Key);
		if (Pair.Key.IsNone() || Pair.Value < 0 || !Committed || Pair.Value > *Committed)
		{
			return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("returned equipment not committed"));
		}
	}
	for (const TPair<FName, int32>& Pair : Result.ReserveDeltaByCategory)
	{
		const int32* Current = WorkingCampaign->ReserveByCategory.Find(Pair.Key);
		int32 Unused = 0;
		if (Pair.Key.IsNone() || !AddCount(Current ? *Current : 0, Pair.Value, Unused))
		{
			return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("reserve out of bounds"));
		}
	}

	USG_Campaign* Next = DuplicateCampaign(WorkingCampaign);
	FExpeditionLedgerEntry Entry;
	Entry.ExpeditionId = ExpeditionId;
	Entry.bAbandoned = false;
	Entry.GoldDelta = Result.GoldDelta;
	Entry.ReserveDeltaByCategory = Result.ReserveDeltaByCategory;
	Entry.AppliedUtc = FDateTime::UtcNow();

	Next->Gold = Gold + Result.GoldDelta;
	for (const TPair<FName, int32>& Pair : Result.ReserveDeltaByCategory)
	{
		int32& Value = Next->ReserveByCategory.FindOrAdd(Pair.Key);
		if (!ensureMsgf(AddCount(Value, Pair.Value, Value), TEXT("Reserve changed between validation and copy")))
		{
			return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("reserve out of bounds"));
		}
	}
	for (const TPair<FName, int32>& Pair : Next->ExpeditionInProgress.CommittedEquipment)
	{
		const int32* Returned = Result.ReturnedEquipment.Find(Pair.Key);
		const int32 Back = Returned ? *Returned : 0;
		if (Back > 0)
		{
			int32& Owned = Next->HubEquipment.FindOrAdd(Pair.Key);
			if (!ensureMsgf(AddCount(Owned, Back, Owned), TEXT("Hub equipment overflow when crediting returned equipment")))
			{
				return Refuse(ESaveOperation::ApplyExpeditionResult, CampaignId, ESaveError::InvalidRequest, TEXT("equipment out of bounds"));
			}
		}
		if (Pair.Value - Back > 0)
		{
			Entry.LostEquipment.Add(Pair.Key, Pair.Value - Back);
		}
	}
	Next->ExpeditionInProgress = FExpeditionInProgressMarker();
	Next->ExpeditionLedger.Add(Entry);

	WorkingCampaign = Next;
	UE_LOGFMT(LogMonkeSave, Display, "Campaign {CampaignId}: expedition {ExpeditionId} result queued (gold {GoldDelta}, {Lost} items lost)",
		CampaignId.ToString(), ExpeditionId.ToString(), Result.GoldDelta, CountItems(Entry.LostEquipment));
	return EnqueueWrite(ESaveOperation::ApplyExpeditionResult, Next, true, &Entry, FText::GetEmpty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Queue

bool USaveSubsystem::IsWriteAllowed() const
{
	if (bActAsClientForTests)
	{
		return false;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	return !World || World->GetNetMode() != NM_Client;
}

TValueOrError<FString, ESaveError> USaveSubsystem::ValidateDisplayName(const FString& DisplayName) const
{
	FString Name = DisplayName.TrimStartAndEnd();
	if (Name.IsEmpty() || Name.Len() > MaxDisplayNameLength)
	{
		return MakeError(ESaveError::InvalidName);
	}
	for (const TCHAR Char : Name)
	{
		if (Char < 32 || Char == 127)
		{
			return MakeError(ESaveError::InvalidName);
		}
	}
	return MakeValue(MoveTemp(Name));
}

FSaveOperationResult USaveSubsystem::Refuse(ESaveOperation Operation, const FGuid& CampaignId, ESaveError Error, const TCHAR* Reason) const
{
	UE_LOGFMT(LogMonkeSave, Warning, "{Operation} refused for campaign {CampaignId}: {Error} ({Reason})",
		LexToString(Operation), CampaignId.ToString(), LexToString(Error), Reason);
	FSaveOperationResult Result;
	Result.Operation = Operation;
	Result.CampaignId = CampaignId;
	Result.Error = Error;
	Result.PlayerMessage = FSaveMessages::ForError(Error);
	return Result;
}

FSaveOperationResult USaveSubsystem::EnqueueWrite(ESaveOperation Operation, USG_Campaign* State, bool bTransactional, const FExpeditionLedgerEntry* LedgerEntry, const FText& SuccessMessage)
{
	check(State); // internal callers only
	if (!IsWriteAllowed())
	{
		return Refuse(Operation, State->CampaignId, ESaveError::NotHost, TEXT("write requested on a client"));
	}
	TValueOrError<MonkeSave::FCampaignPaths, ESaveError> Paths = MonkeSave::MakeCampaignPaths(RootDirectory, State->CampaignId);
	if (Paths.HasError())
	{
		FSaveOperationResult Refused = Refuse(Operation, State->CampaignId, Paths.GetError(), TEXT("path refused"));
		if (bTransactional && CommittedCampaign && WorkingCampaign && CommittedCampaign->CampaignId == State->CampaignId)
		{
			WorkingCampaign = DuplicateCampaign(CommittedCampaign);
		}
		return Refused;
	}

	// Snapshot: later changes of the working state never leak into this write
	USG_Campaign* Snapshot = DuplicateCampaign(State);
	Snapshot->SchemaVersion = USG_Campaign::CurrentSchemaVersion;
	Snapshot->LastSavedUtc = FDateTime::UtcNow();

	TSharedRef<FQueuedOperation> Queued = MakeShared<FQueuedOperation>();
	Queued->RequestId = ++LastRequestId;
	Queued->Operation = Operation;
	Queued->CampaignId = Snapshot->CampaignId;
	Queued->bTransactional = bTransactional;
	Queued->FileBytes = EncodeCampaignFile(Snapshot);
	Queued->StateToCommit.Reset(Snapshot);
	Queued->Paths = Paths.StealValue();
	Queued->bMainKnownDamaged = CampaignsWithRejectedMain.Contains(Queued->CampaignId);
	Queued->SuccessMessage = SuccessMessage;
	if (LedgerEntry)
	{
		Queued->LedgerEntry = *LedgerEntry;
	}

	SetIndicator(ESaveIndicatorState::Saving);
	Enqueue(Queued);

	FSaveOperationResult Result;
	Result.RequestId = Queued->RequestId;
	Result.Operation = Operation;
	Result.CampaignId = Queued->CampaignId;
	Result.bQueued = true;
	Result.LedgerEntry = Queued->LedgerEntry;
	return Result;
}

void USaveSubsystem::Enqueue(TSharedRef<FQueuedOperation> Operation)
{
	Queue.Add(Operation);
	if (Queue.Num() == 1)
	{
		StartOperation(*Queue[0]);
	}
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USaveSubsystem::TickQueue));
	}
}

void USaveSubsystem::StartOperation(FQueuedOperation& Operation)
{
	if (Operation.bStarted)
	{
		return;
	}
	Operation.bStarted = true;
	Operation.StartSeconds = FPlatformTime::Seconds();

	// The worker only gets copies: no UObject, no `this`
	const bool bIsRead = Operation.bIsRead;
	const MonkeSave::FCampaignPaths Paths = Operation.Paths;
	const FGuid CampaignId = Operation.CampaignId;
	const int64 MaxBytes = GetMaxFileBytes();
	const float Delay = DebugWorkerDelaySeconds;
	const bool bMainKnownDamaged = Operation.bMainKnownDamaged;
	TArray<uint8> Bytes = MoveTemp(Operation.FileBytes);
	TSharedPtr<FWorkerStats, ESPMode::ThreadSafe> Stats = WorkerStats;

	Operation.Future = Async(EAsyncExecution::ThreadPool, [bIsRead, bMainKnownDamaged, Paths, CampaignId, MaxBytes, Delay, Bytes = MoveTemp(Bytes), Stats]()
	{
		if (Stats.IsValid())
		{
			const int32 Now = ++Stats->Active;
			int32 Seen = Stats->MaxActive.load();
			while (Now > Seen && !Stats->MaxActive.compare_exchange_weak(Seen, Now))
			{
			}
		}
		if (Delay > 0.0f)
		{
			FPlatformProcess::Sleep(Delay);
		}

		FWorkerOutput Output;
		if (bIsRead)
		{
			Output.Read = MonkeSave::ReadCandidates(Paths, CampaignId, USG_Campaign::CurrentSchemaVersion, MaxBytes);
		}
		else
		{
			Output.Write = MonkeSave::WriteAtomic(Paths, Bytes, CampaignId, USG_Campaign::CurrentSchemaVersion, MaxBytes, bMainKnownDamaged);
		}

		if (Stats.IsValid())
		{
			--Stats->Active;
		}
		return Output;
	});
}

void USaveSubsystem::PumpQueue()
{
	while (Queue.Num() > 0)
	{
		TSharedRef<FQueuedOperation> Front = Queue[0];
		StartOperation(*Front);
		if (Front->Future.IsReady())
		{
			Queue.RemoveAt(0);
			FinishOperation(*Front, Front->Future.Consume());
			continue;
		}

		const double Elapsed = FPlatformTime::Seconds() - Front->StartSeconds;
		if (!Front->bTimeoutReported && Elapsed > OperationTimeoutSeconds)
		{
			// Unlock the caller; the queue keeps waiting for the disk so two writes never overlap
			Front->bTimeoutReported = true;
			UE_LOGFMT(LogMonkeSave, Warning, "{Operation} #{RequestId} for campaign {CampaignId}: still running after {Seconds} s, reported as Timeout",
				LexToString(Front->Operation), Front->RequestId, Front->CampaignId.ToString(), Elapsed);
			FSaveOperationResult Result;
			Result.RequestId = Front->RequestId;
			Result.Operation = Front->Operation;
			Result.CampaignId = Front->CampaignId;
			Result.Error = ESaveError::Timeout;
			Result.PlayerMessage = FSaveMessages::ForError(ESaveError::Timeout);
			if (!Front->bIsRead)
			{
				SetIndicator(ESaveIndicatorState::Failed);
			}
			Broadcast(Result);
		}
		break;
	}
}

void USaveSubsystem::WaitForIdle()
{
	while (Queue.Num() > 0)
	{
		TSharedRef<FQueuedOperation> Front = Queue[0];
		StartOperation(*Front);
		Front->Future.Wait();
		PumpQueue();
	}
}

bool USaveSubsystem::TickQueue(float DeltaSeconds)
{
	PumpQueue();
	if (Queue.Num() == 0)
	{
		TickerHandle.Reset();
		return false;
	}
	return true;
}

void USaveSubsystem::FinishOperation(FQueuedOperation& Operation, FWorkerOutput&& Output)
{
	FSaveOperationResult Result;
	Result.RequestId = Operation.RequestId;
	Result.Operation = Operation.Operation;
	Result.CampaignId = Operation.CampaignId;

	if (Operation.bIsRead)
	{
		if (Operation.bTimeoutReported)
		{
			// The caller already went back to a stable screen: a late load must not replace the current campaign
			UE_LOGFMT(LogMonkeSave, Warning, "Load #{RequestId} of campaign {CampaignId} finished after its timeout: result dropped",
				Operation.RequestId, Operation.CampaignId.ToString());
			return;
		}
		FinishLoad(Operation, Output.Read, Result);
		Broadcast(Result);
		return;
	}

	const MonkeSave::FWriteReport& Write = Output.Write;
	Result.Error = Write.Error;
	if (Write.bQuarantinedDamagedMain)
	{
		UE_LOGFMT(LogMonkeSave, Warning, "Campaign {CampaignId}: damaged main file moved aside (.corrupt), backup kept", Operation.CampaignId.ToString());
	}

	TArray<FSaveOperationResult> CancelledResults;
	if (Write.Error == ESaveError::None)
	{
		USG_Campaign* Written = Operation.StateToCommit.Get();
		CommittedCampaign = DuplicateCampaign(Written);
		CampaignsWithRejectedMain.Remove(Operation.CampaignId);
		if (Operation.Operation == ESaveOperation::Create)
		{
			WorkingCampaign = DuplicateCampaign(Written);
		}
		else if (WorkingCampaign && WorkingCampaign->CampaignId == Written->CampaignId)
		{
			WorkingCampaign->LastSavedUtc = Written->LastSavedUtc;
		}
		Result.LedgerEntry = Operation.LedgerEntry;
		Result.PlayerMessage = Operation.bTimeoutReported ? FSaveMessages::LateSaveDone() : Operation.SuccessMessage;
		UE_LOGFMT(LogMonkeSave, Display, "{Operation} #{RequestId}: campaign {CampaignId} written",
			LexToString(Operation.Operation), Operation.RequestId, Operation.CampaignId.ToString());
		if (!HasPendingWrite())
		{
			SetIndicator(ESaveIndicatorState::Saved);
		}
	}
	else
	{
		Result.PlayerMessage = FSaveMessages::ForError(Write.Error);
		UE_LOGFMT(LogMonkeSave, Error, "{Operation} #{RequestId}: campaign {CampaignId} not written: {Error} ({Detail}); last valid copy kept",
			LexToString(Operation.Operation), Operation.RequestId, Operation.CampaignId.ToString(), LexToString(Write.Error), Write.Detail);
		SetIndicator(ESaveIndicatorState::Failed);
		if (Operation.bTransactional)
		{
			CancelledResults = RollBackAfterFailure(Operation);
		}
	}

	if (Operation.bTimeoutReported)
	{
		// Already reported as Timeout: the final outcome is still broadcast (same RequestId) so the caller learns
		// whether the marker or the result really landed, or was rolled back (ERR-04, ERR-05)
		Result.bLateResult = true;
		UE_LOGFMT(LogMonkeSave, Warning, "{Operation} #{RequestId} finished after its timeout with {Error}",
			LexToString(Operation.Operation), Operation.RequestId, LexToString(Write.Error));
	}
	Broadcast(Result);
	for (const FSaveOperationResult& Cancelled : CancelledResults)
	{
		Broadcast(Cancelled);
	}
}

void USaveSubsystem::FinishLoad(FQueuedOperation& Operation, const MonkeSave::FReadReport& Report, FSaveOperationResult& InOutResult)
{
	const FString CampaignIdText = Operation.CampaignId.ToString();
	if (Report.Candidates.Num() == 0)
	{
		InOutResult.Error = Report.Error != ESaveError::None ? Report.Error : ESaveError::Corrupted;
		InOutResult.PlayerMessage = FSaveMessages::ForError(InOutResult.Error);
		UE_LOGFMT(LogMonkeSave, Warning, "Load #{RequestId}: campaign {CampaignId} unusable: {Error}",
			Operation.RequestId, CampaignIdText, LexToString(InOutResult.Error));
		return;
	}

	USG_Campaign* Loaded = nullptr;
	MonkeSave::EFileSource Source = MonkeSave::EFileSource::Main;
	bool bMigrated = false;
	bool bMainRejected = false;
	for (const MonkeSave::FFileCandidate& Candidate : Report.Candidates)
	{
		USG_Campaign* Campaign = DeserializeCampaign(Candidate.File.Payload, this);
		FString Reason;
		if (!Campaign)
		{
			Reason = TEXT("payload unreadable");
		}
		else
		{
			// The header is authoritative: property serialisation skips values equal to the class default
			Campaign->SchemaVersion = Candidate.File.Header.SchemaVersion;
			const bool bCandidateMigrated = Campaign->Migrate();
			if (Campaign->CampaignId == Operation.CampaignId
				&& Campaign->ValidateLoadedData(MaxMapEntries, MaxLedgerEntries, MaxDisplayNameLength, Reason))
			{
				Loaded = Campaign;
				Source = Candidate.Source;
				bMigrated = bCandidateMigrated;
				break;
			}
			if (Reason.IsEmpty())
			{
				Reason = TEXT("campaign id mismatch");
			}
		}
		bMainRejected |= Candidate.Source == MonkeSave::EFileSource::Main;
		UE_LOGFMT(LogMonkeSave, Warning, "Load #{RequestId}: campaign {CampaignId} candidate {Source} rejected ({Reason})",
			Operation.RequestId, CampaignIdText, static_cast<int32>(Candidate.Source), Reason);
	}
	if (bMainRejected)
	{
		// Decodes but holds impossible values: the next write moves it aside instead of making it the last valid copy
		CampaignsWithRejectedMain.Add(Operation.CampaignId);
	}

	if (!Loaded)
	{
		InOutResult.Error = ESaveError::Corrupted;
		InOutResult.PlayerMessage = FSaveMessages::ForError(ESaveError::Corrupted);
		UE_LOGFMT(LogMonkeSave, Warning, "Load #{RequestId}: campaign {CampaignId} has no valid copy", Operation.RequestId, CampaignIdText);
		return;
	}

	InOutResult.bMigrated = bMigrated;
	InOutResult.bLoadedFromBackup = Report.bMainDamaged || bMainRejected || Source == MonkeSave::EFileSource::Backup;
	TArray<FText> Messages;
	if (InOutResult.bLoadedFromBackup)
	{
		Messages.Add(FSaveMessages::LoadedFromBackup());
		UE_LOGFMT(LogMonkeSave, Warning, "Load #{RequestId}: campaign {CampaignId} main file damaged or missing, last valid copy used", Operation.RequestId, CampaignIdText);
	}
	if (bMigrated)
	{
		UE_LOGFMT(LogMonkeSave, Display, "Load #{RequestId}: campaign {CampaignId} migrated to schema {Schema}", Operation.RequestId, CampaignIdText, USG_Campaign::CurrentSchemaVersion);
	}

	CommittedCampaign = Loaded;
	WorkingCampaign = DuplicateCampaign(Loaded);
	UE_LOGFMT(LogMonkeSave, Display, "Load #{RequestId}: campaign {CampaignId} loaded", Operation.RequestId, CampaignIdText);

	// Marker found: crash or quit mid-expedition. Back at the hub, expedition abandoned, committed equipment lost, nothing credited (§17)
	if (WorkingCampaign->HasExpeditionInProgress())
	{
		const FExpeditionInProgressMarker Marker = WorkingCampaign->ExpeditionInProgress;
		FExpeditionLedgerEntry Entry;
		Entry.ExpeditionId = Marker.ExpeditionId;
		Entry.bAbandoned = true;
		Entry.LostEquipment = Marker.CommittedEquipment;
		Entry.AppliedUtc = FDateTime::UtcNow();

		WorkingCampaign->ExpeditionInProgress = FExpeditionInProgressMarker();
		if (!WorkingCampaign->IsExpeditionApplied(Marker.ExpeditionId))
		{
			WorkingCampaign->ExpeditionLedger.Add(Entry);
		}

		InOutResult.bRecoveredAbandonedExpedition = true;
		InOutResult.LedgerEntry = Entry;
		const FText Summary = FSaveMessages::AbandonedExpedition(CountItems(Entry.LostEquipment));
		Messages.Add(Summary);
		UE_LOGFMT(LogMonkeSave, Warning, "Load #{RequestId}: campaign {CampaignId} had expedition {ExpeditionId} in progress: abandoned, {Lost} items lost, nothing credited",
			Operation.RequestId, CampaignIdText, Marker.ExpeditionId.ToString(), CountItems(Entry.LostEquipment));
		// Not transactional: the recovery is deterministic, so a failed write keeps it in memory (no stuck marker) and the next save retries;
		// until then the file still holds the marker and a new crash recovers the same way, crediting nothing
		// The recovery is also the rollback base: a later failed transaction never brings the marker back
		CommittedCampaign = DuplicateCampaign(WorkingCampaign);
		// The summary is shown once, with the load result: the recovery write itself carries no message
		EnqueueWrite(ESaveOperation::RecoverAbandonedExpedition, WorkingCampaign, false, &Entry, FText::GetEmpty());
	}

	InOutResult.PlayerMessage = Messages.Num() > 0 ? FText::Join(FText::FromString(TEXT("\n")), Messages) : FText::GetEmpty();
}

TArray<FSaveOperationResult> USaveSubsystem::RollBackAfterFailure(const FQueuedOperation& Failed)
{
	if (Failed.Operation != ESaveOperation::Create && WorkingCampaign && WorkingCampaign->CampaignId == Failed.CampaignId)
	{
		// Back to the last state on disk: no half-applied transaction in memory (ERR-05)
		WorkingCampaign = CommittedCampaign && CommittedCampaign->CampaignId == Failed.CampaignId ? DuplicateCampaign(CommittedCampaign) : nullptr;
		UE_LOGFMT(LogMonkeSave, Warning, "Campaign {CampaignId}: rolled back to the last written state", Failed.CampaignId.ToString());
	}

	// The following writes of this campaign were built on the failed state: cancel them
	TArray<TSharedRef<FQueuedOperation>> Cancelled;
	for (int32 Index = Queue.Num() - 1; Index >= 0; --Index)
	{
		const TSharedRef<FQueuedOperation>& Pending = Queue[Index];
		if (!Pending->bStarted && !Pending->bIsRead && Pending->CampaignId == Failed.CampaignId)
		{
			Cancelled.Insert(Pending, 0);
			Queue.RemoveAt(Index);
		}
	}
	TArray<FSaveOperationResult> Results;
	for (const TSharedRef<FQueuedOperation>& Pending : Cancelled)
	{
		FSaveOperationResult& Result = Results.AddDefaulted_GetRef();
		Result.RequestId = Pending->RequestId;
		Result.Operation = Pending->Operation;
		Result.CampaignId = Pending->CampaignId;
		Result.Error = ESaveError::Cancelled;
		Result.PlayerMessage = FSaveMessages::ForError(ESaveError::Cancelled);
		UE_LOGFMT(LogMonkeSave, Warning, "{Operation} #{RequestId} for campaign {CampaignId} cancelled after a failed transaction",
			LexToString(Pending->Operation), Pending->RequestId, Pending->CampaignId.ToString());
	}
	return Results;
}

void USaveSubsystem::Broadcast(const FSaveOperationResult& Result)
{
	OnSaveCompletedNative.Broadcast(Result);
	OnSaveCompleted.Broadcast(Result);
}

void USaveSubsystem::SetIndicator(ESaveIndicatorState NewState)
{
	if (IndicatorState != NewState)
	{
		IndicatorState = NewState;
		OnSaveIndicatorChanged.Broadcast(NewState);
	}
}

bool USaveSubsystem::HasPendingWrite() const
{
	return Queue.ContainsByPredicate([](const TSharedRef<FQueuedOperation>& Operation)
	{
		return !Operation->bIsRead;
	});
}

USG_Campaign* USaveSubsystem::DuplicateCampaign(const USG_Campaign* Source)
{
	check(Source); // internal callers only
	return DuplicateObject<USG_Campaign>(Source, this);
}

// ---------------------------------------------------------------------------------------------------------------------
// Serialisation

TArray<uint8> USaveSubsystem::SerializeCampaign(USG_Campaign* Campaign)
{
	TArray<uint8> Bytes;
	if (!Campaign)
	{
		return Bytes;
	}
	FMemoryWriter Writer(Bytes, true);
	Writer.ArIsSaveGame = true;
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	Archive.ArIsSaveGame = true;
	Campaign->Serialize(Archive);
	return Bytes;
}

USG_Campaign* USaveSubsystem::DeserializeCampaign(const TArray<uint8>& Payload, UObject* Outer)
{
	if (Payload.Num() == 0)
	{
		return nullptr;
	}
	USG_Campaign* Campaign = NewObject<USG_Campaign>(Outer ? Outer : GetTransientPackage());
	// Strings and names longer than the payload are refused before allocation; the proxy copies this limit at construction
	FBoundedMemoryReader Reader(Payload);
	Reader.ArIsSaveGame = true;
	FObjectAndNameAsStringProxyArchive Archive(Reader, false);
	Archive.ArIsSaveGame = true;
	Campaign->Serialize(Archive);
	if (Archive.IsError() || Reader.IsError())
	{
		return nullptr;
	}
	return Campaign;
}

TArray<uint8> USaveSubsystem::EncodeCampaignFile(USG_Campaign* Campaign)
{
	if (!Campaign)
	{
		return TArray<uint8>();
	}
	MonkeSave::FEnvelopeHeader Header;
	Header.Flags = Campaign->HasExpeditionInProgress() ? MonkeSave::FlagExpeditionInProgress : 0;
	Header.SchemaVersion = Campaign->SchemaVersion;
	Header.CampaignId = Campaign->CampaignId;
	Header.LastSavedTicks = Campaign->LastSavedUtc.GetTicks();
	Header.DisplayName = Campaign->DisplayName;
	return MonkeSave::Encode(Header, SerializeCampaign(Campaign));
}

#if WITH_DEV_AUTOMATION_TESTS
int32 USaveSubsystem::GetMaxConcurrentWorkersForTests() const
{
	return WorkerStats.IsValid() ? WorkerStats->MaxActive.load() : 0;
}
#endif
