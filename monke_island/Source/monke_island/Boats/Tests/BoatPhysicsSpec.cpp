// monke_island

#include "Misc/AutomationTest.h"
#include "Boats/BoatPhysics.h"
#include "Boats/BoatRules.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
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
			TestTrue(TEXT("cruise speed 2-3 m/s empty (pleasant, still a rowing boat)"), Speed > 200.0f && Speed < 300.0f);
		});

		It("is slower the heavier it is: about 60 % of the empty speed at full load", [this]()
		{
			const float Empty = BoatPhysics::LoadDragScale(0.0f, Rules->MaxLoadKg, Rules->FullLoadDragScale);
			const float Half = BoatPhysics::LoadDragScale(0.5f * Rules->MaxLoadKg, Rules->MaxLoadKg, Rules->FullLoadDragScale);
			const float Full = BoatPhysics::LoadDragScale(Rules->MaxLoadKg, Rules->MaxLoadKg, Rules->FullLoadDragScale);
			TestEqual(TEXT("empty: no extra drag"), Empty, 1.0f);
			TestTrue(TEXT("heavier, more drag"), Half > Empty && Full > Half);
			const float Ratio = 1.0f / FMath::Sqrt(Full);
			TestTrue(TEXT("full load at 55-70 % of the empty speed"), Ratio > 0.55f && Ratio < 0.7f);
		});
	});

	Describe("Shore", [this]()
	{
		It("is pushed by a player walking into the hull, not by one walking past or away", [this]()
		{
			const FVector Into = BoatPhysics::PushDirection(FVector(500.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0), 10.0f, 45.0f, 0.5f);
			TestTrue(TEXT("walking into it pushes along the walk"), Into.Equals(FVector(1.0, 0.0, 0.0), 0.001));
			TestTrue(TEXT("walking along the hull does not"), BoatPhysics::PushDirection(FVector(0.0, 500.0, 0.0), FVector(100.0, 0.0, 0.0), 10.0f, 45.0f, 0.5f).IsZero());
			TestTrue(TEXT("walking away does not"), BoatPhysics::PushDirection(FVector(-500.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0), 10.0f, 45.0f, 0.5f).IsZero());
			TestTrue(TEXT("standing still does not"), BoatPhysics::PushDirection(FVector::ZeroVector, FVector(100.0, 0.0, 0.0), 0.0f, 45.0f, 0.5f).IsZero());
			TestTrue(TEXT("too far does not"), BoatPhysics::PushDirection(FVector(500.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0), 80.0f, 45.0f, 0.5f).IsZero());
		});

		It("lets one player shove an empty beached hull and four push it loaded", [this]()
		{
			// skidding keel on sand: friction force = mu * (1 - lifted share) * m * g
			const float Mu = Rules->SandFriction * (1.0f - Rules->SkidLiftFraction);
			const float Empty = Mu * Rules->HullMassKg * 9.8f;
			TestTrue(TEXT("one pusher beats the skid friction of the empty hull"), Rules->PushForcePerPersonN > Empty);
			const float Loaded = Mu * (Rules->HullMassKg + Rules->MaxLoadKg) * 9.8f;
			TestTrue(TEXT("the pushers allowed beat it at full load"), Rules->PushForcePerPersonN * Rules->MaxPushers > Loaded);
		});

		It("counts a stuck boat only while it is rowed, grounded and still", [this]()
		{
			float Time = 0.0f;
			for (int32 Step = 0; Step < 30; ++Step)
			{
				Time = BoatPhysics::StuckTime(Time, true, true, 5.0f, Rules->StuckSpeed, 0.1f);
			}
			TestTrue(TEXT("3 s of rowing in the sand"), Time >= Rules->UnstickAfterSeconds);
			TestEqual(TEXT("moving again resets"), BoatPhysics::StuckTime(Time, true, true, 80.0f, Rules->StuckSpeed, 0.1f), 0.0f);
			TestEqual(TEXT("not rowing resets"), BoatPhysics::StuckTime(Time, false, true, 0.0f, Rules->StuckSpeed, 0.1f), 0.0f);
			TestEqual(TEXT("afloat resets"), BoatPhysics::StuckTime(Time, true, false, 0.0f, Rules->StuckSpeed, 0.1f), 0.0f);
		});

		It("frees a stuck boat toward the deepest water, nowhere when it is all sand", [this]()
		{
			TestEqual(TEXT("deepest probe"), BoatPhysics::DeepestProbe({0.0f, 10.0f, 45.0f, 20.0f}, 0.0f), 2);
			TestEqual(TEXT("no way out"), BoatPhysics::DeepestProbe({0.0f, 3.0f, 0.0f}, 0.0f), INDEX_NONE);
		});
	});

	Describe("Seats", [this]()
	{
		// rider upside down in the keel (T-059): the FBX sockets come in with yaw 180, roll 180
		It("seats the rider upright above a flipped socket, facing the bow", [this]()
		{
			const FTransform Socket(FRotator(0.0, 180.0, 180.0), FVector(-145.0, -42.0, 19.5));
			const FTransform Rider = BoatPhysics::SeatedRiderRelative(Socket.GetRotation(), Rules->SeatedCapsuleOffsetCm) * Socket;
			TestTrue(TEXT("up is the hull's up"), Rider.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector, 0.001));
			TestTrue(TEXT("forward is the bow"), Rider.GetUnitAxis(EAxis::X).Equals(FVector::ForwardVector, 0.001));
			TestTrue(TEXT("capsule above the seat"), Rider.GetLocation().Equals(Socket.GetLocation() + FVector(0.0, 0.0, Rules->SeatedCapsuleOffsetCm), 0.01));
		});

		It("orients the oars in the hull frame whatever the oarlock socket rotation", [this]()
		{
			const FQuat Socket = FRotator(0.0, 180.0, 180.0).Quaternion();
			const FRotator Target(-20.0, -90.0, 0.0);
			const FQuat InHull = Socket * BoatPhysics::OarRelativeRotation(Socket, Target);
			TestTrue(TEXT("same rotation in the hull frame"), InHull.Equals(Target.Quaternion(), 0.0001));
		});

		It("seats every SM_Barque seat upright", [this]()
		{
			const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/MonkeIsland/Boat/Meshes/SM_Barque.SM_Barque"));
			if (!TestNotNull(TEXT("SM_Barque"), Mesh))
			{
				return;
			}
			int32 Seats = 0;
			for (const UStaticMeshSocket* Socket : Mesh->Sockets)
			{
				if (!Socket || !Socket->SocketName.ToString().StartsWith(Rules->SeatSocketPrefix))
				{
					continue;
				}
				++Seats;
				const FTransform SocketInHull(Socket->RelativeRotation, Socket->RelativeLocation);
				const FTransform Rider = BoatPhysics::SeatedRiderRelative(SocketInHull.GetRotation(), Rules->SeatedCapsuleOffsetCm) * SocketInHull;
				TestTrue(FString::Printf(TEXT("%s upright"), *Socket->SocketName.ToString()), Rider.GetUnitAxis(EAxis::Z).Z > 0.999);
				TestTrue(FString::Printf(TEXT("%s above the seat"), *Socket->SocketName.ToString()), Rider.GetLocation().Z > SocketInHull.GetLocation().Z);
			}
			TestEqual(TEXT("4 seats"), Seats, 4);
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

	It("keeps an exhausted swimmer afloat (no sinking) and kills it at once (the owner's rule)", [this]()
	{
		TestTrue(TEXT("buoyancy of an exhausted swimmer: floats"), Move->DrowningBuoyancy >= 1.0f);
		TestTrue(TEXT("out of stamina in the water: dead at once"), Move->bExhaustionKillsInWater);
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
