// monke_island

#include "Misc/AutomationTest.h"
#include "Arena/ArenaZone.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/GiantCrabMath.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGiantCrabSpec, "monke_island.Creatures.GiantCrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGiantCrabSpec)

void FGiantCrabSpec::Define()
{
	Describe("PickTarget", [this]()
	{
		It("hunts the nearest living thing", [this]()
		{
			TestEqual(TEXT("nearest"), GiantCrabMath::PickTarget({ 900.0f, 400.0f, 1200.0f }, INDEX_NONE, 300.0f), 1);
		});

		It("keeps its prey unless another one is much closer", [this]()
		{
			TestEqual(TEXT("kept"), GiantCrabMath::PickTarget({ 600.0f, 400.0f }, 0, 300.0f), 0);
			TestEqual(TEXT("switched"), GiantCrabMath::PickTarget({ 900.0f, 400.0f }, 0, 300.0f), 1);
		});

		It("ignores the ones that are not candidates (outside the arena, dead)", [this]()
		{
			TestEqual(TEXT("none"), GiantCrabMath::PickTarget({ -1.0f, -1.0f }, INDEX_NONE, 300.0f), static_cast<int32>(INDEX_NONE));
			TestEqual(TEXT("lost prey"), GiantCrabMath::PickTarget({ -1.0f, 800.0f }, 0, 300.0f), 1);
		});
	});

	Describe("PickAttack", [this]()
	{
		It("only draws blows whose reach covers the distance", [this]()
		{
			// swipe 320, slam 280, grab 220: at 300 cm only the swipe reaches
			TestEqual(TEXT("swipe"), GiantCrabMath::PickAttack({ 320.0f, 280.0f, 220.0f }, { 1.0f, 1.0f, 1.0f }, 300.0f, 0.99f), 0);
			TestEqual(TEXT("out of reach"), GiantCrabMath::PickAttack({ 320.0f, 280.0f, 220.0f }, { 1.0f, 1.0f, 1.0f }, 400.0f, 0.5f),
				static_cast<int32>(INDEX_NONE));
		});

		It("follows the weights among the blows in reach", [this]()
		{
			const TArray<float> Reaches = { 320.0f, 280.0f, 220.0f };
			const TArray<float> Weights = { 2.0f, 1.0f, 1.0f };
			TestEqual(TEXT("low roll"), GiantCrabMath::PickAttack(Reaches, Weights, 100.0f, 0.1f), 0);
			TestEqual(TEXT("mid roll"), GiantCrabMath::PickAttack(Reaches, Weights, 100.0f, 0.6f), 1);
			TestEqual(TEXT("high roll"), GiantCrabMath::PickAttack(Reaches, Weights, 100.0f, 0.9f), 2);
		});

		It("never draws a blow of weight zero", [this]()
		{
			TestEqual(TEXT("no grab"), GiantCrabMath::PickAttack({ 320.0f, 280.0f, 220.0f }, { 1.0f, 1.0f, 0.0f }, 100.0f, 0.999f), 1);
		});
	});

	Describe("Weak points", [this]()
	{
		It("turns over after the third hit within the window", [this]()
		{
			TArray<double> Hits;
			TestFalse(TEXT("first"), GiantCrabMath::RecordWeakHit(Hits, 10.0, 6.0f, 3));
			TestFalse(TEXT("second"), GiantCrabMath::RecordWeakHit(Hits, 12.0, 6.0f, 3));
			TestTrue(TEXT("third"), GiantCrabMath::RecordWeakHit(Hits, 15.0, 6.0f, 3));
		});

		It("forgets the hits older than the window", [this]()
		{
			TArray<double> Hits;
			GiantCrabMath::RecordWeakHit(Hits, 0.0, 6.0f, 3);
			GiantCrabMath::RecordWeakHit(Hits, 2.0, 6.0f, 3);
			TestFalse(TEXT("late third"), GiantCrabMath::RecordWeakHit(Hits, 9.0, 6.0f, 3));
			TestEqual(TEXT("kept"), Hits.Num(), 1);
		});
	});

	Describe("Facing and walking", [this]()
	{
		It("measures the angle to the target, right positive", [this]()
		{
			TestEqual(TEXT("ahead"), GiantCrabMath::SignedAngle(FVector::ForwardVector, FVector(100.0, 0.0, 0.0)), 0.0f, 0.01f);
			TestEqual(TEXT("right"), GiantCrabMath::SignedAngle(FVector::ForwardVector, FVector(0.0, 100.0, 0.0)), 90.0f, 0.01f);
			TestEqual(TEXT("left"), GiantCrabMath::SignedAngle(FVector::ForwardVector, FVector(0.0, -100.0, 50.0)), -90.0f, 0.01f);
			TestTrue(TEXT("in front"), GiantCrabMath::IsInFront(FVector::ForwardVector, FVector(100.0, 50.0, 0.0), 55.0f));
			TestFalse(TEXT("behind"), GiantCrabMath::IsInFront(FVector::ForwardVector, FVector(-100.0, 10.0, 0.0), 55.0f));
		});

		It("closes in while circling, then only circles in reach", [this]()
		{
			const FVector Far = GiantCrabMath::WalkDirection(FVector(1000.0, 0.0, 0.0), 800.0f, 320.0f, 0.35f, 1.0f);
			TestTrue(TEXT("toward"), Far.X > 0.9f);
			// (1, 0.35) brought back to a unit length
			TestEqual(TEXT("circles"), static_cast<float>(Far.Y), 0.35f / FMath::Sqrt(1.0f + 0.35f * 0.35f), 0.01f);
			const FVector Near = GiantCrabMath::WalkDirection(FVector(300.0, 0.0, 0.0), 200.0f, 320.0f, 0.35f, -1.0f);
			TestEqual(TEXT("no closer"), static_cast<float>(Near.X), 0.0f, 0.01f);
			TestEqual(TEXT("other side"), static_cast<float>(Near.Y), -0.35f, 0.01f);
		});

		It("measures between the edges of the bodies", [this]()
		{
			TestEqual(TEXT("edges"), GiantCrabMath::EdgeDistance(FVector::ZeroVector, 95.0f, FVector(300.0, 400.0, 80.0), 30.0f), 375.0f, 0.01f);
			TestEqual(TEXT("overlap"), GiantCrabMath::EdgeDistance(FVector::ZeroVector, 95.0f, FVector(50.0, 0.0, 0.0), 30.0f), 0.0f);
		});

		It("throws the struck one away from the blow and up", [this]()
		{
			const FVector Push = GiantCrabMath::Knockback(FVector::ZeroVector, FVector(0.0, 200.0, 40.0), 600.0f, 250.0f);
			TestEqual(TEXT("away"), static_cast<float>(Push.Y), 600.0f, 0.01f);
			TestEqual(TEXT("up"), static_cast<float>(Push.Z), 250.0f, 0.01f);
		});
	});

	Describe("Throw", [this]()
	{
		It("flings the caught one over the left claw, up at the angle, at the speed", [this]()
		{
			// crab facing +X: over its left claw is local (0.6, -0.8), ahead and to the left
			const FVector Throw = GiantCrabMath::ThrowVelocity(FRotator::ZeroRotator, 2400.0f, 38.0f);
			TestEqual(TEXT("speed"), static_cast<float>(Throw.Size()), 2400.0f, 0.1f);
			TestEqual(TEXT("up"), static_cast<float>(Throw.Z), 2400.0f * FMath::Sin(FMath::DegreesToRadians(38.0f)), 0.1f);
			const FVector Ground = FVector(Throw.X, Throw.Y, 0.0).GetSafeNormal();
			TestEqual(TEXT("ahead"), static_cast<float>(Ground.X), 0.6f, 0.001f);
			TestEqual(TEXT("left"), static_cast<float>(Ground.Y), -0.8f, 0.001f);
		});

		It("turns the throw with the crab", [this]()
		{
			// facing +Y (yaw 90): local (0.6, -0.8) becomes (0.8, 0.6) in the world
			const FVector Throw = GiantCrabMath::ThrowVelocity(FRotator(0.0f, 90.0f, 0.0f), 900.0f, 35.0f);
			const float Ground = 900.0f * FMath::Cos(FMath::DegreesToRadians(35.0f));
			TestEqual(TEXT("x"), static_cast<float>(Throw.X), 0.8f * Ground, 0.1f);
			TestEqual(TEXT("y"), static_cast<float>(Throw.Y), 0.6f * Ground, 0.1f);
			TestEqual(TEXT("speed"), static_cast<float>(Throw.Size()), 900.0f, 0.1f);
		});
	});

	Describe("Arena", [this]()
	{
		It("holds the giant crab in the zone with the tiki warriors", [this]()
		{
			const AArenaZone* Defaults = GetDefault<AArenaZone>();
			TestTrue(TEXT("crab confined"), Defaults->ExtraConfinedClasses.Contains(AGiantCrab::StaticClass()));
			TestTrue(TEXT("crab instance"), Defaults->IsConfined(GetDefault<AGiantCrab>()));
		});

		It("does not hunt another giant crab, nor a thing without health", [this]()
		{
			TestFalse(TEXT("crab"), AGiantCrab::IsPrey(GetDefault<AGiantCrab>()));
			TestFalse(TEXT("null"), AGiantCrab::IsPrey(nullptr));
		});
	});
}

#endif
