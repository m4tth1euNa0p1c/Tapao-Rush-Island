// monke_island

#include "Run/SaveSlotFile.h"
#include "GameFramework/SaveGame.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Memory/MemoryView.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Templates/UniquePtr.h"
#include "UObject/ObjectVersion.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogMonkeSaveSlot);

#define LOCTEXT_NAMESPACE "SaveSlot"

const TCHAR* LexToString(ESaveSlotError Error)
{
	switch (Error)
	{
	case ESaveSlotError::None: return TEXT("none");
	case ESaveSlotError::NotFound: return TEXT("not found");
	case ESaveSlotError::Corrupted: return TEXT("damaged");
	case ESaveSlotError::WrongClass: return TEXT("of another kind");
	case ESaveSlotError::FutureVersion: return TEXT("from a newer version");
	case ESaveSlotError::ReadFailed: return TEXT("unreadable");
	case ESaveSlotError::WriteFailed: return TEXT("not written");
	case ESaveSlotError::PathRejected: return TEXT("path refused");
	}
	return TEXT("unknown");
}

namespace MonkeSaveSlot
{
	namespace
	{
		static_assert(PLATFORM_LITTLE_ENDIAN, "the envelope is read and written little-endian");

		/** 'M' 'K' 'S' 'V' read as a little-endian uint32 */
		constexpr uint32 EnvelopeMagic = 0x56534B4Du;

		/** Layout of the envelope written now */
		constexpr uint16 EnvelopeVersion = 1;

		/** int32 zero, magic, envelope version, flags, schema version, payload bytes, CRC32 */
		constexpr int32 EnvelopeBytes = 4 + 4 + 2 + 2 + 4 + 4 + 4;

		/** "SAVG", the tag of the engine's save header (GameplayStatics.cpp) */
		constexpr int32 EngineSaveTag = 0x53415647;

		/** FSaveGameFileVersion::PackageFileSummaryVersionChange: the header layout UE5 writes (GameplayStatics.cpp) */
		constexpr int32 EngineSaveFileVersion = 3;

		/** Custom version entries in an engine header (the engine registers a few hundred) */
		constexpr int32 MaxCustomVersions = 4096;

		/** Bytes of one custom version entry: FGuid and int32 */
		constexpr int64 CustomVersionBytes = 16 + 4;

		/** Branch name and class path in an engine header */
		constexpr int32 MaxHeaderStringChars = 512;

		/** Scanners (Malwarebytes) hold a file for a moment after it was written: a few short tries */
		constexpr int32 FileTries = 3;
		constexpr float FileRetrySeconds = 0.02f;

		/** Runs Operation until it succeeds, at most FileTries times */
		template <typename FOperation>
		bool Retry(FOperation&& Operation)
		{
			for (int32 Try = 0; Try < FileTries; ++Try)
			{
				if (Try > 0)
				{
					FPlatformProcess::SleepNoStats(FileRetrySeconds);
				}
				if (Operation())
				{
					return true;
				}
			}
			return false;
		}

		/** What the envelope holds */
		struct FEnvelope
		{
			/** The engine save (the whole file for a legacy save) */
			TConstArrayView<uint8> Payload;
			/** Schema version written in the envelope, 0 for a legacy save */
			int32 SchemaVersion = 0;
			/** A bare engine save, written before the envelope */
			bool bLegacy = false;
		};

		/** Size, envelope and checksum; a bare engine save passes as legacy (its header is checked next) */
		ESaveSlotError Unwrap(TConstArrayView<uint8> Bytes, int32 CurrentSchemaVersion, FEnvelope& Out)
		{
			if (Bytes.Num() <= 0 || Bytes.Num() > MaxFileBytes)
			{
				return ESaveSlotError::Corrupted;
			}
			FMemoryReaderView Ar(FMemoryView(Bytes.GetData(), static_cast<uint64>(Bytes.Num())));
			int32 First = 0;
			Ar << First;
			if (Ar.IsError())
			{
				return ESaveSlotError::Corrupted;
			}
			if (First == EngineSaveTag)
			{
				Out.Payload = Bytes;
				Out.SchemaVersion = 0;
				Out.bLegacy = true;
				return ESaveSlotError::None;
			}
			uint32 Magic = 0;
			uint16 Version = 0;
			uint16 Flags = 0;
			Ar << Magic << Version << Flags;
			if (Ar.IsError() || First != 0 || Magic != EnvelopeMagic)
			{
				return ESaveSlotError::Corrupted;
			}
			if (Version > EnvelopeVersion)
			{
				// a newer layout: nothing more of it is read
				return ESaveSlotError::FutureVersion;
			}
			int32 Schema = 0;
			int32 PayloadBytes = 0;
			uint32 Crc = 0;
			Ar << Schema << PayloadBytes << Crc;
			if (Ar.IsError() || Version < 1 || PayloadBytes <= 0 || static_cast<int64>(EnvelopeBytes) + PayloadBytes != Bytes.Num())
			{
				return ESaveSlotError::Corrupted;
			}
			const TConstArrayView<uint8> Payload = Bytes.Slice(EnvelopeBytes, PayloadBytes);
			if (FCrc::MemCrc32(Payload.GetData(), PayloadBytes) != Crc || Schema < 1)
			{
				return ESaveSlotError::Corrupted;
			}
			if (Schema > CurrentSchemaVersion)
			{
				return ESaveSlotError::FutureVersion;
			}
			Out.Payload = Payload;
			Out.SchemaVersion = Schema;
			Out.bLegacy = false;
			return ESaveSlotError::None;
		}

		/** An engine string (int32 count with its terminator: ANSI when positive, UTF-16 when negative), bounds checked first */
		bool ReadBoundedString(FArchive& Ar, int32 MaxChars, FString& Out)
		{
			Out.Reset();
			int32 SaveNum = 0;
			Ar << SaveNum;
			if (Ar.IsError())
			{
				return false;
			}
			if (SaveNum == 0)
			{
				return true;
			}
			if (SaveNum == MIN_int32)
			{
				return false;
			}
			const bool bWide = SaveNum < 0;
			const int32 Chars = bWide ? -SaveNum : SaveNum;
			const int64 Bytes = static_cast<int64>(Chars) * (bWide ? sizeof(UCS2CHAR) : sizeof(ANSICHAR));
			if (Chars > MaxChars || Ar.Tell() + Bytes > Ar.TotalSize())
			{
				return false;
			}
			if (bWide)
			{
				TArray<UCS2CHAR> Buffer;
				Buffer.SetNumUninitialized(Chars);
				Ar.Serialize(Buffer.GetData(), Bytes);
				if (Ar.IsError() || Buffer.Last() != 0)
				{
					return false;
				}
				Out = FString::ConstructFromPtrSize(Buffer.GetData(), Chars - 1);
			}
			else
			{
				TArray<ANSICHAR> Buffer;
				Buffer.SetNumUninitialized(Chars);
				Ar.Serialize(Buffer.GetData(), Bytes);
				if (Ar.IsError() || Buffer.Last() != 0)
				{
					return false;
				}
				Out = FString::ConstructFromPtrSize(Buffer.GetData(), Chars - 1);
			}
			return true;
		}

		/** The engine's save header, read field by field with every bound checked (FSaveGameHeader::Read does not) */
		struct FEngineHeader
		{
			FPackageFileVersion PackageVersion;
			FEngineVersion EngineVersion;
			FCustomVersionContainer CustomVersions;
			FString ClassPath;
			int64 PropertiesOffset = 0;
		};

		/**
		 *  Same layout as FSaveGameHeader::Write: tag, file version, package version, engine version, custom versions
		 *  (optimized format), class path. A bogus custom version format would hit a check(false) in the engine, a bogus
		 *  count or string length a huge allocation: both are refused here, before the engine reads anything.
		 */
		ESaveSlotError ParseEngineHeader(TConstArrayView<uint8> Payload, FEngineHeader& Out)
		{
			FMemoryReaderView Ar(FMemoryView(Payload.GetData(), static_cast<uint64>(Payload.Num())));
			int32 Tag = 0;
			int32 FileVersion = 0;
			Ar << Tag << FileVersion;
			if (Ar.IsError() || Tag != EngineSaveTag)
			{
				return ESaveSlotError::Corrupted;
			}
			if (FileVersion > EngineSaveFileVersion)
			{
				return ESaveSlotError::FutureVersion;
			}
			if (FileVersion != EngineSaveFileVersion)
			{
				// older layouts were never written by this game (UE5)
				return ESaveSlotError::Corrupted;
			}

			Ar << Out.PackageVersion;
			if (Ar.IsError())
			{
				return ESaveSlotError::Corrupted;
			}
			if (Out.PackageVersion.FileVersionUE4 > GPackageFileUEVersion.FileVersionUE4 || Out.PackageVersion.FileVersionUE5 > GPackageFileUEVersion.FileVersionUE5)
			{
				return ESaveSlotError::FutureVersion;
			}
			if (Out.PackageVersion.FileVersionUE4 < static_cast<int32>(VER_UE4_OLDEST_LOADABLE_PACKAGE)
				|| Out.PackageVersion.FileVersionUE5 < static_cast<int32>(EUnrealEngineObjectUE5Version::INITIAL_VERSION))
			{
				return ESaveSlotError::Corrupted;
			}

			uint16 Major = 0;
			uint16 Minor = 0;
			uint16 Patch = 0;
			uint32 Changelist = 0;
			FString Branch;
			Ar << Major << Minor << Patch << Changelist;
			if (Ar.IsError() || !ReadBoundedString(Ar, MaxHeaderStringChars, Branch))
			{
				return ESaveSlotError::Corrupted;
			}
			Out.EngineVersion = FEngineVersion(Major, Minor, Patch, Changelist, Branch);

			int32 Format = 0;
			Ar << Format;
			if (Ar.IsError())
			{
				return ESaveSlotError::Corrupted;
			}
			if (Format > static_cast<int32>(ECustomVersionSerializationFormat::Latest))
			{
				return ESaveSlotError::FutureVersion;
			}
			if (Format != static_cast<int32>(ECustomVersionSerializationFormat::Optimized))
			{
				return ESaveSlotError::Corrupted;
			}
			int32 Count = 0;
			Ar << Count;
			if (Ar.IsError() || Count < 0 || Count > MaxCustomVersions || Ar.Tell() + Count * CustomVersionBytes > Ar.TotalSize())
			{
				return ESaveSlotError::Corrupted;
			}
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FGuid Key;
				int32 Version = 0;
				Ar << Key << Version;
				Out.CustomVersions.SetVersion(Key, Version, NAME_None);
			}
			if (Ar.IsError() || !ReadBoundedString(Ar, MaxHeaderStringChars, Out.ClassPath))
			{
				return ESaveSlotError::Corrupted;
			}
			Out.PropertiesOffset = Ar.Tell();
			return ESaveSlotError::None;
		}

		/**
		 *  Memory reader whose first short read is a critical error, with zeros for the bytes it could not give. The engine's
		 *  tagged property loader stops at a critical error; after a mere error it checks the size of a property cut short
		 *  (checkf "Size mismatch" in UStruct::SerializeVersionedTaggedProperties), which a truncated file would hit.
		 */
		class FStrictMemoryReader final : public FMemoryReaderView
		{
		public:

			explicit FStrictMemoryReader(FMemoryView InBytes)
				: FMemoryReaderView(InBytes, /*bIsPersistent*/ true)
			{
			}

			virtual void Serialize(void* Data, int64 Num) override
			{
				if (Num <= 0)
				{
					return;
				}
				if (IsError() || Tell() < 0 || Tell() + Num > TotalSize())
				{
					FMemory::Memzero(Data, Num);
					SetCriticalError();
					return;
				}
				FMemoryReaderView::Serialize(Data, Num);
			}

			virtual FString GetArchiveName() const override
			{
				return TEXT("MonkeSaveSlot");
			}
		};

		/** A concrete save class, or a programming error */
		bool IsSaveClass(const UClass* Class)
		{
			return Class && Class->IsChildOf(USaveGame::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract);
		}

		/** Reads and decodes a file, then lets the owner check its values */
		ESaveSlotError TryLoad(const FString& Path, UClass* ExpectedClass, int32 CurrentSchemaVersion, FAcceptSave Accept, USaveGame*& OutObject,
			bool& bOutLegacy)
		{
			OutObject = nullptr;
			bOutLegacy = false;
			TValueOrError<TArray<uint8>, ESaveSlotError> Bytes = ReadBoundedFile(Path);
			if (Bytes.HasError())
			{
				return Bytes.GetError();
			}
			TValueOrError<USaveGame*, ESaveSlotError> Decoded = Decode(Bytes.GetValue(), ExpectedClass, CurrentSchemaVersion, bOutLegacy);
			if (Decoded.HasError())
			{
				return Decoded.GetError();
			}
			USaveGame* Object = Decoded.GetValue();
			if (!Accept(*Object))
			{
				return ESaveSlotError::Corrupted;
			}
			OutObject = Object;
			return ESaveSlotError::None;
		}

		/** Renames a file (no other file is replaced unless bReplace) */
		bool MoveFile(const FString& To, const FString& From, bool bReplace)
		{
			return Retry([&To, &From, bReplace]()
			{
				return IFileManager::Get().Move(*To, *From, bReplace, /*EvenIfReadOnly*/ true, /*Attributes*/ false, /*bDoNotRetryOrError*/ true);
			});
		}

		/** Renames a file aside under a name no file has; its bare name, empty when it could not be moved */
		FString PutAside(const FSlotPaths& Paths, const FString& File, ESaveSlotError Why, const FDateTime& Now)
		{
			const FString Aside = Paths.MakeAsidePath(Why == ESaveSlotError::FutureVersion ? TEXT("future") : TEXT("corrupt"), Now);
			if (!MoveFile(Aside, File, /*bReplace*/ false))
			{
				return FString();
			}
			const FString Name = FPaths::GetCleanFilename(Aside);
			UE_LOG(LogMonkeSaveSlot, Warning, TEXT("Save: %s is %s, put aside as %s (kept, never overwritten)"), *FPaths::GetCleanFilename(File),
				LexToString(Why), *Name);
			return Name;
		}

		void DeleteQuietly(const FString& File)
		{
			IFileManager::Get().Delete(*File, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
		}
	}

	FString FSlotPaths::MakeAsidePath(const TCHAR* Reason, const FDateTime& When) const
	{
		const FString Stem = FString::Printf(TEXT("%s.%s"), *Main, *When.ToString(TEXT("%Y%m%d-%H%M%S")));
		FString Path = FString::Printf(TEXT("%s.%s"), *Stem, Reason);
		for (int32 Index = 2; Index < 100 && IFileManager::Get().FileSize(*Path) >= 0; ++Index)
		{
			Path = FString::Printf(TEXT("%s-%d.%s"), *Stem, Index, Reason);
		}
		return Path;
	}

	bool IsValidSlotName(const FString& SlotName)
	{
		if (SlotName.IsEmpty() || SlotName.Len() > 64)
		{
			return false;
		}
		for (const TCHAR Char : SlotName)
		{
			const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('A') && Char <= TEXT('Z'))
				|| (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('_');
			if (!bAllowed)
			{
				return false;
			}
		}
		return true;
	}

	FString DefaultDirectory()
	{
		return FPaths::ProjectSavedDir() / TEXT("SaveGames");
	}

	TValueOrError<FSlotPaths, ESaveSlotError> MakePaths(const FString& Directory, const FString& SlotName)
	{
		if (!IsValidSlotName(SlotName) || Directory.IsEmpty())
		{
			return MakeError(ESaveSlotError::PathRejected);
		}
		auto Normalise = [](const FString& In)
		{
			FString Full = FPaths::ConvertRelativePathToFull(In);
			FPaths::NormalizeDirectoryName(Full);
			FPaths::CollapseRelativeDirectories(Full);
			return Full;
		};
		const FString Folder = Normalise(Directory);
		const FString Saved = Normalise(FPaths::ProjectSavedDir());
		// SEC-04: the files of a slot stay under the Saved folder
		if (Folder.Contains(TEXT("..")) || !FPaths::IsUnderDirectory(Folder, Saved))
		{
			return MakeError(ESaveSlotError::PathRejected);
		}
		FSlotPaths Paths;
		Paths.Main = Folder / (SlotName + TEXT(".sav"));
		Paths.Temp = Paths.Main + TEXT(".tmp");
		Paths.Backup = Paths.Main + TEXT(".bak");
		return MakeValue(MoveTemp(Paths));
	}

	TValueOrError<TArray<uint8>, ESaveSlotError> Encode(USaveGame& Object, int32 SchemaVersion)
	{
		TArray<uint8> Payload;
		if (SchemaVersion < 1 || !UGameplayStatics::SaveGameToMemory(&Object, Payload) || Payload.IsEmpty()
			|| EnvelopeBytes + static_cast<int64>(Payload.Num()) > MaxFileBytes)
		{
			return MakeError(ESaveSlotError::WriteFailed);
		}
		TArray<uint8> Bytes;
		Bytes.Reserve(EnvelopeBytes + Payload.Num());
		FMemoryWriter Ar(Bytes);
		int32 Zero = 0;
		uint32 Magic = EnvelopeMagic;
		uint16 Version = EnvelopeVersion;
		uint16 Flags = 0;
		int32 Schema = SchemaVersion;
		int32 PayloadBytes = Payload.Num();
		uint32 Crc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());
		Ar << Zero << Magic << Version << Flags << Schema << PayloadBytes << Crc;
		Bytes.Append(Payload);
		return MakeValue(MoveTemp(Bytes));
	}

	ESaveSlotError Check(TConstArrayView<uint8> Bytes, const UClass* ExpectedClass, int32 CurrentSchemaVersion)
	{
		FEnvelope Envelope;
		const ESaveSlotError EnvelopeError = Unwrap(Bytes, CurrentSchemaVersion, Envelope);
		if (EnvelopeError != ESaveSlotError::None)
		{
			return EnvelopeError;
		}
		FEngineHeader Header;
		const ESaveSlotError HeaderError = ParseEngineHeader(Envelope.Payload, Header);
		if (HeaderError != ESaveSlotError::None)
		{
			return HeaderError;
		}
		return IsSaveClass(ExpectedClass) && Header.ClassPath == ExpectedClass->GetPathName() ? ESaveSlotError::None : ESaveSlotError::WrongClass;
	}

	TValueOrError<USaveGame*, ESaveSlotError> Decode(TConstArrayView<uint8> Bytes, UClass* ExpectedClass, int32 CurrentSchemaVersion, bool& bOutLegacy)
	{
		bOutLegacy = false;
		if (!ensureMsgf(IsSaveClass(ExpectedClass), TEXT("Save: %s is not a concrete USaveGame class"), *GetNameSafe(ExpectedClass)))
		{
			return MakeError(ESaveSlotError::WrongClass);
		}
		FEnvelope Envelope;
		const ESaveSlotError EnvelopeError = Unwrap(Bytes, CurrentSchemaVersion, Envelope);
		if (EnvelopeError != ESaveSlotError::None)
		{
			return MakeError(EnvelopeError);
		}
		FEngineHeader Header;
		const ESaveSlotError HeaderError = ParseEngineHeader(Envelope.Payload, Header);
		if (HeaderError != ESaveSlotError::None)
		{
			return MakeError(HeaderError);
		}
		// the class is compared, never loaded from the file (UGameplayStatics::LoadGameFromMemory loads the class it names)
		if (Header.ClassPath != ExpectedClass->GetPathName())
		{
			return MakeError(ESaveSlotError::WrongClass);
		}

		FStrictMemoryReader Reader(FMemoryView(Envelope.Payload.GetData(), static_cast<uint64>(Envelope.Payload.Num())));
		// strings, names and paths of the properties: no length past this (no huge allocation, no FName over its limit)
		Reader.ArMaxSerializeSize = MaxStringChars;
		Reader.SetUEVer(Header.PackageVersion);
		Reader.SetEngineVer(Header.EngineVersion);
		Reader.SetCustomVersions(Header.CustomVersions);
		Reader.Seek(Header.PropertiesOffset);

		USaveGame* Object = NewObject<USaveGame>(GetTransientPackage(), ExpectedClass);
		bool bFailed = false;
		{
			// no object is loaded on behalf of the file
			FObjectAndNameAsStringProxyArchive Ar(Reader, /*bInLoadIfFindFails*/ false);
			Object->Serialize(Ar);
			bFailed = Ar.IsError() || Ar.IsCriticalError();
		}
		if (bFailed || Reader.IsError() || Reader.IsCriticalError())
		{
			return MakeError(ESaveSlotError::Corrupted);
		}
		bOutLegacy = Envelope.bLegacy;
		return MakeValue(Object);
	}

	TValueOrError<TArray<uint8>, ESaveSlotError> ReadBoundedFile(const FString& Path)
	{
		IFileManager& Files = IFileManager::Get();
		if (Files.FileSize(*Path) < 0)
		{
			return MakeError(ESaveSlotError::NotFound);
		}
		for (int32 Try = 0; Try < FileTries; ++Try)
		{
			if (Try > 0)
			{
				FPlatformProcess::SleepNoStats(FileRetrySeconds);
			}
			TUniquePtr<FArchive> Reader(Files.CreateFileReader(*Path, FILEREAD_Silent));
			if (!Reader)
			{
				continue;
			}
			// the size is known before anything is allocated
			const int64 Size = Reader->TotalSize();
			if (Size <= 0 || Size > MaxFileBytes)
			{
				return MakeError(ESaveSlotError::Corrupted);
			}
			TArray<uint8> Bytes;
			Bytes.SetNumUninitialized(static_cast<int32>(Size));
			Reader->Serialize(Bytes.GetData(), Size);
			if (Reader->Close())
			{
				return MakeValue(MoveTemp(Bytes));
			}
		}
		return MakeError(ESaveSlotError::ReadFailed);
	}

	FReadResult Read(const FSlotPaths& Paths, UClass* ExpectedClass, int32 CurrentSchemaVersion, FAcceptSave Accept, const FDateTime& Now)
	{
		FReadResult Result;
		USaveGame* Object = nullptr;
		bool bLegacy = false;
		const FString MainName = FPaths::GetCleanFilename(Paths.Main);

		// 1. the slot file
		Result.MainError = TryLoad(Paths.Main, ExpectedClass, CurrentSchemaVersion, Accept, Object, bLegacy);
		if (Result.MainError == ESaveSlotError::None)
		{
			Result.Object = Object;
			Result.Source = ESource::Main;
			Result.bLegacy = bLegacy;
			return Result;
		}
		if (Result.MainError == ESaveSlotError::ReadFailed)
		{
			// there but locked: left as it is, and nothing is written over it
			UE_LOG(LogMonkeSaveSlot, Warning, TEXT("Save: %s cannot be read now, left as it is (nothing will be written over it this session)"), *MainName);
			Result.bWritable = false;
			return Result;
		}
		if (Result.MainError != ESaveSlotError::NotFound)
		{
			// damaged or too new: renamed aside, untouched (cahier §16: no destructive conversion; error-handling.md §3)
			Result.AsideFileName = PutAside(Paths, Paths.Main, Result.MainError, Now);
			if (Result.AsideFileName.IsEmpty())
			{
				UE_LOG(LogMonkeSaveSlot, Warning, TEXT("Save: %s is %s and cannot be put aside: left as it is (nothing will be written over it this session)"),
					*MainName, LexToString(Result.MainError));
				Result.bWritable = false;
				return Result;
			}
		}

		// 2. a checked temporary file left by a write cut short before it took the slot file's place
		const ESaveSlotError TempError = TryLoad(Paths.Temp, ExpectedClass, CurrentSchemaVersion, Accept, Object, bLegacy);
		if (TempError == ESaveSlotError::None)
		{
			MoveFile(Paths.Main, Paths.Temp, /*bReplace*/ false);
			UE_LOG(LogMonkeSaveSlot, Log, TEXT("Save: %s taken from the temporary file of a write cut short"), *MainName);
			Result.Object = Object;
			Result.Source = ESource::Temp;
			Result.bLegacy = bLegacy;
			return Result;
		}
		if (TempError != ESaveSlotError::NotFound && TempError != ESaveSlotError::ReadFailed)
		{
			PutAside(Paths, Paths.Temp, TempError, Now);
		}

		// 3. the last valid copy, copied back as the slot file (the copy stays)
		const ESaveSlotError BackupError = TryLoad(Paths.Backup, ExpectedClass, CurrentSchemaVersion, Accept, Object, bLegacy);
		if (BackupError == ESaveSlotError::None)
		{
			IFileManager& Files = IFileManager::Get();
			Retry([&Files, &Paths]()
			{
				return Files.Copy(*Paths.Main, *Paths.Backup, /*Replace*/ false, /*EvenIfReadOnly*/ true) == COPY_OK;
			});
			UE_LOG(LogMonkeSaveSlot, Warning, TEXT("Save: %s taken from its last valid copy"), *MainName);
			Result.Object = Object;
			Result.Source = ESource::Backup;
			Result.bLegacy = bLegacy;
			return Result;
		}
		if (BackupError != ESaveSlotError::NotFound && BackupError != ESaveSlotError::ReadFailed)
		{
			PutAside(Paths, Paths.Backup, BackupError, Now);
		}
		// nothing valid: a fresh save starts
		return Result;
	}

	ESaveSlotError Write(const FSlotPaths& Paths, USaveGame& Object, int32 SchemaVersion, const FDateTime& Now)
	{
		TValueOrError<TArray<uint8>, ESaveSlotError> Encoded = Encode(Object, SchemaVersion);
		if (Encoded.HasError())
		{
			return Encoded.GetError();
		}
		const TArray<uint8>& Bytes = Encoded.GetValue();
		const UClass* Class = Object.GetClass();
		IFileManager& Files = IFileManager::Get();

		const FString Folder = FPaths::GetPath(Paths.Main);
		if (!Files.DirectoryExists(*Folder) && !Files.MakeDirectory(*Folder, /*Tree*/ true))
		{
			return ESaveSlotError::WriteFailed;
		}

		// 1. the temporary file, read back and checked (a stale one is a write cut short: Read took it already)
		DeleteQuietly(Paths.Temp);
		const bool bWritten = Retry([&Bytes, &Paths]()
		{
			return FFileHelper::SaveArrayToFile(Bytes, *Paths.Temp);
		});
		bool bChecked = false;
		if (bWritten)
		{
			const TValueOrError<TArray<uint8>, ESaveSlotError> ReadBack = ReadBoundedFile(Paths.Temp);
			bChecked = ReadBack.HasValue() && ReadBack.GetValue() == Bytes && Check(ReadBack.GetValue(), Class, SchemaVersion) == ESaveSlotError::None;
		}
		if (!bChecked)
		{
			DeleteQuietly(Paths.Temp);
			return ESaveSlotError::WriteFailed;
		}

		// 2. the slot file becomes the last valid copy, or is put aside when damaged: never overwritten
		if (Files.FileSize(*Paths.Main) >= 0)
		{
			const TValueOrError<TArray<uint8>, ESaveSlotError> Current = ReadBoundedFile(Paths.Main);
			if (Current.HasError() && Current.GetError() == ESaveSlotError::ReadFailed)
			{
				// cannot tell what it is: left alone
				DeleteQuietly(Paths.Temp);
				return ESaveSlotError::WriteFailed;
			}
			const ESaveSlotError CurrentError = Current.HasError() ? Current.GetError() : Check(Current.GetValue(), Class, SchemaVersion);
			const bool bMoved = CurrentError == ESaveSlotError::None
				? MoveFile(Paths.Backup, Paths.Main, /*bReplace*/ true)
				: !PutAside(Paths, Paths.Main, CurrentError, Now).IsEmpty();
			if (!bMoved)
			{
				DeleteQuietly(Paths.Temp);
				return ESaveSlotError::WriteFailed;
			}
		}

		// 3. the temporary file takes its place (if this fails, the checked temporary file is what Read takes)
		return MoveFile(Paths.Main, Paths.Temp, /*bReplace*/ true) ? ESaveSlotError::None : ESaveSlotError::WriteFailed;
	}

	FText DescribeForPlayer(const FReadResult& Result, const FText& SaveName, const FText& FreshStart)
	{
		if (!Result.bWritable)
		{
			return FText::Format(LOCTEXT("Unreadable", "{0} n'a pas pu être lue : elle reste telle quelle et rien ne sera enregistré par-dessus avant le prochain lancement."),
				SaveName);
		}
		const bool bRestored = Result.Source == ESource::Backup || Result.Source == ESource::Temp;
		const FText Then = bRestored ? LOCTEXT("Restored", "la dernière copie valide a été reprise") : FreshStart;
		const FText Aside = FText::FromString(Result.AsideFileName);
		switch (Result.MainError)
		{
		case ESaveSlotError::None:
			return FText::GetEmpty();
		case ESaveSlotError::NotFound:
			return Result.Source == ESource::Backup
				? FText::Format(LOCTEXT("Missing", "{0} était absente : la dernière copie valide a été reprise."), SaveName)
				: FText::GetEmpty();
		case ESaveSlotError::FutureVersion:
			return FText::Format(LOCTEXT("Future", "{0} vient d'une version plus récente du jeu : elle est conservée à part ({1}), sans modification, et {2}."),
				SaveName, Aside, Then);
		default:
			return FText::Format(LOCTEXT("Damaged", "{0} est endommagée : elle est mise de côté ({1}) et {2}."), SaveName, Aside, Then);
		}
	}

	FText DescribeWriteFailureForPlayer(const FText& SaveName)
	{
		return FText::Format(LOCTEXT("NotWritten", "{0} n'a pas pu être enregistrée : la dernière copie valide est conservée."), SaveName);
	}
}

#undef LOCTEXT_NAMESPACE
