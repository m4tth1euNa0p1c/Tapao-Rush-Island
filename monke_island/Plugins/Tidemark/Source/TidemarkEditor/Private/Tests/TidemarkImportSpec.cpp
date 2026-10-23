// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Grid/TidemarkTerrainAsset.h"
#include "Tidemark/Rules/TidemarkRules.h"
#include "Tidemark/Solver/TidemarkHash.h"
#include "Tidemark/Solver/TidemarkSchedule.h"
#include "Tidemark/Solver/TidemarkSeed.h"
#include "Tidemark/Solver/TidemarkSolver.h"
#include "TidemarkEditor/Import/TextureHeightSource.h"
#include "TidemarkEditor/Import/TidemarkBaker.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"
#include "TidemarkEditor/TidemarkEditorModule.h"

#if WITH_AUTOMATION_TESTS

/**
 * Tidemark.Import.Scenario (TM-11, Tidemark cahier section 10 week 2, CONVENTIONS.md sections 5 and 6).
 *
 * The same scenario (seed, grid, schedule) must give the same state hash whatever the import path of the grid.
 * Two paths are automated here, with no world: the same synthetic relief is sampled (a) by FTidemarkFunctionHeightSource
 * (a lambda) and (b) written as a raw R16 heightmap under Saved/Tidemark/Tests/ and read back by FTidemarkTextureHeightSource.
 * Both are baked by Tidemark::Bake::BakeGrid with the same FTidemarkBakeSettings, then simulated by FTidemarkSolver with
 * UTidemarkRules::DefaultSchedule (650 cm ramp over 1 200 steps, exponent 1.6) for 60 s at 0.05 s = 1 200 steps.
 *
 * Relief: a pure function of the cell (floor(x / cell), floor(y / cell)), piecewise constant per cell so that the exact
 * sample position inside a cell (cell centre for the baker, pixel centre for the file) cannot change the value. Its
 * parameters (hills, bowl) come from FTidemarkRng seeded by FTidemarkSeed::FromName("ImportScenario"), in fixed draw order.
 * Heights are rounded to whole centimetres before they are returned.
 *
 * R16 quantization: the file is decoded as Normalized with MinCm = -32768 and MaxCm = 32767, so a raw value R decodes to
 * -32768 + (R / 65535) * 65535 cm, which is R - 32768 up to about 0.004 cm of float rounding; the baker rounds every
 * height to the nearest centimetre (ToInt16Cm), so an integer synthetic height comes back bit-identical from the file.
 * A pixel is written as R = height + 32768, which covers the whole int16 range of FTidemarkGrid::ElevationCm.
 *
 * Alignment: the texture's OriginWorld is the centre of the first bake cell (half a cell from the grid origin) and its
 * pixel pitch is the bake cell, so every cell centre falls exactly on a pixel centre (bilinear weights 0, exact value)
 * and the texture bounds [Origin - cell/2, Origin + (N - 1) cell + cell/2] equal the function bounds [0, N cell].
 *
 * Landscape (manual recipe, no spec: a bake of a Landscape needs an editor world, Plugins/Tidemark/Docs/README.md):
 *  1. Editor lock taken, map opened with its Landscape (World Partition: load the whole region, unloaded proxies give
 *     Forbidden cells).
 *  2. Console: tidemark.BakeLandscape /Game/Tidemark/Terrains/TA_<Map> 600 0 60000, then
 *     tidemark.ExportTmk /Game/Tidemark/Terrains/TA_<Map> <dir_landscape>.
 *  3. Export the heightmap of the same Landscape (Landscape mode > Manage > Export heightmap, .r16 or 16-bit .png),
 *     note its world origin, pitch and the Z scale of the actor, then
 *     UTidemarkImportLibrary::BakeFromHeightmapFile with the same MinCm / MaxCm (actor Z scale: -256 m .. +256 m at
 *     scale 100), origin, pitch, cell 600, sea level 0, margin 60000, and tidemark.ExportTmk to <dir_heightmap>.
 *  4. Compare: python3 -I Plugins/Tidemark/Tools/tmk.py info <dir_landscape> and ... info <dir_heightmap> give the
 *     same source_hash; then tidemark.Replay <seed> on each asset gives the same final hash (tidemark.Hash).
 *     A different source_hash with the same dimensions means a different elevation: diff the two elevation.bin with
 *     numpy to find the cells (Landscape component edge or a proxy not loaded).
 *
 * Run with: ./scripts/ue.ps1 test -Filter Tidemark.Import.Scenario
 */
namespace TidemarkImportScenario
{
	/** Cells of the synthetic grid along X and Y. */
	constexpr int32 GridX = 64;
	constexpr int32 GridY = 48;
	/** Bake cell and pixel pitch (cm): UTidemarkRules::DefaultSimCellSizeCm. */
	constexpr float CellSizeCm = Tidemark::Rules::DefaultSimCellSizeCm;
	/** Sea level at rest and open sea depth given to the bake and to FillSolver (cm). */
	constexpr float SeaLevelCm = 0.0f;
	constexpr float OpenSeaDepthCm = Tidemark::Rules::DefaultOpenSeaDepthCm;
	/** Clock of the scenario (s): 60 s at 0.05 s = 1 200 steps, the length of the default ramp. */
	constexpr float ScenarioSeconds = 60.0f;
	constexpr uint32 ExpectedStepCount = Tidemark::Rules::DefaultScheduleDurationSteps;
	/** Decoding of the R16 file: raw R maps to R - 32768 cm. */
	constexpr float R16MinCm = -32768.0f;
	constexpr float R16MaxCm = 32767.0f;
	constexpr int32 R16Offset = 32768;
	/** Beach slope of the relief: -600 cm on the west edge (open sea) to +900 cm on the east edge. */
	constexpr int32 SlopeWestCm = -600;
	constexpr int32 SlopeEastCm = 900;
	constexpr int32 HillCount = 10;

	/** One hill (HeightCm > 0) or bowl (HeightCm < 0) with a quadratic falloff over Radius cells. */
	struct FHill
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Radius = 1;
		float HeightCm = 0.0f;
	};

	/**
	 * Synthetic relief generated from a seed only. HeightCmAtCell is a pure function of the cell coordinates:
	 * integer slope plus the hills, rounded to the nearest centimetre and clamped to int16.
	 */
	struct FRelief
	{
		TArray<FHill> Hills;

		static FRelief FromSeed(FTidemarkSeed Seed)
		{
			FRelief Relief;
			FTidemarkRng Rng(Seed.Derive(FName(TEXT("Terrain"))));
			Relief.Hills.Reserve(HillCount + 1);
			for (int32 Index = 0; Index < HillCount; ++Index)
			{
				FHill Hill;
				Hill.X = Rng.NextRange(10, GridX - 4);
				Hill.Y = Rng.NextRange(3, GridY - 4);
				Hill.Radius = Rng.NextRange(3, 10);
				Hill.HeightCm = 100.0f + 600.0f * Rng.NextFloat();
				Relief.Hills.Add(Hill);
			}
			FHill Bowl;
			Bowl.X = Rng.NextRange(20, 50);
			Bowl.Y = Rng.NextRange(10, GridY - 10);
			Bowl.Radius = Rng.NextRange(5, 9);
			Bowl.HeightCm = -(100.0f + 300.0f * Rng.NextFloat());
			Relief.Hills.Add(Bowl);
			return Relief;
		}

		/** Height (whole cm) of cell (CX, CY), coordinates clamped to the grid. */
		int32 HeightCmAtCell(int32 CX, int32 CY) const
		{
			CX = FMath::Clamp(CX, 0, GridX - 1);
			CY = FMath::Clamp(CY, 0, GridY - 1);
			// Integer slope: exact on every machine.
			const int32 SlopeCm = SlopeWestCm + ((SlopeEastCm - SlopeWestCm) * CX) / (GridX - 1);
			float Bump = 0.0f;
			for (const FHill& Hill : Hills)
			{
				const float DX = static_cast<float>(CX - Hill.X);
				const float DY = static_cast<float>(CY - Hill.Y);
				const float RadiusSquared = static_cast<float>(Hill.Radius * Hill.Radius);
				const float W = FMath::Max(0.0f, 1.0f - (DX * DX + DY * DY) / RadiusSquared);
				Bump += Hill.HeightCm * (W * W);
			}
			const int32 HeightCm = SlopeCm + FMath::RoundToInt(Bump);
			return FMath::Clamp(HeightCm, static_cast<int32>(TNumericLimits<int16>::Min()), static_cast<int32>(TNumericLimits<int16>::Max()));
		}

		/** Height at a world position: the value of the cell containing it (floor), whatever the position inside the cell. */
		float HeightCmAtWorld(double WorldX, double WorldY) const
		{
			const int32 CX = static_cast<int32>(FMath::FloorToDouble(WorldX / static_cast<double>(CellSizeCm)));
			const int32 CY = static_cast<int32>(FMath::FloorToDouble(WorldY / static_cast<double>(CellSizeCm)));
			return static_cast<float>(HeightCmAtCell(CX, CY));
		}
	};
}

BEGIN_DEFINE_SPEC(FTidemarkImportScenarioSpec, "Tidemark.Import.Scenario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FString TestRoot;

	static FTidemarkSeed RootSeed()
	{
		return FTidemarkSeed::FromName(FName(TEXT("ImportScenario")));
	}

	/** World rectangle covered by the synthetic grid: [0, GridX cell] x [0, GridY cell]. */
	static FBox2D ReliefBounds()
	{
		using namespace TidemarkImportScenario;
		return FBox2D(FVector2D(0.0, 0.0), FVector2D(static_cast<double>(GridX) * CellSizeCm, static_cast<double>(GridY) * CellSizeCm));
	}

	/** The single bake settings of every case: cell, sea level and open sea depth of the rules, no margin. */
	static FTidemarkBakeSettings ScenarioSettings()
	{
		using namespace TidemarkImportScenario;
		FTidemarkBakeSettings Settings;
		Settings.CellSizeCm = CellSizeCm;
		Settings.SeaLevelCm = SeaLevelCm;
		Settings.OpenSeaDepthCm = OpenSeaDepthCm;
		Settings.OpenSeaMarginCm = 0.0f;
		Settings.MissingElevationCm = -30000.0f;
		return Settings;
	}

	/** Path (a): the relief as a function source. The relief is copied into the lambda. */
	static FTidemarkFunctionHeightSource MakeFunctionSource(const TidemarkImportScenario::FRelief& Relief, const FString& Description)
	{
		return FTidemarkFunctionHeightSource(ReliefBounds(), [Relief](double X, double Y, float& Out)
		{
			Out = Relief.HeightCmAtWorld(X, Y);
			return true;
		}, Description);
	}

	/** Path (b): writes the relief as a raw little-endian R16 file, one pixel per cell, row 0 = lowest Y. */
	static bool WriteReliefR16(const TidemarkImportScenario::FRelief& Relief, const FString& Path)
	{
		using namespace TidemarkImportScenario;
		TArray<uint8> Bytes;
		Bytes.Reserve(GridX * GridY * 2);
		for (int32 Y = 0; Y < GridY; ++Y)
		{
			for (int32 X = 0; X < GridX; ++X)
			{
				const int32 Raw = Relief.HeightCmAtCell(X, Y) + R16Offset;
				check(Raw >= 0 && Raw <= 65535);
				const uint16 Value = static_cast<uint16>(Raw);
				Bytes.Add(static_cast<uint8>(Value & 0xFF));
				Bytes.Add(static_cast<uint8>(Value >> 8));
			}
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	/** Decoding parameters of the R16 file: pixel centres on the bake cell centres, R - 32768 cm. */
	static FTidemarkTextureHeightParams ReliefTextureParams()
	{
		using namespace TidemarkImportScenario;
		FTidemarkTextureHeightParams Params;
		Params.MinCm = R16MinCm;
		Params.MaxCm = R16MaxCm;
		Params.OriginWorld = FVector2D(0.5 * CellSizeCm, 0.5 * CellSizeCm);
		Params.CellSizeCm = CellSizeCm;
		Params.Encoding = ETidemarkHeightEncoding::Normalized;
		Params.bFlipY = false;
		Params.RawWidth = GridX;
		Params.RawHeight = GridY;
		return Params;
	}

	/** Bakes a source with the scenario settings and asserts the bake is complete (no missing cell, expected size). */
	bool BakeScenario(const ITidemarkHeightSource& Source, const TCHAR* Label, FTidemarkGrid& OutGrid)
	{
		using namespace TidemarkImportScenario;
		const FTidemarkBakeResult Result = Tidemark::Bake::BakeGrid(Source, ScenarioSettings(), OutGrid);
		TestTrue(FString::Printf(TEXT("%s bake ok: %s"), Label, *Result.Error), Result.bOk);
		TestEqual(FString::Printf(TEXT("%s SizeX"), Label), OutGrid.SizeX, GridX);
		TestEqual(FString::Printf(TEXT("%s SizeY"), Label), OutGrid.SizeY, GridY);
		TestEqual(FString::Printf(TEXT("%s missing cells"), Label), Result.Missing, 0);
		TestEqual(FString::Printf(TEXT("%s sampled cells"), Label), Result.Sampled, GridX * GridY);
		return Result.bOk && OutGrid.SizeX == GridX && OutGrid.SizeY == GridY && Result.Missing == 0;
	}

	/** State hash of the solver: depths converted to centimetres, 32 x 32 blocks, 1 mm quantum (as Tidemark.Determinism). */
	static FTidemarkStateHash HashSolver(const FTidemarkSolver& Solver, uint32 StepIndex)
	{
		TArray<float> DepthCm;
		DepthCm.SetNumUninitialized(Solver.Depth.Num());
		for (int32 I = 0; I < Solver.Depth.Num(); ++I)
		{
			DepthCm[I] = Solver.Depth[I] * 100.0f;
		}
		return Tidemark::Hash::HashDepth(DepthCm, Solver.GetSizeX(), Solver.GetSizeY(), StepIndex);
	}

	/**
	 * The scenario: FillSolver from the grid, then the default schedule of UTidemarkRules for ScenarioSeconds at the rules'
	 * step. The level at step k is Tidemark::Schedule::LevelCm(DefaultSchedule, k) / 100 m; Gravity and FluxDamping come
	 * from the rules. Returns the final state hash; OutSolver keeps the final state.
	 */
	FTidemarkStateHash RunScenario(const FTidemarkGrid& Grid, const TCHAR* Label, FTidemarkSolver& OutSolver)
	{
		using namespace TidemarkImportScenario;
		const UTidemarkRules* Rules = GetDefault<UTidemarkRules>();
		const uint32 StepCount = Rules->StepsFromSeconds(ScenarioSeconds);
		TestEqual(FString::Printf(TEXT("%s: %.0f s at %.3f s per step"), Label, ScenarioSeconds, Rules->StepSeconds), static_cast<int32>(StepCount), static_cast<int32>(ExpectedStepCount));

		Grid.FillSolver(OutSolver, SeaLevelCm, OpenSeaDepthCm);
		TestTrue(FString::Printf(TEXT("%s solver valid"), Label), OutSolver.IsValid());

		const double Start = FPlatformTime::Seconds(); // wall clock for the log only, never fed to the simulation
		for (uint32 Step = 1; Step <= StepCount; ++Step)
		{
			const float LevelM = Tidemark::Schedule::LevelCm(Rules->DefaultSchedule, Step) / 100.0f;
			OutSolver.Step(Rules->StepSeconds, LevelM, Rules->Gravity, Rules->FluxDamping);
		}
		const double WallSeconds = FPlatformTime::Seconds() - Start;

		bool bAllFinite = true;
		for (const float Depth : OutSolver.Depth)
		{
			bAllFinite = bAllFinite && FMath::IsFinite(Depth);
		}
		// A NaN hashes to a sentinel on both paths and would hide a divergence: refuse it explicitly.
		TestTrue(FString::Printf(TEXT("%s has only finite depths"), Label), bAllFinite);

		const FTidemarkStateHash Final = HashSolver(OutSolver, StepCount);
		UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark.Import.Scenario %s: %u steps in %.3f s, final 0x%016llx, free volume %.1f m3"),
			Label, StepCount, WallSeconds, Final.Combined, OutSolver.FreeVolume());
		return Final;
	}

	/** Asserts two state hashes are identical; on mismatch the message lists every differing block with both hashes. */
	bool ExpectSameHash(const FString& What, const FTidemarkStateHash& A, const FTidemarkStateHash& B)
	{
		const bool bSame = A.Combined == B.Combined && A.Blocks == B.Blocks && A.StepIndex == B.StepIndex;
		FString Message = FString::Printf(TEXT("%s: step %u combined 0x%016llx vs step %u combined 0x%016llx"),
			*What, A.StepIndex, A.Combined, B.StepIndex, B.Combined);
		if (!bSame)
		{
			const TArray<FIntPoint> Diff = Tidemark::Hash::DiffBlocks(A, B);
			Message += FString::Printf(TEXT(" - %d differing block(s)"), Diff.Num());
			for (const FIntPoint& Block : Diff)
			{
				const int32 Index = Block.Y * A.BlocksX + Block.X;
				const uint64 HashA = A.Blocks.IsValidIndex(Index) ? A.Blocks[Index] : 0;
				const uint64 HashB = B.Blocks.IsValidIndex(Index) ? B.Blocks[Index] : 0;
				Message += FString::Printf(TEXT("\n  block (%d, %d): 0x%016llx vs 0x%016llx"), Block.X, Block.Y, HashA, HashB);
			}
		}
		TestTrue(Message, bSame);
		return bSame;
	}

	void ExpectSameU64(const FString& What, uint64 A, uint64 B)
	{
		TestTrue(FString::Printf(TEXT("%s: 0x%016llx vs 0x%016llx"), *What, A, B), A == B);
	}

	void ExpectNotU64(const FString& What, uint64 A, uint64 B)
	{
		TestTrue(FString::Printf(TEXT("%s: both 0x%016llx"), *What, A), A != B);
	}

	/** Asserts two grids are equal; on mismatch the message names the first differing cell with both values. */
	void ExpectSameGrid(const FString& What, const FTidemarkGrid& A, const FTidemarkGrid& B)
	{
		if (A == B)
		{
			TestTrue(What, true);
			return;
		}
		FString Message = FString::Printf(TEXT("%s: grids differ (%d x %d at %.1f cm, origin %s vs %d x %d at %.1f cm, origin %s)"),
			*What, A.SizeX, A.SizeY, A.CellSizeCm, *A.OriginWorld.ToString(), B.SizeX, B.SizeY, B.CellSizeCm, *B.OriginWorld.ToString());
		if (A.SizeX == B.SizeX && A.SizeY == B.SizeY)
		{
			int32 Differing = 0;
			for (int32 Y = 0; Y < A.SizeY; ++Y)
			{
				for (int32 X = 0; X < A.SizeX; ++X)
				{
					const int32 Index = A.Index(X, Y);
					if (A.ElevationCm[Index] != B.ElevationCm[Index] || A.Flags[Index] != B.Flags[Index])
					{
						if (Differing == 0)
						{
							Message += FString::Printf(TEXT("; first at (%d, %d): %d cm flags 0x%02x vs %d cm flags 0x%02x"),
								X, Y, A.ElevationCm[Index], A.Flags[Index], B.ElevationCm[Index], B.Flags[Index]);
						}
						++Differing;
					}
				}
			}
			Message += FString::Printf(TEXT("; %d differing cell(s)"), Differing);
		}
		TestTrue(Message, false);
	}

	/** Cells whose solver Fixed disagrees with the baked OpenSea | Border flags (both use the same strict threshold). */
	static int32 CountFixedMismatches(const FTidemarkGrid& Grid, const FTidemarkSolver& Solver, FString& OutFirst)
	{
		constexpr uint8 FixedMask = static_cast<uint8>(ETidemarkCellFlag::OpenSea) | static_cast<uint8>(ETidemarkCellFlag::Border);
		int32 Mismatches = 0;
		for (int32 Y = 0; Y < Grid.SizeY; ++Y)
		{
			for (int32 X = 0; X < Grid.SizeX; ++X)
			{
				const int32 Index = Grid.Index(X, Y);
				const bool bFlagged = (Grid.Flags[Index] & FixedMask) != 0;
				const bool bFixed = Solver.Fixed[Index] != 0;
				if (bFlagged != bFixed)
				{
					if (Mismatches == 0)
					{
						OutFirst = FString::Printf(TEXT("(%d, %d): %d cm, flags 0x%02x, Fixed %d"), X, Y, Grid.ElevationCm[Index], Grid.Flags[Index], Solver.Fixed[Index]);
					}
					++Mismatches;
				}
			}
		}
		return Mismatches;
	}

	static int32 CountFlag(const FTidemarkGrid& Grid, ETidemarkCellFlag Flag)
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

	static FTidemarkCredit MakeCredit(const TCHAR* Producer, const TCHAR* License, const TCHAR* Url, const TCHAR* Date)
	{
		FTidemarkCredit Credit;
		Credit.Producer = Producer;
		Credit.License = License;
		Credit.Url = Url;
		Credit.Date = Date;
		return Credit;
	}

END_DEFINE_SPEC(FTidemarkImportScenarioSpec)

void FTidemarkImportScenarioSpec::Define()
{
	using namespace TidemarkImportScenario;

	BeforeEach([this]()
	{
		TestRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tidemark"), TEXT("Tests"));
	});

	It("SameScenarioSameHash_TextureVsFunction", [this]()
	{
		const FTidemarkSeed Seed = RootSeed().Derive(FName(TEXT("A")));
		const FRelief Relief = FRelief::FromSeed(Seed);

		// (a) Function path.
		const FTidemarkFunctionHeightSource FunctionSource = MakeFunctionSource(Relief, TEXT("synthetic relief (function)"));
		FTidemarkGrid FunctionGrid;
		const bool bFunctionBaked = BakeScenario(FunctionSource, TEXT("function"), FunctionGrid);

		// (b) Heightmap path: R16 on disk, read back with the same origin, pitch and MinCm / MaxCm.
		const FString Path = FPaths::Combine(TestRoot, TEXT("import_scenario_64x48.r16"));
		TestTrue(TEXT("R16 written"), WriteReliefR16(Relief, Path));
		FTidemarkTextureHeightSource TextureSource;
		FString Error;
		const bool bLoaded = TextureSource.LoadFile(Path, ReliefTextureParams(), Error);
		TestTrue(FString::Printf(TEXT("R16 loaded: %s"), *Error), bLoaded);
		TestEqual(TEXT("R16 width"), TextureSource.GetWidth(), GridX);
		TestEqual(TEXT("R16 height"), TextureSource.GetHeight(), GridY);
		TestTrue(TEXT("texture bounds equal the function bounds"),
			TextureSource.GetBoundsWorld().Min.Equals(ReliefBounds().Min) && TextureSource.GetBoundsWorld().Max.Equals(ReliefBounds().Max));
		// Quantization check on one pixel: the decoded value is the synthetic integer up to float rounding (about 0.01 cm,
		// far under the 0.5 cm the baker's rounding absorbs).
		TestEqual(TEXT("pixel (5, 7) decodes to the synthetic height"), TextureSource.GetPixelCm(5, 7), static_cast<float>(Relief.HeightCmAtCell(5, 7)), 0.05f);
		FTidemarkGrid TextureGrid;
		const bool bTextureBaked = bLoaded && BakeScenario(TextureSource, TEXT("heightmap"), TextureGrid);
		IFileManager::Get().Delete(*Path);
		if (!bFunctionBaked || !bTextureBaked)
		{
			return;
		}

		// Same grid, same source hash: the geometry does not depend on the import path.
		ExpectSameGrid(TEXT("function grid == heightmap grid"), FunctionGrid, TextureGrid);
		ExpectSameU64(TEXT("source hash function vs heightmap"), FunctionGrid.ComputeSourceHash(), TextureGrid.ComputeSourceHash());
		TestTrue(TEXT("the relief has open sea cells"), CountFlag(FunctionGrid, ETidemarkCellFlag::OpenSea) > 0);
		TestTrue(TEXT("the relief has free cells"), CountFlag(FunctionGrid, ETidemarkCellFlag::OpenSea) + CountFlag(FunctionGrid, ETidemarkCellFlag::Border) < FunctionGrid.Num());
		UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark.Import.Scenario seed 0x%016llx: grid %d x %d, source hash %s, %d open sea, %d border"),
			Seed.Value, FunctionGrid.SizeX, FunctionGrid.SizeY, *FTidemarkGrid::HashToHex(FunctionGrid.ComputeSourceHash()),
			CountFlag(FunctionGrid, ETidemarkCellFlag::OpenSea), CountFlag(FunctionGrid, ETidemarkCellFlag::Border));

		// Same scenario on both grids: same final state hash.
		FTidemarkSolver FunctionSolver;
		FTidemarkSolver TextureSolver;
		const FTidemarkStateHash FunctionFinal = RunScenario(FunctionGrid, TEXT("function"), FunctionSolver);
		const FTidemarkStateHash TextureFinal = RunScenario(TextureGrid, TEXT("heightmap"), TextureSolver);
		ExpectSameHash(TEXT("final state hash function vs heightmap"), FunctionFinal, TextureFinal);
		TestTrue(TEXT("the sea reached free cells (hash is not that of a dry grid)"), FunctionSolver.FreeVolume() > 0.0);
	});

	It("DifferentTerrainDifferentOrder", [this]()
	{
		const FTidemarkSeed SeedA = RootSeed().Derive(FName(TEXT("A")));
		const FTidemarkSeed SeedB = RootSeed().Derive(FName(TEXT("B")));
		ExpectNotU64(TEXT("the two seeds differ"), SeedA.Value, SeedB.Value);
		const FRelief ReliefA = FRelief::FromSeed(SeedA);
		const FRelief ReliefB = FRelief::FromSeed(SeedB);

		FTidemarkGrid GridA;
		FTidemarkGrid GridB;
		const bool bBakedA = BakeScenario(MakeFunctionSource(ReliefA, TEXT("relief A")), TEXT("relief A"), GridA);
		const bool bBakedB = BakeScenario(MakeFunctionSource(ReliefB, TEXT("relief B")), TEXT("relief B"), GridB);
		if (!bBakedA || !bBakedB)
		{
			return;
		}
		ExpectNotU64(TEXT("source hash A vs B"), GridA.ComputeSourceHash(), GridB.ComputeSourceHash());
		TestTrue(TEXT("grids differ"), GridA != GridB);

		FTidemarkSolver SolverA;
		FTidemarkSolver SolverB;
		const FTidemarkStateHash FinalA = RunScenario(GridA, TEXT("relief A"), SolverA);
		const FTidemarkStateHash FinalB = RunScenario(GridB, TEXT("relief B"), SolverB);
		ExpectNotU64(TEXT("final state hash A vs B"), FinalA.Combined, FinalB.Combined);
		TestTrue(TEXT("at least one block differs"), Tidemark::Hash::DiffBlocks(FinalA, FinalB).Num() > 0);

		// Guard: FillSolver + SetupSea fix exactly the cells flagged OpenSea | Border (same strict threshold on both sides),
		// and a fixed cell holds the sea level.
		FString First;
		const int32 MismatchesA = CountFixedMismatches(GridA, SolverA, First);
		TestEqual(FString::Printf(TEXT("relief A: Fixed cells match OpenSea | Border flags (first mismatch %s)"), *First), MismatchesA, 0);
		const int32 MismatchesB = CountFixedMismatches(GridB, SolverB, First);
		TestEqual(FString::Printf(TEXT("relief B: Fixed cells match OpenSea | Border flags (first mismatch %s)"), *First), MismatchesB, 0);
		TestTrue(TEXT("relief A: (0, 0) is Border and Fixed"), GridA.HasFlag(0, 0, ETidemarkCellFlag::Border) && SolverA.Fixed[SolverA.Index(0, 0)] != 0);
		TestTrue(TEXT("relief A: west column is OpenSea (slope -600 cm)"), GridA.HasFlag(0, GridY / 2, ETidemarkCellFlag::OpenSea));
		TestEqual(TEXT("relief A: open sea cell holds the final sea level"),
			SolverA.Surface(SolverA.Index(0, GridY / 2)), Tidemark::Schedule::LevelCm(GetDefault<UTidemarkRules>()->DefaultSchedule, ExpectedStepCount) / 100.0f, 1.0e-3f);
	});

	It("GridHashIndependentOfSourceDescription", [this]()
	{
		const FRelief Relief = FRelief::FromSeed(RootSeed().Derive(FName(TEXT("A"))));
		FTidemarkGrid GridA;
		FTidemarkGrid GridB;
		const bool bBakedA = BakeScenario(MakeFunctionSource(Relief, TEXT("described as A")), TEXT("description A"), GridA);
		const bool bBakedB = BakeScenario(MakeFunctionSource(Relief, TEXT("a completely different description, B")), TEXT("description B"), GridB);
		if (!bBakedA || !bBakedB)
		{
			return;
		}
		ExpectSameGrid(TEXT("grid with description A == grid with description B"), GridA, GridB);
		ExpectSameU64(TEXT("source hash with description A vs B"), GridA.ComputeSourceHash(), GridB.ComputeSourceHash());

		// Credits live on the asset, not in the grid: two assets with different credits carry the same SourceHashHex.
		UTidemarkTerrainAsset* AssetA = NewObject<UTidemarkTerrainAsset>(GetTransientPackage(), NAME_None, RF_Transient);
		UTidemarkTerrainAsset* AssetB = NewObject<UTidemarkTerrainAsset>(GetTransientPackage(), NAME_None, RF_Transient);
		AssetA->Grid = GridA;
		AssetB->Grid = GridB;
		AssetA->Credits = { MakeCredit(TEXT("spec"), TEXT("CC0 1.0"), TEXT("https://example.invalid/a"), TEXT("2026-10-23")) };
		AssetB->Credits = {
			MakeCredit(TEXT("another producer"), TEXT("Licence Ouverte Etalab 2.0"), TEXT("https://example.invalid/b"), TEXT("2026-01-01")),
			MakeCredit(TEXT("a second source"), TEXT("CC-BY 4.0"), TEXT("https://example.invalid/c"), TEXT("2025-12-31"))
		};
		AssetA->RefreshSourceHash();
		AssetB->RefreshSourceHash();
		TestTrue(TEXT("asset A credits complete"), AssetA->HasCompleteCredits());
		TestTrue(TEXT("asset B credits complete"), AssetB->HasCompleteCredits());
		TestFalse(TEXT("asset A not stale"), AssetA->IsStale());
		TestFalse(TEXT("asset B not stale"), AssetB->IsStale());
		TestEqual(TEXT("SourceHashHex with different credits"), AssetA->SourceHashHex, AssetB->SourceHashHex);
		TestEqual(TEXT("SourceHashHex is the grid hash"), AssetA->SourceHashHex, FTidemarkGrid::HashToHex(GridA.ComputeSourceHash()));
	});

	It("BakeIsDeterministic", [this]()
	{
		const FRelief Relief = FRelief::FromSeed(RootSeed().Derive(FName(TEXT("A"))));
		const FTidemarkFunctionHeightSource Source = MakeFunctionSource(Relief, TEXT("synthetic relief"));

		// Two consecutive bakes of the same function source.
		FTidemarkGrid First;
		FTidemarkGrid Second;
		const bool bBakedFirst = BakeScenario(Source, TEXT("first bake"), First);
		const bool bBakedSecond = BakeScenario(Source, TEXT("second bake"), Second);
		if (!bBakedFirst || !bBakedSecond)
		{
			return;
		}
		ExpectSameGrid(TEXT("first bake == second bake (function)"), First, Second);
		ExpectSameU64(TEXT("source hash first vs second bake (function)"), First.ComputeSourceHash(), Second.ComputeSourceHash());

		// Two consecutive loads and bakes of the same R16 file.
		const FString Path = FPaths::Combine(TestRoot, TEXT("import_scenario_determinism.r16"));
		TestTrue(TEXT("R16 written"), WriteReliefR16(Relief, Path));
		FTidemarkTextureHeightSource TextureFirst;
		FTidemarkTextureHeightSource TextureSecond;
		FString Error;
		const bool bLoadedFirst = TextureFirst.LoadFile(Path, ReliefTextureParams(), Error);
		TestTrue(FString::Printf(TEXT("first load: %s"), *Error), bLoadedFirst);
		const bool bLoadedSecond = TextureSecond.LoadFile(Path, ReliefTextureParams(), Error);
		TestTrue(FString::Printf(TEXT("second load: %s"), *Error), bLoadedSecond);
		IFileManager::Get().Delete(*Path);
		if (!bLoadedFirst || !bLoadedSecond)
		{
			return;
		}
		FTidemarkGrid TextureGridFirst;
		FTidemarkGrid TextureGridSecond;
		const bool bTextureFirst = BakeScenario(TextureFirst, TEXT("first heightmap bake"), TextureGridFirst);
		const bool bTextureSecond = BakeScenario(TextureSecond, TEXT("second heightmap bake"), TextureGridSecond);
		if (!bTextureFirst || !bTextureSecond)
		{
			return;
		}
		ExpectSameGrid(TEXT("first bake == second bake (heightmap)"), TextureGridFirst, TextureGridSecond);
		ExpectSameU64(TEXT("source hash first vs second bake (heightmap)"), TextureGridFirst.ComputeSourceHash(), TextureGridSecond.ComputeSourceHash());
		ExpectSameU64(TEXT("source hash heightmap vs function"), TextureGridFirst.ComputeSourceHash(), First.ComputeSourceHash());
	});
}

#endif // WITH_AUTOMATION_TESTS
