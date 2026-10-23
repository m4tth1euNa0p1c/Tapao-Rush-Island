// Tidemark plugin

#include "Tidemark/Grid/TidemarkFormat.h"
#include "Tidemark/TidemarkModule.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Tidemark::Format
{
	namespace
	{
		// ---- SHA-256 (FIPS 180-4). Own implementation so the digest is identical on every platform and in Tools/tmk.py
		//      (FPlatformMisc::GetSHA256Signature is only implemented on some platforms and FSHA1 is not SHA-256).

		constexpr uint32 Sha256K[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
			0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
			0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
			0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

		FORCEINLINE uint32 RotR(uint32 Value, uint32 Bits)
		{
			return (Value >> Bits) | (Value << (32 - Bits));
		}

		struct FSha256Context
		{
			uint32 State[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
			uint8 Block[64] = {};
			uint64 TotalBytes = 0;
			int32 BlockFill = 0;

			void Transform(const uint8* Data)
			{
				uint32 W[64];
				for (int32 I = 0; I < 16; ++I)
				{
					W[I] = (static_cast<uint32>(Data[I * 4]) << 24) | (static_cast<uint32>(Data[I * 4 + 1]) << 16)
						| (static_cast<uint32>(Data[I * 4 + 2]) << 8) | static_cast<uint32>(Data[I * 4 + 3]);
				}
				for (int32 I = 16; I < 64; ++I)
				{
					const uint32 S0 = RotR(W[I - 15], 7) ^ RotR(W[I - 15], 18) ^ (W[I - 15] >> 3);
					const uint32 S1 = RotR(W[I - 2], 17) ^ RotR(W[I - 2], 19) ^ (W[I - 2] >> 10);
					W[I] = W[I - 16] + S0 + W[I - 7] + S1;
				}

				uint32 A = State[0], B = State[1], C = State[2], D = State[3];
				uint32 E = State[4], F = State[5], G = State[6], H = State[7];
				for (int32 I = 0; I < 64; ++I)
				{
					const uint32 S1 = RotR(E, 6) ^ RotR(E, 11) ^ RotR(E, 25);
					const uint32 Ch = (E & F) ^ (~E & G);
					const uint32 T1 = H + S1 + Ch + Sha256K[I] + W[I];
					const uint32 S0 = RotR(A, 2) ^ RotR(A, 13) ^ RotR(A, 22);
					const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
					const uint32 T2 = S0 + Maj;
					H = G; G = F; F = E; E = D + T1;
					D = C; C = B; B = A; A = T1 + T2;
				}
				State[0] += A; State[1] += B; State[2] += C; State[3] += D;
				State[4] += E; State[5] += F; State[6] += G; State[7] += H;
			}

			void Update(const uint8* Data, int64 Size)
			{
				TotalBytes += static_cast<uint64>(Size);
				int64 Offset = 0;
				if (BlockFill > 0)
				{
					const int64 Take = FMath::Min<int64>(64 - BlockFill, Size);
					FMemory::Memcpy(Block + BlockFill, Data, Take);
					BlockFill += static_cast<int32>(Take);
					Offset += Take;
					if (BlockFill == 64)
					{
						Transform(Block);
						BlockFill = 0;
					}
				}
				while (Size - Offset >= 64)
				{
					Transform(Data + Offset);
					Offset += 64;
				}
				if (Offset < Size)
				{
					BlockFill = static_cast<int32>(Size - Offset);
					FMemory::Memcpy(Block, Data + Offset, BlockFill);
				}
			}

			void Final(uint8 OutDigest[32])
			{
				const uint64 TotalBits = TotalBytes * 8;
				Block[BlockFill++] = 0x80;
				if (BlockFill > 56)
				{
					FMemory::Memzero(Block + BlockFill, 64 - BlockFill);
					Transform(Block);
					BlockFill = 0;
				}
				FMemory::Memzero(Block + BlockFill, 56 - BlockFill);
				for (int32 I = 0; I < 8; ++I)
				{
					Block[56 + I] = static_cast<uint8>(TotalBits >> (8 * (7 - I)));
				}
				Transform(Block);
				for (int32 I = 0; I < 8; ++I)
				{
					OutDigest[I * 4] = static_cast<uint8>(State[I] >> 24);
					OutDigest[I * 4 + 1] = static_cast<uint8>(State[I] >> 16);
					OutDigest[I * 4 + 2] = static_cast<uint8>(State[I] >> 8);
					OutDigest[I * 4 + 3] = static_cast<uint8>(State[I]);
				}
			}
		};

		// ---- Helpers

		FString JoinPath(const FString& Dir, const TCHAR* FileName)
		{
			return FPaths::Combine(Dir, FileName);
		}

		TSharedRef<FJsonObject> CreditToJson(const FTidemarkCredit& Credit)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("producer"), Credit.Producer);
			Object->SetStringField(TEXT("license"), Credit.License);
			Object->SetStringField(TEXT("url"), Credit.Url);
			Object->SetStringField(TEXT("date"), Credit.Date);
			return Object;
		}

		TSharedRef<FJsonObject> LayerToJson(const TCHAR* Name, const TCHAR* Type, const TCHAR* FileName, const FString& Sha256)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("name"), Name);
			Object->SetStringField(TEXT("type"), Type);
			Object->SetStringField(TEXT("file"), FileName);
			Object->SetStringField(TEXT("sha256"), Sha256);
			return Object;
		}

		/** Serialises an int16 layer as little-endian bytes whatever the host endianness. */
		TArray<uint8> PackInt16(const TArray<int16>& Values)
		{
			TArray<uint8> Bytes;
			Bytes.SetNumUninitialized(Values.Num() * 2);
			for (int32 I = 0; I < Values.Num(); ++I)
			{
				const uint16 Raw = static_cast<uint16>(Values[I]);
				Bytes[I * 2] = static_cast<uint8>(Raw & 0xFF);
				Bytes[I * 2 + 1] = static_cast<uint8>(Raw >> 8);
			}
			return Bytes;
		}

		void UnpackInt16(const TArray<uint8>& Bytes, TArray<int16>& OutValues)
		{
			const int32 Count = Bytes.Num() / 2;
			OutValues.SetNumUninitialized(Count);
			for (int32 I = 0; I < Count; ++I)
			{
				const uint16 Raw = static_cast<uint16>(Bytes[I * 2]) | (static_cast<uint16>(Bytes[I * 2 + 1]) << 8);
				OutValues[I] = static_cast<int16>(Raw);
			}
		}

		bool WriteBinary(const FString& Path, const TArray<uint8>& Bytes, FString& OutError)
		{
			if (!FFileHelper::SaveArrayToFile(Bytes, *Path))
			{
				OutError = FString::Printf(TEXT("cannot write %s"), *Path);
				return false;
			}
			return true;
		}

		/** Reads one layer file, checks its size and digest against the manifest. */
		bool ReadLayer(const FString& Dir, const TSharedPtr<FJsonObject>& Layer, const TCHAR* ExpectedType, int64 ExpectedBytes,
			TArray<uint8>& OutBytes, FString& OutError)
		{
			FString Name, Type, File, Sha;
			if (!Layer.IsValid() || !Layer->TryGetStringField(TEXT("name"), Name) || !Layer->TryGetStringField(TEXT("type"), Type)
				|| !Layer->TryGetStringField(TEXT("file"), File) || !Layer->TryGetStringField(TEXT("sha256"), Sha))
			{
				OutError = TEXT("manifest.json: a layer lacks name, type, file or sha256");
				return false;
			}
			if (Type != ExpectedType)
			{
				OutError = FString::Printf(TEXT("manifest.json: layer %s has type %s, expected %s"), *Name, *Type, ExpectedType);
				return false;
			}
			// The file name must stay inside the folder: no separators, no parent references.
			if (File.IsEmpty() || File.Contains(TEXT("/")) || File.Contains(TEXT("\\")) || File.Contains(TEXT("..")))
			{
				OutError = FString::Printf(TEXT("manifest.json: layer %s has an invalid file name"), *Name);
				return false;
			}
			const FString Path = FPaths::Combine(Dir, File);
			if (!FFileHelper::LoadFileToArray(OutBytes, *Path))
			{
				OutError = FString::Printf(TEXT("cannot read layer file %s"), *Path);
				return false;
			}
			if (OutBytes.Num() != ExpectedBytes)
			{
				OutError = FString::Printf(TEXT("%s: %d bytes, expected %lld"), *File, OutBytes.Num(), ExpectedBytes);
				return false;
			}
			const FString Actual = Sha256Hex(OutBytes);
			if (!Actual.Equals(Sha, ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(TEXT("%s: sha256 mismatch (manifest %s, file %s)"), *File, *Sha, *Actual);
				return false;
			}
			return true;
		}

		const TSharedPtr<FJsonObject>* FindLayer(const TArray<TSharedPtr<FJsonValue>>& Layers, const TCHAR* Name)
		{
			for (const TSharedPtr<FJsonValue>& Value : Layers)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				FString LayerName;
				if (Value.IsValid() && Value->TryGetObject(Object) && Object && (*Object)->TryGetStringField(TEXT("name"), LayerName) && LayerName == Name)
				{
					return Object;
				}
			}
			return nullptr;
		}
	}

	FString Sha256Hex(const uint8* Data, int64 Size)
	{
		FSha256Context Context;
		if (Data && Size > 0)
		{
			Context.Update(Data, Size);
		}
		uint8 Digest[32];
		Context.Final(Digest);
		return BytesToHex(Digest, 32).ToLower();
	}

	FString Sha256Hex(const TArray<uint8>& Data)
	{
		return Sha256Hex(Data.GetData(), Data.Num());
	}

	bool SaveTmk(const FTidemarkGrid& Grid, const TArray<FTidemarkCredit>& Credits, const FString& Dir, FString& OutError)
	{
		OutError.Reset();
		if (!Grid.IsValid())
		{
			OutError = TEXT("grid is invalid (empty, or layer sizes do not match SizeX * SizeY, or above MaxCells)");
			return false;
		}
		if (Credits.Num() == 0)
		{
			OutError = TEXT("credits[] is empty: producer, license, url and date are mandatory (Tidemark conventions section 7)");
			return false;
		}
		for (int32 I = 0; I < Credits.Num(); ++I)
		{
			if (!Credits[I].IsComplete())
			{
				OutError = FString::Printf(TEXT("credits[%d] is incomplete: producer, license, url and date are mandatory"), I);
				return false;
			}
		}

		IFileManager& FileManager = IFileManager::Get();
		if (!FileManager.MakeDirectory(*Dir, true))
		{
			OutError = FString::Printf(TEXT("cannot create folder %s"), *Dir);
			return false;
		}

		const TArray<uint8> ElevationBytes = PackInt16(Grid.ElevationCm);
		const TArray<uint8>& FlagBytes = Grid.Flags;
		if (!WriteBinary(JoinPath(Dir, ElevationFileName), ElevationBytes, OutError)
			|| !WriteBinary(JoinPath(Dir, FlagsFileName), FlagBytes, OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
		Manifest->SetNumberField(TEXT("schema_version"), SchemaVersion);
		Manifest->SetStringField(TEXT("generator"), TEXT("Tidemark/TidemarkFormat"));
		TArray<TSharedPtr<FJsonValue>> OriginValues;
		OriginValues.Add(MakeShared<FJsonValueNumber>(Grid.OriginWorld.X));
		OriginValues.Add(MakeShared<FJsonValueNumber>(Grid.OriginWorld.Y));
		Manifest->SetArrayField(TEXT("origin_cm"), OriginValues);
		Manifest->SetNumberField(TEXT("cell_size_cm"), Grid.CellSizeCm);
		TArray<TSharedPtr<FJsonValue>> SizeValues;
		SizeValues.Add(MakeShared<FJsonValueNumber>(Grid.SizeX));
		SizeValues.Add(MakeShared<FJsonValueNumber>(Grid.SizeY));
		Manifest->SetArrayField(TEXT("size"), SizeValues);
		Manifest->SetField(TEXT("crs"), MakeShared<FJsonValueNull>());
		TArray<TSharedPtr<FJsonValue>> LayerValues;
		LayerValues.Add(MakeShared<FJsonValueObject>(LayerToJson(ElevationLayerName, TEXT("int16"), ElevationFileName, Sha256Hex(ElevationBytes))));
		LayerValues.Add(MakeShared<FJsonValueObject>(LayerToJson(FlagsLayerName, TEXT("uint8"), FlagsFileName, Sha256Hex(FlagBytes))));
		Manifest->SetArrayField(TEXT("layers"), LayerValues);
		Manifest->SetStringField(TEXT("source_hash"), FTidemarkGrid::HashToHex(Grid.ComputeSourceHash()));

		TArray<TSharedPtr<FJsonValue>> CreditValues;
		for (const FTidemarkCredit& Credit : Credits)
		{
			CreditValues.Add(MakeShared<FJsonValueObject>(CreditToJson(Credit)));
		}
		Manifest->SetArrayField(TEXT("credits"), CreditValues);

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		if (!FJsonSerializer::Serialize(Manifest, Writer))
		{
			OutError = TEXT("cannot serialise manifest.json");
			return false;
		}
		const FString ManifestPath = JoinPath(Dir, ManifestFileName);
		if (!FFileHelper::SaveStringToFile(Text, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("cannot write %s"), *ManifestPath);
			return false;
		}

		UE_LOG(LogTidemark, Log, TEXT("Tidemark: wrote %s (%d x %d cells, %lld bytes)"), *Dir, Grid.SizeX, Grid.SizeY, Grid.NumBytes());
		return true;
	}

	bool LoadTmk(const FString& Dir, FTidemarkGrid& OutGrid, TArray<FTidemarkCredit>& OutCredits, FString& OutError)
	{
		OutError.Reset();
		OutGrid.Reset();
		OutCredits.Reset();

		const FString ManifestPath = JoinPath(Dir, ManifestFileName);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ManifestPath))
		{
			OutError = FString::Printf(TEXT("missing or unreadable %s"), *ManifestPath);
			return false;
		}

		TSharedPtr<FJsonObject> Manifest;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Manifest) || !Manifest.IsValid())
		{
			OutError = FString::Printf(TEXT("%s: malformed JSON"), *ManifestPath);
			return false;
		}

		int32 Schema = 0;
		if (!Manifest->TryGetNumberField(TEXT("schema_version"), Schema) || Schema != SchemaVersion)
		{
			OutError = FString::Printf(TEXT("manifest.json: schema_version %d, expected %d"), Schema, SchemaVersion);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Origin = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
		double CellSize = 0.0;
		if (!Manifest->TryGetArrayField(TEXT("origin_cm"), Origin) || !Origin || Origin->Num() != 2
			|| !Manifest->TryGetArrayField(TEXT("size"), Size) || !Size || Size->Num() != 2
			|| !Manifest->TryGetNumberField(TEXT("cell_size_cm"), CellSize)
			|| !Manifest->TryGetArrayField(TEXT("layers"), Layers) || !Layers)
		{
			OutError = TEXT("manifest.json: origin_cm[2], size[2], cell_size_cm or layers[] missing");
			return false;
		}

		double OriginX = 0.0, OriginY = 0.0, SizeXD = 0.0, SizeYD = 0.0;
		if (!(*Origin)[0]->TryGetNumber(OriginX) || !(*Origin)[1]->TryGetNumber(OriginY)
			|| !(*Size)[0]->TryGetNumber(SizeXD) || !(*Size)[1]->TryGetNumber(SizeYD))
		{
			OutError = TEXT("manifest.json: origin_cm and size must be numbers");
			return false;
		}
		const int32 SizeX = static_cast<int32>(SizeXD);
		const int32 SizeY = static_cast<int32>(SizeYD);
		if (SizeX <= 0 || SizeY <= 0 || static_cast<int64>(SizeX) * SizeY > FTidemarkGrid::MaxCells || !(CellSize > 0.0) || !FMath::IsFinite(CellSize))
		{
			OutError = FString::Printf(TEXT("manifest.json: size %d x %d or cell_size_cm %g out of range (max %d cells)"), SizeX, SizeY, CellSize, FTidemarkGrid::MaxCells);
			return false;
		}
		const int64 CellCount = static_cast<int64>(SizeX) * SizeY;

		const TSharedPtr<FJsonObject>* ElevationLayer = FindLayer(*Layers, ElevationLayerName);
		const TSharedPtr<FJsonObject>* FlagsLayer = FindLayer(*Layers, FlagsLayerName);
		if (!ElevationLayer || !FlagsLayer)
		{
			OutError = TEXT("manifest.json: layers elevation_cm and flags are both required");
			return false;
		}

		TArray<uint8> ElevationBytes;
		TArray<uint8> FlagBytes;
		if (!ReadLayer(Dir, *ElevationLayer, TEXT("int16"), CellCount * 2, ElevationBytes, OutError)
			|| !ReadLayer(Dir, *FlagsLayer, TEXT("uint8"), CellCount, FlagBytes, OutError))
		{
			return false;
		}

		OutGrid.OriginWorld = FVector2D(OriginX, OriginY);
		OutGrid.CellSizeCm = static_cast<float>(CellSize);
		OutGrid.SizeX = SizeX;
		OutGrid.SizeY = SizeY;
		UnpackInt16(ElevationBytes, OutGrid.ElevationCm);
		OutGrid.Flags = MoveTemp(FlagBytes);

		FString StoredHash;
		if (Manifest->TryGetStringField(TEXT("source_hash"), StoredHash) && !StoredHash.IsEmpty())
		{
			const FString Actual = FTidemarkGrid::HashToHex(OutGrid.ComputeSourceHash());
			if (!Actual.Equals(StoredHash, ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(TEXT("manifest.json: source_hash %s does not match the layers (%s)"), *StoredHash, *Actual);
				OutGrid.Reset();
				return false;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* CreditValues = nullptr;
		if (Manifest->TryGetArrayField(TEXT("credits"), CreditValues) && CreditValues)
		{
			for (const TSharedPtr<FJsonValue>& Value : *CreditValues)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object)
				{
					continue;
				}
				FTidemarkCredit Credit;
				(*Object)->TryGetStringField(TEXT("producer"), Credit.Producer);
				(*Object)->TryGetStringField(TEXT("license"), Credit.License);
				(*Object)->TryGetStringField(TEXT("url"), Credit.Url);
				(*Object)->TryGetStringField(TEXT("date"), Credit.Date);
				OutCredits.Add(MoveTemp(Credit));
			}
		}
		if (OutCredits.Num() == 0)
		{
			UE_LOG(LogTidemark, Warning, TEXT("Tidemark: %s has no credits[]; the CI will refuse it"), *Dir);
		}

		return true;
	}
}
