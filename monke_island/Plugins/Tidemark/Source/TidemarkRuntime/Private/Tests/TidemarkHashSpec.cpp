// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "Tidemark/Solver/TidemarkHash.h"

#include <limits>

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter Tidemark.Hash
BEGIN_DEFINE_SPEC(FTidemarkHashSpec, "Tidemark.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	void ExpectU64(const TCHAR* What, uint64 Actual, uint64 Expected)
	{
		TestTrue(FString::Printf(TEXT("%s: got 0x%016llx, expected 0x%016llx"), What, Actual, Expected), Actual == Expected);
	}
	void ExpectNotU64(const TCHAR* What, uint64 A, uint64 B)
	{
		TestTrue(FString::Printf(TEXT("%s: both 0x%016llx"), What, A), A != B);
	}
	/** Row-major depth field of SizeX * SizeY cells, Depth(X, Y) = Base + X + 100 * Y, in cm. */
	static TArray<float> MakeDepth(int32 SizeX, int32 SizeY, float Base = 0.0f)
	{
		TArray<float> Depth;
		Depth.SetNumUninitialized(SizeX * SizeY);
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				Depth[Y * SizeX + X] = Base + static_cast<float>(X) + 100.0f * static_cast<float>(Y);
			}
		}
		return Depth;
	}
END_DEFINE_SPEC(FTidemarkHashSpec)

void FTidemarkHashSpec::Define()
{
	using namespace Tidemark::Hash;

	Describe("Fnv1a64", [this]()
	{
		It("returns the offset basis for empty input", [this]()
		{
			ExpectU64(TEXT("empty"), Fnv1a64(nullptr, 0), 0xcbf29ce484222325ULL);
		});

		It("matches the reference value for a known string", [this]()
		{
			const ANSICHAR* Text = "Tidemark";
			ExpectU64(TEXT("Tidemark"), Fnv1a64(Text, 8), 0xc415b2835dd67644ULL);
		});

		It("chains through the Basis argument", [this]()
		{
			const ANSICHAR* Text = "Tidemark";
			const uint64 Partial = Fnv1a64(Text, 4);
			ExpectU64(TEXT("chained"), Fnv1a64(Text + 4, 4, Partial), Fnv1a64(Text, 8));
		});
	});

	Describe("QuantizeDepth", [this]()
	{
		It("rounds to millimetres by default", [this]()
		{
			TestEqual(TEXT("0"), QuantizeDepth(0.0f), 0);
			TestEqual(TEXT("10 cm"), QuantizeDepth(10.0f), 100);
			TestEqual(TEXT("20.04 cm"), QuantizeDepth(20.04f), 200);
			TestEqual(TEXT("-3 cm"), QuantizeDepth(-3.0f), -30);
		});

		It("maps non-finite values to a sentinel instead of undefined behaviour", [this]()
		{
			TestEqual(TEXT("NaN"), QuantizeDepth(std::numeric_limits<float>::quiet_NaN()), MIN_int32);
			TestEqual(TEXT("+inf"), QuantizeDepth(std::numeric_limits<float>::infinity()), MIN_int32);
		});
	});

	Describe("HashDepth", [this]()
	{
		It("matches the reference value for a 2x2 field at step 7", [this]()
		{
			// Reference computed by the Python script of T-343: quantized [0, 100, 200, -30] as int32 LE, then step 7 (uint32 LE) + block (uint64 LE).
			const TArray<float> Depth = { 0.0f, 10.0f, 20.04f, -3.0f };
			const FTidemarkStateHash Hash = HashDepth(Depth, 2, 2, 7);
			TestEqual(TEXT("blocks x"), Hash.BlocksX, 1);
			TestEqual(TEXT("blocks y"), Hash.BlocksY, 1);
			TestEqual(TEXT("block count"), Hash.Blocks.Num(), 1);
			ExpectU64(TEXT("block"), Hash.Blocks[0], 0x95f1b6e7f14f4280ULL);
			ExpectU64(TEXT("combined"), Hash.Combined, 0xd180d77ef34981bfULL);
			TestEqual(TEXT("step"), static_cast<int32>(Hash.StepIndex), 7);
		});

		It("gives the same hash for two equal fields", [this]()
		{
			const TArray<float> A = MakeDepth(70, 45);
			const TArray<float> B = MakeDepth(70, 45);
			const FTidemarkStateHash HashA = HashDepth(A, 70, 45, 12);
			const FTidemarkStateHash HashB = HashDepth(B, 70, 45, 12);
			ExpectU64(TEXT("combined"), HashA.Combined, HashB.Combined);
			TestTrue(TEXT("blocks"), HashA.Blocks == HashB.Blocks);
			TestEqual(TEXT("no diff"), DiffBlocks(HashA, HashB).Num(), 0);
		});

		It("lays out ceil(Size / 32) blocks per axis", [this]()
		{
			const FTidemarkStateHash Hash = HashDepth(MakeDepth(70, 45), 70, 45, 0);
			TestEqual(TEXT("blocks x"), Hash.BlocksX, 3);
			TestEqual(TEXT("blocks y"), Hash.BlocksY, 2);
			TestEqual(TEXT("block count"), Hash.Blocks.Num(), 6);
		});

		It("changes exactly one block when one cell changes", [this]()
		{
			const TArray<float> A = MakeDepth(70, 45);
			TArray<float> B = A;
			// Cell (40, 33): block (1, 1).
			B[33 * 70 + 40] += 1.0f;
			const FTidemarkStateHash HashA = HashDepth(A, 70, 45, 3);
			const FTidemarkStateHash HashB = HashDepth(B, 70, 45, 3);
			ExpectNotU64(TEXT("combined"), HashA.Combined, HashB.Combined);
			const TArray<FIntPoint> Diff = DiffBlocks(HashA, HashB);
			TestEqual(TEXT("one block"), Diff.Num(), 1);
			if (Diff.Num() == 1)
			{
				TestTrue(TEXT("block coords"), Diff[0] == FIntPoint(1, 1));
			}
		});

		It("ignores a change below the quantum", [this]()
		{
			const TArray<float> A = MakeDepth(40, 40);
			TArray<float> B = A;
			B[5] += 0.02f; // 0.2 mm: rounds to the same millimetre.
			ExpectU64(TEXT("same combined"), HashDepth(A, 40, 40, 1).Combined, HashDepth(B, 40, 40, 1).Combined);
		});

		It("gives a different combined hash for a different StepIndex", [this]()
		{
			const TArray<float> A = MakeDepth(40, 40);
			const FTidemarkStateHash Step1 = HashDepth(A, 40, 40, 1);
			const FTidemarkStateHash Step2 = HashDepth(A, 40, 40, 2);
			ExpectNotU64(TEXT("combined"), Step1.Combined, Step2.Combined);
			TestTrue(TEXT("blocks unchanged"), Step1.Blocks == Step2.Blocks);
			TestEqual(TEXT("no block diff"), DiffBlocks(Step1, Step2).Num(), 0);
		});

		It("gives a different hash for a different quantum or block size", [this]()
		{
			const TArray<float> A = MakeDepth(40, 40, 0.37f);
			ExpectNotU64(TEXT("block size"), HashDepth(A, 40, 40, 0, 32).Combined, HashDepth(A, 40, 40, 0, 16).Combined);
			ExpectNotU64(TEXT("quantum"), HashDepth(A, 40, 40, 0, 32, 0.1f).Combined, HashDepth(A, 40, 40, 0, 32, 1.0f).Combined);
		});

		It("handles an invalid field without crashing", [this]()
		{
			const TArray<float> TooShort = { 1.0f, 2.0f };
			const FTidemarkStateHash Hash = HashDepth(TooShort, 2, 2, 9);
			TestEqual(TEXT("no blocks"), Hash.Blocks.Num(), 0);
			TestEqual(TEXT("step kept"), static_cast<int32>(Hash.StepIndex), 9);
		});
	});

	Describe("DiffBlocks", [this]()
	{
		It("reports every block when the layouts differ", [this]()
		{
			const FTidemarkStateHash A = HashDepth(MakeDepth(40, 40), 40, 40, 0);
			const FTidemarkStateHash B = HashDepth(MakeDepth(70, 40), 70, 40, 0);
			TestEqual(TEXT("all blocks of the larger layout"), DiffBlocks(A, B).Num(), 3 * 2);
		});

		It("lists differing blocks in row-major order", [this]()
		{
			const TArray<float> A = MakeDepth(70, 45);
			TArray<float> B = A;
			B[40 * 70 + 65] += 1.0f; // block (2, 1)
			B[2 * 70 + 3] += 1.0f;   // block (0, 0)
			const TArray<FIntPoint> Diff = DiffBlocks(HashDepth(A, 70, 45, 0), HashDepth(B, 70, 45, 0));
			TestEqual(TEXT("two blocks"), Diff.Num(), 2);
			if (Diff.Num() == 2)
			{
				TestTrue(TEXT("first"), Diff[0] == FIntPoint(0, 0));
				TestTrue(TEXT("second"), Diff[1] == FIntPoint(2, 1));
			}
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
