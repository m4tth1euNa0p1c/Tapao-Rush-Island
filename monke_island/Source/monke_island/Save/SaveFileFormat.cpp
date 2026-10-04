// monke_island

#include "Save/SaveFileFormat.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Templates/UniquePtr.h"

namespace MonkeSave
{
	namespace
	{
		/** Writes the header fields in a fixed order */
		void WriteHeader(FArchive& Ar, const FEnvelopeHeader& Header, const TArray<uint8>& NameUtf8, int32 PayloadBytes, uint32 PayloadCrc)
		{
			uint32 Magic = FileMagic;
			uint16 Version = EnvelopeVersion;
			uint16 Flags = Header.Flags;
			int32 Schema = Header.SchemaVersion;
			uint32 A = Header.CampaignId.A, B = Header.CampaignId.B, C = Header.CampaignId.C, D = Header.CampaignId.D;
			int64 Ticks = Header.LastSavedTicks;
			int32 NameBytes = NameUtf8.Num();
			Ar << Magic << Version << Flags << Schema << A << B << C << D << Ticks << NameBytes;
			if (NameBytes > 0)
			{
				Ar.Serialize(const_cast<uint8*>(NameUtf8.GetData()), NameBytes);
			}
			Ar << PayloadBytes << PayloadCrc;
		}

		/**
		 *  Reads the header with every bound checked. FMemoryReader only flags an error on overrun (no log, no assert).
		 *  Returns the offset of the payload, or an error.
		 */
		TValueOrError<int64, ESaveError> ReadHeader(const TArray<uint8>& Bytes, int32 CurrentSchemaVersion, int64 MaxFileBytes, FEnvelopeHeader& OutHeader)
		{
			if (Bytes.Num() < FixedHeaderBytes + 8)
			{
				return MakeError(ESaveError::Corrupted);
			}
			FMemoryReader Ar(Bytes);
			uint32 Magic = 0;
			uint16 Version = 0;
			uint16 Flags = 0;
			int32 Schema = 0;
			uint32 A = 0, B = 0, C = 0, D = 0;
			int64 Ticks = 0;
			int32 NameBytes = 0;
			Ar << Magic << Version << Flags << Schema << A << B << C << D << Ticks << NameBytes;
			if (Ar.IsError() || Magic != FileMagic)
			{
				return MakeError(ESaveError::Corrupted);
			}
			if (Version > EnvelopeVersion)
			{
				return MakeError(ESaveError::FutureVersion);
			}
			if (Version < 1 || Schema < 1 || NameBytes < 0 || NameBytes > MaxHeaderNameBytes || Ticks < 0)
			{
				return MakeError(ESaveError::Corrupted);
			}
			if (Schema > CurrentSchemaVersion)
			{
				return MakeError(ESaveError::FutureVersion);
			}
			TArray<uint8> NameUtf8;
			NameUtf8.SetNumZeroed(NameBytes);
			if (NameBytes > 0)
			{
				Ar.Serialize(NameUtf8.GetData(), NameBytes);
			}
			int32 PayloadBytes = 0;
			uint32 PayloadCrc = 0;
			Ar << PayloadBytes << PayloadCrc;
			if (Ar.IsError() || PayloadBytes <= 0 || PayloadBytes > MaxFileBytes)
			{
				return MakeError(ESaveError::Corrupted);
			}

			OutHeader.Flags = Flags;
			OutHeader.SchemaVersion = Schema;
			OutHeader.CampaignId = FGuid(A, B, C, D);
			OutHeader.LastSavedTicks = Ticks;
			const FUTF8ToTCHAR Converted(reinterpret_cast<const UTF8CHAR*>(NameUtf8.GetData()), NameUtf8.Num());
			OutHeader.DisplayName = FString(Converted.Length(), Converted.Get());
			OutHeader.PayloadBytes = PayloadBytes;
			OutHeader.PayloadCrc = PayloadCrc;
			if (!OutHeader.CampaignId.IsValid())
			{
				return MakeError(ESaveError::Corrupted);
			}
			return MakeValue(Ar.Tell());
		}
	}

	FString NormaliseDirectory(const FString& Directory)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Full);
		FPaths::CollapseRelativeDirectories(Full);
		Full /= TEXT("");
		return Full;
	}

	bool IsAllowedRoot(const FString& Directory)
	{
		if (Directory.IsEmpty() || !FPaths::ValidatePath(Directory))
		{
			return false;
		}
		const FString Root = NormaliseDirectory(Directory);
		const FString Saved = NormaliseDirectory(FPaths::ProjectSavedDir());
		return FPaths::IsUnderDirectory(Root, Saved);
	}

	TValueOrError<FCampaignPaths, ESaveError> MakeCampaignPaths(const FString& RootDirectory, const FGuid& CampaignId)
	{
		if (!CampaignId.IsValid() || !IsAllowedRoot(RootDirectory))
		{
			return MakeError(ESaveError::PathRejected);
		}
		// The file name is the identifier only (32 hex digits): never a name typed by a player (§17, SEC-04)
		const FString Root = NormaliseDirectory(RootDirectory);
		FCampaignPaths Paths;
		Paths.Main = Root / (CampaignId.ToString(EGuidFormats::Digits) + TEXT(".sav"));
		Paths.Temp = Paths.Main + TEXT(".tmp");
		Paths.Backup = Paths.Main + TEXT(".bak");
		Paths.Corrupt = Paths.Main + TEXT(".corrupt");
		if (!FPaths::ValidatePath(Paths.Main) || !FPaths::IsUnderDirectory(Paths.Main, Root))
		{
			return MakeError(ESaveError::PathRejected);
		}
		return MakeValue(MoveTemp(Paths));
	}

	TArray<uint8> Encode(const FEnvelopeHeader& Header, const TArray<uint8>& Payload)
	{
		FTCHARToUTF8 NameConv(*Header.DisplayName);
		TArray<uint8> NameUtf8(reinterpret_cast<const uint8*>(NameConv.Get()), FMath::Min(NameConv.Length(), MaxHeaderNameBytes));
		TArray<uint8> Bytes;
		Bytes.Reserve(FixedHeaderBytes + NameUtf8.Num() + 8 + Payload.Num());
		FMemoryWriter Ar(Bytes);
		WriteHeader(Ar, Header, NameUtf8, Payload.Num(), FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
		Bytes.Append(Payload);
		return Bytes;
	}

	TValueOrError<FDecodedFile, ESaveError> Decode(const TArray<uint8>& Bytes, int32 CurrentSchemaVersion, int64 MaxFileBytes)
	{
		if (Bytes.Num() > MaxFileBytes)
		{
			return MakeError(ESaveError::Corrupted);
		}
		FDecodedFile Out;
		TValueOrError<int64, ESaveError> Offset = ReadHeader(Bytes, CurrentSchemaVersion, MaxFileBytes, Out.Header);
		if (Offset.HasError())
		{
			return MakeError(Offset.GetError());
		}
		const int64 PayloadStart = Offset.GetValue();
		if (PayloadStart + Out.Header.PayloadBytes != Bytes.Num())
		{
			return MakeError(ESaveError::Corrupted);
		}
		const uint8* PayloadData = Bytes.GetData() + PayloadStart;
		if (FCrc::MemCrc32(PayloadData, Out.Header.PayloadBytes) != Out.Header.PayloadCrc)
		{
			return MakeError(ESaveError::Corrupted);
		}
		Out.Payload.Append(PayloadData, Out.Header.PayloadBytes);
		return MakeValue(MoveTemp(Out));
	}

	bool ReadHeaderOnly(const FString& FilePath, int64 MaxFileBytes, FEnvelopeHeader& OutHeader)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath, FILEREAD_Silent));
		if (!Reader)
		{
			return false;
		}
		const int64 Size = Reader->TotalSize();
		if (Size <= 0 || Size > MaxFileBytes)
		{
			return false;
		}
		const int64 HeaderMax = FMath::Min<int64>(Size, FixedHeaderBytes + MaxHeaderNameBytes + 8);
		TArray<uint8> Bytes;
		Bytes.SetNumUninitialized(static_cast<int32>(HeaderMax));
		Reader->Serialize(Bytes.GetData(), HeaderMax);
		if (Reader->IsError() || !Reader->Close())
		{
			return false;
		}
		// Only the header is read: the payload size is checked against the real size, not the checksum
		TValueOrError<int64, ESaveError> Offset = ReadHeader(Bytes, MAX_int32, MaxFileBytes, OutHeader);
		return Offset.HasValue() && Offset.GetValue() + OutHeader.PayloadBytes == Size;
	}

	TValueOrError<TArray<uint8>, ESaveError> ReadBoundedFile(const FString& FilePath, int64 MaxFileBytes)
	{
		IFileManager& FileManager = IFileManager::Get();
		const int64 Size = FileManager.FileSize(*FilePath);
		if (Size < 0)
		{
			return MakeError(ESaveError::CampaignNotFound);
		}
		if (Size == 0 || Size > MaxFileBytes)
		{
			return MakeError(ESaveError::Corrupted);
		}
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *FilePath, FILEREAD_Silent))
		{
			return MakeError(ESaveError::ReadFailed);
		}
		return MakeValue(MoveTemp(Bytes));
	}

	FReadReport ReadCandidates(const FCampaignPaths& Paths, const FGuid& ExpectedId, int32 CurrentSchemaVersion, int64 MaxFileBytes)
	{
		FReadReport Report;
		bool bAnyFileFound = false;
		bool bReadFailure = false;

		const TPair<EFileSource, const FString*> Sources[] = {
			{ EFileSource::Main, &Paths.Main },
			{ EFileSource::Temp, &Paths.Temp },
			{ EFileSource::Backup, &Paths.Backup }
		};
		for (const TPair<EFileSource, const FString*>& Source : Sources)
		{
			TValueOrError<TArray<uint8>, ESaveError> Bytes = ReadBoundedFile(*Source.Value, MaxFileBytes);
			if (Bytes.HasError())
			{
				const ESaveError Error = Bytes.GetError();
				if (Error != ESaveError::CampaignNotFound)
				{
					bAnyFileFound = true;
					bReadFailure |= Error == ESaveError::ReadFailed;
					Report.bMainDamaged |= Source.Key == EFileSource::Main;
				}
				continue;
			}
			bAnyFileFound = true;
			TValueOrError<FDecodedFile, ESaveError> Decoded = Decode(Bytes.GetValue(), CurrentSchemaVersion, MaxFileBytes);
			if (Decoded.HasError())
			{
				if (Source.Key == EFileSource::Main && Decoded.GetError() == ESaveError::FutureVersion)
				{
					// Written by a newer game: refused as a whole, never replaced by an older copy without the player knowing
					Report.Candidates.Reset();
					Report.Error = ESaveError::FutureVersion;
					return Report;
				}
				Report.bMainDamaged |= Source.Key == EFileSource::Main;
				continue;
			}
			if (Decoded.GetValue().Header.CampaignId != ExpectedId)
			{
				Report.bMainDamaged |= Source.Key == EFileSource::Main;
				continue;
			}
			FFileCandidate& Candidate = Report.Candidates.AddDefaulted_GetRef();
			Candidate.Source = Source.Key;
			Candidate.File = Decoded.StealValue();
		}

		if (Report.Candidates.Num() == 0)
		{
			Report.Error = !bAnyFileFound ? ESaveError::CampaignNotFound : (bReadFailure ? ESaveError::ReadFailed : ESaveError::Corrupted);
		}
		return Report;
	}

	FWriteReport WriteAtomic(const FCampaignPaths& Paths, const TArray<uint8>& FileBytes, const FGuid& ExpectedId, int32 CurrentSchemaVersion, int64 MaxFileBytes, bool bMainKnownDamaged)
	{
		FWriteReport Report;
		IFileManager& FileManager = IFileManager::Get();

		if (FileBytes.Num() <= 0 || FileBytes.Num() > MaxFileBytes)
		{
			Report.Error = ESaveError::InvalidRequest;
			Report.Detail = FString::Printf(TEXT("size %d out of bounds"), FileBytes.Num());
			return Report;
		}

		const FString Directory = FPaths::GetPath(Paths.Main);
		if (!FileManager.DirectoryExists(*Directory) && !FileManager.MakeDirectory(*Directory, true))
		{
			Report.Error = ESaveError::WriteFailed;
			Report.Detail = TEXT("cannot create the save folder");
			return Report;
		}

		// 1. Temporary file (a stale one from an earlier crash is discarded first)
		if (FileManager.FileExists(*Paths.Temp))
		{
			FileManager.Delete(*Paths.Temp, false, true, true);
		}
		if (!FFileHelper::SaveArrayToFile(FileBytes, *Paths.Temp))
		{
			FileManager.Delete(*Paths.Temp, false, true, true);
			Report.Error = ESaveError::WriteFailed;
			Report.Detail = TEXT("temporary file not written");
			return Report;
		}

		// 2. Read back: same bytes, decodable, same campaign
		TArray<uint8> ReadBack;
		const bool bReadBack = FFileHelper::LoadFileToArray(ReadBack, *Paths.Temp, FILEREAD_Silent);
		bool bValid = bReadBack && ReadBack == FileBytes;
		if (bValid)
		{
			TValueOrError<FDecodedFile, ESaveError> Decoded = Decode(ReadBack, CurrentSchemaVersion, MaxFileBytes);
			bValid = Decoded.HasValue() && Decoded.GetValue().Header.CampaignId == ExpectedId;
		}
		if (!bValid)
		{
			FileManager.Delete(*Paths.Temp, false, true, true);
			Report.Error = ESaveError::ValidationFailed;
			Report.Detail = bReadBack ? TEXT("temporary file differs from the written bytes") : TEXT("temporary file unreadable");
			return Report;
		}

		// 3. The current main file becomes the last valid copy, or is moved aside when damaged (the backup stays)
		bool bMainMovedToBackup = false;
		if (FileManager.FileExists(*Paths.Main))
		{
			TValueOrError<TArray<uint8>, ESaveError> MainBytes = ReadBoundedFile(Paths.Main, MaxFileBytes);
			const bool bMainValid = !bMainKnownDamaged && MainBytes.HasValue() && Decode(MainBytes.GetValue(), CurrentSchemaVersion, MaxFileBytes).HasValue();
			if (bMainValid)
			{
				if (!FileManager.Move(*Paths.Backup, *Paths.Main, true, true, false, true))
				{
					FileManager.Delete(*Paths.Temp, false, true, true);
					Report.Error = ESaveError::WriteFailed;
					Report.Detail = TEXT("main file could not become the backup");
					return Report;
				}
				bMainMovedToBackup = true;
			}
			else
			{
				if (!FileManager.Move(*Paths.Corrupt, *Paths.Main, true, true, false, true))
				{
					FileManager.Delete(*Paths.Temp, false, true, true);
					Report.Error = ESaveError::WriteFailed;
					Report.Detail = TEXT("damaged main file could not be moved aside");
					return Report;
				}
				Report.bQuarantinedDamagedMain = true;
			}
		}

		// 4. Temporary file becomes the main file
		if (!FileManager.Move(*Paths.Main, *Paths.Temp, true, true, false, true))
		{
			if (bMainMovedToBackup && !FileManager.FileExists(*Paths.Main))
			{
				// Put the previous valid file back as main; it still is the last valid copy
				FileManager.Copy(*Paths.Main, *Paths.Backup);
			}
			FileManager.Delete(*Paths.Temp, false, true, true);
			Report.Error = ESaveError::WriteFailed;
			Report.Detail = TEXT("temporary file could not replace the main file");
			return Report;
		}
		return Report;
	}
}
