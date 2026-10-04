// monke_island

#include "Misc/AutomationTest.h"
#include "Items/LootMath.h"
#include "Items/LootRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Items.Loot
BEGIN_DEFINE_SPEC(FLootSpec, "monke_island.Items.Loot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLootSpec)

void FLootSpec::Define()
{
	It("draws each kind in proportion to its weight", [this]()
	{
		const TArray<float> Weights = { 1.0f, 3.0f, 0.0f };
		TestEqual(TEXT("start of the first"), LootMath::PickWeighted(Weights, 0.0f), 0);
		TestEqual(TEXT("end of the first"), LootMath::PickWeighted(Weights, 0.24f), 0);
		TestEqual(TEXT("second"), LootMath::PickWeighted(Weights, 0.26f), 1);
		TestEqual(TEXT("top"), LootMath::PickWeighted(Weights, 1.0f), 1);
		int32 Counts[3] = { 0, 0, 0 };
		for (int32 Step = 0; Step < 1000; ++Step)
		{
			++Counts[LootMath::PickWeighted(Weights, (Step + 0.5f) / 1000.0f)];
		}
		TestEqual(TEXT("a quarter"), Counts[0], 250);
		TestEqual(TEXT("three quarters"), Counts[1], 750);
		TestEqual(TEXT("weight 0 never"), Counts[2], 0);
		TestEqual(TEXT("nothing to draw"), LootMath::PickWeighted({ 0.0f, -1.0f }, 0.5f), INDEX_NONE);
	});

	It("keeps the items apart", [this]()
	{
		const TArray<FVector> Taken = { FVector(0.0, 0.0, 0.0) };
		TestFalse(TEXT("too close"), LootMath::IsFarFromAll(FVector(1000.0, 0.0, 500.0), Taken, 1200.0f));
		TestTrue(TEXT("far enough"), LootMath::IsFarFromAll(FVector(1300.0, 0.0, 0.0), Taken, 1200.0f));
	});

	It("scatters around the PlayerStarts, not on them", [this]()
	{
		const TArray<FVector> Starts = { FVector(0.0, 0.0, 0.0) };
		TestFalse(TEXT("on the start"), LootMath::IsNearAStart(FVector(500.0, 0.0, 0.0), Starts, 800.0f, 200000.0f));
		TestTrue(TEXT("around"), LootMath::IsNearAStart(FVector(5000.0, 0.0, 0.0), Starts, 800.0f, 200000.0f));
		TestFalse(TEXT("too far"), LootMath::IsNearAStart(FVector(300000.0, 0.0, 0.0), Starts, 800.0f, 200000.0f));
		TestTrue(TEXT("whole island"), LootMath::IsNearAStart(FVector(300000.0, 0.0, 0.0), Starts, 800.0f, 0.0f));
		TestTrue(TEXT("no start"), LootMath::IsNearAStart(FVector(1.0, 0.0, 0.0), {}, 800.0f, 1000.0f));
	});

	It("has sane defaults", [this]()
	{
		const ULootRules* Rules = GetDefault<ULootRules>();
		TestTrue(TEXT("count"), Rules->Count > 0);
		TestTrue(TEXT("spacing"), Rules->MinSpacing > 0.0f);
		TestTrue(TEXT("start ring"), Rules->MaxDistanceFromStart == 0.0f || Rules->MaxDistanceFromStart > Rules->MinDistanceFromStart);
	});
}

#endif
