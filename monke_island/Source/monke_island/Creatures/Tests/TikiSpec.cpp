// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiBody.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki
BEGIN_DEFINE_SPEC(FTikiSpec, "monke_island.Tiki.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiSpec)

void FTikiSpec::Define()
{
	Describe("Locomotion", [this]()
	{
		It("idles when nearly still, walks at walking speed, runs above", [this]()
		{
			TestTrue(TEXT("idle"), TikiMath::ChooseLocomotion(5.0f, 120.0f) == ETikiLocomotion::Idle);
			TestTrue(TEXT("walk"), TikiMath::ChooseLocomotion(120.0f, 120.0f) == ETikiLocomotion::Walk);
			TestTrue(TEXT("run"), TikiMath::ChooseLocomotion(400.0f, 120.0f) == ETikiLocomotion::Run);
		});
	});

	Describe("Attack", [this]()
	{
		It("starts only in range and after the cooldown", [this]()
		{
			TestTrue(TEXT("in range, cooled down"), TikiMath::CanStartAttack(100.0f, 160.0f, 10.0, 8.0, 1.6f));
			TestFalse(TEXT("out of range"), TikiMath::CanStartAttack(200.0f, 160.0f, 10.0, 8.0, 1.6f));
			TestFalse(TEXT("cooldown running"), TikiMath::CanStartAttack(100.0f, 160.0f, 10.0, 9.0, 1.6f));
		});
	});

	Describe("Bushes", [this]()
	{
		It("recognises the shrub meshes of Raraka and ignores the palms and the grass", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("miki-miki"), TikiMath::IsBushMeshName(TEXT("SM_MikiMiki_B"), Rules->BushMeshKeywords));
			TestTrue(TEXT("pandanus"), TikiMath::IsBushMeshName(TEXT("SM_Pandanus_Young"), Rules->BushMeshKeywords));
			TestFalse(TEXT("palm"), TikiMath::IsBushMeshName(TEXT("SM_CocoPalm_A"), Rules->BushMeshKeywords));
			TestFalse(TEXT("grass"), TikiMath::IsBushMeshName(TEXT("SM_Grass_Tuft"), Rules->BushMeshKeywords));
		});

		It("picks the nearest bush within the radius, none beyond", [this]()
		{
			const TArray<FVector> Bushes = { FVector(1000, 0, 0), FVector(300, 0, 0), FVector(0, 2000, 0) };
			TestEqual(TEXT("nearest"), TikiMath::NearestIndex(Bushes, FVector::ZeroVector, 3000.0f), 1);
			TestEqual(TEXT("none within 200 cm"), TikiMath::NearestIndex(Bushes, FVector::ZeroVector, 200.0f), INDEX_NONE);
		});
	});

	Describe("Villages", [this]()
	{
		It("reads the village and the role from the marker tags", [this]()
		{
			const TArray<FName> Tags = { TEXT("VillageMarker"), TEXT("NpcSpawn"), TEXT("Village.V01"), TEXT("Role.guardian") };
			TestEqual(TEXT("village"), TikiMath::TagValue(Tags, TEXT("Village")), FName(TEXT("V01")));
			TestEqual(TEXT("role"), TikiMath::TagValue(Tags, TEXT("Role")), FName(TEXT("guardian")));
			TestEqual(TEXT("absent"), TikiMath::TagValue(Tags, TEXT("Theme")), FName(NAME_None));
			TestEqual(TEXT("prefix alone is not a value"), TikiMath::TagValue({ TEXT("Village.") }, TEXT("Village")), FName(NAME_None));
		});

		It("spawns villagers only on free points of the village", [this]()
		{
			const TArray<FVector> Points = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(2000, 0, 0) };
			TestEqual(TEXT("first free point"), TikiMath::PickFreeSpawn(Points, { FVector(10, 0, 0) }, 150.0f, 0.0f), 1);
			TestEqual(TEXT("last free point"), TikiMath::PickFreeSpawn(Points, { FVector(10, 0, 0) }, 150.0f, 0.99f), 2);
			TestEqual(TEXT("all taken"), TikiMath::PickFreeSpawn(Points, { FVector(0, 0, 500), FVector(1000, 50, 0), FVector(2000, 0, 0) }, 150.0f, 0.5f), INDEX_NONE);
			TestEqual(TEXT("no point"), TikiMath::PickFreeSpawn({}, {}, 150.0f, 0.5f), INDEX_NONE);
		});

		It("reads the activity of a point of interest, none when unknown", [this]()
		{
			TestTrue(TEXT("well"), TikiMath::ActivityFromName(TEXT("work")) == ETikiActivity::Work);
			TestTrue(TEXT("shrine, any case"), TikiMath::ActivityFromName(TEXT("Pray")) == ETikiActivity::Pray);
			TestTrue(TEXT("square"), TikiMath::ActivityFromName(TEXT("chat")) == ETikiActivity::Chat);
			TestTrue(TEXT("unknown"), TikiMath::ActivityFromName(TEXT("dance")) == ETikiActivity::None);
			TestTrue(TEXT("absent"), TikiMath::ActivityFromName(NAME_None) == ETikiActivity::None);
		});

		It("starts the routine only when a player comes near the village", [this]()
		{
			const FVector Center(1000, 1000, 0);
			TestFalse(TEXT("nobody"), TikiMath::IsAnyWithin(Center, {}, 15000.0f));
			TestFalse(TEXT("far away"), TikiMath::IsAnyWithin(Center, { FVector(20000, 1000, 0) }, 15000.0f));
			TestTrue(TEXT("one player near, height ignored"), TikiMath::IsAnyWithin(Center, { FVector(20000, 1000, 0), FVector(1000, 9000, 900) }, 15000.0f));
		});

		It("spreads the alarm warriors evenly on the ring around the village", [this]()
		{
			const FVector Center(100, -200, 30);
			for (int32 Index = 0; Index < 4; ++Index)
			{
				const FVector Point = TikiMath::AlarmRingPoint(Center, 6300.0f, Index, 4, 0.25f);
				TestTrue(TEXT("on the ring"), FMath::IsNearlyEqual(FVector::Dist2D(Point, Center), 6300.0, 1.0));
				TestEqual(TEXT("Z kept"), Point.Z, 30.0);
				const FVector Next = TikiMath::AlarmRingPoint(Center, 6300.0f, (Index + 1) % 4, 4, 0.25f);
				TestTrue(TEXT("a quarter turn apart"), FMath::IsNearlyEqual(FVector::Dist2D(Point, Next), 6300.0 * FMath::Sqrt(2.0), 2.0));
			}
		});

		It("brings out only the missing warriors against a player", [this]()
		{
			TestEqual(TEXT("nobody hunts it yet"), TikiMath::MissingWarriors(4, 6, 0), 4);
			TestEqual(TEXT("capped"), TikiMath::MissingWarriors(4, 6, 4), 2);
			TestEqual(TEXT("cap reached"), TikiMath::MissingWarriors(3, 9, 9), 0);
			TestEqual(TEXT("over the cap"), TikiMath::MissingWarriors(3, 9, 12), 0);
			TestEqual(TEXT("none wanted"), TikiMath::MissingWarriors(0, 9, 0), 0);
		});

		It("still brings tiki out around a killer right after a full alarm", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			const int32 AfterAlarm = TikiMath::MissingWarriors(Rules->AlarmWarriorCount, Rules->AlarmMaxAlivePerPlayer, 0);
			TestTrue(TEXT("the alarm brings tiki out"), AfterAlarm > 0);
			TestTrue(TEXT("the kill brings more"), TikiMath::MissingWarriors(Rules->KillWarriorCount, Rules->KillMaxAlivePerPlayer, AfterAlarm) > 0);
		});

		It("sends a guardian back to its post beyond the leash", [this]()
		{
			TestFalse(TEXT("near its bush"), TikiMath::IsOffPost(FVector(1000, 0, 0), FVector::ZeroVector, 1200.0f));
			TestTrue(TEXT("strayed"), TikiMath::IsOffPost(FVector(2000, 0, 0), FVector::ZeroVector, 1200.0f));
		});
	});

	Describe("Ambush", [this]()
	{
		It("hides at least one monster in a zone with enough bushes, more in a thick one", [this]()
		{
			TestEqual(TEXT("too few bushes"), TikiMath::AmbushSlots(5, 12, 80, 3), 0);
			TestEqual(TEXT("enough for one"), TikiMath::AmbushSlots(12, 12, 80, 3), 1);
			TestEqual(TEXT("one more per 80 bushes"), TikiMath::AmbushSlots(170, 12, 80, 3), 3);
			TestEqual(TEXT("capped"), TikiMath::AmbushSlots(5000, 12, 80, 3), 3);
		});
	});

	Describe("Scatter", [this]()
	{
		It("keeps the scattered items apart", [this]()
		{
			const TArray<FVector> Taken = { FVector(0, 0, 0), FVector(50000, 0, 0) };
			TestFalse(TEXT("too close to the first"), TikiMath::IsFarFromAll(FVector(20000, 0, 900), Taken, 30000.0f));
			TestTrue(TEXT("far from both, height ignored"), TikiMath::IsFarFromAll(FVector(25000, 30000, 0), Taken, 30000.0f));
			TestTrue(TEXT("nothing taken yet"), TikiMath::IsFarFromAll(FVector::ZeroVector, {}, 30000.0f));
		});

		It("draws every spot once, in an order that depends on the seed", [this]()
		{
			FRandomStream A(42);
			FRandomStream B(42);
			FRandomStream C(7);
			const TArray<int32> First = TikiMath::ShuffledIndices(50, A);
			TestEqual(TEXT("same seed, same draw"), First, TikiMath::ShuffledIndices(50, B));
			TestNotEqual(TEXT("other seed, other draw"), First, TikiMath::ShuffledIndices(50, C));
			TArray<int32> Sorted = First;
			Sorted.Sort();
			bool bEachOnce = Sorted.Num() == 50;
			for (int32 Index = 0; bEachOnce && Index < 50; ++Index)
			{
				bEachOnce = Sorted[Index] == Index;
			}
			TestTrue(TEXT("each spot once"), bEachOnce);
		});
	});

	Describe("Corpse parts", [this]()
	{
		// root 0 > pelvis 1 > spine_01 2 > spine_02 3 > head 4 ; upperarm_r 5 (on spine_02) > hand_r 6 ; thigh_l 7 (on pelvis)
		const TArray<int32> Parents = { INDEX_NONE, 0, 1, 2, 3, 3, 5, 1 };
		TArray<FTransform> Local;
		for (int32 Index = 0; Index < Parents.Num(); ++Index)
		{
			Local.Add(FTransform(FVector(Index == 5 ? 20.0 : 0.0, 0.0, Index == 7 ? -10.0 : 15.0)));
		}
		const TArray<FTransform> Space = TikiBody::ComponentSpace(Local, Parents);
		const auto IsCollapsed = [](const FTransform& T) { return T.GetScale3D().GetMax() < 0.01; };
		const auto IsKept = [](const FTransform& T, const FTransform& Was) { return T.GetLocation().Equals(Was.GetLocation(), 0.01) && T.GetScale3D().Equals(FVector::OneVector, 0.001); };

		It("keeps the belly and the legs in the lower half, collapses the rest", [this, Parents, Local, Space, IsCollapsed, IsKept]()
		{
			const TArray<FTransform> Out = TikiBody::ComponentSpace(TikiBody::MaskPose(Local, Parents, ETikiBodyPart::Lower, 3, {}), Parents);
			for (const int32 Bone : { 0, 1, 2, 7 })
			{
				TestTrue(*FString::Printf(TEXT("bone %d kept"), Bone), IsKept(Out[Bone], Space[Bone]));
			}
			for (const int32 Bone : { 3, 4, 5, 6 })
			{
				TestTrue(*FString::Printf(TEXT("bone %d collapsed"), Bone), IsCollapsed(Out[Bone]));
			}
		});

		It("keeps the chest, the head and the arms in place in the upper half", [this, Parents, Local, Space, IsCollapsed, IsKept]()
		{
			const TArray<FTransform> Out = TikiBody::ComponentSpace(TikiBody::MaskPose(Local, Parents, ETikiBodyPart::Upper, 3, {}), Parents);
			for (const int32 Bone : { 3, 4, 5, 6 })
			{
				TestTrue(*FString::Printf(TEXT("bone %d kept"), Bone), IsKept(Out[Bone], Space[Bone]));
			}
			for (const int32 Bone : { 0, 1, 2, 7 })
			{
				TestTrue(*FString::Printf(TEXT("bone %d collapsed"), Bone), IsCollapsed(Out[Bone]));
			}
		});

		It("keeps only the arm in a limb, and hides a cut arm on the body", [this, Parents, Local, Space, IsCollapsed, IsKept]()
		{
			const TArray<FTransform> Limb = TikiBody::ComponentSpace(TikiBody::MaskPose(Local, Parents, ETikiBodyPart::Limb, 5, {}), Parents);
			TestTrue(TEXT("arm kept"), IsKept(Limb[5], Space[5]) && IsKept(Limb[6], Space[6]));
			TestTrue(TEXT("head gone"), IsCollapsed(Limb[4]));
			const TArray<FTransform> Body = TikiBody::ComponentSpace(TikiBody::MaskPose(Local, Parents, ETikiBodyPart::Whole, INDEX_NONE, { 5 }), Parents);
			TestTrue(TEXT("cut arm gone"), IsCollapsed(Body[5]) && IsCollapsed(Body[6]));
			TestTrue(TEXT("head still there"), IsKept(Body[4], Space[4]));
		});

		It("sizes the box of a part on the joints it keeps", [this, Parents, Local, Space]()
		{
			const FBox Upper = TikiBody::PartBounds(Local, Parents, ETikiBodyPart::Upper, 3, {}, FTransform::Identity, 5.0f);
			TestTrue(TEXT("holds the head"), Upper.IsInside(Space[4].GetLocation()));
			TestFalse(TEXT("not the thigh"), Upper.IsInside(Space[7].GetLocation()));
			TestTrue(TEXT("starts at the cut"), FMath::IsNearlyEqual(Upper.Min.Z, Space[3].GetLocation().Z - 5.0, 0.01));
		});
	});

	Describe("Smooth bodies", [this]()
	{
		It("loops a clip, holds the last frame of a one-shot", [this]()
		{
			TestTrue(TEXT("advances"), FMath::IsNearlyEqual(TikiMath::AdvanceClip(0.2f, 0.3f, 1.0f, true), 0.5f));
			TestTrue(TEXT("wraps"), FMath::IsNearlyEqual(TikiMath::AdvanceClip(0.9f, 0.3f, 1.0f, true), 0.2f, 1.0e-4f));
			TestTrue(TEXT("holds"), FMath::IsNearlyEqual(TikiMath::AdvanceClip(0.9f, 0.3f, 1.0f, false), 1.0f));
			TestEqual(TEXT("no clip"), TikiMath::AdvanceClip(0.5f, 0.3f, 0.0f, true), 0.0f);
		});

		It("plays the locomotion as fast as the body moves, within bounds", [this]()
		{
			TestTrue(TEXT("authored speed"), FMath::IsNearlyEqual(TikiMath::ClipRate(110.0f, 110.0f, 0.6f, 1.6f), 1.0f));
			TestTrue(TEXT("slow walk"), FMath::IsNearlyEqual(TikiMath::ClipRate(80.0f, 110.0f, 0.6f, 1.6f), 80.0f / 110.0f));
			TestTrue(TEXT("never below the minimum"), FMath::IsNearlyEqual(TikiMath::ClipRate(10.0f, 110.0f, 0.6f, 1.6f), 0.6f));
			TestTrue(TEXT("never above the maximum"), FMath::IsNearlyEqual(TikiMath::ClipRate(900.0f, 380.0f, 0.6f, 1.6f), 1.6f));
		});

		It("tells a blow in the back from a blow in the face", [this]()
		{
			const FVector Forward(1, 0, 0);
			TestTrue(TEXT("from behind"), TikiMath::IsBlowFromBehind(FVector(1, 0.2, 0), Forward));
			TestFalse(TEXT("in the face"), TikiMath::IsBlowFromBehind(FVector(-1, 0, 0), Forward));
			TestFalse(TEXT("from the side"), TikiMath::IsBlowFromBehind(FVector(0, 1, 0), Forward));
		});

		It("goes through the gate of its plot only from outside the fence", [this]()
		{
			const FVector Door(0, 0, 0);
			const FVector Gate(0, 500, 0);
			TestTrue(TEXT("on the road"), TikiMath::ShouldPassGate(FVector(300, 1200, 0), Door, Gate));
			TestFalse(TEXT("in the yard"), TikiMath::ShouldPassGate(FVector(100, 250, 0), Door, Gate));
		});
	});

	Describe("Night spawn ring", [this]()
	{
		It("stays between the min and max distance", [this]()
		{
			for (int32 Step = 0; Step <= 10; ++Step)
			{
				const FVector Point = TikiMath::PointOnRing(FVector(5, 5, 7), Step * 0.6f, Step / 10.0f, 1500.0f, 3500.0f);
				const double Distance = FVector::Dist2D(Point, FVector(5, 5, 7));
				TestTrue(TEXT("on the ring"), Distance >= 1499.0 && Distance <= 3501.0);
				TestEqual(TEXT("Z kept"), Point.Z, 7.0);
			}
		});
	});
}

#endif
