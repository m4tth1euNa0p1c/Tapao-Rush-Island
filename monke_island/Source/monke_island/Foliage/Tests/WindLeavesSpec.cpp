// monke_island

#include "Misc/AutomationTest.h"
#include "Foliage/WindLeavesSim.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Foliage
BEGIN_DEFINE_SPEC(FWindLeavesSpec, "monke_island.Foliage.WindLeaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	WindLeavesSim::FLeaf MakeLeaf(float Height) const
	{
		WindLeavesSim::FLeaf Leaf;
		Leaf.State = WindLeavesSim::EState::Flying;
		Leaf.Location = FVector(0.0, 0.0, Height);
		Leaf.GroundZ = 0.0f;
		Leaf.Life = 60.0f;
		Leaf.Seed = 0.5f;
		Leaf.Spin = 3.0f;
		return Leaf;
	}
	WindLeavesSim::FAir MakeAir(float WindX) const
	{
		WindLeavesSim::FAir Air;
		Air.Wind = FVector(WindX, 0.0, 0.0);
		Air.Turbulence = 150.0f;
		return Air;
	}
END_DEFINE_SPEC(FWindLeavesSpec)

void FWindLeavesSpec::Define()
{
	It("is carried downwind by the storm", [this]()
	{
		WindLeavesSim::FLeaf Leaf = MakeLeaf(1200.0f);
		WindLeavesSim::FAir Air = MakeAir(1100.0f);
		for (int32 Step = 0; Step < 60; ++Step)
		{
			Air.Time += 1.0f / 30.0f;
			WindLeavesSim::Step(Leaf, Air, 1.0f / 30.0f);
		}
		TestTrue(TEXT("travelled more than 10 m downwind in 2 s"), Leaf.Location.X > 1000.0);
	});

	It("falls slowly in still air and lands flat on the ground", [this]()
	{
		WindLeavesSim::FLeaf Leaf = MakeLeaf(300.0f);
		WindLeavesSim::FAir Air = MakeAir(0.0f);
		Air.Turbulence = 0.0f;
		float Time = 0.0f;
		while (Leaf.State == WindLeavesSim::EState::Flying && Time < 20.0f)
		{
			Time += 1.0f / 30.0f;
			Air.Time = Time;
			WindLeavesSim::Step(Leaf, Air, 1.0f / 30.0f);
		}
		TestEqual(TEXT("landed"), Leaf.State, WindLeavesSim::EState::Landed);
		TestTrue(TEXT("takes 2-8 s to fall 3 m (a leaf, not a stone)"), Time > 2.0f && Time < 8.0f);
		TestTrue(TEXT("rests on the ground"), FMath::IsNearlyEqual(Leaf.Location.Z, 1.0, 0.01));
		TestTrue(TEXT("lies flat"), FMath::Abs(Leaf.Rotation.GetUpVector().Z) > 0.999);
	});

	It("floats on the water and drifts with the wind", [this]()
	{
		WindLeavesSim::FLeaf Leaf = MakeLeaf(50.0f);
		Leaf.GroundZ = -300.0f;
		WindLeavesSim::FAir Air = MakeAir(400.0f);
		Air.WaterZ = 0.0f;
		for (int32 Step = 0; Step < 90 && Leaf.State == WindLeavesSim::EState::Flying; ++Step)
		{
			WindLeavesSim::Step(Leaf, Air, 1.0f / 30.0f);
		}
		TestEqual(TEXT("floating"), Leaf.State, WindLeavesSim::EState::Floating);
		const double X = Leaf.Location.X;
		WindLeavesSim::Step(Leaf, Air, 0.5f);
		TestTrue(TEXT("drifts downwind"), Leaf.Location.X > X);
		TestTrue(TEXT("at the surface"), FMath::Abs(Leaf.Location.Z) < 5.0);
	});

	It("grows in, shrinks out and dies at the end of its life", [this]()
	{
		WindLeavesSim::FLeaf Leaf = MakeLeaf(500.0f);
		Leaf.Life = 3.0f;
		TestEqual(TEXT("invisible at birth"), WindLeavesSim::FadeScale(Leaf), 0.0f);
		Leaf.Age = 1.0f;
		TestEqual(TEXT("full size"), WindLeavesSim::FadeScale(Leaf), 1.0f);
		const WindLeavesSim::FAir Air = MakeAir(0.0f);
		bool bAlive = true;
		for (int32 Step = 0; Step < 100 && bAlive; ++Step)
		{
			bAlive = WindLeavesSim::Step(Leaf, Air, 0.05f);
		}
		TestFalse(TEXT("dead"), bAlive);
	});

	It("is born upwind of the player", [this]()
	{
		const FVector2D Wind(-1.0f, 0.0f);
		double MeanX = 0.0;
		for (int32 Index = 0; Index < 200; ++Index)
		{
			MeanX += WindLeavesSim::SpawnPoint(FVector2D::ZeroVector, Wind, 2000.0f, (Index * 0.618f) - FMath::FloorToFloat(Index * 0.618f), (Index + 0.5f) / 200.0f).X / 200.0;
		}
		TestTrue(TEXT("upwind of the player (wind blows toward -X)"), MeanX > 500.0);
	});
}

#endif
