// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiDismember.h"
#include "Creatures/TikiMath.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** 0 root, 1 pelvis, 2 spine_02, 3 head, 4 upperarm_l, 5 lowerarm_l, 6 upperarm_r, 7 thigh_l, 8 thigh_r */
	const TArray<int32> Parents = { INDEX_NONE, 0, 1, 2, 2, 4, 2, 1, 1 };

	/** First bone of the modules Head, Torso, Pelvis, Arm_L, Arm_R, Leg_L, Leg_R (the pack, T-155) */
	const TArray<int32> Anchors = { 3, 2, 1, 4, 6, 7, 8 };

	/** Shown flags as a string of 0/1 in the order of Anchors, empty when the modules cannot be used */
	FString Shown(ETikiBodyPart Part, int32 PartBone, const TArray<int32>& Hidden, const TArray<int32>& InAnchors = Anchors)
	{
		TArray<bool> Flags;
		if (!TikiDismember::SelectModules(InAnchors, Parents, Part, PartBone, Hidden, Flags))
		{
			return FString();
		}
		FString Out;
		for (const bool bFlag : Flags)
		{
			Out += bFlag ? TEXT("1") : TEXT("0");
		}
		return Out;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.Dismember
BEGIN_DEFINE_SPEC(FTikiDismemberSpec, "monke_island.Tiki.Dismember",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiDismemberSpec)

void FTikiDismemberSpec::Define()
{
	Describe("speed of a cut piece", [this]()
	{
		It("leaves flat along the blow, bounded, whatever the fling speed", [this]()
		{
			// the old fling of a limb: 250 cm/s along the blow and 250 cm/s up
			const FVector Start = TikiMath::CutPieceVelocity(FVector(250.0, 0.0, 250.0), 40.0f);
			TestTrue(TEXT("bounded to 40 cm/s"), FMath::IsNearlyEqual(Start.Size(), 40.0, 0.01));
			TestEqual(TEXT("never thrown up"), Start.Z, 0.0);
			TestTrue(TEXT("along the blow"), Start.GetSafeNormal().Equals(FVector::ForwardVector, 0.001));
			TestTrue(TEXT("a slow nudge is kept"), TikiMath::CutPieceVelocity(FVector(0.0, 25.0, -10.0), 40.0f).Equals(FVector(0.0, 25.0, 0.0), 0.01));
			TestEqual(TEXT("no speed allowed: none"), TikiMath::CutPieceVelocity(FVector(250.0, 0.0, 0.0), 0.0f), FVector::ZeroVector);
		});

		It("a blow on a piece lying on the ground only nudges it", [this]()
		{
			// sabre impulse 25000 kg cm/s on a 4 kg arm: 6250 cm/s before T-155
			const FVector After = TikiMath::BoundedBlowVelocity(FVector::ZeroVector, FVector(6250.0, 0.0, 0.0), 40.0f);
			TestTrue(TEXT("bounded to 40 cm/s"), FMath::IsNearlyEqual(After.Size(), 40.0, 0.01));
			TestTrue(TEXT("a small blow is kept"), TikiMath::BoundedBlowVelocity(FVector::ZeroVector, FVector(0.0, 10.0, 0.0), 40.0f).Equals(FVector(0.0, 10.0, 0.0), 0.01));
		});

		It("a blow never speeds up a falling piece, and no bound keeps the engine behaviour", [this]()
		{
			const FVector Falling(0.0, 0.0, -300.0);
			TestEqual(TEXT("falling: unchanged"), TikiMath::BoundedBlowVelocity(Falling, FVector(5000.0, 0.0, 0.0), 40.0f), Falling);
			TestEqual(TEXT("no bound: unchanged"), TikiMath::BoundedBlowVelocity(Falling, FVector(5000.0, 0.0, 0.0), 0.0f), Falling);
		});

		It("goes to sleep once still, not before it had time to fall", [this]()
		{
			TestFalse(TEXT("just spawned"), TikiMath::IsPieceAtRest(0.0f, 0.1f, 8.0f, 0.2f));
			TestTrue(TEXT("still on the ground"), TikiMath::IsPieceAtRest(3.0f, 1.0f, 8.0f, 0.2f));
			TestFalse(TEXT("still sliding"), TikiMath::IsPieceAtRest(30.0f, 1.0f, 8.0f, 0.2f));
		});
	});

	Describe("modules of a cut body", [this]()
	{
		It("a limb is its own module, the body keeps the others", [this]()
		{
			TestEqual(TEXT("arm cut off: Arm_R alone"), Shown(ETikiBodyPart::Limb, 6, {}), FString(TEXT("0000100")));
			TestEqual(TEXT("body without its left arm"), Shown(ETikiBodyPart::Whole, INDEX_NONE, { 4 }), FString(TEXT("1110111")));
		});

		It("a body cut in two at the torso: chest, head and arms above, pelvis and legs below", [this]()
		{
			TestEqual(TEXT("upper half"), Shown(ETikiBodyPart::Upper, 2, {}), FString(TEXT("1101100")));
			TestEqual(TEXT("lower half"), Shown(ETikiBodyPart::Lower, 2, {}), FString(TEXT("0010011")));
			TestEqual(TEXT("upper half, right arm lost before"), Shown(ETikiBodyPart::Upper, 2, { 6 }), FString(TEXT("1101000")));
		});

		It("falls back on the whole mesh when the modules cannot close the cut", [this]()
		{
			TestEqual(TEXT("cut at the forearm: no module starts there"), Shown(ETikiBodyPart::Limb, 5, {}), FString());
			TestEqual(TEXT("forearm lost before"), Shown(ETikiBodyPart::Whole, INDEX_NONE, { 5 }), FString());
			TestEqual(TEXT("a module missing (not imported)"), Shown(ETikiBodyPart::Limb, 6, {}, { 3, 2, 1, 4, INDEX_NONE, 7, 8 }), FString());
			TestEqual(TEXT("no set"), Shown(ETikiBodyPart::Limb, 6, {}, {}), FString());
		});
	});
}

#endif
