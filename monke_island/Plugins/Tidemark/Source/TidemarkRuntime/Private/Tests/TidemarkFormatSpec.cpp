// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tidemark/Grid/TidemarkFormat.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Grid/TidemarkTerrainAsset.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter Tidemark.Format
BEGIN_DEFINE_SPEC(FTidemarkFormatSpec, "Tidemark.Format",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FString TestRoot;
	static FTidemarkGrid MakeGrid(int32 SizeX, int32 SizeY)
	{
		FTidemarkGrid Grid;
		Grid.Init(SizeX, SizeY, 400.0f, FVector2D(12.5, -40.0));
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				Grid.ElevationCm[Grid.Index(X, Y)] = static_cast<int16>(-300 + 100 * X + 1000 * Y);
			}
		}
		Grid.SetFlag(0, 0, ETidemarkCellFlag::Border, true);
		Grid.SetFlag(1, 1, ETidemarkCellFlag::OpenSea, true);
		Grid.SetFlag(2, 1, ETidemarkCellFlag::Forbidden, true);
		return Grid;
	}
	static TArray<FTidemarkCredit> MakeCredits()
	{
		FTidemarkCredit Credit;
		Credit.Producer = TEXT("Tidemark spec");
		Credit.License = TEXT("CC0 1.0");
		Credit.Url = TEXT("https://example.invalid/spec");
		Credit.Date = TEXT("2026-10-23");
		return { Credit };
	}
	FString Folder(const TCHAR* Name) const
	{
		return FPaths::Combine(TestRoot, Name);
	}
	void CleanFolder(const FString& Dir) const
	{
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
	}
END_DEFINE_SPEC(FTidemarkFormatSpec)

void FTidemarkFormatSpec::Define()
{
	using namespace Tidemark::Format;

	BeforeEach([this]()
	{
		TestRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tidemark"), TEXT("Tests"));
	});

	Describe("Sha256", [this]()
	{
		It("matches the FIPS 180-4 vectors", [this]()
		{
			TestEqual(TEXT("empty"), Sha256Hex(nullptr, 0), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
			const ANSICHAR* Abc = "abc";
			TestEqual(TEXT("abc"), Sha256Hex(reinterpret_cast<const uint8*>(Abc), 3), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
			const ANSICHAR* Long = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
			TestEqual(TEXT("two blocks"), Sha256Hex(reinterpret_cast<const uint8*>(Long), 56), FString(TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));
		});
	});

	Describe("RoundTrip", [this]()
	{
		It("saves then loads the same grid, hash and credits", [this]()
		{
			const FString Dir = Folder(TEXT("RoundTrip.tmk"));
			CleanFolder(Dir);
			const FTidemarkGrid Grid = MakeGrid(4, 3);
			const TArray<FTidemarkCredit> Credits = MakeCredits();
			FString Error;
			TestTrue(FString::Printf(TEXT("save: %s"), *Error), SaveTmk(Grid, Credits, Dir, Error));
			TestTrue(TEXT("manifest exists"), FPaths::FileExists(FPaths::Combine(Dir, ManifestFileName)));
			TestTrue(TEXT("elevation exists"), FPaths::FileExists(FPaths::Combine(Dir, ElevationFileName)));
			TestTrue(TEXT("flags exists"), FPaths::FileExists(FPaths::Combine(Dir, FlagsFileName)));

			FTidemarkGrid Loaded;
			TArray<FTidemarkCredit> LoadedCredits;
			TestTrue(FString::Printf(TEXT("load: %s"), *Error), LoadTmk(Dir, Loaded, LoadedCredits, Error));
			TestTrue(TEXT("grid equal"), Loaded == Grid);
			TestTrue(TEXT("same hash"), Loaded.ComputeSourceHash() == Grid.ComputeSourceHash());
			TestEqual(TEXT("credits count"), LoadedCredits.Num(), 1);
			if (LoadedCredits.Num() == 1)
			{
				TestTrue(TEXT("credit equal"), LoadedCredits[0] == Credits[0]);
			}
			CleanFolder(Dir);
		});

		It("writes little-endian int16 rows and the expected manifest keys", [this]()
		{
			const FString Dir = Folder(TEXT("Layout.tmk"));
			CleanFolder(Dir);
			FTidemarkGrid Grid;
			Grid.Init(2, 1, 400.0f);
			Grid.ElevationCm = { 258, -2 }; // 0x0102, 0xFFFE
			Grid.Flags = { 1, 4 };
			FString Error;
			TestTrue(FString::Printf(TEXT("save: %s"), *Error), SaveTmk(Grid, MakeCredits(), Dir, Error));

			TArray<uint8> Bytes;
			TestTrue(TEXT("read elevation"), FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Dir, ElevationFileName)));
			TestEqual(TEXT("bytes"), Bytes.Num(), 4);
			if (Bytes.Num() == 4)
			{
				TestEqual(TEXT("b0"), static_cast<int32>(Bytes[0]), 0x02);
				TestEqual(TEXT("b1"), static_cast<int32>(Bytes[1]), 0x01);
				TestEqual(TEXT("b2"), static_cast<int32>(Bytes[2]), 0xFE);
				TestEqual(TEXT("b3"), static_cast<int32>(Bytes[3]), 0xFF);
			}

			FString Manifest;
			TestTrue(TEXT("read manifest"), FFileHelper::LoadFileToString(Manifest, *FPaths::Combine(Dir, ManifestFileName)));
			for (const TCHAR* Key : { TEXT("\"schema_version\""), TEXT("\"origin_cm\""), TEXT("\"cell_size_cm\""), TEXT("\"size\""),
				TEXT("\"layers\""), TEXT("\"sha256\""), TEXT("\"source_hash\""), TEXT("\"credits\""), TEXT("\"producer\""),
				TEXT("\"license\""), TEXT("\"url\""), TEXT("\"date\"") })
			{
				TestTrue(FString::Printf(TEXT("manifest has %s"), Key), Manifest.Contains(Key));
			}
			TestTrue(TEXT("manifest has the hash"), Manifest.Contains(FTidemarkGrid::HashToHex(Grid.ComputeSourceHash())));
			CleanFolder(Dir);
		});
	});

	Describe("Errors", [this]()
	{
		It("fails cleanly on a missing manifest", [this]()
		{
			const FString Dir = Folder(TEXT("Missing.tmk"));
			CleanFolder(Dir);
			FTidemarkGrid Loaded;
			TArray<FTidemarkCredit> Credits;
			FString Error;
			TestFalse(TEXT("load"), LoadTmk(Dir, Loaded, Credits, Error));
			TestFalse(TEXT("error set"), Error.IsEmpty());
			TestFalse(TEXT("grid reset"), Loaded.IsValid());
		});

		It("refuses to save an invalid grid or incomplete credits", [this]()
		{
			const FString Dir = Folder(TEXT("Refused.tmk"));
			CleanFolder(Dir);
			FString Error;
			FTidemarkGrid Empty;
			TestFalse(TEXT("invalid grid"), SaveTmk(Empty, MakeCredits(), Dir, Error));
			TestFalse(TEXT("error for grid"), Error.IsEmpty());

			TestFalse(TEXT("no credits"), SaveTmk(MakeGrid(2, 2), {}, Dir, Error));
			TestTrue(TEXT("error names credits"), Error.Contains(TEXT("credits")));

			TArray<FTidemarkCredit> Incomplete = MakeCredits();
			Incomplete[0].Date.Empty();
			TestFalse(TEXT("incomplete credit"), SaveTmk(MakeGrid(2, 2), Incomplete, Dir, Error));
			TestFalse(TEXT("nothing written"), FPaths::FileExists(FPaths::Combine(Dir, ManifestFileName)));
		});

		It("detects a corrupted layer by its sha256", [this]()
		{
			const FString Dir = Folder(TEXT("Corrupt.tmk"));
			CleanFolder(Dir);
			FString Error;
			TestTrue(TEXT("save"), SaveTmk(MakeGrid(3, 3), MakeCredits(), Dir, Error));
			const FString FlagsPath = FPaths::Combine(Dir, FlagsFileName);
			TArray<uint8> Bytes;
			TestTrue(TEXT("read"), FFileHelper::LoadFileToArray(Bytes, *FlagsPath));
			Bytes[0] ^= 0x04;
			TestTrue(TEXT("rewrite"), FFileHelper::SaveArrayToFile(Bytes, *FlagsPath));

			FTidemarkGrid Loaded;
			TArray<FTidemarkCredit> Credits;
			TestFalse(TEXT("load refused"), LoadTmk(Dir, Loaded, Credits, Error));
			TestTrue(TEXT("error names sha256"), Error.Contains(TEXT("sha256")));
			TestFalse(TEXT("grid reset"), Loaded.IsValid());
			CleanFolder(Dir);
		});

		It("refuses a manifest with another schema version", [this]()
		{
			const FString Dir = Folder(TEXT("Schema.tmk"));
			CleanFolder(Dir);
			FString Error;
			TestTrue(TEXT("save"), SaveTmk(MakeGrid(2, 2), MakeCredits(), Dir, Error));
			const FString ManifestPath = FPaths::Combine(Dir, ManifestFileName);
			FString Manifest;
			TestTrue(TEXT("read"), FFileHelper::LoadFileToString(Manifest, *ManifestPath));
			Manifest = Manifest.Replace(TEXT("\"schema_version\":1"), TEXT("\"schema_version\":99")).Replace(TEXT("\"schema_version\": 1"), TEXT("\"schema_version\": 99"));
			TestTrue(TEXT("rewrite"), FFileHelper::SaveStringToFile(Manifest, *ManifestPath));

			FTidemarkGrid Loaded;
			TArray<FTidemarkCredit> Credits;
			TestFalse(TEXT("load refused"), LoadTmk(Dir, Loaded, Credits, Error));
			TestTrue(TEXT("error names schema"), Error.Contains(TEXT("schema_version")));
			CleanFolder(Dir);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
