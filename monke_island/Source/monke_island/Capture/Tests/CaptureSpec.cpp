// monke_island

#include "Misc/AutomationTest.h"
#include "Capture/CaptureMath.h"
#include "Capture/CaptureRules.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/TikiVillager.h"
#include "Creatures/TikiWarrior.h"
#include "Fort/FortSoldier.h"
#include "Player/IslanderCharacter.h"
#include <limits>

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Capture
BEGIN_DEFINE_SPEC(FCaptureSpec, "monke_island.Capture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCaptureSpec)

void FCaptureSpec::Define()
{
	Describe("Rules", [this]()
	{
		It("asks two minutes of cage, 10 000 crowns a prisoner, the key F, 35 % of health and 2 m of reach (owner 2026-10-11)", [this]()
		{
			const UCaptureRules* Rules = GetDefault<UCaptureRules>();
			TestEqual(TEXT("cage"), Rules->CageLifetimeSeconds, 120.0f);
			TestEqual(TEXT("price"), Rules->SalePrice, 10000);
			TestTrue(TEXT("key F"), Rules->UseKeys.Contains(EKeys::F));
			TestEqual(TEXT("subdued"), Rules->SubduedHealthFraction, 0.35f);
			TestEqual(TEXT("reach"), Rules->CuffReachCm, 200.0f);
			TestEqual(TEXT("hold"), Rules->CuffHoldSeconds, 1.5f);
			TestTrue(TEXT("down counts"), Rules->bDownIsSubdued);
		});

		It("cuffs the French soldiers, the Veilleurs and the villagers, never the giant crab nor a player", [this]()
		{
			const UCaptureRules* Rules = GetDefault<UCaptureRules>();
			TestTrue(TEXT("soldier"), Rules->IsCapturableClass(AFortSoldier::StaticClass()));
			TestTrue(TEXT("Veilleur"), Rules->IsCapturableClass(ATikiWarrior::StaticClass()));
			TestTrue(TEXT("villager (owner 2026-10-11)"), Rules->IsCapturableClass(ATikiVillager::StaticClass()));
			TestFalse(TEXT("giant crab"), Rules->IsCapturableClass(AGiantCrab::StaticClass()));
			TestFalse(TEXT("islander"), Rules->IsCapturableClass(AIslanderCharacter::StaticClass()));
			TestFalse(TEXT("no class"), Rules->IsCapturableClass(nullptr));
		});

		It("has a message for every refusal", [this]()
		{
			const UCaptureRules* Rules = GetDefault<UCaptureRules>();
			for (int32 Value = static_cast<int32>(ECaptureRefusal::NoTarget); Value <= static_cast<int32>(ECaptureRefusal::Busy); ++Value)
			{
				const ECaptureRefusal Reason = static_cast<ECaptureRefusal>(Value);
				TestFalse(*FString::Printf(TEXT("text of %s"), *UEnum::GetValueAsString(Reason)), Rules->GetRefusalText(Reason).IsEmpty());
			}
			TestTrue(TEXT("none says nothing"), Rules->GetRefusalText(ECaptureRefusal::None).IsEmpty());
			TestTrue(TEXT("the health in the text"), Rules->GetRefusalText(ECaptureRefusal::NotSubdued, 0.62f).ToString().Contains(TEXT("62")));
		});
	});

	Describe("Cuffing", [this]()
	{
		It("takes a target at or under 35 % of its health, or down", [this]()
		{
			TestTrue(TEXT("30 %"), CaptureMath::IsSubdued(0.30f, false, 0.35f, true));
			TestTrue(TEXT("35 %"), CaptureMath::IsSubdued(0.35f, false, 0.35f, true));
			TestFalse(TEXT("36 %"), CaptureMath::IsSubdued(0.36f, false, 0.35f, true));
			TestTrue(TEXT("down at full health"), CaptureMath::IsSubdued(1.0f, true, 0.35f, true));
			TestFalse(TEXT("down does not count"), CaptureMath::IsSubdued(1.0f, true, 0.35f, false));
			TestFalse(TEXT("health not a number"), CaptureMath::IsSubdued(std::numeric_limits<float>::quiet_NaN(), false, 0.35f, true));
		});

		It("says the first reason against it, in order", [this]()
		{
			CaptureMath::FCuffFacts Facts;
			Facts.bHasTarget = true;
			Facts.bCapturable = true;
			Facts.bInReach = true;
			Facts.HealthFraction = 0.3f;
			TestEqual(TEXT("all good"), CaptureMath::JudgeCuff(Facts, 0.35f, true), ECaptureRefusal::None);

			CaptureMath::FCuffFacts Busy = Facts;
			Busy.bClaimedByOther = true;
			TestEqual(TEXT("someone else at it (one winner)"), CaptureMath::JudgeCuff(Busy, 0.35f, true), ECaptureRefusal::Busy);

			CaptureMath::FCuffFacts Healthy = Facts;
			Healthy.HealthFraction = 0.8f;
			TestEqual(TEXT("healthy"), CaptureMath::JudgeCuff(Healthy, 0.35f, true), ECaptureRefusal::NotSubdued);

			CaptureMath::FCuffFacts Far = Healthy;
			Far.bInReach = false;
			TestEqual(TEXT("far before healthy"), CaptureMath::JudgeCuff(Far, 0.35f, true), ECaptureRefusal::OutOfReach);

			CaptureMath::FCuffFacts Crab = Far;
			Crab.bCapturable = false;
			TestEqual(TEXT("never the crab"), CaptureMath::JudgeCuff(Crab, 0.35f, true), ECaptureRefusal::NotCapturable);

			CaptureMath::FCuffFacts Nobody = Crab;
			Nobody.bHasTarget = false;
			TestEqual(TEXT("nobody"), CaptureMath::JudgeCuff(Nobody, 0.35f, true), ECaptureRefusal::NoTarget);

			CaptureMath::FCuffFacts Leading = Facts;
			Leading.bLeading = true;
			TestEqual(TEXT("one prisoner per player"), CaptureMath::JudgeCuff(Leading, 0.35f, true), ECaptureRefusal::AlreadyLeading);

			CaptureMath::FCuffFacts InUse = Leading;
			InUse.bHandcuffsInUse = true;
			TestEqual(TEXT("one prisoner per pair"), CaptureMath::JudgeCuff(InUse, 0.35f, true), ECaptureRefusal::HandcuffsInUse);

			CaptureMath::FCuffFacts Swimming = InUse;
			Swimming.bCarrierAble = false;
			TestEqual(TEXT("the player first"), CaptureMath::JudgeCuff(Swimming, 0.35f, true), ECaptureRefusal::CarrierUnable);
		});

		It("aims at what is in front of the view", [this]()
		{
			TestEqual(TEXT("straight ahead"), CaptureMath::AimAngleDeg(FVector::ForwardVector, FVector(100.0, 0.0, 0.0)), 0.0f, 0.01f);
			TestEqual(TEXT("to the side"), CaptureMath::AimAngleDeg(FVector::ForwardVector, FVector(0.0, 100.0, 0.0)), 90.0f, 0.01f);
			TestEqual(TEXT("at the eyes"), CaptureMath::AimAngleDeg(FVector::ForwardVector, FVector::ZeroVector), 0.0f);
			TestTrue(TEXT("the nearer the aim, the better"), CaptureMath::TargetScore(5.0f, 150.0f, 200.0f) < CaptureMath::TargetScore(20.0f, 100.0f, 200.0f));
		});
	});

	Describe("Beach", [this]()
	{
		It("stands on dry ground just above the sea, no steeper than the rules", [this]()
		{
			TestTrue(TEXT("50 cm above"), CaptureMath::IsBeachHeight(0.0f, -50.0f, 0.0f, 300.0f));
			TestTrue(TEXT("at the water line"), CaptureMath::IsBeachHeight(-50.0f, -50.0f, 0.0f, 300.0f));
			TestFalse(TEXT("under the sea"), CaptureMath::IsBeachHeight(-60.0f, -50.0f, 0.0f, 300.0f));
			TestFalse(TEXT("a cliff"), CaptureMath::IsBeachHeight(400.0f, -50.0f, 0.0f, 300.0f));
			TestTrue(TEXT("flat"), CaptureMath::IsWalkableSlope(1.0f, 20.0f));
			TestTrue(TEXT("19 deg"), CaptureMath::IsWalkableSlope(FMath::Cos(FMath::DegreesToRadians(19.0f)), 20.0f));
			TestFalse(TEXT("30 deg"), CaptureMath::IsWalkableSlope(FMath::Cos(FMath::DegreesToRadians(30.0f)), 20.0f));
			TestTrue(TEXT("a seabed 20 cm under"), CaptureMath::IsUnderSea(-70.0f, -50.0f, 10.0f));
			TestFalse(TEXT("a puddle 5 cm under"), CaptureMath::IsUnderSea(-55.0f, -50.0f, 10.0f));
		});

		It("looks for the sea on rings around the spot", [this]()
		{
			TestEqual(TEXT("count"), CaptureMath::ShoreProbeCount(8, 3), 24);
			TestEqual(TEXT("never none"), CaptureMath::ShoreProbeCount(0, 0), 1);
			TestEqual(TEXT("first ring"), static_cast<float>(CaptureMath::ShoreProbeOffset(0, 8, 3, 3000.0f).Size()), 1000.0f, 0.1f);
			TestEqual(TEXT("last ring"), static_cast<float>(CaptureMath::ShoreProbeOffset(23, 8, 3, 3000.0f).Size()), 3000.0f, 0.1f);
			TestEqual(TEXT("first direction east"), static_cast<float>(CaptureMath::ShoreProbeOffset(0, 8, 3, 3000.0f).Y), 0.0f, 0.1f);
			// the second ring turned half a step: no probe on the line of the first ring's
			const FVector2D Second = CaptureMath::ShoreProbeOffset(8, 8, 3, 3000.0f);
			TestTrue(TEXT("second ring turned"), FMath::Abs(Second.Y) > 1.0f);
		});

		It("says the first reason against a spot", [this]()
		{
			CaptureMath::FCageSpotFacts Facts;
			Facts.bFloor = true;
			Facts.FloorZ = 20.0f;
			Facts.SeaLevel = -50.0f;
			Facts.bShoreNear = true;
			TestEqual(TEXT("a beach"), CaptureMath::JudgeCageSpot(Facts, 0.0f, 300.0f, 20.0f), ECaptureRefusal::None);

			CaptureMath::FCageSpotFacts Crowded = Facts;
			Crowded.bClear = false;
			TestEqual(TEXT("a palm in the way"), CaptureMath::JudgeCageSpot(Crowded, 0.0f, 300.0f, 20.0f), ECaptureRefusal::NoRoom);

			CaptureMath::FCageSpotFacts Near = Crowded;
			Near.bCageNear = true;
			TestEqual(TEXT("another cage"), CaptureMath::JudgeCageSpot(Near, 0.0f, 300.0f, 20.0f), ECaptureRefusal::CageTooClose);

			CaptureMath::FCageSpotFacts Steep = Near;
			Steep.FloorNormalZ = 0.5f;
			TestEqual(TEXT("steep"), CaptureMath::JudgeCageSpot(Steep, 0.0f, 300.0f, 20.0f), ECaptureRefusal::TooSteep);

			CaptureMath::FCageSpotFacts Inland = Steep;
			Inland.bShoreNear = false;
			TestEqual(TEXT("far from the sea (the flat of the test map)"), CaptureMath::JudgeCageSpot(Inland, 0.0f, 300.0f, 20.0f), ECaptureRefusal::NotOnBeach);

			CaptureMath::FCageSpotFacts High = Facts;
			High.FloorZ = 500.0f;
			TestEqual(TEXT("high above the sea"), CaptureMath::JudgeCageSpot(High, 0.0f, 300.0f, 20.0f), ECaptureRefusal::NotOnBeach);

			CaptureMath::FCageSpotFacts NoFloor = Facts;
			NoFloor.bFloor = false;
			TestEqual(TEXT("no ground (a boat, a body)"), CaptureMath::JudgeCageSpot(NoFloor, 0.0f, 300.0f, 20.0f), ECaptureRefusal::NotOnBeach);

			CaptureMath::FCageSpotFacts Swimming = NoFloor;
			Swimming.bCarrierAble = false;
			TestEqual(TEXT("the player first"), CaptureMath::JudgeCageSpot(Swimming, 0.0f, 300.0f, 20.0f), ECaptureRefusal::CarrierUnable);
		});
	});

	Describe("Leading", [this]()
	{
		It("waits close to its jailer, keeps its pace, hurries on a taut chain", [this]()
		{
			TestEqual(TEXT("close"), CaptureMath::FollowSpeed(150.0f, 180.0f, 350.0f, 300.0f, 150.0f, 650.0f), 0.0f);
			TestEqual(TEXT("jailer still: the walk"), CaptureMath::FollowSpeed(181.0f, 180.0f, 350.0f, 0.0f, 150.0f, 650.0f), 150.0f, 5.0f);
			TestTrue(TEXT("jailer walking: its pace at least"), CaptureMath::FollowSpeed(200.0f, 180.0f, 350.0f, 300.0f, 150.0f, 650.0f) >= 300.0f);
			TestEqual(TEXT("taut: the fastest"), CaptureMath::FollowSpeed(350.0f, 180.0f, 350.0f, 300.0f, 150.0f, 650.0f), 650.0f, 0.01f);
			TestEqual(TEXT("beyond: the fastest"), CaptureMath::FollowSpeed(900.0f, 180.0f, 350.0f, 300.0f, 150.0f, 650.0f), 650.0f, 0.01f);
			TestEqual(TEXT("never faster than the fastest"), CaptureMath::FollowSpeed(200.0f, 180.0f, 350.0f, 2000.0f, 150.0f, 650.0f), 650.0f, 0.01f);
			TestEqual(TEXT("distance not a number"), CaptureMath::FollowSpeed(std::numeric_limits<float>::quiet_NaN(), 180.0f, 350.0f, 0.0f, 150.0f, 650.0f), 0.0f);
		});

		It("lets go when the jailer dies, swims, leaves or is too far", [this]()
		{
			CaptureMath::FLeadFacts Facts;
			Facts.Distance = 300.0f;
			TestEqual(TEXT("on the chain"), CaptureMath::JudgeLead(Facts, 1200.0f), ECaptiveRelease::None);
			CaptureMath::FLeadFacts Far = Facts;
			Far.Distance = 1300.0f;
			TestEqual(TEXT("too far"), CaptureMath::JudgeLead(Far, 1200.0f), ECaptiveRelease::TooFar);
			CaptureMath::FLeadFacts Water = Far;
			Water.bJailerSwimming = true;
			TestEqual(TEXT("in the water"), CaptureMath::JudgeLead(Water, 1200.0f), ECaptiveRelease::JailerInWater);
			CaptureMath::FLeadFacts Dead = Water;
			Dead.bJailerDead = true;
			TestEqual(TEXT("dead"), CaptureMath::JudgeLead(Dead, 1200.0f), ECaptiveRelease::JailerDead);
			CaptureMath::FLeadFacts Gone = Dead;
			Gone.bJailerValid = false;
			TestEqual(TEXT("gone"), CaptureMath::JudgeLead(Gone, 1200.0f), ECaptiveRelease::JailerGone);
		});
	});

	Describe("Chain", [this]()
	{
		It("sags with its slack and keeps a little sag taut", [this]()
		{
			TestEqual(TEXT("taut"), CaptureMath::ChainSag(400.0f, 350.0f, 6.0f), 6.0f);
			TestTrue(TEXT("slack"), CaptureMath::ChainSag(200.0f, 350.0f, 6.0f) > 6.0f);
			// the parabola's length: about the chain's
			const float Sag = CaptureMath::ChainSag(200.0f, 350.0f, 6.0f);
			TestEqual(TEXT("length kept"), 200.0f + 8.0f * Sag * Sag / (3.0f * 200.0f), 350.0f, 1.0f);
		});

		It("hangs between its ends", [this]()
		{
			const FVector A(0.0, 0.0, 100.0);
			const FVector B(200.0, 0.0, 100.0);
			TestEqual(TEXT("start"), CaptureMath::ChainPoint(A, B, 0.0f, 50.0f), A);
			TestEqual(TEXT("end"), CaptureMath::ChainPoint(A, B, 1.0f, 50.0f), B);
			TestEqual(TEXT("middle sags"), CaptureMath::ChainPoint(A, B, 0.5f, 50.0f), FVector(100.0, 0.0, 50.0));
		});

		It("has one link per pitch, within bounds", [this]()
		{
			TestEqual(TEXT("350 cm of 5 cm links"), CaptureMath::ChainLinkCount(350.0f, 6.0f, 5.0f, 90), 71);
			TestEqual(TEXT("bounded"), CaptureMath::ChainLinkCount(5000.0f, 6.0f, 5.0f, 90), 90);
			TestEqual(TEXT("at least two"), CaptureMath::ChainLinkCount(1.0f, 0.0f, 5.0f, 90), 2);
		});
	});

	Describe("Gestures and cage", [this]()
	{
		It("stretches a clip over its gesture and counts its progress", [this]()
		{
			TestEqual(TEXT("cuff clip over the hold"), CaptureMath::GesturePlayRate(1.2f, 1.5f), 0.8f, 0.0001f);
			TestEqual(TEXT("no clip length"), CaptureMath::GesturePlayRate(0.0f, 1.5f), 1.0f);
			TestEqual(TEXT("half way"), CaptureMath::Progress(10.75, 10.0, 1.5f), 0.5f, 0.0001f);
			TestEqual(TEXT("over"), CaptureMath::Progress(20.0, 10.0, 1.5f), 1.0f);
			TestEqual(TEXT("not begun"), CaptureMath::Progress(9.0, 10.0, 1.5f), 0.0f);
		});

		It("counts its two minutes down and swings its door", [this]()
		{
			TestEqual(TEXT("left"), CaptureMath::SecondsLeft(130.0, 30.0), 100.0f, 0.0001f);
			TestEqual(TEXT("never negative"), CaptureMath::SecondsLeft(30.0, 130.0), 0.0f);
			TestEqual(TEXT("clock"), CaptureMath::ClockText(101.2f), FString(TEXT("1:42")));
			TestEqual(TEXT("clock at zero"), CaptureMath::ClockText(0.0f), FString(TEXT("0:00")));
			TestEqual(TEXT("door half open"), CaptureMath::DoorStep(0.0f, true, 0.3f, 0.6f), 0.5f, 0.0001f);
			TestEqual(TEXT("door open"), CaptureMath::DoorStep(0.8f, true, 0.3f, 0.6f), 1.0f);
			TestEqual(TEXT("door closing"), CaptureMath::DoorStep(1.0f, false, 0.3f, 0.6f), 0.5f, 0.0001f);
			TestEqual(TEXT("a sale is never negative"), CaptureMath::SaleAmount(-5), 0);
		});
	});
}

#endif
