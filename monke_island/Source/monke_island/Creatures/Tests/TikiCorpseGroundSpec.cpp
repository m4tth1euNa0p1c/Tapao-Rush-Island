// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiBody.h"
#include "Creatures/TikiMath.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** root at the origin, spine 10 cm above it, arm 20 cm along X from the spine */
	void MakePose(TArray<FTransform>& OutLocal, TArray<int32>& OutParents)
	{
		OutLocal = { FTransform::Identity, FTransform(FVector(0, 0, 10)), FTransform(FVector(20, 0, 0)) };
		OutParents = { INDEX_NONE, 0, 1 };
	}

	TikiBody::FBoneGeometry Sphere(int32 Bone, float Radius, const FVector& Center = FVector::ZeroVector)
	{
		TikiBody::FBoneGeometry Body;
		Body.Bone = Bone;
		FKSphereElem Elem(Radius);
		Elem.Center = Center;
		Body.Geom.SphereElems.Add(Elem);
		return Body;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.CorpseGround
BEGIN_DEFINE_SPEC(FTikiCorpseGroundSpec, "monke_island.Tiki.CorpseGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiCorpseGroundSpec)

void FTikiCorpseGroundSpec::Define()
{
	Describe("height of a frozen corpse on the ground", [this]()
	{
		It("finds the lowest point of the posed collision, not of the bones", [this]()
		{
			TArray<FTransform> Local;
			TArray<int32> Parents;
			MakePose(Local, Parents);
			// the spine body reaches 15 cm around its joint (10 cm high): the body goes 5 cm under the root
			const FKAggregateGeom Geom = TikiBody::PoseGeometry({ Sphere(1, 15.0f) }, Local, Parents, ETikiBodyPart::Whole, INDEX_NONE, {}, FTransform::Identity);
			FVector Lowest;
			TestTrue(TEXT("has a shape"), TikiBody::LowestPoint(Geom, FTransform::Identity, Lowest));
			TestTrue(TEXT("lowest under the joint by the radius"), FMath::IsNearlyEqual(Lowest.Z, -5.0, 0.01));
		});

		It("places the shapes by the pose and the corpse transform", [this]()
		{
			TArray<FTransform> Local;
			TArray<int32> Parents;
			MakePose(Local, Parents);
			// a lying capsule on the arm (axis along X), the mesh 50 cm up and 100 cm along X
			TikiBody::FBoneGeometry Arm;
			Arm.Bone = 2;
			FKSphylElem Capsule(6.0f, 30.0f);
			Capsule.Rotation = FRotator(90.0f, 0.0f, 0.0f);
			Arm.Geom.SphylElems.Add(Capsule);
			const FTransform MeshWorld(FVector(100, 0, 50));
			const FKAggregateGeom Geom = TikiBody::PoseGeometry({ Arm }, Local, Parents, ETikiBodyPart::Whole, INDEX_NONE, {}, MeshWorld);
			FVector Lowest;
			TestTrue(TEXT("has a shape"), TikiBody::LowestPoint(Geom, FTransform::Identity, Lowest));
			TestTrue(TEXT("height: mesh + spine - radius"), FMath::IsNearlyEqual(Lowest.Z, 50.0 + 10.0 - 6.0, 0.01));
			TestTrue(TEXT("under the arm"), FMath::IsNearlyEqual(Lowest.X, 120.0, 0.01));
		});

		It("keeps only the bodies of the part and grows with the bone scale", [this]()
		{
			TArray<FTransform> Local;
			TArray<int32> Parents;
			MakePose(Local, Parents);
			Local[2].SetScale3D(FVector(2.0));
			const TArray<TikiBody::FBoneGeometry> Bodies = { Sphere(0, 40.0f), Sphere(2, 5.0f) };
			// the limb cut at bone 2 keeps its own body only, twice as large
			const FKAggregateGeom Limb = TikiBody::PoseGeometry(Bodies, Local, Parents, ETikiBodyPart::Limb, 2, {}, FTransform::Identity);
			TestEqual(TEXT("one shape"), Limb.SphereElems.Num(), 1);
			TestTrue(TEXT("scaled radius"), Limb.SphereElems.Num() == 1 && FMath::IsNearlyEqual(Limb.SphereElems[0].Radius, 10.0f, 0.01f));
			// a bone cut off before is left out of the whole body
			const FKAggregateGeom Maimed = TikiBody::PoseGeometry(Bodies, Local, Parents, ETikiBodyPart::Whole, INDEX_NONE, { 2 }, FTransform::Identity);
			TestEqual(TEXT("arm gone"), Maimed.SphereElems.Num(), 1);
		});

		It("lays the collision on the ground with the clearance, floating or sunk", [this]()
		{
			TestEqual(TEXT("floating: lowered"), TikiMath::CorpseRestOffset(12.0f, 0.0f, 0.5f, true), -11.5f);
			TestEqual(TEXT("sunk: lifted"), TikiMath::CorpseRestOffset(-20.0f, 0.0f, 0.5f, true), 20.5f);
			TestEqual(TEXT("flung piece: never lowered"), TikiMath::CorpseRestOffset(80.0f, 0.0f, 0.5f, false), 0.0f);
			TestEqual(TEXT("flung piece: lifted out of the ground"), TikiMath::CorpseRestOffset(-3.0f, 0.0f, 0.5f, false), 3.5f);
		});
	});

	Describe("a frozen corpse does not jump", [this]()
	{
		It("starts without speed, a flung piece keeps its bounded speed", [this]()
		{
			TestEqual(TEXT("frozen: still"), TikiMath::CorpseSpawnVelocity(FVector(0, 0, 500), 300.0f, true), FVector::ZeroVector);
			TestTrue(TEXT("piece: bounded"), FMath::IsNearlyEqual(TikiMath::CorpseSpawnVelocity(FVector(0, 0, 500), 300.0f, false).Size(), 300.0, 0.01));
		});

		It("is never lifted (no overlap pushed out), left within 5 cm, lowered exactly onto the ground", [this]()
		{
			TestEqual(TEXT("mesh in the ground: not lifted"), TikiMath::FrozenSettleOffset(-8.0f, 0.0f, 5.0f), 0.0f);
			TestEqual(TEXT("touching: left"), TikiMath::FrozenSettleOffset(0.0f, 0.0f, 5.0f), 0.0f);
			TestEqual(TEXT("4 cm above: left"), TikiMath::FrozenSettleOffset(4.0f, 0.0f, 5.0f), 0.0f);
			const float Offset = TikiMath::FrozenSettleOffset(12.0f, 0.0f, 5.0f);
			TestEqual(TEXT("12 cm above: down to the ground"), Offset, -12.0f);
			TestTrue(TEXT("never under the ground"), 12.0f + Offset >= 0.0f);
		});

		It("goes down slowly, never past the ground", [this]()
		{
			float Left = -12.0f;
			int32 Steps = 0;
			while (Left < 0.0f && Steps < 1000)
			{
				const float Step = TikiMath::SettleStep(Left, 30.0f, 1.0f / 30.0f);
				TestTrue(TEXT("small step"), Step >= -1.0f - KINDA_SMALL_NUMBER && Step <= 0.0f);
				Left -= Step;
				++Steps;
			}
			TestTrue(TEXT("reaches the ground"), FMath::IsNearlyZero(Left, 1.0e-3f));
			TestTrue(TEXT("over several frames"), Steps >= 12);
			TestEqual(TEXT("nothing left: no move"), TikiMath::SettleStep(0.0f, 30.0f, 0.1f), 0.0f);
		});
	});

	Describe("water over a corpse", [this]()
	{
		It("does not wash a corpse away on dry ground above the sea (L_Test_Flat: sea -50 cm, crest +37 cm)", [this]()
		{
			// the lowest point reported 48 cm under the dry ground: the old test read 35 cm of water
			const float Surface = -50.0f + 37.0f;
			TestTrue(TEXT("old reading"), TikiMath::IsCorpseWashedAway(Surface - (-48.0f), 30.0f));
			const float Depth = TikiMath::CorpseWaterDepth(Surface, -48.0f, true, 0.0f);
			TestTrue(TEXT("no water above the ground"), Depth < 0.0f);
			TestFalse(TEXT("stays"), TikiMath::IsCorpseWashedAway(Depth, 30.0f));
		});

		It("washes a corpse away once the rising water covers its ground", [this]()
		{
			TestTrue(TEXT("flooded beach"), TikiMath::IsCorpseWashedAway(TikiMath::CorpseWaterDepth(40.0f, 1.0f, true, 0.0f), 30.0f));
			TestFalse(TEXT("shallow"), TikiMath::IsCorpseWashedAway(TikiMath::CorpseWaterDepth(20.0f, 1.0f, true, 0.0f), 30.0f));
			TestTrue(TEXT("no ground found (deep water)"), TikiMath::IsCorpseWashedAway(TikiMath::CorpseWaterDepth(0.0f, -300.0f, false, 0.0f), 30.0f));
		});
	});
}

#endif
