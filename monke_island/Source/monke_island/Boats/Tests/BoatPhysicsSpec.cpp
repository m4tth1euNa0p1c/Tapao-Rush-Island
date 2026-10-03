// monke_island

#include "Misc/AutomationTest.h"
#include "Boats/BoatPhysics.h"
#include "Boats/BoatRules.h"
#include "Player/IslanderMovementComponent.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Boats
BEGIN_DEFINE_SPEC(FBoatPhysicsSpec, "monke_island.Boats.Physics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UBoatRules* Rules = nullptr;
END_DEFINE_SPEC(FBoatPhysicsSpec)

void FBoatPhysicsSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UBoatRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Buoyancy", [this]()
	{
		It("holds up FloatCapacityKg when every point is fully immersed", [this]()
		{
			constexpr int32 Points = 10;
			float Total = 0.0f;
			for (int32 Index = 0; Index < Points; ++Index)
			{
				Total += BoatPhysics::PointBuoyancy(Rules->HullDepthCm * 2.0f, Rules->HullDepthCm, Rules->FloatCapacityKg, Points);
			}
			TestEqual(TEXT("force"), Total, Rules->FloatCapacityKg * BoatPhysics::GravityCm, 1.0f);
		});

		It("gives nothing out of the water", [this]()
		{
			TestEqual(TEXT("force"), BoatPhysics::PointBuoyancy(-5.0f, Rules->HullDepthCm, Rules->FloatCapacityKg, 10), 0.0f);
		});

		It("floats the empty hull low in the water (about 16 cm) and 8 players under the gunwale", [this]()
		{
			const float Empty = BoatPhysics::EquilibriumImmersion(Rules->HullMassKg, Rules->HullDepthCm, Rules->FloatCapacityKg);
			TestTrue(TEXT("empty hull immersion 10-25 cm"), Empty > 10.0f && Empty < 25.0f);
			const float Full = BoatPhysics::EquilibriumImmersion(Rules->HullMassKg + 8.0f * Rules->PersonMassKg, Rules->HullDepthCm, Rules->FloatCapacityKg);
			TestTrue(TEXT("480 kg aboard keeps freeboard"), Full < Rules->HullDepthCm);
		});
	});

	Describe("Load", [this]()
	{
		It("weighs each player PersonMassKg (60 kg) plus the objects", [this]()
		{
			TestEqual(TEXT("load"), BoatPhysics::LoadKg(3, Rules->PersonMassKg, 25.0f), 205.0f, 0.01f);
		});

		It("uses the owner's values: 60 kg a player, 500 kg max", [this]()
		{
			TestEqual(TEXT("person"), Rules->PersonMassKg, 60.0f);
			TestEqual(TEXT("max"), Rules->MaxLoadKg, 500.0f);
		});
	});

	Describe("Flooding", [this]()
	{
		It("drains under the limit", [this]()
		{
			const float Rate = BoatPhysics::BilgeRate(400.0f, 500.0f, 15.0f, 0.6f, 6.0f, false, 80.0f);
			TestEqual(TEXT("rate"), Rate, -6.0f, 0.01f);
		});

		It("floods faster the more the boat is overloaded", [this]()
		{
			const float Small = BoatPhysics::BilgeRate(510.0f, 500.0f, 15.0f, 0.6f, 6.0f, false, 80.0f);
			const float Big = BoatPhysics::BilgeRate(600.0f, 500.0f, 15.0f, 0.6f, 6.0f, false, 80.0f);
			TestEqual(TEXT("10 kg over"), Small, 21.0f, 0.01f);
			TestTrue(TEXT("100 kg over floods faster"), Big > Small);
		});

		It("ships water through a submerged gunwale even under the limit", [this]()
		{
			TestEqual(TEXT("rate"), BoatPhysics::BilgeRate(100.0f, 500.0f, 15.0f, 0.6f, 6.0f, true, 80.0f), 80.0f, 0.01f);
		});

		It("sinks 9 players (540 kg) within half a minute", [this]()
		{
			float Water = 0.0f;
			float Time = 0.0f;
			while (Water < Rules->FloodCapacityKg && Time < 120.0f)
			{
				Water += BoatPhysics::BilgeRate(9.0f * Rules->PersonMassKg, Rules->MaxLoadKg, Rules->FloodRateKgPerS,
					Rules->FloodRatePerKgOver, Rules->DrainRateKgPerS, false, Rules->SwampRateKgPerS) * 0.1f;
				Time += 0.1f;
			}
			TestTrue(TEXT("sunk in under 30 s"), Time < 30.0f);
		});
	});

	Describe("Rowing", [this]()
	{
		It("pulls during the drive and glides during the recovery", [this]()
		{
			TestEqual(TEXT("start"), BoatPhysics::StrokeEnvelope(0.0f, 0.45f), 0.0f, 0.001f);
			TestEqual(TEXT("mid drive"), BoatPhysics::StrokeEnvelope(0.225f, 0.45f), 1.0f, 0.001f);
			TestEqual(TEXT("recovery"), BoatPhysics::StrokeEnvelope(0.7f, 0.45f), 0.0f);
		});

		It("rows straight with both oars and turns by pulling one side", [this]()
		{
			const FVector2D Straight = BoatPhysics::OarDrives(FVector2D(0.0f, 1.0f));
			TestTrue(TEXT("both oars"), Straight.Equals(FVector2D(1.0f, 1.0f)));
			const FVector2D Right = BoatPhysics::OarDrives(FVector2D(1.0f, 0.0f));
			TestTrue(TEXT("turning right: port pulls, starboard backs"), Right.X > 0.0f && Right.Y < 0.0f);
		});

		It("averages a rowing speed close to MaxRowSpeed (slow boat)", [this]()
		{
			// mean stroke force against the quadratic drag
			float Mean = 0.0f;
			for (int32 Step = 0; Step < 1000; ++Step)
			{
				Mean += BoatPhysics::StrokeEnvelope(Step / 1000.0f, Rules->StrokeDriveFraction) / 1000.0f;
			}
			const float Force = Rules->RowForceN * BoatPhysics::NewtonToUnreal * Mean;
			const float Speed = FMath::Sqrt(Force / Rules->ForwardQuadDrag);
			TestTrue(TEXT("cruise speed 1-2 m/s"), Speed > 100.0f && Speed < 200.0f);
		});
	});
}

BEGIN_DEFINE_SPEC(FSwimDrowningSpec, "monke_island.Boats.Drowning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UIslanderMovementComponent* Move = nullptr;
END_DEFINE_SPEC(FSwimDrowningSpec)

void FSwimDrowningSpec::Define()
{
	BeforeEach([this]()
	{
		Move = NewObject<UIslanderMovementComponent>();
		Move->AddToRoot();
	});

	AfterEach([this]()
	{
		Move->RemoveFromRoot();
		Move = nullptr;
	});

	It("does not hurt during the grace time", [this]()
	{
		float Damage = 0.0f;
		for (int32 Step = 0; Step < 20; ++Step)
		{
			Damage += Move->UpdateDrowning(0.1f, true);
		}
		TestEqual(TEXT("2 s drowning"), Damage, 0.0f);
	});

	It("hurts DrownDamagePerSecond after the grace time and stops once out of the water", [this]()
	{
		float Damage = 0.0f;
		const int32 Steps = FMath::RoundToInt((Move->DrownGraceTime + 2.0f) / 0.1f);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			Damage += Move->UpdateDrowning(0.1f, true);
		}
		TestEqual(TEXT("2 s of drowning damage"), Damage, 2.0f * Move->DrownDamagePerSecond, 0.5f);
		TestEqual(TEXT("saved"), Move->UpdateDrowning(1.0f, false), 0.0f);
		TestEqual(TEXT("clock reset"), Move->UpdateDrowning(1.0f, true), 0.0f);
	});

	It("spends less stamina treading water than swimming", [this]()
	{
		Move->UpdateStamina(2.0f, false, true, false);
		TestEqual(TEXT("idle swim"), Move->GetStamina(), Move->MaxStamina - 2.0f * Move->SwimIdleStaminaCost, 0.01f);
	});
}

#endif
