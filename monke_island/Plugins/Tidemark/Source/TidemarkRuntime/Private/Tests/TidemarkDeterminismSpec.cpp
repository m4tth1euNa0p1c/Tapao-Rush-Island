// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "Tidemark/Rules/TidemarkRules.h"
#include "Tidemark/Solver/TidemarkHash.h"
#include "Tidemark/Solver/TidemarkSeed.h"
#include "Tidemark/Solver/TidemarkSolver.h"
#include "Tidemark/TidemarkModule.h"

#if WITH_AUTOMATION_TESTS

/**
 * Tidemark.Determinism.* (TM-04, Tidemark cahier section 10 week 1, CONVENTIONS.md section 5).
 *
 * Scenario shared by the cases: a 64 x 64 grid of 6 m cells (384 m x 384 m) whose relief is generated from a
 * seed only (FTidemarkRng, SplitMix64): a beach slope from -6 m at the open-sea edge (X = 0) to +4 m inland,
 * 12 hills and one bowl with a quadratic falloff (only + - * /, no transcendental function, so the ground is
 * bit-identical on every IEEE-754 machine). The sea is open on the X = 0 edge (SeaCells columns are Fixed)
 * and rises linearly from 0 to +3 m over the run: 600 s at Dt = 0.05 s = 12 000 steps per run.
 *
 * Cost: 4 096 cells x 12 000 steps = 49 M cell-steps per run (two ParallelFor passes each). SameSeedSameHash
 * runs 10 seeds x 2 runs = 20 runs, the other cases 6 more (IndependentOfWorkerCount runs one of them single-threaded). A float32 port of the solver in numpy takes
 * ~2.8 s per run; the C++ should be well under that. The wall time per run is logged (LogTidemark) so that
 * the size can be revisited in UTidemarkRules (TM-10) if the suite becomes too slow for the editor.
 *
 * Units: FTidemarkSolver::Depth is in metres, Tidemark::Hash::HashDepth takes centimetres (quantum 1 mm):
 * HashSolver converts (x 100, exact to float rounding, same on every machine).
 *
 * Run with: ./scripts/ue.ps1 test -Filter Tidemark.Determinism
 */
namespace TidemarkDeterminismSpecScenario
{
	constexpr int32 GridSize = 64;
	constexpr float CellSizeM = 6.0f;
	constexpr float Dt = 0.05f;
	constexpr int32 StepCount = 12000;
	constexpr float SeaRiseM = 3.0f;
	/** Columns X < SeaCells stay Fixed: the open sea on the west edge. */
	constexpr int32 SeaCells = 4;
	/** Intermediate hashes compared between two runs, to locate when a divergence starts. */
	constexpr int32 CheckpointCount = 10;
	constexpr int32 SeedCount = 10;
	/**
	 * Volume tolerance of the closed basin, relative to its volume: UTidemarkRules::VolumeToleranceRelative (D-1, 1e-5 by
	 * default, 23 L for 2 304 m3). float32 drifts about 4e-6 over 12 000 steps, under one hash quantum of one cell (36 L).
	 */
	constexpr double VolumeToleranceRelative = Tidemark::Rules::DefaultVolumeToleranceRelative;
	constexpr int32 HillCount = 12;
}

BEGIN_DEFINE_SPEC(FTidemarkDeterminismSpec, "Tidemark.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** One run: hashes at every checkpoint (index 0 = initial state at step 0), final hash and volume. */
	struct FRunResult
	{
		TArray<FTidemarkStateHash> Checkpoints;
		FTidemarkStateHash Final;
		double FinalFreeVolumeM3 = 0.0;
		bool bAllFinite = true;
		double WallSeconds = 0.0;
	};

	static FTidemarkSeed RootSeed()
	{
		return FTidemarkSeed::FromName(FName(TEXT("Determinism")));
	}

	/** Seed of run i: Derive("i"), as the cahier asks (10 derived seeds). */
	static FTidemarkSeed SeedFor(int32 Index)
	{
		return RootSeed().Derive(FName(*FString::FromInt(Index)));
	}

	/** Adds a hill (Height > 0) or a bowl (Height < 0) of radius Radius cells with a quadratic falloff. Fixed cell order. */
	static void AddHill(FTidemarkSolver& Solver, int32 CenterX, int32 CenterY, int32 Radius, float Height)
	{
		const float RadiusSquared = static_cast<float>(Radius * Radius);
		for (int32 Y = 0; Y < Solver.GetSizeY(); ++Y)
		{
			for (int32 X = 0; X < Solver.GetSizeX(); ++X)
			{
				const float DX = static_cast<float>(X) - static_cast<float>(CenterX);
				const float DY = static_cast<float>(Y) - static_cast<float>(CenterY);
				const float DistanceSquared = DX * DX + DY * DY;
				const float W = FMath::Max(0.0f, 1.0f - DistanceSquared / RadiusSquared);
				Solver.Ground[Solver.Index(X, Y)] += Height * (W * W);
			}
		}
	}

	/**
	 * Relief generated from the seed only. bSeaSlope: beach slope -6 m (X = 0) to +4 m (X = 63) under the hills;
	 * otherwise a flat 0 m floor (closed basin). Every random draw comes from the "Terrain" sub-stream, in fixed order.
	 */
	static void BuildTerrain(FTidemarkSolver& Solver, FTidemarkSeed Seed, bool bSeaSlope)
	{
		using namespace TidemarkDeterminismSpecScenario;
		Solver.Init(GridSize, GridSize, CellSizeM);
		if (bSeaSlope)
		{
			for (int32 Y = 0; Y < GridSize; ++Y)
			{
				for (int32 X = 0; X < GridSize; ++X)
				{
					Solver.Ground[Solver.Index(X, Y)] = -6.0f + 10.0f * (static_cast<float>(X) / static_cast<float>(GridSize - 1));
				}
			}
		}

		FTidemarkRng Rng(Seed.Derive(FName(TEXT("Terrain"))));
		struct FHill { int32 X; int32 Y; int32 Radius; float Height; };
		TArray<FHill> Hills;
		Hills.Reserve(HillCount + 1);
		for (int32 Hill = 0; Hill < HillCount; ++Hill)
		{
			FHill H;
			H.X = Rng.NextRange(8, 60);
			H.Y = Rng.NextRange(3, 60);
			H.Radius = Rng.NextRange(4, 12);
			H.Height = 1.0f + 5.0f * Rng.NextFloat();
			Hills.Add(H);
		}
		FHill Bowl;
		Bowl.X = Rng.NextRange(20, 50);
		Bowl.Y = Rng.NextRange(12, 52);
		Bowl.Radius = Rng.NextRange(6, 10);
		Bowl.Height = -(1.0f + 2.0f * Rng.NextFloat());
		Hills.Add(Bowl);

		for (const FHill& H : Hills)
		{
			AddHill(Solver, H.X, H.Y, H.Radius, H.Height);
		}
	}

	/** Open sea on the west edge only: SetupSea then free every cell with X >= SeaCells (as Tidemark.Solver does). */
	static void SetupOpenSea(FTidemarkSolver& Solver)
	{
		using namespace TidemarkDeterminismSpecScenario;
		Solver.SetupSea(0.0f, 1.0f);
		for (int32 Y = 0; Y < Solver.GetSizeY(); ++Y)
		{
			for (int32 X = SeaCells; X < Solver.GetSizeX(); ++X)
			{
				Solver.Fixed[Solver.Index(X, Y)] = 0;
				Solver.Depth[Solver.Index(X, Y)] = 0.0f;
			}
		}
	}

	/** State hash of the solver: depths converted to centimetres, 32 x 32 blocks, 1 mm quantum. */
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

	static bool AllFinite(const TArray<float>& Values)
	{
		for (const float Value : Values)
		{
			if (!FMath::IsFinite(Value))
			{
				return false;
			}
		}
		return true;
	}

	/** Sea level at a step: linear from 0 (step 0) to SeaRiseM (last step). Pure function of the step index. */
	static float SeaLevelAt(int32 Step)
	{
		using namespace TidemarkDeterminismSpecScenario;
		return SeaRiseM * (static_cast<float>(Step) / static_cast<float>(StepCount));
	}

	/** Full rising-sea run of a seed: terrain, open sea, StepCount steps, hashes at every checkpoint. */
	static FRunResult RunRisingSea(FTidemarkSeed Seed, FTidemarkSolver& Solver)
	{
		using namespace TidemarkDeterminismSpecScenario;
		FRunResult Result;
		BuildTerrain(Solver, Seed, true);
		SetupOpenSea(Solver);
		Result.Checkpoints.Add(HashSolver(Solver, 0));

		const double Start = FPlatformTime::Seconds(); // wall clock for the log only, never fed to the simulation
		const int32 CheckpointEvery = StepCount / CheckpointCount;
		for (int32 Step = 1; Step <= StepCount; ++Step)
		{
			Solver.Step(Dt, SeaLevelAt(Step));
			if (Step % CheckpointEvery == 0 && Step < StepCount)
			{
				Result.Checkpoints.Add(HashSolver(Solver, static_cast<uint32>(Step)));
			}
		}
		Result.WallSeconds = FPlatformTime::Seconds() - Start;
		Result.Final = HashSolver(Solver, static_cast<uint32>(StepCount));
		Result.FinalFreeVolumeM3 = Solver.FreeVolume();
		Result.bAllFinite = AllFinite(Solver.Depth);
		return Result;
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

	void ExpectNotU64(const TCHAR* What, uint64 A, uint64 B)
	{
		TestTrue(FString::Printf(TEXT("%s: both 0x%016llx"), What, A), A != B);
	}

END_DEFINE_SPEC(FTidemarkDeterminismSpec)

void FTidemarkDeterminismSpec::Define()
{
	using namespace TidemarkDeterminismSpecScenario;

	It("SameSeedSameHash", [this]()
	{
		// Cahier section 10 week 1: two full runs x 600 s, for 10 derived seeds, identical final hash.
		for (int32 SeedIndex = 0; SeedIndex < SeedCount; ++SeedIndex)
		{
			const FTidemarkSeed Seed = SeedFor(SeedIndex);
			FTidemarkSolver SolverA;
			FTidemarkSolver SolverB;
			const FRunResult RunA = RunRisingSea(Seed, SolverA);
			const FRunResult RunB = RunRisingSea(Seed, SolverB);
			const FString Label = FString::Printf(TEXT("seed %d (0x%016llx)"), SeedIndex, Seed.Value);

			UE_LOG(LogTidemark, Log, TEXT("Tidemark.Determinism.SameSeedSameHash %s: run A %.3f s, run B %.3f s, final 0x%016llx, free volume %.1f m3"),
				*Label, RunA.WallSeconds, RunB.WallSeconds, RunA.Final.Combined, RunA.FinalFreeVolumeM3);

			// A NaN hashes to a sentinel on both runs and would hide a divergence: refuse it explicitly.
			TestTrue(Label + TEXT(": run A has only finite depths"), RunA.bAllFinite);
			TestTrue(Label + TEXT(": run B has only finite depths"), RunB.bAllFinite);

			TestEqual(*(Label + TEXT(": checkpoint count")), RunA.Checkpoints.Num(), CheckpointCount);
			for (int32 Checkpoint = 0; Checkpoint < RunA.Checkpoints.Num() && Checkpoint < RunB.Checkpoints.Num(); ++Checkpoint)
			{
				ExpectSameHash(FString::Printf(TEXT("%s checkpoint %d"), *Label, Checkpoint), RunA.Checkpoints[Checkpoint], RunB.Checkpoints[Checkpoint]);
			}
			ExpectSameHash(Label + TEXT(" final"), RunA.Final, RunB.Final);

			// Not vacuous: the sea did flood free cells and the state did change since step 0.
			TestTrue(Label + TEXT(": the flood reached free cells (free volume > 0)"), RunA.FinalFreeVolumeM3 > 0.0);
			TestTrue(Label + TEXT(": the state changed between step 0 and the end"), RunA.Checkpoints[0].Blocks != RunA.Final.Blocks);
		}
	});

	It("DifferentSeedDifferentHash", [this]()
	{
		// Guard: the generator really depends on the seed, otherwise SameSeedSameHash would prove nothing.
		FTidemarkSolver SolverA;
		FTidemarkSolver SolverB;
		const FRunResult RunA = RunRisingSea(SeedFor(0), SolverA);
		const FRunResult RunB = RunRisingSea(SeedFor(1), SolverB);
		ExpectNotU64(TEXT("seed 0 vs seed 1 derive to different seeds"), SeedFor(0).Value, SeedFor(1).Value);
		ExpectNotU64(TEXT("final state differs"), RunA.Final.Combined, RunB.Final.Combined);
	});

	It("BlockHashLocatesDivergence", [this]()
	{
		// After a run, a 1 cm change of one cell is found in exactly one 32 x 32 block, the right one.
		FTidemarkSolver Solver;
		const FRunResult Run = RunRisingSea(SeedFor(0), Solver);
		const FTidemarkStateHash Reference = Run.Final;

		const int32 CellX = 40;
		const int32 CellY = 33;
		Solver.Depth[Solver.Index(CellX, CellY)] += 0.01f; // 1 cm = 10 quanta of 1 mm
		const FTidemarkStateHash Modified = HashSolver(Solver, static_cast<uint32>(StepCount));

		TestEqual(TEXT("2 x 2 blocks on a 64 x 64 grid"), Reference.Blocks.Num(), 4);
		ExpectNotU64(TEXT("combined hash changed"), Reference.Combined, Modified.Combined);
		const TArray<FIntPoint> Diff = Tidemark::Hash::DiffBlocks(Reference, Modified);
		FString DiffList;
		for (const FIntPoint& Block : Diff)
		{
			DiffList += FString::Printf(TEXT(" (%d, %d)"), Block.X, Block.Y);
		}
		TestTrue(FString::Printf(TEXT("exactly one differing block, got %d:%s"), Diff.Num(), *DiffList), Diff.Num() == 1);
		if (Diff.Num() == 1)
		{
			const FIntPoint Expected(CellX / Tidemark::Hash::DefaultBlockSize, CellY / Tidemark::Hash::DefaultBlockSize);
			TestTrue(FString::Printf(TEXT("the differing block is (%d, %d), got (%d, %d)"), Expected.X, Expected.Y, Diff[0].X, Diff[0].Y), Diff[0] == Expected);
		}
	});

	It("IndependentOfWorkerCount", [this]()
	{
		// D-2 (TM-10): the same run with the two ParallelFor passes on the task graph and forced on the calling thread
		// (EParallelForFlags::ForceSingleThread) ends on the same hash at every checkpoint. Gravity and damping are the
		// UTidemarkRules defaults, the only values the subsystem will pass.
		FTidemarkSolver Multi;
		FTidemarkSolver Single;
		BuildTerrain(Multi, SeedFor(0), true);
		SetupOpenSea(Multi);
		BuildTerrain(Single, SeedFor(0), true);
		SetupOpenSea(Single);
		const int32 CheckpointEvery = StepCount / CheckpointCount;
		for (int32 Step = 1; Step <= StepCount; ++Step)
		{
			Multi.Step(Dt, SeaLevelAt(Step), Tidemark::Rules::DefaultGravity, Tidemark::Rules::DefaultFluxDamping, /*bForceSingleThread*/ false);
			Single.Step(Dt, SeaLevelAt(Step), Tidemark::Rules::DefaultGravity, Tidemark::Rules::DefaultFluxDamping, /*bForceSingleThread*/ true);
			if (Step % CheckpointEvery == 0)
			{
				if (!ExpectSameHash(FString::Printf(TEXT("multi-thread vs single-thread at step %d"), Step),
					HashSolver(Multi, static_cast<uint32>(Step)), HashSolver(Single, static_cast<uint32>(Step))))
				{
					break; // the first divergent checkpoint is the useful one
				}
			}
		}
		TestTrue(TEXT("the flood reached free cells (free volume > 0)"), Single.FreeVolume() > 0.0);
	});

	It("VolumeConserved", [this]()
	{
		// Closed basin: no Fixed cell at all (Init leaves Fixed at 0, SetupSea is not called), the grid border has no
		// outward pipe. 1 m of water on the central 8 x 8 cells (64 x 36 m2 = 2 304 m3), 12 000 steps, total volume
		// constant within VolumeToleranceRelative (D-1). FreeVolume sums in double; the per-step arithmetic is float32 (solver).
		FTidemarkSolver Solver;
		BuildTerrain(Solver, RootSeed().Derive(FName(TEXT("Basin"))), false);
		int32 FixedCount = 0;
		for (const uint8 F : Solver.Fixed)
		{
			FixedCount += F ? 1 : 0;
		}
		TestEqual(TEXT("no fixed cell in the basin"), FixedCount, 0);
		for (int32 Y = 28; Y < 36; ++Y)
		{
			for (int32 X = 28; X < 36; ++X)
			{
				Solver.Depth[Solver.Index(X, Y)] = 1.0f;
			}
		}
		const double Before = Solver.FreeVolume();
		TestEqual(TEXT("initial volume (m3)"), Before, 2304.0, 0.001);

		// D-1: relative criterion read from the rules (the asset default must agree with the compile-time constant).
		const UTidemarkRules* Rules = GetDefault<UTidemarkRules>();
		TestEqual(TEXT("UTidemarkRules::VolumeToleranceRelative matches Tidemark::Rules::DefaultVolumeToleranceRelative"),
			Rules->VolumeToleranceRelative, VolumeToleranceRelative, 1.0e-12);
		const double VolumeToleranceM3 = Before * Rules->VolumeToleranceRelative;

		double WorstDriftM3 = 0.0;
		const int32 CheckpointEvery = StepCount / CheckpointCount;
		for (int32 Step = 1; Step <= StepCount; ++Step)
		{
			Solver.Step(Dt, -100.0f);
			if (Step % CheckpointEvery == 0)
			{
				const double Drift = Solver.FreeVolume() - Before;
				WorstDriftM3 = FMath::Max(WorstDriftM3, FMath::Abs(Drift));
				UE_LOG(LogTidemark, Log, TEXT("Tidemark.Determinism.VolumeConserved step %d: volume %.6f m3, drift %+.3f L"), Step, Before + Drift, Drift * 1000.0);
			}
		}
		const double After = Solver.FreeVolume();
		TestTrue(TEXT("all depths finite"), AllFinite(Solver.Depth));
		TestTrue(FString::Printf(TEXT("volume conserved within %.1f L (relative %.1e) over %d steps: before %.6f m3, after %.6f m3, drift %+.3f L, worst %.3f L"),
			VolumeToleranceM3 * 1000.0, Rules->VolumeToleranceRelative, StepCount, Before, After, (After - Before) * 1000.0, WorstDriftM3 * 1000.0),
			FMath::Abs(After - Before) <= VolumeToleranceM3);
		TestTrue(FString::Printf(TEXT("worst checkpoint drift within %.1f L: %.3f L"), VolumeToleranceM3 * 1000.0, WorstDriftM3 * 1000.0), WorstDriftM3 <= VolumeToleranceM3);
		// Not vacuous: the water spread across the basin.
		TestTrue(TEXT("water reached the far corner (61, 61)"), Solver.Depth[Solver.Index(61, 61)] > 0.005f);
	});
}

#endif // WITH_AUTOMATION_TESTS
