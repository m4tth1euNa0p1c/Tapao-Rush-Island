// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "TidemarkEditor/Import/TextureHeightSource.h"
#include "TidemarkEditor/Import/TidemarkBaker.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"
#include "TidemarkEditor/Import/TidemarkImportLibrary.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter Tidemark.Import.Bake
// Landscape bakes need a world: manual recipe in Plugins/Tidemark/Docs/README.md (Import de terrain).
BEGIN_DEFINE_SPEC(FTidemarkBakeSpec, "Tidemark.Import.Bake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FString TestRoot;
	/** 1 000 x 600 cm rectangle starting at the origin. */
	static FBox2D Rect()
	{
		return FBox2D(FVector2D(0.0, 0.0), FVector2D(1000.0, 600.0));
	}
	static FTidemarkBakeSettings Settings(float CellSizeCm, float MarginCm = 0.0f)
	{
		FTidemarkBakeSettings S;
		S.CellSizeCm = CellSizeCm;
		S.SeaLevelCm = 0.0f;
		S.OpenSeaDepthCm = 150.0f;
		S.OpenSeaMarginCm = MarginCm;
		S.MissingElevationCm = -30000.0f;
		return S;
	}
	static FTidemarkFunctionHeightSource Flat(float HeightCm)
	{
		return FTidemarkFunctionHeightSource(Rect(), [HeightCm](double, double, float& Out) { Out = HeightCm; return true; }, TEXT("flat"));
	}
	int32 CountFlag(const FTidemarkGrid& Grid, ETidemarkCellFlag Flag) const
	{
		int32 Count = 0;
		for (int32 Y = 0; Y < Grid.SizeY; ++Y)
		{
			for (int32 X = 0; X < Grid.SizeX; ++X)
			{
				Count += Grid.HasFlag(X, Y, Flag) ? 1 : 0;
			}
		}
		return Count;
	}
END_DEFINE_SPEC(FTidemarkBakeSpec)

void FTidemarkBakeSpec::Define()
{
	using namespace Tidemark::Bake;

	BeforeEach([this]()
	{
		TestRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tidemark"), TEXT("Tests"));
	});

	Describe("Dimensions", [this]()
	{
		It("sizes the grid from the bounds and the cell, origin at the lower-left corner", [this]()
		{
			FTidemarkGrid Grid;
			const FTidemarkBakeResult Result = BakeGrid(Flat(100.0f), Settings(100.0f), Grid);
			TestTrue(FString::Printf(TEXT("ok: %s"), *Result.Error), Result.bOk);
			TestEqual(TEXT("SizeX"), Grid.SizeX, 10);
			TestEqual(TEXT("SizeY"), Grid.SizeY, 6);
			TestEqual(TEXT("cell"), Grid.CellSizeCm, 100.0f);
			TestTrue(TEXT("origin"), Grid.OriginWorld.Equals(FVector2D(0.0, 0.0)));
			TestEqual(TEXT("sampled"), Result.Sampled, 60);
			TestEqual(TEXT("missing"), Result.Missing, 0);
			TestTrue(TEXT("timed"), Result.Seconds >= 0.0);
		});

		It("rounds a partial cell up", [this]()
		{
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Flat(0.0f), Settings(300.0f), Grid).bOk);
			TestEqual(TEXT("SizeX 1000/300"), Grid.SizeX, 4);
			TestEqual(TEXT("SizeY 600/300"), Grid.SizeY, 2);
		});

		It("adds the open sea margin on every side", [this]()
		{
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Flat(0.0f), Settings(100.0f, 200.0f), Grid).bOk);
			TestEqual(TEXT("SizeX"), Grid.SizeX, 14);
			TestEqual(TEXT("SizeY"), Grid.SizeY, 10);
			TestTrue(TEXT("origin shifted"), Grid.OriginWorld.Equals(FVector2D(-200.0, -200.0)));
		});

		It("uses the bounds override when valid", [this]()
		{
			FTidemarkBakeSettings S = Settings(100.0f);
			S.BoundsOverride = FBox2D(FVector2D(300.0, 100.0), FVector2D(500.0, 400.0));
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Flat(0.0f), S, Grid).bOk);
			TestEqual(TEXT("SizeX"), Grid.SizeX, 2);
			TestEqual(TEXT("SizeY"), Grid.SizeY, 3);
			TestTrue(TEXT("origin"), Grid.OriginWorld.Equals(FVector2D(300.0, 100.0)));
		});

		It("samples at the centre of each cell", [this]()
		{
			FTidemarkFunctionHeightSource Ramp(Rect(), [](double X, double Y, float& Out) { Out = static_cast<float>(X + 10.0 * Y); return true; });
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Ramp, Settings(100.0f), Grid).bOk);
			TestEqual(TEXT("cell (0,0) = 50 + 500"), static_cast<int32>(Grid.GetElevationCm(0, 0)), 550);
			TestEqual(TEXT("cell (3,2) = 350 + 2500"), static_cast<int32>(Grid.GetElevationCm(3, 2)), 2850);
		});

		It("refuses a grid above the cell budget, an invalid source or a bad cell size", [this]()
		{
			FTidemarkFunctionHeightSource Huge(FBox2D(FVector2D(0.0, 0.0), FVector2D(1.0e6, 1.0e6)), [](double, double, float& Out) { Out = 0.0f; return true; });
			FTidemarkGrid Grid;
			FTidemarkBakeResult Result = BakeGrid(Huge, Settings(1.0f), Grid);
			TestFalse(TEXT("too big refused"), Result.bOk);
			TestTrue(TEXT("error names the budget"), Result.Error.Contains(TEXT("budget")));
			TestFalse(TEXT("grid left empty"), Grid.IsValid());

			FTidemarkFunctionHeightSource NoBounds(FBox2D(ForceInit), [](double, double, float& Out) { Out = 0.0f; return true; });
			Result = BakeGrid(NoBounds, Settings(100.0f), Grid);
			TestFalse(TEXT("no bounds refused"), Result.bOk);

			Result = BakeGrid(Flat(0.0f), Settings(0.0f), Grid);
			TestFalse(TEXT("zero cell refused"), Result.bOk);
		});
	});

	Describe("Flags", [this]()
	{
		It("flags Border on the outer ring only", [this]()
		{
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Flat(100.0f), Settings(100.0f), Grid).bOk);
			TestEqual(TEXT("border count 2*10 + 2*4"), CountFlag(Grid, ETidemarkCellFlag::Border), 28);
			TestTrue(TEXT("corner"), Grid.HasFlag(0, 0, ETidemarkCellFlag::Border));
			TestTrue(TEXT("edge"), Grid.HasFlag(9, 3, ETidemarkCellFlag::Border));
			TestFalse(TEXT("inside"), Grid.HasFlag(1, 1, ETidemarkCellFlag::Border));
			TestEqual(TEXT("no open sea on land"), CountFlag(Grid, ETidemarkCellFlag::OpenSea), 0);
			TestEqual(TEXT("no forbidden"), CountFlag(Grid, ETidemarkCellFlag::Forbidden), 0);
		});

		It("flags OpenSea strictly under sea level minus the open sea depth", [this]()
		{
			// West half at -1 000 cm (open sea), x in [500, 700) exactly at -150 (not open sea: strict), east at +500.
			FTidemarkFunctionHeightSource Step(Rect(), [](double X, double, float& Out)
			{
				Out = X < 500.0 ? -1000.0f : (X < 700.0 ? -150.0f : 500.0f);
				return true;
			});
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Step, Settings(100.0f), Grid).bOk);
			TestEqual(TEXT("open sea = 5 columns x 6"), CountFlag(Grid, ETidemarkCellFlag::OpenSea), 30);
			TestTrue(TEXT("(0,0) open sea"), Grid.HasFlag(0, 0, ETidemarkCellFlag::OpenSea));
			TestFalse(TEXT("(5,0) at the threshold is not open sea"), Grid.HasFlag(5, 0, ETidemarkCellFlag::OpenSea));
			TestFalse(TEXT("(9,0) land"), Grid.HasFlag(9, 0, ETidemarkCellFlag::OpenSea));
		});
	});

	Describe("Missing", [this]()
	{
		It("flags a missing cell Forbidden and fills it from the lowest sampled neighbour", [this]()
		{
			// Cell (4, 2) (centre 450, 250) is missing; its neighbours ramp with X so the lowest is column 3 = 350.
			FTidemarkFunctionHeightSource Hole(Rect(), [](double X, double Y, float& Out)
			{
				if (X > 400.0 && X < 500.0 && Y > 200.0 && Y < 300.0)
				{
					return false;
				}
				Out = static_cast<float>(X);
				return true;
			});
			FTidemarkGrid Grid;
			const FTidemarkBakeResult Result = BakeGrid(Hole, Settings(100.0f), Grid);
			TestTrue(TEXT("ok"), Result.bOk);
			TestEqual(TEXT("missing"), Result.Missing, 1);
			TestEqual(TEXT("sampled"), Result.Sampled, 59);
			TestTrue(TEXT("forbidden"), Grid.HasFlag(4, 2, ETidemarkCellFlag::Forbidden));
			TestEqual(TEXT("filled from (3, *) = 350"), static_cast<int32>(Grid.GetElevationCm(4, 2)), 350);
			TestEqual(TEXT("only one forbidden"), CountFlag(Grid, ETidemarkCellFlag::Forbidden), 1);
			TestFalse(TEXT("neighbour untouched"), Grid.HasFlag(3, 2, ETidemarkCellFlag::Forbidden));
		});

		It("gives MissingElevationCm to a missing cell without sampled neighbour, which becomes open sea", [this]()
		{
			FTidemarkFunctionHeightSource Nothing(Rect(), [](double, double, float&) { return false; });
			FTidemarkGrid Grid;
			const FTidemarkBakeResult Result = BakeGrid(Nothing, Settings(100.0f), Grid);
			TestTrue(TEXT("ok"), Result.bOk);
			TestEqual(TEXT("all missing"), Result.Missing, 60);
			TestEqual(TEXT("all forbidden"), CountFlag(Grid, ETidemarkCellFlag::Forbidden), 60);
			TestEqual(TEXT("all open sea"), CountFlag(Grid, ETidemarkCellFlag::OpenSea), 60);
			TestEqual(TEXT("value"), static_cast<int32>(Grid.GetElevationCm(2, 2)), -30000);
		});

		It("does not depend on the pass order: the margin ring reads sampled cells only", [this]()
		{
			// Margin of one cell around a flat 100 cm terrain: every margin cell touches a sampled cell or not; the second ring
			// (corners of a 2-cell margin) has no sampled neighbour and takes MissingElevationCm, never a filled neighbour.
			FTidemarkFunctionHeightSource Island(Rect(), [](double X, double Y, float& Out)
			{
				if (X < 0.0 || Y < 0.0 || X > 1000.0 || Y > 600.0)
				{
					return false;
				}
				Out = 100.0f;
				return true;
			});
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Island, Settings(100.0f, 200.0f), Grid).bOk);
			TestEqual(TEXT("ring 1 touches land: 100"), static_cast<int32>(Grid.GetElevationCm(1, 1)), 100);
			TestEqual(TEXT("ring 2 corner: missing value"), static_cast<int32>(Grid.GetElevationCm(0, 0)), -30000);
			TestEqual(TEXT("ring 2 edge: missing value"), static_cast<int32>(Grid.GetElevationCm(5, 0)), -30000);
			TestTrue(TEXT("ring 2 open sea"), Grid.HasFlag(5, 0, ETidemarkCellFlag::OpenSea));
			TestTrue(TEXT("ring 1 forbidden"), Grid.HasFlag(1, 1, ETidemarkCellFlag::Forbidden));
			TestFalse(TEXT("land not forbidden"), Grid.HasFlag(2, 2, ETidemarkCellFlag::Forbidden));
		});
	});

	Describe("Clamp", [this]()
	{
		It("saturates heights to int16 and rounds to the nearest centimetre", [this]()
		{
			FTidemarkFunctionHeightSource Extreme(Rect(), [](double X, double, float& Out)
			{
				Out = X < 300.0 ? 1.0e6f : (X < 600.0 ? -1.0e6f : 12.6f);
				return true;
			});
			FTidemarkGrid Grid;
			TestTrue(TEXT("ok"), BakeGrid(Extreme, Settings(100.0f), Grid).bOk);
			TestEqual(TEXT("max"), static_cast<int32>(Grid.GetElevationCm(0, 0)), 32767);
			TestEqual(TEXT("min"), static_cast<int32>(Grid.GetElevationCm(4, 0)), -32768);
			TestEqual(TEXT("rounded"), static_cast<int32>(Grid.GetElevationCm(8, 0)), 13);
			TestTrue(TEXT("min is open sea"), Grid.HasFlag(4, 0, ETidemarkCellFlag::OpenSea));
		});
	});

	Describe("Determinism", [this]()
	{
		It("gives the same hash for two bakes of the same source", [this]()
		{
			FTidemarkFunctionHeightSource Ramp(Rect(), [](double X, double Y, float& Out) { Out = static_cast<float>(0.3 * X - 0.7 * Y); return true; });
			FTidemarkGrid A;
			FTidemarkGrid B;
			TestTrue(TEXT("a"), BakeGrid(Ramp, Settings(50.0f, 100.0f), A).bOk);
			TestTrue(TEXT("b"), BakeGrid(Ramp, Settings(50.0f, 100.0f), B).bOk);
			TestTrue(TEXT("equal"), A == B);
			TestEqual(TEXT("hash"), A.ComputeSourceHash(), B.ComputeSourceHash());
		});
	});

	Describe("Heightmap", [this]()
	{
		It("reads a raw R16 file written on disk, normalized between MinCm and MaxCm, bilinear between pixels", [this]()
		{
			// 4 x 4 little-endian uint16: value = 1000 * (x + 4 * y), so pixel (1, 2) = 9 000.
			const FString Path = FPaths::Combine(TestRoot, TEXT("bake_4x4.r16"));
			IFileManager::Get().MakeDirectory(*TestRoot, true);
			TArray<uint8> Bytes;
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const uint16 Value = static_cast<uint16>(1000 * Index);
				Bytes.Add(static_cast<uint8>(Value & 0xFF));
				Bytes.Add(static_cast<uint8>(Value >> 8));
			}
			TestTrue(TEXT("written"), FFileHelper::SaveArrayToFile(Bytes, *Path));

			FTidemarkTextureHeightParams Params;
			Params.MinCm = 0.0f;
			Params.MaxCm = 65535.0f; // so that 1 cm = 1 raw unit
			Params.OriginWorld = FVector2D(1000.0, 2000.0);
			Params.CellSizeCm = 100.0f;
			FTidemarkTextureHeightSource Source;
			FString Error;
			TestTrue(FString::Printf(TEXT("load: %s"), *Error), Source.LoadFile(Path, Params, Error));
			TestEqual(TEXT("width"), Source.GetWidth(), 4);
			TestEqual(TEXT("height"), Source.GetHeight(), 4);
			TestEqual(TEXT("pixel (1,2)"), Source.GetPixelCm(1, 2), 9000.0f, 0.5f);

			const FBox2D Bounds = Source.GetBoundsWorld();
			TestTrue(TEXT("bounds min"), Bounds.Min.Equals(FVector2D(950.0, 1950.0)));
			TestTrue(TEXT("bounds max"), Bounds.Max.Equals(FVector2D(1350.0, 2350.0)));

			float H = 0.0f;
			TestTrue(TEXT("sample on pixel"), Source.SampleHeightCm(1100.0, 2200.0, H));
			TestEqual(TEXT("pixel value"), H, 9000.0f, 0.5f);
			TestTrue(TEXT("sample between"), Source.SampleHeightCm(1150.0, 2250.0, H));
			TestEqual(TEXT("bilinear of 9000, 10000, 13000, 14000"), H, 11500.0f, 0.5f);
			TestFalse(TEXT("outside"), Source.SampleHeightCm(0.0, 0.0, H));

			// Bake at the file's own resolution: cell centres fall on pixel centres.
			FTidemarkBakeSettings S = Settings(100.0f);
			FTidemarkGrid Grid;
			const FTidemarkBakeResult Result = Tidemark::Bake::BakeGrid(Source, S, Grid);
			TestTrue(TEXT("bake ok"), Result.bOk);
			TestEqual(TEXT("4 x 4"), Grid.SizeX * Grid.SizeY, 16);
			TestEqual(TEXT("missing"), Result.Missing, 0);
			TestEqual(TEXT("cell (1,2) = pixel"), static_cast<int32>(Grid.GetElevationCm(1, 2)), 9000);
			TestEqual(TEXT("cell (3,3) = pixel"), static_cast<int32>(Grid.GetElevationCm(3, 3)), 15000);
			IFileManager::Get().Delete(*Path);
		});

		It("flips rows with bFlipY and refuses a non-square raw file without dimensions", [this]()
		{
			FTidemarkTextureHeightParams Params;
			Params.OriginWorld = FVector2D::ZeroVector;
			Params.CellSizeCm = 100.0f;
			Params.bFlipY = true;
			FTidemarkTextureHeightSource Source;
			FString Error;
			TestTrue(TEXT("set"), Source.SetHeightsCm({ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }, 3, 2, Params, Error));
			TestEqual(TEXT("row 0 is the last file row"), Source.GetPixelCm(0, 0), 4.0f);
			TestEqual(TEXT("row 1 is the first file row"), Source.GetPixelCm(2, 1), 3.0f);

			const FString Path = FPaths::Combine(TestRoot, TEXT("bake_6px.r16"));
			IFileManager::Get().MakeDirectory(*TestRoot, true);
			TArray<uint8> Bytes;
			Bytes.Init(0, 12);
			TestTrue(TEXT("written"), FFileHelper::SaveArrayToFile(Bytes, *Path));
			FTidemarkTextureHeightParams Square;
			TestFalse(TEXT("6 pixels is not a square"), Source.LoadFile(Path, Square, Error));
			TestTrue(TEXT("error names RawWidth"), Error.Contains(TEXT("RawWidth")));
			Square.RawWidth = 3;
			TestTrue(TEXT("3 x deduced 2"), Source.LoadFile(Path, Square, Error));
			TestEqual(TEXT("height deduced"), Source.GetHeight(), 2);
			IFileManager::Get().Delete(*Path);
		});
	});

	Describe("Credits", [this]()
	{
		It("refuses a bake without complete credits before touching any package", [this]()
		{
			FString Error;
			FTidemarkBakeResult Result;
			TArray<FTidemarkCredit> None;
			TestNull(TEXT("no credits"), UTidemarkImportLibrary::BakeFromSource(Flat(0.0f), Settings(100.0f), TEXT("/Game/Tidemark/Tests/TA_Refused"), None, Error, Result));
			TestTrue(TEXT("error names credits"), Error.Contains(TEXT("credits")));
			TestFalse(TEXT("result not ok"), Result.bOk);

			FTidemarkCredit Partial;
			Partial.Producer = TEXT("spec");
			TArray<FTidemarkCredit> Incomplete = { Partial };
			TestFalse(TEXT("incomplete"), UTidemarkImportLibrary::ValidateCredits(Incomplete, Error));
			TestTrue(TEXT("error says incomplete"), Error.Contains(TEXT("incomplete")));

			Partial.License = TEXT("CC0 1.0");
			Partial.Url = TEXT("https://example.invalid");
			Partial.Date = TEXT("2026-10-23");
			TArray<FTidemarkCredit> Complete = { Partial };
			TestTrue(TEXT("complete"), UTidemarkImportLibrary::ValidateCredits(Complete, Error));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
