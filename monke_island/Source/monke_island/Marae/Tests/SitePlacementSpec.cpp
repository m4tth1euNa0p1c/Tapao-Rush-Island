// monke_island

#include "Misc/AutomationTest.h"
#include "Marae/SitePlacementMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Marae
BEGIN_DEFINE_SPEC(FSitePlacementSpec, "monke_island.Marae.Placement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSitePlacementSpec)

void FSitePlacementSpec::Define()
{
	Describe("Middle", [this]()
	{
		It("is the mean of the free spots, and nothing without spots", [this]()
		{
			const TArray<FVector> Spots = { FVector(0, 0, 100), FVector(4000, 0, 300), FVector(2000, 3000, 200) };
			TestTrue(TEXT("mean"), SitePlacement::Centroid(Spots).Equals(FVector(2000, 1000, 200), 0.01));
			TestTrue(TEXT("empty"), SitePlacement::Centroid({}).IsZero());
		});

		It("orders the spots nearest to the middle first, ties in their order", [this]()
		{
			const TArray<FVector> Spots = { FVector(9000, 0, 0), FVector(100, 0, 0), FVector(-3000, 0, 0), FVector(0, 100, 0) };
			const TArray<int32> Order = SitePlacement::ByDistanceTo(Spots, FVector::ZeroVector);
			TestEqual(TEXT("all"), Order.Num(), 4);
			TestEqual(TEXT("nearest"), Order[0], 1);
			TestEqual(TEXT("tie keeps its order"), Order[1], 3);
			TestEqual(TEXT("farthest last"), Order[3], 0);
		});
	});

	Describe("Ground", [this]()
	{
		It("accepts level land above the beach", [this]()
		{
			const TArray<double> Level = { 210, 230, 250, 220, 205, 260, 240, 215, 225 };
			TestTrue(TEXT("level land"), SitePlacement::IsBuildableGround(Level, 9, 0.0, 80.0f, 120.0f));
		});

		It("refuses the beach, a slope and a missing sample (water, a hole in the island)", [this]()
		{
			const TArray<double> Beach = { 150, 140, 60, 130, 120, 150, 145, 135, 140 };
			TestFalse(TEXT("one sample on the beach"), SitePlacement::IsBuildableGround(Beach, 9, 0.0, 80.0f, 120.0f));
			const TArray<double> Slope = { 200, 250, 360, 220, 210, 230, 240, 215, 225 };
			TestFalse(TEXT("1.6 m of slope"), SitePlacement::IsBuildableGround(Slope, 9, 0.0, 80.0f, 120.0f));
			const TArray<double> Missing = { 200, 210, 220, 230, 240, 250, 260, 270 };
			TestFalse(TEXT("eight of nine"), SitePlacement::IsBuildableGround(Missing, 9, 0.0, 80.0f, 120.0f));
		});
	});

	Describe("Facing", [this]()
	{
		It("turns the gate (local +Y) towards the players", [this]()
		{
			// players to the east (+X): local +Y must point along +X, a yaw of -90
			TestEqual(TEXT("east"), SitePlacement::YawFacing(FVector::ZeroVector, FVector(1000, 0, 0)), -90.0f, 0.01f);
			// players to the north (+Y): no turn
			TestEqual(TEXT("north"), SitePlacement::YawFacing(FVector::ZeroVector, FVector(0, 1000, 0)), 0.0f, 0.01f);
			const FVector Gate = FRotator(0.0f, SitePlacement::YawFacing(FVector(500, 500, 0), FVector(-2000, 3000, 0)), 0.0f).RotateVector(FVector(0, 1, 0));
			TestTrue(TEXT("any direction"), Gate.Equals(FVector(-2500, 2500, 0).GetSafeNormal(), 0.001));
		});
	});
}

#endif
