// Tidemark plugin

#include "Misc/AutomationTest.h"
#include "Tidemark/Solver/TidemarkSeed.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter Tidemark.Seed
BEGIN_DEFINE_SPEC(FTidemarkSeedSpec, "Tidemark.Seed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	void ExpectU64(const TCHAR* What, uint64 Actual, uint64 Expected)
	{
		TestTrue(FString::Printf(TEXT("%s: got 0x%016llx, expected 0x%016llx"), What, Actual, Expected), Actual == Expected);
	}
	void ExpectNotU64(const TCHAR* What, uint64 A, uint64 B)
	{
		TestTrue(FString::Printf(TEXT("%s: both 0x%016llx"), What, A), A != B);
	}
END_DEFINE_SPEC(FTidemarkSeedSpec)

void FTidemarkSeedSpec::Define()
{
	Describe("FromName", [this]()
	{
		It("hashes the name with FNV-1a 64 (value fixed by the reference Python script)", [this]()
		{
			// fnv1a64("Tidemark") computed outside the engine: must never change, saved scenarios depend on it.
			ExpectU64(TEXT("Tidemark"), FTidemarkSeed::FromName(FName(TEXT("Tidemark"))).Value, 0xc415b2835dd67644ULL);
			ExpectU64(TEXT("Tide"), FTidemarkSeed::FromName(FName(TEXT("Tide"))).Value, 0x48f98afb3022ed11ULL);
		});

		It("is stable across calls and FName instances", [this]()
		{
			const FTidemarkSeed A = FTidemarkSeed::FromName(FName(TEXT("Raraka")));
			const FTidemarkSeed B = FTidemarkSeed::FromName(FName(*FString::Printf(TEXT("%s%s"), TEXT("Rar"), TEXT("aka"))));
			ExpectU64(TEXT("same string, same seed"), A.Value, B.Value);
		});

		It("is case sensitive even though FName compares case-insensitively", [this]()
		{
			ExpectNotU64(TEXT("Tide vs tide"), FTidemarkSeed::FromName(FName(TEXT("Tide"))).Value, FTidemarkSeed::FromName(FName(TEXT("tide"))).Value);
		});
	});

	Describe("Derive", [this]()
	{
		It("gives the same sub-seed for the same seed and stream", [this]()
		{
			const FTidemarkSeed Seed = FTidemarkSeed::FromValue(123456789ULL);
			ExpectU64(TEXT("twice"), Seed.Derive(FName(TEXT("Storm"))).Value, Seed.Derive(FName(TEXT("Storm"))).Value);
			ExpectU64(TEXT("copy"), FTidemarkSeed::FromValue(123456789ULL).Derive(FName(TEXT("Storm"))).Value, Seed.Derive(FName(TEXT("Storm"))).Value);
		});

		It("matches the reference SplitMix64 values", [this]()
		{
			ExpectU64(TEXT("FromName(Tidemark).Derive(Tide)"), FTidemarkSeed::FromName(FName(TEXT("Tidemark"))).Derive(FName(TEXT("Tide"))).Value, 0xd94056c520fe4eafULL);
			ExpectU64(TEXT("42.Derive(Tide)"), FTidemarkSeed::FromValue(42ULL).Derive(FName(TEXT("Tide"))).Value, 0x5f0814f80d7990acULL);
			ExpectU64(TEXT("Mix(42)"), FTidemarkSeed::Mix(42ULL), 0xbdd732262feb6e95ULL);
		});

		It("gives different sub-seeds for two stream names", [this]()
		{
			const FTidemarkSeed Seed = FTidemarkSeed::FromValue(7ULL);
			ExpectNotU64(TEXT("Storm vs Tide"), Seed.Derive(FName(TEXT("Storm"))).Value, Seed.Derive(FName(TEXT("Tide"))).Value);
			ExpectNotU64(TEXT("derived vs parent"), Seed.Derive(FName(TEXT("Storm"))).Value, Seed.Value);
		});

		It("gives different sub-seeds for two seeds and the same stream", [this]()
		{
			ExpectNotU64(TEXT("0 vs 1"), FTidemarkSeed::FromValue(0ULL).Derive(FName(TEXT("Tide"))).Value, FTidemarkSeed::FromValue(1ULL).Derive(FName(TEXT("Tide"))).Value);
		});

		It("chains: deriving twice is deterministic and order-dependent", [this]()
		{
			const FTidemarkSeed Seed = FTidemarkSeed::FromValue(99ULL);
			const FTidemarkSeed AB = Seed.Derive(FName(TEXT("A"))).Derive(FName(TEXT("B")));
			const FTidemarkSeed BA = Seed.Derive(FName(TEXT("B"))).Derive(FName(TEXT("A")));
			ExpectU64(TEXT("A.B twice"), AB.Value, Seed.Derive(FName(TEXT("A"))).Derive(FName(TEXT("B"))).Value);
			ExpectNotU64(TEXT("A.B vs B.A"), AB.Value, BA.Value);
		});
	});

	Describe("ToRandomStream", [this]()
	{
		It("produces the same sequence for the same seed", [this]()
		{
			FRandomStream A = FTidemarkSeed::FromValue(2024ULL).ToRandomStream();
			FRandomStream B = FTidemarkSeed::FromValue(2024ULL).ToRandomStream();
			for (int32 Index = 0; Index < 16; ++Index)
			{
				TestEqual(FString::Printf(TEXT("draw %d"), Index), A.FRand(), B.FRand());
			}
		});

		It("uses the upper 32 bits of Mix(Value) as engine seed", [this]()
		{
			// Mix(42) = 0xbdd732262feb6e95 -> upper 32 bits 0xbdd73226 reinterpreted as int32.
			const FRandomStream Expected(static_cast<int32>(0xbdd73226u));
			const FRandomStream Actual = FTidemarkSeed::FromValue(42ULL).ToRandomStream();
			TestEqual(TEXT("initial seed"), Actual.GetInitialSeed(), Expected.GetInitialSeed());
		});
	});

	Describe("FTidemarkRng", [this]()
	{
		It("replays the same sequence from the same seed", [this]()
		{
			FTidemarkRng A(FTidemarkSeed::FromValue(5ULL));
			FTidemarkRng B(FTidemarkSeed::FromValue(5ULL));
			for (int32 Index = 0; Index < 32; ++Index)
			{
				ExpectU64(*FString::Printf(TEXT("u64 %d"), Index), A.NextU64(), B.NextU64());
			}
		});

		It("first draw equals Mix(seed) (SplitMix64 reference)", [this]()
		{
			FTidemarkRng Rng(FTidemarkSeed::FromValue(42ULL));
			ExpectU64(TEXT("first"), Rng.NextU64(), 0xbdd732262feb6e95ULL);
		});

		It("keeps NextFloat in [0, 1) and NextRange within bounds", [this]()
		{
			FTidemarkRng Rng(FTidemarkSeed::FromName(FName(TEXT("Bounds"))));
			for (int32 Index = 0; Index < 1000; ++Index)
			{
				const float F = Rng.NextFloat();
				TestTrue(TEXT("float >= 0"), F >= 0.0f);
				TestTrue(TEXT("float < 1"), F < 1.0f);
				const int32 R = Rng.NextRange(-3, 3);
				TestTrue(TEXT("range >= -3"), R >= -3);
				TestTrue(TEXT("range <= 3"), R <= 3);
			}
			TestEqual(TEXT("empty range returns Min"), Rng.NextRange(4, 4), 4);
			TestEqual(TEXT("inverted range returns Min"), Rng.NextRange(4, 1), 4);
		});

		It("diverges between two different sub-streams", [this]()
		{
			const FTidemarkSeed Seed = FTidemarkSeed::FromValue(11ULL);
			FTidemarkRng A(Seed.Derive(FName(TEXT("Waves"))));
			FTidemarkRng B(Seed.Derive(FName(TEXT("Gusts"))));
			ExpectNotU64(TEXT("first draw"), A.NextU64(), B.NextU64());
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
