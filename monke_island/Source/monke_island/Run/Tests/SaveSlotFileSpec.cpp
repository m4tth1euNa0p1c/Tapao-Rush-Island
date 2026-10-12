// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Run/RunSaveGame.h"
#include "Run/SaveSlotFile.h"
#include "Serialization/MemoryWriter.h"
#include "UI/Tapao/TapaoSaveGame.h"
#include "UObject/ObjectVersion.h"
#include "UObject/Package.h"
#include <limits>

// T-302 (M6 of docs/audit/revue-securite-T269.md): a truncated, random, foreign or newer save never crashes nor is
// overwritten; writes are transactions with a last valid copy. Every file lives in Saved/Automation/Transient: the tests
// never touch Saved/SaveGames.
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Save
BEGIN_DEFINE_SPEC(FSaveSlotFileSpec, "monke_island.Save",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** Transient folder of the disk tests */
	FString Folder;

	/** Files of the "Run" slot in Folder */
	MonkeSaveSlot::FSlotPaths Paths;

	/** Fixed time for the names of the files put aside */
	FDateTime Now = FDateTime(2026, 10, 11, 15, 30, 0);

	/** A run with values worth checking */
	static URunSaveGame* MakeRun(int32 Treasury)
	{
		URunSaveGame* Run = NewObject<URunSaveGame>(GetTransientPackage());
		Run->State.bActive = true;
		Run->State.ZoneIndex = 1;
		Run->State.DayInContract = 2;
		Run->State.DaysPlayed = 4;
		Run->State.Quota = 10500;
		Run->State.Treasury = Treasury;
		FRunLoadoutItem& Item = Run->Loadouts.Add(TEXT("Player-1")).Items.AddDefaulted_GetRef();
		Item.Slot = 0;
		Item.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Weapons/Musket/DA_Item_Musket.DA_Item_Musket")));
		Item.Count = 1;
		Item.LoadedRounds = 1;
		Item.ReserveRounds = 12;
		return Run;
	}

	/** A whole file of MakeRun */
	static TArray<uint8> EncodeRun(int32 Treasury, int32 Schema = URunSaveGame::CurrentVersion)
	{
		TValueOrError<TArray<uint8>, ESaveSlotError> Bytes = MonkeSaveSlot::Encode(*MakeRun(Treasury), Schema);
		return Bytes.HasValue() ? Bytes.StealValue() : TArray<uint8>();
	}

	/** A bare engine save of MakeRun, as the game wrote before the envelope */
	static TArray<uint8> LegacyRun(int32 Treasury)
	{
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(MakeRun(Treasury), Bytes);
		return Bytes;
	}

	/** Decodes as a run: the error, None when it was read */
	static ESaveSlotError DecodeError(const TArray<uint8>& Bytes)
	{
		bool bLegacy = false;
		const TValueOrError<USaveGame*, ESaveSlotError> Decoded = MonkeSaveSlot::Decode(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, bLegacy);
		return Decoded.HasError() ? Decoded.GetError() : ESaveSlotError::None;
	}

	/** Treasury of a decoded run, -1 when it cannot be read */
	static int32 TreasuryOf(const TArray<uint8>& Bytes)
	{
		bool bLegacy = false;
		const TValueOrError<USaveGame*, ESaveSlotError> Decoded = MonkeSaveSlot::Decode(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, bLegacy);
		const URunSaveGame* Run = Decoded.HasValue() ? Cast<URunSaveGame>(Decoded.GetValue()) : nullptr;
		return Run ? Run->State.Treasury : -1;
	}

	/** The check URunSubsystem runs on a loaded run */
	static bool AcceptRun(USaveGame& Save)
	{
		int32 Fixes = 0;
		return CastChecked<URunSaveGame>(&Save)->SanitizeLoaded(Fixes);
	}

	/** Reads the "Run" slot of Folder */
	MonkeSaveSlot::FReadResult ReadRun() const
	{
		return MonkeSaveSlot::Read(Paths, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, &FSaveSlotFileSpec::AcceptRun, Now);
	}

	/** Bytes of a file, empty when there is none */
	static TArray<uint8> Load(const FString& Path)
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
		return Bytes;
	}

	/** Files of Folder */
	TArray<FString> FilesInFolder() const
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*")), /*Files*/ true, /*Directories*/ false);
		return Files;
	}

	/** An engine header as FSaveGameHeader::Write lays it out, up to the custom versions (count and format given) */
	static void WriteEngineHeaderStart(FArchive& Ar, int32 CustomFormat)
	{
		int32 Tag = 0x53415647;
		int32 FileVersion = 3;
		FPackageFileVersion Package = GPackageFileUEVersion;
		FEngineVersion Engine = FEngineVersion::Current();
		Ar << Tag << FileVersion << Package;
		Ar << Engine;
		Ar << CustomFormat;
	}

END_DEFINE_SPEC(FSaveSlotFileSpec)

void FSaveSlotFileSpec::Define()
{
	Describe("Malformed", [this]()
	{
		It("reads back what it wrote", [this]()
		{
			const TArray<uint8> Bytes = EncodeRun(1234);
			bool bLegacy = true;
			const TValueOrError<USaveGame*, ESaveSlotError> Decoded = MonkeSaveSlot::Decode(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, bLegacy);
			const URunSaveGame* Run = Decoded.HasValue() ? Cast<URunSaveGame>(Decoded.GetValue()) : nullptr;
			if (!TestNotNull(TEXT("the run is read"), Run))
			{
				return;
			}
			TestFalse(TEXT("current format"), bLegacy);
			TestEqual(TEXT("treasury"), Run->State.Treasury, 1234);
			TestEqual(TEXT("days played"), Run->State.DaysPlayed, 4);
			const FRunLoadout* Loadout = Run->Loadouts.Find(TEXT("Player-1"));
			TestTrue(TEXT("the musket and its rounds come back"), Loadout && Loadout->Items.Num() == 1 && Loadout->Items[0].ReserveRounds == 12);
			TestEqual(TEXT("checked whole"), MonkeSaveSlot::Check(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion), ESaveSlotError::None);
		});

		It("reads a bare engine save of the older format", [this]()
		{
			const TArray<uint8> Bytes = LegacyRun(77);
			bool bLegacy = false;
			const TValueOrError<USaveGame*, ESaveSlotError> Decoded = MonkeSaveSlot::Decode(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, bLegacy);
			const URunSaveGame* Run = Decoded.HasValue() ? Cast<URunSaveGame>(Decoded.GetValue()) : nullptr;
			if (!TestNotNull(TEXT("the older save is read"), Run))
			{
				return;
			}
			TestTrue(TEXT("told as legacy"), bLegacy);
			TestEqual(TEXT("treasury"), Run->State.Treasury, 77);
		});

		It("refuses an empty file and a file over 1 MB", [this]()
		{
			TestEqual(TEXT("empty"), DecodeError(TArray<uint8>()), ESaveSlotError::Corrupted);
			TArray<uint8> Huge;
			Huge.SetNumZeroed(static_cast<int32>(MonkeSaveSlot::MaxFileBytes) + 1);
			TestEqual(TEXT("too large"), DecodeError(Huge), ESaveSlotError::Corrupted);
		});

		It("refuses a file truncated at any length, without crashing", [this]()
		{
			// a property of the older format cut short: the engine logs it, the strict reader stops it (no "Size mismatch" checkf)
			AddExpectedError(TEXT("Failed loading tagged"), EAutomationExpectedErrorFlags::Contains, -1);
			// a string length cut short reads as garbage: the engine logs it and flags the archive, the reader refuses the file
			AddExpectedError(TEXT("String is too large"), EAutomationExpectedErrorFlags::Contains, -1);
			const TArray<uint8> Bytes = EncodeRun(500);
			const TArray<uint8> Legacy = LegacyRun(500);
			int32 Accepted = 0;
			for (int32 Length = 0; Length < Bytes.Num(); Length += FMath::Max(1, Bytes.Num() / 150))
			{
				Accepted += DecodeError(TArray<uint8>(Bytes.GetData(), Length)) == ESaveSlotError::None ? 1 : 0;
			}
			// the older format has no checksum: the header checks and the strict reader refuse it
			for (int32 Length = 0; Length < Legacy.Num(); Length += FMath::Max(1, Legacy.Num() / 150))
			{
				Accepted += DecodeError(TArray<uint8>(Legacy.GetData(), Length)) == ESaveSlotError::None ? 1 : 0;
			}
			Accepted += DecodeError(TArray<uint8>(Legacy.GetData(), Legacy.Num() - 1)) == ESaveSlotError::None ? 1 : 0;
			TestEqual(TEXT("no truncated file is accepted"), Accepted, 0);
		});

		It("refuses random bytes, even behind the tag of an engine save", [this]()
		{
			FRandomStream Random(20261011);
			int32 Accepted = 0;
			for (int32 Try = 0; Try < 300; ++Try)
			{
				TArray<uint8> Bytes;
				Bytes.SetNumUninitialized(Random.RandRange(1, 4096));
				for (uint8& Byte : Bytes)
				{
					Byte = static_cast<uint8>(Random.RandRange(0, 255));
				}
				if (Try % 2 == 0 && Bytes.Num() >= 8)
				{
					const int32 Tag = 0x53415647;
					const int32 FileVersion = 3;
					FMemory::Memcpy(Bytes.GetData(), &Tag, sizeof(Tag));
					FMemory::Memcpy(Bytes.GetData() + 4, &FileVersion, sizeof(FileVersion));
				}
				Accepted += DecodeError(Bytes) == ESaveSlotError::None ? 1 : 0;
			}
			TestEqual(TEXT("no random file is accepted"), Accepted, 0);
		});

		It("refuses a file whose checksum does not match", [this]()
		{
			TArray<uint8> Bytes = EncodeRun(800);
			Bytes[Bytes.Num() / 2] ^= 0x5A;
			TestEqual(TEXT("one byte changed"), DecodeError(Bytes), ESaveSlotError::Corrupted);
		});

		It("refuses a hostile engine header before the engine reads it (counts, format, lengths)", [this]()
		{
			{
				TArray<uint8> Bytes;
				FMemoryWriter Ar(Bytes);
				WriteEngineHeaderStart(Ar, static_cast<int32>(ECustomVersionSerializationFormat::Optimized));
				int32 Count = MAX_int32;
				Ar << Count;
				TestEqual(TEXT("custom versions without end"), DecodeError(Bytes), ESaveSlotError::Corrupted);
			}
			{
				TArray<uint8> Bytes;
				FMemoryWriter Ar(Bytes);
				// the engine meets an unknown format with check(false)
				WriteEngineHeaderStart(Ar, static_cast<int32>(ECustomVersionSerializationFormat::Unknown));
				int32 Count = 0;
				Ar << Count;
				TestEqual(TEXT("unknown custom version format"), DecodeError(Bytes), ESaveSlotError::Corrupted);
			}
			{
				TArray<uint8> Bytes;
				FMemoryWriter Ar(Bytes);
				WriteEngineHeaderStart(Ar, static_cast<int32>(ECustomVersionSerializationFormat::Optimized));
				int32 Count = 0;
				int32 ClassNameLength = 0x40000000;
				Ar << Count << ClassNameLength;
				TestEqual(TEXT("class name of a billion characters"), DecodeError(Bytes), ESaveSlotError::Corrupted);
			}
			{
				// the class named in the file is never loaded: a map, here
				TArray<uint8> Bytes;
				FMemoryWriter Ar(Bytes);
				WriteEngineHeaderStart(Ar, static_cast<int32>(ECustomVersionSerializationFormat::Optimized));
				int32 Count = 0;
				FString ClassName = TEXT("/Script/Engine.World");
				Ar << Count << ClassName;
				TestEqual(TEXT("foreign class"), DecodeError(Bytes), ESaveSlotError::WrongClass);
			}
		});

		It("refuses a save of another kind", [this]()
		{
			UTapaoSaveGame* Campaign = NewObject<UTapaoSaveGame>(GetTransientPackage());
			const TValueOrError<TArray<uint8>, ESaveSlotError> Bytes = MonkeSaveSlot::Encode(*Campaign, UTapaoSaveGame::CurrentVersion);
			TestTrue(TEXT("campaign encoded"), Bytes.HasValue());
			TestEqual(TEXT("a campaign is not a run"), Bytes.HasValue() ? DecodeError(Bytes.GetValue()) : ESaveSlotError::None, ESaveSlotError::WrongClass);
		});

		It("refuses a newer format without reading it", [this]()
		{
			TestEqual(TEXT("newer schema"), DecodeError(EncodeRun(10, URunSaveGame::CurrentVersion + 1)), ESaveSlotError::FutureVersion);
			TArray<uint8> Bytes = EncodeRun(10);
			// envelope version: bytes 8 and 9
			Bytes[8] = 0x7F;
			TestEqual(TEXT("newer envelope"), DecodeError(Bytes), ESaveSlotError::FutureVersion);
		});
	});

	Describe("Atomic", [this]()
	{
		BeforeEach([this]()
		{
			Folder = FPaths::AutomationTransientDir() / TEXT("SaveSlotSpec");
			IFileManager::Get().DeleteDirectory(*Folder, /*RequireExists*/ false, /*Tree*/ true);
			IFileManager::Get().MakeDirectory(*Folder, /*Tree*/ true);
			const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Made = MonkeSaveSlot::MakePaths(Folder, TEXT("Run"));
			TestTrue(TEXT("a transient folder under Saved is allowed"), Made.HasValue());
			if (Made.HasValue())
			{
				Paths = Made.GetValue();
			}
		});

		AfterEach([this]()
		{
			IFileManager::Get().DeleteDirectory(*Folder, /*RequireExists*/ false, /*Tree*/ true);
		});

		It("writes, then keeps the previous file as the last valid copy", [this]()
		{
			TestEqual(TEXT("first write"), MonkeSaveSlot::Write(Paths, *MakeRun(100), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			TestEqual(TEXT("second write"), MonkeSaveSlot::Write(Paths, *MakeRun(200), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			const MonkeSaveSlot::FReadResult Result = ReadRun();
			const URunSaveGame* Run = Cast<URunSaveGame>(Result.Object);
			TestTrue(TEXT("the slot file is read"), Run && Result.Source == MonkeSaveSlot::ESource::Main && Run->State.Treasury == 200);
			TestEqual(TEXT("the last valid copy holds the previous run"), TreasuryOf(Load(Paths.Backup)), 100);
			TestFalse(TEXT("no temporary file left"), IFileManager::Get().FileExists(*Paths.Temp));
			TestTrue(TEXT("nothing to tell the player"), MonkeSaveSlot::DescribeForPlayer(Result, FText::FromString(TEXT("S")), FText::FromString(TEXT("F"))).IsEmpty());
		});

		It("puts a damaged slot file aside, untouched, and takes the last valid copy", [this]()
		{
			TestEqual(TEXT("first write"), MonkeSaveSlot::Write(Paths, *MakeRun(100), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			TestEqual(TEXT("second write"), MonkeSaveSlot::Write(Paths, *MakeRun(200), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			const TArray<uint8> Whole = Load(Paths.Main);
			const TArray<uint8> Damaged(Whole.GetData(), Whole.Num() / 2);
			TestTrue(TEXT("slot file cut in half"), FFileHelper::SaveArrayToFile(Damaged, *Paths.Main));

			const MonkeSaveSlot::FReadResult Result = ReadRun();
			const URunSaveGame* Run = Cast<URunSaveGame>(Result.Object);
			TestTrue(TEXT("the last valid copy is taken"), Run && Result.Source == MonkeSaveSlot::ESource::Backup && Run->State.Treasury == 100);
			TestEqual(TEXT("the slot file was damaged"), Result.MainError, ESaveSlotError::Corrupted);
			TestTrue(TEXT("its name is given"), Result.AsideFileName.EndsWith(TEXT(".corrupt")));
			TestTrue(TEXT("kept aside as it was"), Load(Folder / Result.AsideFileName) == Damaged);
			TestEqual(TEXT("the slot file is the last valid copy again"), TreasuryOf(Load(Paths.Main)), 100);
			TestTrue(TEXT("still writable"), Result.bWritable);
			TestFalse(TEXT("the player is told"), MonkeSaveSlot::DescribeForPlayer(Result, FText::FromString(TEXT("S")), FText::FromString(TEXT("F"))).IsEmpty());
		});

		It("starts fresh when nothing valid is left, and never writes over the damaged file", [this]()
		{
			TArray<uint8> Garbage;
			Garbage.Init(0xCD, 300);
			TestTrue(TEXT("garbage slot file"), FFileHelper::SaveArrayToFile(Garbage, *Paths.Main));
			const MonkeSaveSlot::FReadResult Result = ReadRun();
			TestNull(TEXT("nothing read"), Result.Object);
			TestEqual(TEXT("fresh start"), Result.Source, MonkeSaveSlot::ESource::None);
			TestFalse(TEXT("the slot file is gone"), IFileManager::Get().FileExists(*Paths.Main));
			TestTrue(TEXT("the damaged file is kept aside"), Load(Folder / Result.AsideFileName) == Garbage);

			TestEqual(TEXT("a new run is written"), MonkeSaveSlot::Write(Paths, *MakeRun(300), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			TestEqual(TEXT("new run"), TreasuryOf(Load(Paths.Main)), 300);
			TestTrue(TEXT("the damaged file is still there"), Load(Folder / Result.AsideFileName) == Garbage);
		});

		It("puts a damaged slot file aside when writing, instead of making it the last valid copy", [this]()
		{
			TArray<uint8> Garbage;
			Garbage.Init(0x11, 64);
			TestTrue(TEXT("garbage slot file"), FFileHelper::SaveArrayToFile(Garbage, *Paths.Main));
			TestEqual(TEXT("write"), MonkeSaveSlot::Write(Paths, *MakeRun(600), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			TestEqual(TEXT("new run"), TreasuryOf(Load(Paths.Main)), 600);
			TestFalse(TEXT("no garbage as the last valid copy"), IFileManager::Get().FileExists(*Paths.Backup));
			const TArray<FString> Files = FilesInFolder();
			TestTrue(TEXT("the garbage is kept aside"), Files.ContainsByPredicate([](const FString& File) { return File.EndsWith(TEXT(".corrupt")); }));
		});

		It("keeps a save of a newer version apart, unchanged", [this]()
		{
			const TArray<uint8> Newer = EncodeRun(500, URunSaveGame::CurrentVersion + 1);
			TestTrue(TEXT("newer slot file"), FFileHelper::SaveArrayToFile(Newer, *Paths.Main));
			const MonkeSaveSlot::FReadResult Result = ReadRun();
			TestNull(TEXT("not read"), Result.Object);
			TestEqual(TEXT("refused as newer"), Result.MainError, ESaveSlotError::FutureVersion);
			TestTrue(TEXT("kept apart"), Result.AsideFileName.EndsWith(TEXT(".future")));
			TestTrue(TEXT("unchanged"), Load(Folder / Result.AsideFileName) == Newer);
		});

		It("takes the checked temporary file of a write cut short", [this]()
		{
			TestTrue(TEXT("temporary file only"), FFileHelper::SaveArrayToFile(EncodeRun(400), *Paths.Temp));
			const MonkeSaveSlot::FReadResult Result = ReadRun();
			const URunSaveGame* Run = Cast<URunSaveGame>(Result.Object);
			TestTrue(TEXT("taken"), Run && Result.Source == MonkeSaveSlot::ESource::Temp && Run->State.Treasury == 400);
			TestEqual(TEXT("it is the slot file now"), TreasuryOf(Load(Paths.Main)), 400);
		});

		It("converts a save of the older format and keeps it as the last valid copy", [this]()
		{
			const TArray<uint8> Legacy = LegacyRun(42);
			TestTrue(TEXT("older slot file"), FFileHelper::SaveArrayToFile(Legacy, *Paths.Main));
			const MonkeSaveSlot::FReadResult Result = ReadRun();
			const URunSaveGame* Run = Cast<URunSaveGame>(Result.Object);
			TestTrue(TEXT("read as legacy"), Run && Result.bLegacy && Run->State.Treasury == 42);
			TestEqual(TEXT("written in the new format"), MonkeSaveSlot::Write(Paths, *MakeRun(43), URunSaveGame::CurrentVersion, Now), ESaveSlotError::None);
			bool bLegacy = true;
			const TValueOrError<USaveGame*, ESaveSlotError> Decoded = MonkeSaveSlot::Decode(Load(Paths.Main), URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, bLegacy);
			TestTrue(TEXT("new format"), Decoded.HasValue() && !bLegacy);
			TestTrue(TEXT("the older file is the last valid copy"), Load(Paths.Backup) == Legacy);
		});
	});

	Describe("Bounds", [this]()
	{
		It("clamps the numbers of a run and drops an item that is not an item of the game", [this]()
		{
			URunSaveGame* Run = MakeRun(-50);
			Run->State.DaysPlayed = -3;
			Run->State.Outcome = static_cast<ERunOutcome>(200);
			Run->LastDayReport.Verdict = static_cast<ERunDayVerdict>(99);
			FRunLoadoutItem& Foreign = Run->Loadouts.FindChecked(TEXT("Player-1")).Items.AddDefaulted_GetRef();
			Foreign.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Maps/Lvl_Raraka.Lvl_Raraka")));
			Foreign.Slot = 3;
			int32 Fixes = 0;
			TestTrue(TEXT("kept"), Run->SanitizeLoaded(Fixes));
			TestEqual(TEXT("treasury"), Run->State.Treasury, 0);
			TestEqual(TEXT("days played"), Run->State.DaysPlayed, 0);
			TestEqual(TEXT("outcome"), Run->State.Outcome, ERunOutcome::InProgress);
			TestEqual(TEXT("verdict"), Run->LastDayReport.Verdict, ERunDayVerdict::Invalid);
			TestEqual(TEXT("only the musket stays"), Run->Loadouts.FindChecked(TEXT("Player-1")).Items.Num(), 1);
			TestTrue(TEXT("changes counted"), Fixes >= 5);
		});

		It("refuses a run of a newer format, or with more loadouts than a game holds", [this]()
		{
			int32 Fixes = 0;
			URunSaveGame* Newer = MakeRun(10);
			Newer->Version = URunSaveGame::CurrentVersion + 1;
			TestFalse(TEXT("newer"), Newer->SanitizeLoaded(Fixes));
			URunSaveGame* Crowded = MakeRun(10);
			for (int32 Index = 0; Index <= URunSaveGame::MaxLoadouts; ++Index)
			{
				Crowded->Loadouts.Add(FString::Printf(TEXT("P%d"), Index));
			}
			TestFalse(TEXT("too many loadouts"), Crowded->SanitizeLoaded(Fixes));
		});

		It("only lets an item definition of the game come back from a file", [this]()
		{
			TestTrue(TEXT("musket"), URunSaveGame::IsAllowedItemDefinition(FSoftObjectPath(TEXT("/Game/MonkeIsland/Weapons/Musket/DA_Item_Musket.DA_Item_Musket"))));
			TestFalse(TEXT("a map"), URunSaveGame::IsAllowedItemDefinition(FSoftObjectPath(TEXT("/Game/MonkeIsland/Maps/Lvl_Raraka.Lvl_Raraka"))));
			TestFalse(TEXT("engine content"), URunSaveGame::IsAllowedItemDefinition(FSoftObjectPath(TEXT("/Engine/EngineMaterials/DA_Item_X.DA_Item_X"))));
			TestFalse(TEXT("a sub-object"), URunSaveGame::IsAllowedItemDefinition(FSoftObjectPath(TEXT("/Game/MonkeIsland/Weapons/Musket/DA_Item_Musket.DA_Item_Musket:Inner"))));
			TestFalse(TEXT("nothing"), URunSaveGame::IsAllowedItemDefinition(FSoftObjectPath()));
		});

		It("clamps the counters of the campaign save", [this]()
		{
			UTapaoSaveGame* Campaign = NewObject<UTapaoSaveGame>(GetTransientPackage());
			Campaign->Expeditions = -1;
			Campaign->PlaySeconds = std::numeric_limits<float>::quiet_NaN();
			Campaign->LastIsland = FString::ChrN(400, TEXT('x'));
			int32 Fixes = 0;
			TestTrue(TEXT("kept"), Campaign->SanitizeLoaded(Fixes));
			TestEqual(TEXT("expeditions"), Campaign->Expeditions, 0);
			TestEqual(TEXT("time played"), Campaign->PlaySeconds, 0.0f);
			TestTrue(TEXT("island name cleared"), Campaign->LastIsland.IsEmpty());
			Campaign->Version = UTapaoSaveGame::CurrentVersion + 1;
			TestFalse(TEXT("newer format refused"), Campaign->SanitizeLoaded(Fixes));
		});
	});

	Describe("Paths", [this]()
	{
		It("names a slot by an identifier only, under the Saved folder (SEC-04)", [this]()
		{
			TestTrue(TEXT("Run"), MonkeSaveSlot::IsValidSlotName(TEXT("Run")));
			TestTrue(TEXT("TapaoCampaign"), MonkeSaveSlot::IsValidSlotName(TEXT("TapaoCampaign")));
			TestFalse(TEXT("climbing"), MonkeSaveSlot::IsValidSlotName(TEXT("../Run")));
			TestFalse(TEXT("a typed name"), MonkeSaveSlot::IsValidSlotName(TEXT("Ma partie")));
			TestFalse(TEXT("empty"), MonkeSaveSlot::IsValidSlotName(FString()));
			TestFalse(TEXT("accent"), MonkeSaveSlot::IsValidSlotName(TEXT("Partieé")));
			const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Default = MonkeSaveSlot::MakePaths(MonkeSaveSlot::DefaultDirectory(), TEXT("Run"));
			TestTrue(TEXT("the slot folder"), Default.HasValue());
			if (Default.HasValue())
			{
				TestTrue(TEXT("the very file of the engine's slot"),
					FPaths::IsSamePath(Default.GetValue().Main, FPaths::ProjectSavedDir() / TEXT("SaveGames/Run.sav")));
			}
			TestFalse(TEXT("outside Saved"), MonkeSaveSlot::MakePaths(FPaths::EngineDir(), TEXT("Run")).HasValue());
			TestFalse(TEXT("climbing out of Saved"), MonkeSaveSlot::MakePaths(FPaths::ProjectSavedDir() / TEXT("../.."), TEXT("Run")).HasValue());
		});
	});
}

#endif
