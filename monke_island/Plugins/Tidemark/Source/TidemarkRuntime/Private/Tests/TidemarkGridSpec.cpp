// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Grid/TidemarkTerrainAsset.h"
#include "Tidemark/Solver/TidemarkSolver.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter Tidemark.Grid
BEGIN_DEFINE_SPEC(FTidemarkGridSpec, "Tidemark.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	/** SizeX * SizeY grid whose cell (X, Y) holds Base + 100 * X + 1000 * Y cm, cell 400 cm, origin (0, 0). */
	static FTidemarkGrid MakeGrid(int32 SizeX, int32 SizeY, int16 Base = 0, const FVector2D& Origin = FVector2D::ZeroVector)
	{
		FTidemarkGrid Grid;
		Grid.Init(SizeX, SizeY, 400.0f, Origin);
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				Grid.ElevationCm[Grid.Index(X, Y)] = static_cast<int16>(Base + 100 * X + 1000 * Y);
			}
		}
		return Grid;
	}
	void ExpectU64(const TCHAR* What, uint64 Actual, uint64 Expected)
	{
		TestTrue(FString::Printf(TEXT("%s: got 0x%016llx, expected 0x%016llx"), What, Actual, Expected), Actual == Expected);
	}
END_DEFINE_SPEC(FTidemarkGridSpec)

void FTidemarkGridSpec::Define()
{
	Describe("Index", [this]()
	{
		It("is row-major with X fastest", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(5, 3);
			TestEqual(TEXT("(0,0)"), Grid.Index(0, 0), 0);
			TestEqual(TEXT("(4,0)"), Grid.Index(4, 0), 4);
			TestEqual(TEXT("(0,1)"), Grid.Index(0, 1), 5);
			TestEqual(TEXT("(3,2)"), Grid.Index(3, 2), 13);
			TestEqual(TEXT("count"), Grid.Num(), 15);
			TestEqual(TEXT("bytes"), static_cast<int32>(Grid.NumBytes()), 45);
		});

		It("validates sizes and layer lengths", [this]()
		{
			FTidemarkGrid Grid = MakeGrid(4, 4);
			TestTrue(TEXT("valid"), Grid.IsValid());
			Grid.Flags.Pop();
			TestFalse(TEXT("short flags"), Grid.IsValid());
			FTidemarkGrid Empty;
			TestFalse(TEXT("empty"), Empty.IsValid());
			TestEqual(TEXT("empty sample"), Empty.SampleElevationCm(FVector2D(10.0, 10.0)), 0.0f);
		});

		It("clamps GetElevationCm to the grid", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(3, 2);
			TestEqual(TEXT("inside"), static_cast<int32>(Grid.GetElevationCm(2, 1)), 1200);
			TestEqual(TEXT("left"), static_cast<int32>(Grid.GetElevationCm(-5, 0)), 0);
			TestEqual(TEXT("far right"), static_cast<int32>(Grid.GetElevationCm(10, 10)), 1200);
		});

		It("sets, reads and clears flags as a bitmask", [this]()
		{
			FTidemarkGrid Grid = MakeGrid(3, 3);
			Grid.SetFlag(1, 1, ETidemarkCellFlag::OpenSea, true);
			Grid.SetFlag(1, 1, ETidemarkCellFlag::Forbidden, true);
			TestTrue(TEXT("open sea"), Grid.HasFlag(1, 1, ETidemarkCellFlag::OpenSea));
			TestTrue(TEXT("forbidden"), Grid.HasFlag(1, 1, ETidemarkCellFlag::Forbidden));
			TestFalse(TEXT("border"), Grid.HasFlag(1, 1, ETidemarkCellFlag::Border));
			TestEqual(TEXT("raw"), static_cast<int32>(Grid.Flags[Grid.Index(1, 1)]), 5);
			Grid.SetFlag(1, 1, ETidemarkCellFlag::OpenSea, false);
			TestEqual(TEXT("cleared"), static_cast<int32>(Grid.Flags[Grid.Index(1, 1)]), 4);
			Grid.SetFlag(10, 10, ETidemarkCellFlag::Border, true); // outside: ignored
			TestEqual(TEXT("count unchanged"), Grid.Flags.Num(), 9);
		});
	});

	Describe("World", [this]()
	{
		It("maps cell centres to world and back", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(4, 4, 0, FVector2D(1000.0, -2000.0));
			const FVector2D Centre = Grid.CellToWorld(2, 1);
			TestEqual(TEXT("centre x"), Centre.X, 1000.0 + 2.5 * 400.0);
			TestEqual(TEXT("centre y"), Centre.Y, -2000.0 + 1.5 * 400.0);
			TestTrue(TEXT("cell of centre"), Grid.WorldToCell(Centre) == FIntPoint(2, 1));
			const FVector2D Continuous = Grid.WorldToCellContinuous(Centre);
			TestEqual(TEXT("continuous x"), Continuous.X, 2.0);
			TestEqual(TEXT("continuous y"), Continuous.Y, 1.0);
			TestTrue(TEXT("outside is not inside"), !Grid.IsInside(Grid.WorldToCell(FVector2D(0.0, 0.0)).X, Grid.WorldToCell(FVector2D(0.0, 0.0)).Y));
		});
	});

	Describe("Sample", [this]()
	{
		It("returns the cell value at a cell centre", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(4, 4);
			TestEqual(TEXT("(1,2)"), Grid.SampleElevationCm(Grid.CellToWorld(1, 2)), 2100.0f);
		});

		It("interpolates bilinearly between centres", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(4, 4);
			// Midway between (1,1) = 1100 and (2,1) = 1200 along X.
			const FVector2D MidX = (Grid.CellToWorld(1, 1) + Grid.CellToWorld(2, 1)) * 0.5;
			TestEqual(TEXT("mid x"), Grid.SampleElevationCm(MidX), 1150.0f, 0.01f);
			// Quarter of the way from (1,1) to (2,2) on both axes: 1100 + 0.25 * 100 + 0.25 * 1000.
			const FVector2D Quarter = Grid.CellToWorld(1, 1) + FVector2D(100.0, 100.0);
			TestEqual(TEXT("quarter"), Grid.SampleElevationCm(Quarter), 1375.0f, 0.01f);
		});

		It("clamps to the edge values outside the grid", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(4, 4);
			TestEqual(TEXT("far below"), Grid.SampleElevationCm(FVector2D(-100000.0, -100000.0)), 0.0f);
			TestEqual(TEXT("far above"), Grid.SampleElevationCm(FVector2D(100000.0, 100000.0)), 3300.0f);
			TestEqual(TEXT("right edge, row 2"), Grid.SampleElevationCm(FVector2D(100000.0, Grid.CellToWorld(0, 2).Y)), 2300.0f);
		});
	});

	Describe("Hash", [this]()
	{
		It("matches the reference value computed by tmk.py", [this]()
		{
			// Same vector as Tools/tests/test_tmk.py::test_source_hash_reference.
			FTidemarkGrid Grid;
			Grid.Init(2, 2, 400.0f, FVector2D(1000.0, -500.0));
			Grid.ElevationCm = { 10, -20, 300, 32767 };
			Grid.Flags = { 0, 1, 2, 4 };
			ExpectU64(TEXT("source hash"), Grid.ComputeSourceHash(), 0x2cc618ee4cf82873ULL);
			TestEqual(TEXT("hex"), FTidemarkGrid::HashToHex(Grid.ComputeSourceHash()), FString(TEXT("2cc618ee4cf82873")));
		});

		It("is stable for two equal grids and changes with one cell, one flag, the origin or the cell size", [this]()
		{
			const FTidemarkGrid A = MakeGrid(40, 30, 7, FVector2D(12.5, -3.0));
			FTidemarkGrid B = MakeGrid(40, 30, 7, FVector2D(12.5, -3.0));
			TestTrue(TEXT("equal"), A == B);
			ExpectU64(TEXT("same"), A.ComputeSourceHash(), B.ComputeSourceHash());

			B.ElevationCm[B.Index(13, 7)] += 1;
			TestTrue(TEXT("cell"), A.ComputeSourceHash() != B.ComputeSourceHash());
			B = A;
			B.SetFlag(0, 0, ETidemarkCellFlag::Forbidden, true);
			TestTrue(TEXT("flag"), A.ComputeSourceHash() != B.ComputeSourceHash());
			B = A;
			B.OriginWorld.X += 1.0;
			TestTrue(TEXT("origin"), A.ComputeSourceHash() != B.ComputeSourceHash());
			B = A;
			B.CellSizeCm = 401.0f;
			TestTrue(TEXT("cell size"), A.ComputeSourceHash() != B.ComputeSourceHash());
		});

		It("marks a terrain asset stale when its grid changes after the refresh", [this]()
		{
			UTidemarkTerrainAsset* Asset = NewObject<UTidemarkTerrainAsset>(GetTransientPackage());
			Asset->Grid = MakeGrid(8, 8);
			TestTrue(TEXT("never hashed"), Asset->IsStale());
			Asset->RefreshSourceHash();
			TestFalse(TEXT("fresh"), Asset->IsStale());
			TestEqual(TEXT("schema"), Asset->SchemaVersion, UTidemarkTerrainAsset::CurrentSchemaVersion);
			Asset->Grid.ElevationCm[3] += 5;
			TestTrue(TEXT("edited"), Asset->IsStale());
			TestTrue(TEXT("primary id type"), Asset->GetPrimaryAssetId().PrimaryAssetType == UTidemarkTerrainAsset::PrimaryAssetType);
			TestEqual(TEXT("primary type name"), Asset->GetPrimaryAssetId().PrimaryAssetType.ToString(), FString(TEXT("TidemarkTerrain")));
			TestFalse(TEXT("no credits"), Asset->HasCompleteCredits());
			FTidemarkCredit Credit;
			Credit.Producer = TEXT("p");
			Credit.License = TEXT("l");
			Credit.Url = TEXT("u");
			Asset->Credits.Add(Credit);
			TestFalse(TEXT("date missing"), Asset->HasCompleteCredits());
			Asset->Credits[0].Date = TEXT("2026-10-23");
			TestTrue(TEXT("complete"), Asset->HasCompleteCredits());
		});
	});

	Describe("FillSolver", [this]()
	{
		It("converts centimetres to metres and sizes the solver like the grid", [this]()
		{
			const FTidemarkGrid Grid = MakeGrid(6, 5, -250);
			FTidemarkSolver Solver;
			Grid.FillSolver(Solver, 0.0f, 200.0f);
			TestEqual(TEXT("size x"), Solver.GetSizeX(), 6);
			TestEqual(TEXT("size y"), Solver.GetSizeY(), 5);
			TestEqual(TEXT("cell m"), Solver.GetCellSize(), 4.0f, 0.0001f);
			TestEqual(TEXT("(0,0) m"), Solver.Ground[Solver.Index(0, 0)], -2.5f, 0.0001f);
			TestEqual(TEXT("(3,2) m"), Solver.Ground[Solver.Index(3, 2)], 20.5f, 0.0001f);
		});

		It("forces Fixed on OpenSea and Border flags and leaves the other interior cells free", [this]()
		{
			FTidemarkGrid Grid;
			Grid.Init(5, 5, 400.0f);
			for (int16& Elevation : Grid.ElevationCm)
			{
				Elevation = 500; // 5 m above the sea everywhere: SetupSea alone fixes only the geometric border.
			}
			Grid.SetFlag(1, 1, ETidemarkCellFlag::OpenSea, true);
			Grid.SetFlag(2, 2, ETidemarkCellFlag::Border, true);
			Grid.SetFlag(3, 3, ETidemarkCellFlag::Forbidden, true);

			FTidemarkSolver Solver;
			Grid.FillSolver(Solver, 0.0f, 200.0f);
			TestEqual(TEXT("open sea flag fixed"), static_cast<int32>(Solver.Fixed[Solver.Index(1, 1)]), 1);
			TestEqual(TEXT("border flag fixed"), static_cast<int32>(Solver.Fixed[Solver.Index(2, 2)]), 1);
			TestEqual(TEXT("forbidden not fixed"), static_cast<int32>(Solver.Fixed[Solver.Index(3, 3)]), 0);
			TestEqual(TEXT("plain interior free"), static_cast<int32>(Solver.Fixed[Solver.Index(1, 3)]), 0);
			TestEqual(TEXT("geometric border fixed"), static_cast<int32>(Solver.Fixed[Solver.Index(0, 2)]), 1);
			TestEqual(TEXT("dry fixed cell"), Solver.Depth[Solver.Index(1, 1)], 0.0f);
		});

		It("fills the sea on deep cells and on flagged cells below the sea level", [this]()
		{
			FTidemarkGrid Grid;
			Grid.Init(5, 5, 400.0f);
			for (int16& Elevation : Grid.ElevationCm)
			{
				Elevation = 100;
			}
			Grid.ElevationCm[Grid.Index(2, 2)] = -1000; // 10 m deep: open sea by depth
			Grid.ElevationCm[Grid.Index(1, 3)] = -50;   // 0.5 m deep, flagged
			Grid.SetFlag(1, 3, ETidemarkCellFlag::OpenSea, true);

			FTidemarkSolver Solver;
			Grid.FillSolver(Solver, 0.0f, 200.0f);
			TestEqual(TEXT("deep by geometry"), static_cast<int32>(Solver.Fixed[Solver.Index(2, 2)]), 1);
			TestEqual(TEXT("deep depth"), Solver.Depth[Solver.Index(2, 2)], 10.0f, 0.0001f);
			TestEqual(TEXT("flagged shallow fixed"), static_cast<int32>(Solver.Fixed[Solver.Index(1, 3)]), 1);
			TestEqual(TEXT("flagged shallow depth"), Solver.Depth[Solver.Index(1, 3)], 0.5f, 0.0001f);
			TestEqual(TEXT("dry land"), Solver.Depth[Solver.Index(3, 1)], 0.0f);
		});

		It("gives an empty solver for an invalid grid", [this]()
		{
			FTidemarkGrid Grid;
			FTidemarkSolver Solver;
			Grid.FillSolver(Solver, 0.0f, 200.0f);
			TestFalse(TEXT("invalid"), Solver.IsValid());
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
