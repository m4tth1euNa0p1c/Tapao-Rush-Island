// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"

#if WITH_DEV_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Creatures.GiantCrab (T-261: the boss; the 16 tests of T-211 stay
// in GiantCrabSpec.cpp)
BEGIN_DEFINE_SPEC(FGiantCrabBossSpec, "monke_island.Creatures.GiantCrab.Boss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const UGiantCrabRules* Rules = nullptr;
END_DEFINE_SPEC(FGiantCrabBossSpec)

void FGiantCrabBossSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = GetDefault<UGiantCrabRules>();
	});

	Describe("Phases", [this]()
	{
		It("begins its phases at 66 % and 33 % of its health, not before", [this]()
		{
			const TArray<float> Thresholds = { 1.0f, 0.66f, 0.33f };
			TestEqual(TEXT("full"), GiantCrabMath::PhaseForHealth(1.0f, Thresholds), 0);
			TestEqual(TEXT("67 %"), GiantCrabMath::PhaseForHealth(0.67f, Thresholds), 0);
			TestEqual(TEXT("66 %"), GiantCrabMath::PhaseForHealth(0.66f, Thresholds), 1);
			TestEqual(TEXT("half"), GiantCrabMath::PhaseForHealth(0.5f, Thresholds), 1);
			TestEqual(TEXT("34 %"), GiantCrabMath::PhaseForHealth(0.34f, Thresholds), 1);
			TestEqual(TEXT("33 %"), GiantCrabMath::PhaseForHealth(0.33f, Thresholds), 2);
			TestEqual(TEXT("dying"), GiantCrabMath::PhaseForHealth(0.01f, Thresholds), 2);
			const TArray<float> NoPhases;
			TestEqual(TEXT("no phases"), GiantCrabMath::PhaseForHealth(0.1f, NoPhases), 0);
		});

		It("never goes back to an earlier phase, and a big blow may skip one", [this]()
		{
			const TArray<float> Thresholds = { 1.0f, 0.66f, 0.33f };
			TestEqual(TEXT("healed"), GiantCrabMath::NextPhase(2, 0), 2);
			TestEqual(TEXT("70 % to 20 % in one blow"), GiantCrabMath::NextPhase(0, GiantCrabMath::PhaseForHealth(0.2f, Thresholds)), 2);
		});

		It("has three phases, each quicker, tougher and more aggressive than the one before", [this]()
		{
			if (!TestEqual(TEXT("phases"), Rules->Phases.Num(), 3))
			{
				return;
			}
			const FGiantCrabPhaseRules& One = Rules->Phases[0];
			const FGiantCrabPhaseRules& Two = Rules->Phases[1];
			const FGiantCrabPhaseRules& Three = Rules->Phases[2];
			TestEqual(TEXT("thresholds"), FVector(One.HealthFraction, Two.HealthFraction, Three.HealthFraction), FVector(1.0, 0.66, 0.33), 0.001f);
			TestTrue(TEXT("phase 1 quicker blows than the T-211 crab"), One.AttackPlayRate > 1.0f);
			TestTrue(TEXT("faster each phase"), One.MoveSpeedScale < Two.MoveSpeedScale && Two.MoveSpeedScale < Three.MoveSpeedScale);
			TestTrue(TEXT("shorter wind-ups each phase"), One.AttackPlayRate < Two.AttackPlayRate && Two.AttackPlayRate < Three.AttackPlayRate);
			TestTrue(TEXT("shorter recoveries each phase"), One.RecoveryScale > Two.RecoveryScale && Two.RecoveryScale > Three.RecoveryScale);
			TestTrue(TEXT("tougher each phase"), One.DamageTakenScale > Two.DamageTakenScale && Two.DamageTakenScale > Three.DamageTakenScale);
			TestTrue(TEXT("roars from phase 2"), !One.bRoar && Two.bRoar && Three.bRoar);
			TestTrue(TEXT("frenzy in phase 3 only"), !One.bFrenzy && !Two.bFrenzy && Three.bFrenzy);
			TestTrue(TEXT("no stomp nor charge in phase 1"), One.StompWeight == 0.0f && One.ChargeWeight == 0.0f);
			TestTrue(TEXT("stomps and charges from phase 2"), Two.StompWeight > 0.0f && Two.ChargeWeight > 0.0f && Three.ChargeWeight > 0.0f);
			TestTrue(TEXT("swipe then slam in phase 2"), Two.Combos.ContainsByPredicate([](const FGiantCrabComboLink& Link)
				{ return Link.From == EGiantCrabAttack::Swipe && Link.To == EGiantCrabAttack::Slam; }) && Two.ComboChance > 0.0f);
			TestTrue(TEXT("grab after grab in phase 3"), Three.Combos.ContainsByPredicate([](const FGiantCrabComboLink& Link)
				{ return Link.From == EGiantCrabAttack::Grab && Link.To == EGiantCrabAttack::Grab; }) && Three.ComboChance > Two.ComboChance);
		});

		It("is nearly unbeatable, never invincible: invulnerable only while it roars", [this]()
		{
			TestFalse(TEXT("not invincible by default"), Rules->bInvulnerable);
			TestTrue(TEXT("the roar takes no damage"), Rules->Roar.bInvulnerable);
			TestFalse(TEXT("shown as a boss"), Rules->BossName.IsEmpty());
			TestTrue(TEXT("boss bar"), Rules->bBossBar);
		});

		It("walks, scuttles and turns much faster than the T-211 crab", [this]()
		{
			// T-211: 100 cm/s sideways, 60 forward, 260 scuttling, 75 deg/s, walk clips up to 3 times their speed
			TestTrue(TEXT("sideways"), Rules->SideSpeed >= 200.0f);
			TestTrue(TEXT("forward"), Rules->ForwardSpeed >= 100.0f);
			TestTrue(TEXT("scuttle"), Rules->ScuttleSpeed >= 1.5f * 260.0f);
			TestTrue(TEXT("turn"), Rules->TurnRateDegrees >= 1.5f * 75.0f);
			TestTrue(TEXT("walk clips"), Rules->MaxLocomotionPlayRate > 3.0f);
		});
	});

	Describe("Belly slam", [this]()
	{
		It("finds a prey under its shell, not beside its legs, in front of its claws nor on its back", [this]()
		{
			const FVector2D Center(25.0f, 0.0f);
			const FVector2D Radii(110.0f, 125.0f);
			TestTrue(TEXT("under the middle"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 0.0f, FVector(25.0, 0.0, 0.0), Center, Radii, 120.0f));
			TestTrue(TEXT("under the front right"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 0.0f, FVector(100.0, 50.0, 0.0), Center, Radii, 120.0f));
			TestFalse(TEXT("beside the legs"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 0.0f, FVector(25.0, 200.0, 0.0), Center, Radii, 120.0f));
			TestFalse(TEXT("in front of the claws"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 0.0f, FVector(300.0, 0.0, 0.0), Center, Radii, 120.0f));
			TestFalse(TEXT("on its back"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 0.0f, FVector(25.0, 0.0, 190.0), Center, Radii, 120.0f));
		});

		It("turns its footprint with the crab", [this]()
		{
			// facing +Y: 130 cm ahead is under the shell (it reaches 135 ahead), 130 cm aside is past it (125)
			const FVector2D Center(25.0f, 0.0f);
			const FVector2D Radii(110.0f, 125.0f);
			TestTrue(TEXT("ahead"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 90.0f, FVector(0.0, 130.0, 0.0), Center, Radii, 120.0f));
			TestFalse(TEXT("aside"), GiantCrabMath::IsUnderShell(FVector::ZeroVector, 90.0f, FVector(130.0, 0.0, 0.0), Center, Radii, 120.0f));
		});

		It("drops on a prey that stayed 0.4 s under it, not on one that passes through", [this]()
		{
			float Time = 0.0f;
			for (int32 Step = 0; Step < 3; ++Step)
			{
				Time = GiantCrabMath::UnderShellTime(Time, true, 0.1f);
			}
			TestTrue(TEXT("0.3 s: not yet"), Time < Rules->BellySlam.TriggerSeconds);
			Time = GiantCrabMath::UnderShellTime(Time, true, 0.1f);
			TestTrue(TEXT("0.4 s: it drops"), Time >= Rules->BellySlam.TriggerSeconds - UE_KINDA_SMALL_NUMBER);
			TestEqual(TEXT("out from under it: back to 0"), GiantCrabMath::UnderShellTime(Time, false, 0.1f), 0.0f);
			TestEqual(TEXT("trigger of the rules"), Rules->BellySlam.TriggerSeconds, 0.4f, 0.001f);
		});
	});

	Describe("Hit zones", [this]()
	{
		It("names the part of every bone of the kit", [this]()
		{
			TestTrue(TEXT("eye"), GiantCrabMath::ZoneOfBone(TEXT("eye_l")) == EGiantCrabZone::Eye);
			TestTrue(TEXT("eye stalk"), GiantCrabMath::ZoneOfBone(TEXT("eye_stalk_r")) == EGiantCrabZone::Eye);
			TestTrue(TEXT("mouth"), GiantCrabMath::ZoneOfBone(TEXT("mouth")) == EGiantCrabZone::Mouth);
			TestTrue(TEXT("mandible"), GiantCrabMath::ZoneOfBone(TEXT("mandible_l")) == EGiantCrabZone::Mouth);
			TestTrue(TEXT("belly"), GiantCrabMath::ZoneOfBone(TEXT("abdomen")) == EGiantCrabZone::Belly);
			TestTrue(TEXT("claw joint"), GiantCrabMath::ZoneOfBone(TEXT("claw_r_forearm")) == EGiantCrabZone::Joint);
			TestTrue(TEXT("claw hand"), GiantCrabMath::ZoneOfBone(TEXT("claw_l_hand")) == EGiantCrabZone::Claw);
			TestTrue(TEXT("pincer"), GiantCrabMath::ZoneOfBone(TEXT("claw_r_finger")) == EGiantCrabZone::Claw);
			TestTrue(TEXT("claw arm"), GiantCrabMath::ZoneOfBone(TEXT("claw_l_arm")) == EGiantCrabZone::Claw);
			TestTrue(TEXT("leg"), GiantCrabMath::ZoneOfBone(TEXT("leg_l2_merus")) == EGiantCrabZone::Leg);
			TestTrue(TEXT("leg tip"), GiantCrabMath::ZoneOfBone(TEXT("leg_r4_dactyl")) == EGiantCrabZone::Leg);
			TestTrue(TEXT("hip"), GiantCrabMath::ZoneOfBone(TEXT("leg_l1_coxa")) == EGiantCrabZone::Leg);
			TestTrue(TEXT("shell"), GiantCrabMath::ZoneOfBone(TEXT("body")) == EGiantCrabZone::Shell);
			TestTrue(TEXT("root"), GiantCrabMath::ZoneOfBone(TEXT("root")) == EGiantCrabZone::Shell);
			TestTrue(TEXT("antenna"), GiantCrabMath::ZoneOfBone(TEXT("antenna_l_01")) == EGiantCrabZone::Shell);
			TestTrue(TEXT("no bone"), GiantCrabMath::ZoneOfBone(NAME_None) == EGiantCrabZone::Shell);
		});

		It("covers the shell, the belly, both claws and their joints, both eyes and the eight legs", [this]()
		{
			TMap<EGiantCrabZone, int32> Count;
			for (const FGiantCrabHitBody& Body : Rules->HitBodies)
			{
				++Count.FindOrAdd(Body.Zone);
				TestTrue(*FString::Printf(TEXT("zone of %s"), *Body.Bone.ToString()), GiantCrabMath::ZoneOfBone(Body.Bone) == Body.Zone);
			}
			TestEqual(TEXT("shell"), Count.FindRef(EGiantCrabZone::Shell), 1);
			TestEqual(TEXT("belly"), Count.FindRef(EGiantCrabZone::Belly), 1);
			TestEqual(TEXT("claws: arm, hand, pincer"), Count.FindRef(EGiantCrabZone::Claw), 6);
			TestEqual(TEXT("claw joints"), Count.FindRef(EGiantCrabZone::Joint), 2);
			TestEqual(TEXT("eyes and stalks"), Count.FindRef(EGiantCrabZone::Eye), 4);
			TestEqual(TEXT("8 legs of 3 segments"), Count.FindRef(EGiantCrabZone::Leg), 24);
		});

		It("caps a blow, then applies the share of the part and the resistance of the phase", [this]()
		{
			TestEqual(TEXT("musket in the eye, phase 3"), GiantCrabMath::IncomingDamage(10000.0f, 100.0f, 3.0f, 0.65f, false), 195.0f, 0.01f);
			TestEqual(TEXT("cutlass on the shell, phase 1"), GiantCrabMath::IncomingDamage(35.0f, 150.0f, 0.1f, 1.0f, false), 3.5f, 0.01f);
			TestEqual(TEXT("cutlass on a leg, phase 2"), GiantCrabMath::IncomingDamage(35.0f, 120.0f, 0.5f, 0.8f, false), 14.0f, 0.01f);
			TestEqual(TEXT("no cap"), GiantCrabMath::IncomingDamage(500.0f, 0.0f, 1.0f, 1.0f, false), 500.0f, 0.01f);
			TestEqual(TEXT("invulnerable"), GiantCrabMath::IncomingDamage(500.0f, 150.0f, 4.0f, 1.0f, true), 0.0f);
			TestEqual(TEXT("no negative blow"), GiantCrabMath::IncomingDamage(-20.0f, 150.0f, 1.0f, 1.0f, false), 0.0f);
		});

		It("keeps the fast limbs out of the camera channel until the weapons trace on their own", [this]()
		{
			TestTrue(TEXT("legs are limbs"), GiantCrabMath::IsLimbZone(EGiantCrabZone::Leg) && GiantCrabMath::IsLimbZone(EGiantCrabZone::Claw));
			TestFalse(TEXT("the shell is not"), GiantCrabMath::IsLimbZone(EGiantCrabZone::Shell));
			TestTrue(TEXT("weak points"), GiantCrabMath::IsWeakZone(EGiantCrabZone::Eye) && GiantCrabMath::IsWeakZone(EGiantCrabZone::Belly)
				&& GiantCrabMath::IsWeakZone(EGiantCrabZone::Joint) && GiantCrabMath::IsWeakZone(EGiantCrabZone::Mouth));
			TestFalse(TEXT("limbs stop no shot by default"), Rules->bLimbsBlockShots);
			TestTrue(TEXT("the bodies stop the players"), Rules->bHitBodiesBlockPawns);
		});
	});

	Describe("Steps", [this]()
	{
		It("sounds once when a tip lifts and touches the sand again", [this]()
		{
			bool bLifted = false;
			TestFalse(TEXT("standing"), GiantCrabMath::FootStep(0.0f, 6.0f, 2.5f, bLifted));
			TestFalse(TEXT("lifted"), GiantCrabMath::FootStep(10.0f, 6.0f, 2.5f, bLifted));
			TestTrue(TEXT("down"), GiantCrabMath::FootStep(1.0f, 6.0f, 2.5f, bLifted));
			TestFalse(TEXT("still down"), GiantCrabMath::FootStep(1.0f, 6.0f, 2.5f, bLifted));
			TestFalse(TEXT("a twitch under the lift"), GiantCrabMath::FootStep(4.0f, 6.0f, 2.5f, bLifted));
			TestFalse(TEXT("back down after the twitch"), GiantCrabMath::FootStep(1.0f, 6.0f, 2.5f, bLifted));
		});

		It("finds the notifies a clip passes, across its loop", [this]()
		{
			const TArray<float> Times = { 0.0f, 0.4f };
			TArray<int32> Crossed;
			GiantCrabMath::CrossedTimes(Times, 0.35f, 0.45f, 0.8f, true, Crossed);
			TestTrue(TEXT("middle"), Crossed.Num() == 1 && Crossed[0] == 1);
			GiantCrabMath::CrossedTimes(Times, 0.75f, 0.05f, 0.8f, true, Crossed);
			TestTrue(TEXT("round the loop"), Crossed.Num() == 1 && Crossed[0] == 0);
			GiantCrabMath::CrossedTimes(Times, -0.001f, 0.1f, 0.8f, true, Crossed);
			TestTrue(TEXT("clip start"), Crossed.Num() == 1 && Crossed[0] == 0);
			GiantCrabMath::CrossedTimes(Times, 0.5f, 0.7f, 0.8f, true, Crossed);
			TestEqual(TEXT("none"), Crossed.Num(), 0);
		});

		It("plays the walk clips at the speed of the body, within bounds", [this]()
		{
			TestEqual(TEXT("2.4 times"), GiantCrabMath::LocomotionRate(240.0f, 100.0f, 0.3f, 4.5f), 2.4f, 0.001f);
			TestEqual(TEXT("at most"), GiantCrabMath::LocomotionRate(1000.0f, 100.0f, 0.3f, 4.5f), 4.5f, 0.001f);
			TestEqual(TEXT("at least"), GiantCrabMath::LocomotionRate(10.0f, 100.0f, 0.3f, 4.5f), 0.3f, 0.001f);
			TestEqual(TEXT("clip without speed"), GiantCrabMath::LocomotionRate(100.0f, 0.0f, 0.3f, 4.5f), 1.0f);
		});

		It("has eight feet, the hind ones heavy", [this]()
		{
			TestEqual(TEXT("feet"), Rules->Feet.Num(), 8);
			int32 Heavy = 0;
			for (const FGiantCrabFoot& Foot : Rules->Feet)
			{
				Heavy += Foot.bHeavy ? 1 : 0;
				TestTrue(*FString::Printf(TEXT("%s is a leg tip"), *Foot.Bone.ToString()), Foot.Bone.ToString().EndsWith(TEXT("_dactyl")));
				// manifest v2 of T-260: the tip at the end of the dactyl, 78.2 to 92 cm along it
				TestTrue(*FString::Printf(TEXT("%s tip along the bone"), *Foot.Bone.ToString()),
					Foot.TipOffsetCm.Y <= -78.0 && Foot.TipOffsetCm.Y >= -92.5 && FMath::IsNearlyZero(Foot.TipOffsetCm.X));
				if (Foot.bHeavy)
				{
					TestTrue(*FString::Printf(TEXT("%s is a hind leg"), *Foot.Bone.ToString()),
						Foot.Bone.ToString().Contains(TEXT("3_")) || Foot.Bone.ToString().Contains(TEXT("4_")));
				}
			}
			TestEqual(TEXT("hind feet"), Heavy, 4);
		});
	});

	Describe("Charge and shakes", [this]()
	{
		It("turns a flank to its prey before it charges, sideways like a crab", [this]()
		{
			TestEqual(TEXT("already at its right"), GiantCrabMath::SideTurn(FVector::ForwardVector, FVector(0.0, 100.0, 0.0)), 0.0f, 0.01f);
			TestEqual(TEXT("ahead: a quarter turn left puts it at the right"), GiantCrabMath::SideTurn(FVector::ForwardVector, FVector(100.0, 0.0, 0.0)),
				-90.0f, 0.01f);
			// behind it, a little to its left: a quarter turn left puts it at its left flank
			TestEqual(TEXT("behind on the left"), GiantCrabMath::SideTurn(FVector::ForwardVector, FVector(-100.0, -0.01, 0.0)), -90.0f, 0.1f);
			TestEqual(TEXT("behind on the right"), GiantCrabMath::SideTurn(FVector::ForwardVector, FVector(-100.0, 0.01, 0.0)), 90.0f, 0.1f);
			TestEqual(TEXT("speed of the run clips of T-260"), Rules->Charge.ClipSpeedCmps, 450.0f);
		});

		It("runs through its prey and stops past it, never farther than its reach", [this]()
		{
			TestEqual(TEXT("past the prey"), GiantCrabMath::ChargeEnd(FVector::ZeroVector, FVector(1000.0, 0.0, 50.0), 500.0f, 3100.0f), FVector(1500.0, 0.0, 0.0), 0.01f);
			TestEqual(TEXT("at most its reach"), GiantCrabMath::ChargeEnd(FVector::ZeroVector, FVector(0.0, 2800.0, 0.0), 500.0f, 3100.0f), FVector(0.0, 3100.0, 0.0), 0.01f);
			TestEqual(TEXT("prey on it"), GiantCrabMath::ChargeEnd(FVector(5.0, 5.0, 0.0), FVector(5.0, 5.0, 0.0), 500.0f, 3100.0f), FVector(5.0, 5.0, 0.0), 0.01f);
		});

		It("shakes the cameras fully near the source, fading out with the distance", [this]()
		{
			TestEqual(TEXT("near"), GiantCrabMath::ShakeFalloff(100.0f, 400.0f, 2500.0f), 1.0f);
			TestEqual(TEXT("half way"), GiantCrabMath::ShakeFalloff(1450.0f, 400.0f, 2500.0f), 0.5f, 0.001f);
			TestEqual(TEXT("far"), GiantCrabMath::ShakeFalloff(2600.0f, 400.0f, 2500.0f), 0.0f);
		});
	});
}

#endif
