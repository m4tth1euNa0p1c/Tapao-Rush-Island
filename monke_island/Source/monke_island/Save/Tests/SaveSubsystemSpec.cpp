// monke_island

#include "Misc/AutomationTest.h"
#include "Save/SaveSubsystem.h"
#include "Save/SaveFileFormat.h"
#include "Save/SG_Campaign.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MonkeSaveTests
{
	/**
	 *  Temporary save folder under Saved/Automation (removed at the end) and subsystems working in it.
	 *  A "fresh" subsystem on the same folder stands for a restart of the game after a quit or a crash.
	 */
	struct FRig
	{
		/** Temporary folder */
		FString Root;
		/** Main subsystem */
		TStrongObjectPtr<USaveSubsystem> Save;
		/** Final results received by the main subsystem */
		TArray<FSaveOperationResult> Results;

		FRig()
		{
			Root = FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("MonkeSaveTests") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
			IFileManager::Get().MakeDirectory(*Root, true);
			Save = MakeSubsystem();
			Save->OnSaveCompletedNative.AddLambda([this](const FSaveOperationResult& Result)
			{
				Results.Add(Result);
			});
		}

		~FRig()
		{
			if (Save.IsValid())
			{
				Save->Shutdown();
			}
			for (TStrongObjectPtr<USaveSubsystem>& Other : Others)
			{
				Other->Shutdown();
			}
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		}

		/** New subsystem on the temporary folder */
		TStrongObjectPtr<USaveSubsystem> MakeSubsystem()
		{
			TStrongObjectPtr<USaveSubsystem> Subsystem(NewObject<USaveSubsystem>(GetTransientPackage()));
			const bool bRootAccepted = Subsystem->InitializeWithRoot(Root);
			ensureMsgf(bRootAccepted, TEXT("test root refused"));
			return Subsystem;
		}

		/** Subsystem of a restarted game, loading CampaignId to completion; its results go to OutResults */
		USaveSubsystem* Restart(const FGuid& CampaignId, TArray<FSaveOperationResult>& OutResults)
		{
			TStrongObjectPtr<USaveSubsystem> Fresh = MakeSubsystem();
			Fresh->OnSaveCompletedNative.AddLambda([&OutResults](const FSaveOperationResult& Result)
			{
				OutResults.Add(Result);
			});
			Fresh->LoadCampaign(CampaignId);
			Fresh->WaitForIdle();
			Others.Add(Fresh);
			return Fresh.Get();
		}

		/** Creates a campaign and waits for its write; returns its id (invalid on failure) */
		FGuid CreateCampaign(const FString& Name = TEXT("Expédition test"))
		{
			const FSaveOperationResult Queued = Save->CreateCampaign(Name);
			Save->WaitForIdle();
			return Queued.bQueued && Save->GetCurrentCampaign() ? Save->GetCurrentCampaign()->CampaignId : FGuid();
		}

		/** Gives hub equipment to the current campaign and saves it */
		void GiveEquipment(const TMap<FName, int32>& Equipment)
		{
			Save->GetCurrentCampaign()->HubEquipment = Equipment;
			Save->SaveCurrentCampaign();
			Save->WaitForIdle();
		}

		/** Files of a campaign */
		MonkeSave::FCampaignPaths Paths(const FGuid& CampaignId) const
		{
			TValueOrError<MonkeSave::FCampaignPaths, ESaveError> Built = MonkeSave::MakeCampaignPaths(Root, CampaignId);
			return Built.HasValue() ? Built.GetValue() : MonkeSave::FCampaignPaths();
		}

		/** Last result received for a request */
		const FSaveOperationResult* FindResult(int32 RequestId) const
		{
			return Results.FindByPredicate([RequestId](const FSaveOperationResult& Result)
			{
				return Result.RequestId == RequestId;
			});
		}

		/** Truncates a file to half its size */
		static void Truncate(const FString& File)
		{
			TArray<uint8> Bytes;
			FFileHelper::LoadFileToArray(Bytes, *File);
			Bytes.SetNum(Bytes.Num() / 2);
			FFileHelper::SaveArrayToFile(Bytes, *File);
		}

	private:
		/** Restarted subsystems, shut down with the rig */
		TArray<TStrongObjectPtr<USaveSubsystem>> Others;
	};

	const FName Musket(TEXT("Musket"));
	const FName Rope(TEXT("Rope"));
	const FName Wood(TEXT("Wood"));
}

// ---------------------------------------------------------------------------------------------------------------------
// Nominal paths

BEGIN_DEFINE_SPEC(FSaveCampaignSpec, "monke_island.Save.Campaign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<MonkeSaveTests::FRig> Rig;
END_DEFINE_SPEC(FSaveCampaignSpec)

void FSaveCampaignSpec::Define()
{
	using namespace MonkeSaveTests;

	BeforeEach([this]()
	{
		Rig = MakeUnique<FRig>();
	});

	AfterEach([this]()
	{
		Rig.Reset();
	});

	It("round-trips a campaign through the disk", [this]()
	{
		const FGuid Id = Rig->CreateCampaign(TEXT("Ma campagne"));
		if (!TestTrue(TEXT("campaign created"), Id.IsValid()))
		{
			return;
		}
		USG_Campaign* Campaign = Rig->Save->GetCurrentCampaign();
		Campaign->Gold = 42;
		Campaign->ReserveByCategory.Add(Wood, 7);
		Campaign->HubEquipment.Add(Musket, 3);
		Rig->Save->RenameCurrentCampaign(TEXT("  Nouvelle campagne "));
		Rig->Save->WaitForIdle();

		TArray<FSaveOperationResult> Results;
		USaveSubsystem* Fresh = Rig->Restart(Id, Results);
		USG_Campaign* Loaded = Fresh->GetCurrentCampaign();
		if (!TestNotNull(TEXT("loaded"), Loaded))
		{
			return;
		}
		TestTrue(TEXT("load succeeded"), Results.Num() == 1 && Results[0].IsSuccess() && !Results[0].bLoadedFromBackup);
		TestEqual(TEXT("id"), Loaded->CampaignId, Id);
		TestEqual(TEXT("name trimmed"), Loaded->DisplayName, FString(TEXT("Nouvelle campagne")));
		TestEqual(TEXT("gold"), Loaded->Gold, static_cast<int64>(42));
		TestEqual(TEXT("reserve"), Loaded->ReserveByCategory.FindRef(Wood), 7);
		TestEqual(TEXT("equipment"), Loaded->HubEquipment.FindRef(Musket), 3);
		TestEqual(TEXT("schema"), Loaded->SchemaVersion, USG_Campaign::CurrentSchemaVersion);

		const TArray<FCampaignSlotInfo> Slots = Fresh->ListCampaigns();
		TestTrue(TEXT("listed with its display name"), Slots.Num() == 1 && Slots[0].CampaignId == Id && Slots[0].bReadable && Slots[0].DisplayName == TEXT("Nouvelle campagne"));
		TestTrue(TEXT("last valid copy kept"), Slots.Num() == 1 && Slots[0].bHasBackup);
		TestEqual(TEXT("indicator"), Rig->Save->GetIndicatorState(), ESaveIndicatorState::Saved);
	});

	It("names files by identifier, never by the typed name", [this]()
	{
		const FGuid Id = Rig->CreateCampaign(TEXT("..\\..\\evil/name"));
		if (!TestTrue(TEXT("campaign created"), Id.IsValid()))
		{
			return;
		}
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Rig->Root / TEXT("*")), true, false);
		TestEqual(TEXT("one file"), Files.Num(), 1);
		TestEqual(TEXT("file named by guid"), Files.Num() > 0 ? Files[0] : FString(), Id.ToString(EGuidFormats::Digits) + TEXT(".sav"));
		TestEqual(TEXT("name kept as display name only"), Rig->Save->GetCurrentCampaign()->DisplayName, FString(TEXT("..\\..\\evil/name")));
	});

	It("migrates a schema 1 file and keeps its applied expeditions", [this]()
	{
		const FGuid AppliedId = FGuid::NewGuid();
		USG_Campaign* Old = NewObject<USG_Campaign>(GetTransientPackage());
		Old->SchemaVersion = 1;
		Old->CampaignId = FGuid::NewGuid();
		Old->DisplayName = TEXT("Ancienne");
		Old->Gold = 300;
		Old->LegacyAppliedExpeditionIds.Add(AppliedId);
		const TArray<uint8> File = USaveSubsystem::EncodeCampaignFile(Old);
		IFileManager::Get().MakeDirectory(*Rig->Root, true);
		TestTrue(TEXT("v1 file written"), FFileHelper::SaveArrayToFile(File, *Rig->Paths(Old->CampaignId).Main));

		Rig->Save->LoadCampaign(Old->CampaignId);
		Rig->Save->WaitForIdle();
		USG_Campaign* Loaded = Rig->Save->GetCurrentCampaign();
		if (!TestNotNull(TEXT("loaded"), Loaded))
		{
			return;
		}
		TestTrue(TEXT("reported as migrated"), Rig->Results.Num() == 1 && Rig->Results[0].bMigrated);
		TestEqual(TEXT("schema upgraded"), Loaded->SchemaVersion, USG_Campaign::CurrentSchemaVersion);
		TestTrue(TEXT("legacy id moved into the ledger"), Loaded->IsExpeditionApplied(AppliedId) && Loaded->LegacyAppliedExpeditionIds.Num() == 0);
		TestEqual(TEXT("gold kept"), Loaded->Gold, static_cast<int64>(300));

		const FSaveOperationResult Again = Rig->Save->ApplyExpeditionResult(AppliedId, FExpeditionResultToApply());
		TestEqual(TEXT("migrated expedition never applied again"), Again.Error, ESaveError::AlreadyApplied);
	});

	It("applies an expedition result once and clears the marker (recette n°9 partiel)", [this]()
	{
		const FGuid Id = Rig->CreateCampaign();
		Rig->GiveEquipment({ { Musket, 2 } });
		const FGuid ExpeditionA = FGuid::NewGuid();

		const FSaveOperationResult Begin = Rig->Save->BeginExpedition(ExpeditionA, { { Musket, 2 } });
		Rig->Save->WaitForIdle();
		const FSaveOperationResult* BeginDone = Rig->FindResult(Begin.RequestId);
		TestTrue(TEXT("marker written"), BeginDone && BeginDone->IsSuccess());
		TestTrue(TEXT("marker on disk"), Rig->Save->GetCommittedCampaign()->HasExpeditionInProgress());
		TestFalse(TEXT("committed equipment left the hub"), Rig->Save->GetCurrentCampaign()->HubEquipment.Contains(Musket));

		FExpeditionResultToApply Result;
		Result.GoldDelta = 100;
		Result.ReserveDeltaByCategory.Add(Wood, 3);
		Result.ReturnedEquipment.Add(Musket, 1);
		const FSaveOperationResult Apply = Rig->Save->ApplyExpeditionResult(ExpeditionA, Result);
		Rig->Save->WaitForIdle();
		const FSaveOperationResult* ApplyDone = Rig->FindResult(Apply.RequestId);
		TestTrue(TEXT("result applied"), ApplyDone && ApplyDone->IsSuccess());
		USG_Campaign* Campaign = Rig->Save->GetCurrentCampaign();
		TestEqual(TEXT("gold credited"), Campaign->Gold, static_cast<int64>(100));
		TestEqual(TEXT("reserve credited"), Campaign->ReserveByCategory.FindRef(Wood), 3);
		TestEqual(TEXT("returned equipment back"), Campaign->HubEquipment.FindRef(Musket), 1);
		TestFalse(TEXT("marker cleared"), Campaign->HasExpeditionInProgress());
		TestTrue(TEXT("lost equipment recorded"), ApplyDone && ApplyDone->LedgerEntry.LostEquipment.FindRef(Musket) == 1);

		// Second expedition: only its own result counts, the first is never credited again
		const FGuid ExpeditionB = FGuid::NewGuid();
		Rig->Save->BeginExpedition(ExpeditionB, TMap<FName, int32>());
		Rig->Save->WaitForIdle();
		FExpeditionResultToApply ResultB;
		ResultB.GoldDelta = 50;
		Rig->Save->ApplyExpeditionResult(ExpeditionB, ResultB);
		Rig->Save->WaitForIdle();
		const FSaveOperationResult Replay = Rig->Save->ApplyExpeditionResult(ExpeditionA, Result);
		TestEqual(TEXT("replay of the first refused"), Replay.Error, ESaveError::AlreadyApplied);
		TestFalse(TEXT("replay not queued"), Replay.bQueued);

		TArray<FSaveOperationResult> Results;
		USG_Campaign* Loaded = Rig->Restart(Id, Results)->GetCurrentCampaign();
		if (TestNotNull(TEXT("reloaded"), Loaded))
		{
			TestEqual(TEXT("gold on disk = 100 + 50"), Loaded->Gold, static_cast<int64>(150));
			TestEqual(TEXT("two ledger entries"), Loaded->ExpeditionLedger.Num(), 2);
			TestFalse(TEXT("no marker on disk"), Loaded->HasExpeditionInProgress());
		}
	});

	It("serialises concurrent writes: one disk operation at a time, in order", [this]()
	{
		const FGuid Id = Rig->CreateCampaign();
		Rig->Save->SetWorkerDelayForTests(0.02f);
		const int32 FirstResult = Rig->Results.Num();
		TArray<int32> RequestIds;
		for (int32 Index = 1; Index <= 5; ++Index)
		{
			Rig->Save->GetCurrentCampaign()->Gold = Index;
			RequestIds.Add(Rig->Save->SaveCurrentCampaign().RequestId);
		}
		TestTrue(TEXT("queued"), Rig->Save->HasPendingOperations());
		TestEqual(TEXT("indicator saving"), Rig->Save->GetIndicatorState(), ESaveIndicatorState::Saving);
		Rig->Save->WaitForIdle();

		TestEqual(TEXT("five results"), Rig->Results.Num() - FirstResult, 5);
		for (int32 Index = 0; Index < 5 && FirstResult + Index < Rig->Results.Num(); ++Index)
		{
			const FSaveOperationResult& Result = Rig->Results[FirstResult + Index];
			TestTrue(FString::Printf(TEXT("result %d in order and successful"), Index), Result.RequestId == RequestIds[Index] && Result.IsSuccess());
		}
		TestEqual(TEXT("never two disk operations at once"), Rig->Save->GetMaxConcurrentWorkersForTests(), 1);

		TArray<FSaveOperationResult> Results;
		USG_Campaign* Loaded = Rig->Restart(Id, Results)->GetCurrentCampaign();
		TestTrue(TEXT("last write wins"), Loaded && Loaded->Gold == 5);
		TestEqual(TEXT("no temp file left"), IFileManager::Get().FileExists(*Rig->Paths(Id).Temp), false);
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// Failure paths (error-handling.md §6)

BEGIN_DEFINE_SPEC(FSaveErrorsSpec, "monke_island.Save.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<MonkeSaveTests::FRig> Rig;
END_DEFINE_SPEC(FSaveErrorsSpec)

void FSaveErrorsSpec::Define()
{
	using namespace MonkeSaveTests;

	BeforeEach([this]()
	{
		Rig = MakeUnique<FRig>();
	});

	AfterEach([this]()
	{
		Rig.Reset();
	});

	It("refuses invalid display names without queuing anything", [this]()
	{
		const FSaveOperationResult Empty = Rig->Save->CreateCampaign(TEXT("   "));
		const FSaveOperationResult Control = Rig->Save->CreateCampaign(TEXT("a\nb"));
		const FSaveOperationResult TooLong = Rig->Save->CreateCampaign(FString::ChrN(200, TEXT('x')));
		TestTrue(TEXT("empty refused"), Empty.Error == ESaveError::InvalidName && !Empty.bQueued);
		TestTrue(TEXT("control char refused"), Control.Error == ESaveError::InvalidName && !Control.bQueued);
		TestTrue(TEXT("too long refused"), TooLong.Error == ESaveError::InvalidName && !TooLong.bQueued);
		TestFalse(TEXT("player message"), Empty.PlayerMessage.IsEmpty());
		TestFalse(TEXT("nothing queued"), Rig->Save->HasPendingOperations());
		TestNull(TEXT("no fake slot"), Rig->Save->GetCurrentCampaign());
	});

	It("refuses a folder outside the saved directory", [this]()
	{
		TestFalse(TEXT("engine dir refused"), Rig->Save->InitializeWithRoot(FPaths::EngineDir()));
		TestFalse(TEXT("escape refused"), Rig->Save->InitializeWithRoot(FPaths::ProjectSavedDir() / TEXT("..") / TEXT("..")));
		TestEqual(TEXT("previous folder kept"), Rig->Save->GetRootDirectory(), MonkeSave::NormaliseDirectory(Rig->Root));
	});

	It("falls back to the last valid copy when the main file is truncated, and never erases it", [this]()
	{
		const FGuid Id = Rig->CreateCampaign();
		Rig->Save->GetCurrentCampaign()->Gold = 10;
		Rig->Save->SaveCurrentCampaign();
		Rig->Save->GetCurrentCampaign()->Gold = 20;
		Rig->Save->SaveCurrentCampaign();
		Rig->Save->WaitForIdle();
		const MonkeSave::FCampaignPaths Paths = Rig->Paths(Id);
		FRig::Truncate(Paths.Main);

		TArray<FSaveOperationResult> Results;
		USaveSubsystem* Fresh = Rig->Restart(Id, Results);
		USG_Campaign* Loaded = Fresh->GetCurrentCampaign();
		if (!TestNotNull(TEXT("loaded from backup"), Loaded))
		{
			return;
		}
		TestTrue(TEXT("reported"), Results.Num() == 1 && Results[0].IsSuccess() && Results[0].bLoadedFromBackup && !Results[0].PlayerMessage.IsEmpty());
		TestEqual(TEXT("previous valid state"), Loaded->Gold, static_cast<int64>(10));

		// Next save: the damaged file goes aside, the valid backup stays the backup
		Fresh->SaveCurrentCampaign();
		Fresh->WaitForIdle();
		TestTrue(TEXT("damaged file kept aside"), IFileManager::Get().FileExists(*Paths.Corrupt));
		TArray<uint8> BackupBytes;
		FFileHelper::LoadFileToArray(BackupBytes, *Paths.Backup);
		TestTrue(TEXT("backup still valid"), MonkeSave::Decode(BackupBytes, USG_Campaign::CurrentSchemaVersion, 1 << 22).HasValue());
	});

	It("reports a clear error when every copy is damaged, and other campaigns still load", [this]()
	{
		const FGuid Healthy = Rig->CreateCampaign(TEXT("Saine"));
		const FGuid Broken = Rig->CreateCampaign(TEXT("Cassée"));
		Rig->Save->SaveCurrentCampaign();
		Rig->Save->WaitForIdle();
		const MonkeSave::FCampaignPaths Paths = Rig->Paths(Broken);
		FRig::Truncate(Paths.Main);
		FRig::Truncate(Paths.Backup);

		TArray<FSaveOperationResult> Results;
		USaveSubsystem* Fresh = Rig->Restart(Broken, Results);
		TestTrue(TEXT("Corrupted reported"), Results.Num() == 1 && Results[0].Error == ESaveError::Corrupted && !Results[0].PlayerMessage.IsEmpty());
		TestNull(TEXT("nothing loaded"), Fresh->GetCurrentCampaign());

		const TArray<FCampaignSlotInfo> Slots = Fresh->ListCampaigns();
		TestEqual(TEXT("both still listed"), Slots.Num(), 2);
		Fresh->LoadCampaign(Healthy);
		Fresh->WaitForIdle();
		TestTrue(TEXT("healthy campaign loads"), Fresh->GetCurrentCampaign() && Fresh->GetCurrentCampaign()->CampaignId == Healthy);
	});

	It("survives a random file without crashing", [this]()
	{
		const FGuid Id = FGuid::NewGuid();
		FRandomStream Random(1234);
		TArray<uint8> Garbage;
		Garbage.SetNum(4096);
		for (uint8& Byte : Garbage)
		{
			Byte = static_cast<uint8>(Random.RandRange(0, 255));
		}
		Garbage[0] = 0x56; Garbage[1] = 0x53; Garbage[2] = 0x4B; Garbage[3] = 0x4D; // right magic, garbage after
		FFileHelper::SaveArrayToFile(Garbage, *Rig->Paths(Id).Main);

		const TArray<FCampaignSlotInfo> Slots = Rig->Save->ListCampaigns();
		TestTrue(TEXT("listed as unreadable"), Slots.Num() == 1 && !Slots[0].bReadable);
		Rig->Save->LoadCampaign(Id);
		Rig->Save->WaitForIdle();
		TestTrue(TEXT("Corrupted"), Rig->Results.Num() == 1 && Rig->Results[0].Error == ESaveError::Corrupted);
		TestNull(TEXT("nothing loaded"), Rig->Save->GetCurrentCampaign());
	});

	It("refuses a file from a future version and leaves it untouched", [this]()
	{
		USG_Campaign* Future = NewObject<USG_Campaign>(GetTransientPackage());
		Future->SchemaVersion = USG_Campaign::CurrentSchemaVersion + 7;
		Future->CampaignId = FGuid::NewGuid();
		Future->DisplayName = TEXT("Futur");
		const TArray<uint8> File = USaveSubsystem::EncodeCampaignFile(Future);
		const FString MainPath = Rig->Paths(Future->CampaignId).Main;
		FFileHelper::SaveArrayToFile(File, *MainPath);

		Rig->Save->LoadCampaign(Future->CampaignId);
		Rig->Save->WaitForIdle();
		TestTrue(TEXT("FutureVersion"), Rig->Results.Num() == 1 && Rig->Results[0].Error == ESaveError::FutureVersion);
		TestNull(TEXT("nothing loaded"), Rig->Save->GetCurrentCampaign());
		TArray<uint8> After;
		FFileHelper::LoadFileToArray(After, *MainPath);
		TestTrue(TEXT("file untouched"), After == File);
	});

	It("reports CampaignNotFound for an unknown id", [this]()
	{
		Rig->Save->LoadCampaign(FGuid::NewGuid());
		Rig->Save->WaitForIdle();
		TestTrue(TEXT("CampaignNotFound"), Rig->Results.Num() == 1 && Rig->Results[0].Error == ESaveError::CampaignNotFound);
	});

	It("keeps the last valid copy and rolls back when the write is impossible", [this]()
	{
		const FGuid Id = Rig->CreateCampaign();
		Rig->GiveEquipment({ { Musket, 1 } });
		const MonkeSave::FCampaignPaths Paths = Rig->Paths(Id);
		// A folder in place of the temporary file makes the write fail
		IFileManager::Get().MakeDirectory(*Paths.Temp, true);
		AddExpectedMessagePlain(TEXT("not written"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);

		Rig->Save->GetCurrentCampaign()->Gold = 999;
		const FSaveOperationResult Save = Rig->Save->SaveCurrentCampaign();
		const FSaveOperationResult Begin = Rig->Save->BeginExpedition(FGuid::NewGuid(), { { Musket, 1 } });
		Rig->Save->WaitForIdle();

		const FSaveOperationResult* SaveDone = Rig->FindResult(Save.RequestId);
		const FSaveOperationResult* BeginDone = Rig->FindResult(Begin.RequestId);
		TestTrue(TEXT("save failed with WriteFailed"), SaveDone && SaveDone->Error == ESaveError::WriteFailed && !SaveDone->PlayerMessage.IsEmpty());
		TestTrue(TEXT("begin failed"), BeginDone && BeginDone->Error == ESaveError::WriteFailed);
		TestEqual(TEXT("indicator failed"), Rig->Save->GetIndicatorState(), ESaveIndicatorState::Failed);
		USG_Campaign* Campaign = Rig->Save->GetCurrentCampaign();
		TestTrue(TEXT("departure rolled back: no marker"), Campaign && !Campaign->HasExpeditionInProgress());
		TestTrue(TEXT("departure rolled back: equipment back"), Campaign && Campaign->HubEquipment.FindRef(Musket) == 1);

		IFileManager::Get().DeleteDirectory(*Paths.Temp, false, true);
		TArray<FSaveOperationResult> Results;
		USG_Campaign* Loaded = Rig->Restart(Id, Results)->GetCurrentCampaign();
		TestTrue(TEXT("last valid copy on disk"), Loaded && Loaded->Gold == 0 && !Loaded->HasExpeditionInProgress());
	});

	It("resumes at the hub after a crash mid-expedition: abandoned, equipment lost, nothing credited (UI10)", [this]()
	{
		const FGuid Id = Rig->CreateCampaign();
		Rig->GiveEquipment({ { Musket, 2 }, { Rope, 1 } });
		const FGuid Expedition = FGuid::NewGuid();
		Rig->Save->BeginExpedition(Expedition, { { Musket, 1 }, { Rope, 1 } });
		Rig->Save->WaitForIdle();
		// "Crash": the game stops here, ApplyExpeditionResult never runs

		{
			const TStrongObjectPtr<USaveSubsystem> Menu = Rig->MakeSubsystem();
			const TArray<FCampaignSlotInfo> Slots = Menu->ListCampaigns();
			TestTrue(TEXT("slot shows the interrupted expedition"), Slots.Num() == 1 && Slots[0].bHasExpeditionInProgress);
		}
		TArray<FSaveOperationResult> Results;
		USaveSubsystem* Fresh = Rig->Restart(Id, Results);
		USG_Campaign* Campaign = Fresh->GetCurrentCampaign();
		if (!TestNotNull(TEXT("loaded"), Campaign))
		{
			return;
		}
		const FSaveOperationResult* Load = Results.FindByPredicate([](const FSaveOperationResult& R) { return R.Operation == ESaveOperation::Load; });
		const FSaveOperationResult* Recover = Results.FindByPredicate([](const FSaveOperationResult& R) { return R.Operation == ESaveOperation::RecoverAbandonedExpedition; });
		TestTrue(TEXT("load reports the abandon with a summary"), Load && Load->IsSuccess() && Load->bRecoveredAbandonedExpedition && !Load->PlayerMessage.IsEmpty());
		TestTrue(TEXT("summary lists the lost equipment"), Load && Load->LedgerEntry.bAbandoned && Load->LedgerEntry.LostEquipment.FindRef(Musket) == 1 && Load->LedgerEntry.LostEquipment.FindRef(Rope) == 1);
		TestTrue(TEXT("recovery written"), Recover && Recover->IsSuccess());
		TestFalse(TEXT("marker cleared"), Campaign->HasExpeditionInProgress());
		TestEqual(TEXT("no gold credited"), Campaign->Gold, static_cast<int64>(0));
		TestEqual(TEXT("committed musket lost"), Campaign->HubEquipment.FindRef(Musket), 1);
		TestFalse(TEXT("committed rope lost"), Campaign->HubEquipment.Contains(Rope));

		FExpeditionResultToApply Late;
		Late.GoldDelta = 500;
		Late.ReturnedEquipment.Add(Musket, 1);
		const FSaveOperationResult Replay = Fresh->ApplyExpeditionResult(Expedition, Late);
		TestEqual(TEXT("late result refused"), Replay.Error, ESaveError::AlreadyApplied);
		TestEqual(TEXT("still no gold"), Campaign->Gold, static_cast<int64>(0));

		TArray<FSaveOperationResult> SecondResults;
		USaveSubsystem* Second = Rig->Restart(Id, SecondResults);
		TestTrue(TEXT("second restart: nothing to recover, nothing doubled"), SecondResults.Num() == 1 && !SecondResults[0].bRecoveredAbandonedExpedition
			&& Second->GetCurrentCampaign() && Second->GetCurrentCampaign()->ExpeditionLedger.Num() == 1);
	});

	It("refuses double resolution and mismatched or impossible results", [this]()
	{
		Rig->CreateCampaign();
		Rig->GiveEquipment({ { Musket, 1 } });
		const FSaveOperationResult NoMarker = Rig->Save->ApplyExpeditionResult(FGuid::NewGuid(), FExpeditionResultToApply());
		TestEqual(TEXT("no marker: mismatch"), NoMarker.Error, ESaveError::ExpeditionMismatch);

		const FSaveOperationResult NotOwned = Rig->Save->BeginExpedition(FGuid::NewGuid(), { { Rope, 1 } });
		TestEqual(TEXT("equipment not owned"), NotOwned.Error, ESaveError::InsufficientEquipment);

		const FGuid Expedition = FGuid::NewGuid();
		Rig->Save->BeginExpedition(Expedition, { { Musket, 1 } });
		const FSaveOperationResult Twice = Rig->Save->BeginExpedition(FGuid::NewGuid(), TMap<FName, int32>());
		TestEqual(TEXT("one expedition at a time"), Twice.Error, ESaveError::ExpeditionAlreadyInProgress);
		Rig->Save->WaitForIdle();

		FExpeditionResultToApply TooMuch;
		TooMuch.ReturnedEquipment.Add(Musket, 2);
		TestEqual(TEXT("more returned than committed"), Rig->Save->ApplyExpeditionResult(Expedition, TooMuch).Error, ESaveError::InvalidRequest);
		FExpeditionResultToApply Negative;
		Negative.GoldDelta = -1;
		TestEqual(TEXT("negative balance"), Rig->Save->ApplyExpeditionResult(Expedition, Negative).Error, ESaveError::InvalidRequest);

		FExpeditionResultToApply Good;
		Good.GoldDelta = 10;
		const FSaveOperationResult First = Rig->Save->ApplyExpeditionResult(Expedition, Good);
		const FSaveOperationResult Second = Rig->Save->ApplyExpeditionResult(Expedition, Good);
		TestTrue(TEXT("first queued"), First.bQueued && First.IsSuccess());
		TestTrue(TEXT("second answered with the existing entry"), Second.Error == ESaveError::AlreadyApplied && Second.LedgerEntry.GoldDelta == 10);
		Rig->Save->WaitForIdle();
		TestEqual(TEXT("credited once"), Rig->Save->GetCurrentCampaign()->Gold, static_cast<int64>(10));
	});

	It("reports a timeout, unlocks the caller and still serialises the late write", [this]()
	{
		const FGuid Id = Rig->CreateCampaign();
		Rig->Save->SetOperationTimeoutForTests(0.05f);
		Rig->Save->SetWorkerDelayForTests(0.4f);
		Rig->Save->GetCurrentCampaign()->Gold = 77;
		const int32 Before = Rig->Results.Num();
		const FSaveOperationResult Save = Rig->Save->SaveCurrentCampaign();
		FPlatformProcess::Sleep(0.15f);
		Rig->Save->PumpQueue();
		TestTrue(TEXT("Timeout reported once"), Rig->Results.Num() == Before + 1 && Rig->Results.Last().RequestId == Save.RequestId && Rig->Results.Last().Error == ESaveError::Timeout);
		TestEqual(TEXT("indicator failed"), Rig->Save->GetIndicatorState(), ESaveIndicatorState::Failed);
		TestTrue(TEXT("still pending"), Rig->Save->HasPendingOperations());

		Rig->Save->WaitForIdle();
		TestEqual(TEXT("no second result"), Rig->Results.Num(), Before + 1);
		Rig->Save->SetWorkerDelayForTests(0.0f);
		TArray<FSaveOperationResult> Results;
		USG_Campaign* Loaded = Rig->Restart(Id, Results)->GetCurrentCampaign();
		TestTrue(TEXT("late write completed on disk"), Loaded && Loaded->Gold == 77);
	});

	It("rejects truncated and tampered envelopes in the decoder", [this]()
	{
		USG_Campaign* Campaign = NewObject<USG_Campaign>(GetTransientPackage());
		Campaign->CampaignId = FGuid::NewGuid();
		Campaign->DisplayName = TEXT("Décodage");
		const TArray<uint8> File = USaveSubsystem::EncodeCampaignFile(Campaign);
		const int32 Current = USG_Campaign::CurrentSchemaVersion;
		TestTrue(TEXT("valid file decodes"), MonkeSave::Decode(File, Current, 1 << 22).HasValue());

		for (int32 Size : { 0, 3, 10, File.Num() / 2, File.Num() - 1 })
		{
			TArray<uint8> Cut(File.GetData(), Size);
			TValueOrError<MonkeSave::FDecodedFile, ESaveError> Decoded = MonkeSave::Decode(Cut, Current, 1 << 22);
			TestTrue(FString::Printf(TEXT("truncated to %d refused"), Size), Decoded.HasError() && Decoded.GetError() == ESaveError::Corrupted);
		}
		TArray<uint8> Flipped = File;
		Flipped.Last() ^= 0xFF;
		TestTrue(TEXT("payload bit flip refused"), MonkeSave::Decode(Flipped, Current, 1 << 22).HasError());
		TArray<uint8> Longer = File;
		Longer.Add(0);
		TestTrue(TEXT("trailing byte refused"), MonkeSave::Decode(Longer, Current, 1 << 22).HasError());
		TestTrue(TEXT("over the size limit refused"), MonkeSave::Decode(File, Current, 16).HasError());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
