// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/GiantCrabLag.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"

#if WITH_DEV_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Creatures.GiantCrab.Net (T-304, T-305; review T-268 R-23 to R-25:
// actions played from the server's start, fixed shapes for the pawns (upright pillars since T-328, whose own cases are
// in monke_island.Creatures.GiantCrab.Pawns), latency tolerance of the weak points)
BEGIN_DEFINE_SPEC(FGiantCrabNetSpec, "monke_island.Creatures.GiantCrab.Net", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGiantCrabNetSpec)

void FGiantCrabNetSpec::Define()
{
	Describe("Actions received late", [this]()
	{
		It("plays the clip as far in as the action is old, holding the end of a death", [this]()
		{
			TestEqual(TEXT("fresh"), GiantCrabLag::ClipStartTime(0.0f, 1.2f, 2.0f, false), 0.0f, 0.001f);
			TestEqual(TEXT("a blow 0.5 s old at rate 1.2"), GiantCrabLag::ClipStartTime(0.5f, 1.2f, 2.0f, false), 0.6f, 0.001f);
			TestEqual(TEXT("a death seen 30 s later: its last pose"), GiantCrabLag::ClipStartTime(30.0f, 1.0f, 2.4f, false), 2.4f, 0.001f);
			TestEqual(TEXT("raised claws loop"), GiantCrabLag::ClipStartTime(3.0f, 1.0f, 1.2f, true), 0.6f, 0.001f);
			TestEqual(TEXT("no clip"), GiantCrabLag::ClipStartTime(1.0f, 1.0f, 0.0f, false), 0.0f, 0.001f);
		});

		It("plays the moments still due and those just missed, never the old ones", [this]()
		{
			const TArray<float> Roar = { 0.0f, 0.6f };
			TestEqual(TEXT("fresh"), GiantCrabLag::FirstFreshMoment(Roar, 0.0f, 0.3f), 0);
			TestEqual(TEXT("the cry just missed still plays"), GiantCrabLag::FirstFreshMoment(Roar, 0.25f, 0.3f), 0);
			TestEqual(TEXT("the cry is past, the wave to come"), GiantCrabLag::FirstFreshMoment(Roar, 0.4f, 0.3f), 1);
			TestEqual(TEXT("a roar seen 5 s later: silence"), GiantCrabLag::FirstFreshMoment(Roar, 5.0f, 0.3f), 2);
			const TArray<float> None;
			TestEqual(TEXT("no moment"), GiantCrabLag::FirstFreshMoment(None, 1.0f, 0.3f), 0);
		});
	});

	Describe("Latency of the weak points", [this]()
	{
		It("looks back the round trip and the view delay, never beyond its bound, not for the host", [this]()
		{
			TestEqual(TEXT("host"), GiantCrabLag::RewindSeconds(0.0f, 0.05f, 0.3f), 0.0f, 0.001f);
			TestEqual(TEXT("100 ms"), GiantCrabLag::RewindSeconds(0.1f, 0.05f, 0.3f), 0.15f, 0.001f);
			TestEqual(TEXT("200 ms"), GiantCrabLag::RewindSeconds(0.2f, 0.05f, 0.3f), 0.25f, 0.001f);
			TestEqual(TEXT("400 ms: bounded"), GiantCrabLag::RewindSeconds(0.4f, 0.05f, 0.3f), 0.3f, 0.001f);
			TestEqual(TEXT("compensation off"), GiantCrabLag::RewindSeconds(0.1f, 0.05f, 0.0f), 0.0f, 0.001f);
		});

		It("counts a blow on the stuck claw as late as the player saw it, exactly the window for the host", [this]()
		{
			// the claw stuck from 10.0 to 10.5 s on the server clock
			TestTrue(TEXT("host inside"), GiantCrabLag::IsInLateWindow(10.2, 10.0, 10.5, 0.0f, 0.05f));
			TestFalse(TEXT("host after"), GiantCrabLag::IsInLateWindow(10.6, 10.0, 10.5, 0.0f, 0.05f));
			TestTrue(TEXT("200 ms player, 0.2 s after the end"), GiantCrabLag::IsInLateWindow(10.7, 10.0, 10.5, 0.2f, 0.05f));
			TestFalse(TEXT("200 ms player, 0.3 s after the end"), GiantCrabLag::IsInLateWindow(10.8, 10.0, 10.5, 0.2f, 0.05f));
			TestFalse(TEXT("before it sticks"), GiantCrabLag::IsInLateWindow(9.9, 10.0, 10.5, 0.2f, 0.05f));
			TestFalse(TEXT("no slam yet"), GiantCrabLag::IsInLateWindow(10.2, 0.0, -1.0, 0.2f, 0.05f));
		});

		It("finds a shot that crosses the path of a weak point, not one that passes by", [this]()
		{
			const TArray<FVector> Path = { FVector(0.0, 0.0, 200.0), FVector(100.0, 0.0, 200.0) };
			TestTrue(TEXT("across its path"), GiantCrabLag::RayNearPath(FVector(50.0, -500.0, 200.0), FVector(50.0, 500.0, 200.0), Path, 10.0f));
			TestFalse(TEXT("50 cm above, radius 30"), GiantCrabLag::RayNearPath(FVector(50.0, -500.0, 250.0), FVector(50.0, 500.0, 250.0), Path, 30.0f));
			TestTrue(TEXT("50 cm above, radius 60"), GiantCrabLag::RayNearPath(FVector(50.0, -500.0, 250.0), FVector(50.0, 500.0, 250.0), Path, 60.0f));
			const TArray<FVector> Still = { FVector(0.0, 0.0, 200.0) };
			TestTrue(TEXT("a point that did not move"), GiantCrabLag::RayNearPath(FVector(0.0, -500.0, 210.0), FVector(0.0, 500.0, 210.0), Still, 30.0f));
			TestTrue(TEXT("a blow without a line, on the point"), GiantCrabLag::RayNearPath(FVector(5.0, 0.0, 200.0), FVector(5.0, 0.0, 200.0), Still, 30.0f));
			const TArray<FVector> Empty;
			TestFalse(TEXT("no path"), GiantCrabLag::RayNearPath(FVector::ZeroVector, FVector(100.0, 0.0, 0.0), Empty, 30.0f));
		});

		It("keeps the latest samples and gives the path over a span, its edges included", [this]()
		{
			FGiantCrabPoseHistory History;
			TArray<FVector> Path;
			History.Reset(2, 4);
			TestFalse(TEXT("empty"), History.GetPath(0, 0.0, 1.0, Path));
			for (int32 Step = 0; Step <= 5; ++Step)
			{
				const TArray<FVector> Points = { FVector(10.0 * Step, 0.0, 0.0), FVector(0.0, 10.0 * Step, 0.0) };
				History.Add(static_cast<double>(Step), Points);
			}
			TestEqual(TEXT("four kept"), History.Num(), 4);
			TestTrue(TEXT("span 2.5 - 3.5"), History.GetPath(0, 2.5, 3.5, Path));
			if (TestEqual(TEXT("the samples around and in it"), Path.Num(), 3))
			{
				TestEqual(TEXT("before"), Path[0].X, 20.0, 0.01);
				TestEqual(TEXT("inside"), Path[1].X, 30.0, 0.01);
				TestEqual(TEXT("after"), Path[2].X, 40.0, 0.01);
			}
			TestTrue(TEXT("after the newest"), History.GetPath(1, 10.0, 11.0, Path));
			TestTrue(TEXT("the newest"), Path.Num() == 1 && FMath::IsNearlyEqual(Path[0].Y, 50.0));
			TestTrue(TEXT("before the oldest"), History.GetPath(0, -5.0, -4.0, Path));
			TestTrue(TEXT("the oldest kept"), Path.Num() == 1 && FMath::IsNearlyEqual(Path[0].X, 20.0));
			TestFalse(TEXT("no such point"), History.GetPath(2, 2.0, 3.0, Path));
		});
	});

	Describe("Fixed shapes for the pawns", [this]()
	{
		It("hang from the root where the bodies stand at rest (the kit faces +Y, the crab +X)", [this]()
		{
			const FTransform MeshInRoot(FRotator(0.0, -90.0, 0.0), FVector(0.0, 0.0, -100.0));
			const FTransform Shell = GiantCrabMath::RestInRoot(FTransform(FQuat::Identity, FVector(0.0, 25.0, 160.0)), MeshInRoot);
			TestEqual(TEXT("the shell 25 cm ahead of the capsule, 60 cm up"), Shell.GetTranslation(), FVector(25.0, 0.0, 60.0), 0.01f);
		});

		It("are upright pillars on every machine (T-328), and the compensation stays bounded", [this]()
		{
			const UGiantCrabRules* Rules = GetDefault<UGiantCrabRules>();
			TestFalse(TEXT("pillars, not copies of the bodies"), Rules->PawnPillars.IsEmpty());
			TestTrue(TEXT("the shapes stop the players"), Rules->bHitBodiesBlockPawns);
			TestTrue(TEXT("never more than half a second back"), Rules->LagCompensationMaxSeconds > 0.0f && Rules->LagCompensationMaxSeconds <= 0.5f);
			TestTrue(TEXT("history finer than the window"), Rules->WeakPointSampleSeconds <= 2.0f * Rules->LagWindowSeconds);
		});
	});
}

#endif
