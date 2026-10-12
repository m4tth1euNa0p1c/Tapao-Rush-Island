// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiLimbs.h"
#include "Creatures/TikiRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** 0 root, 1 pelvis, 2 spine_02, 3 head, 4 upperarm_l, 5 lowerarm_l, 6 hand_l, 7 upperarm_r, 8 weapon_r, 9 thigh_l, 10 calf_l, 11 thigh_r */
	const TArray<int32> Parents = { INDEX_NONE, 0, 1, 2, 2, 4, 5, 2, 7, 1, 9, 1 };

	/** First bones of the default limbs of DA_TikiRules: Head, Arm_L, Arm_R, Leg_L, Leg_R */
	const TArray<int32> LimbBones = { 3, 4, 7, 9, 11 };
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.Limbs
BEGIN_DEFINE_SPEC(FTikiLimbsSpec, "monke_island.Tiki.Limbs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiLimbsSpec)

void FTikiLimbsSpec::Define()
{
	Describe("LimbOfBone", [this]()
	{
		It("gives a bone the limb of its nearest limb ancestor, the body none", [this]()
		{
			TestEqual(TEXT("head"), TikiLimbs::LimbOfBone(3, LimbBones, Parents), 0);
			TestEqual(TEXT("hand_l is the left arm"), TikiLimbs::LimbOfBone(6, LimbBones, Parents), 1);
			TestEqual(TEXT("weapon_r is the right arm"), TikiLimbs::LimbOfBone(8, LimbBones, Parents), 2);
			TestEqual(TEXT("calf_l is the left leg"), TikiLimbs::LimbOfBone(10, LimbBones, Parents), 3);
			TestEqual(TEXT("spine_02 is the body"), TikiLimbs::LimbOfBone(2, LimbBones, Parents), INDEX_NONE);
			TestEqual(TEXT("pelvis is the body"), TikiLimbs::LimbOfBone(1, LimbBones, Parents), INDEX_NONE);
			TestEqual(TEXT("unknown bone"), TikiLimbs::LimbOfBone(INDEX_NONE, LimbBones, Parents), INDEX_NONE);
		});
	});

	Describe("LimbHealth", [this]()
	{
		It("adds the hits of a limb up and tears it once, when they cross its health", [this]()
		{
			TArray<float> Damage;
			TestFalse(TEXT("first pellet"), TikiLimbs::AddDamage(Damage, 2, 14.0f, 25.0f));
			TestTrue(TEXT("second pellet crosses 25"), TikiLimbs::AddDamage(Damage, 2, 14.0f, 25.0f));
			TestFalse(TEXT("past it, no second tear"), TikiLimbs::AddDamage(Damage, 2, 14.0f, 25.0f));
			TestEqual(TEXT("accumulated"), Damage[2], 42.0f);
			TestEqual(TEXT("other limbs untouched"), Damage[0], 0.0f);
			TestFalse(TEXT("the body (no limb) never tears"), TikiLimbs::AddDamage(Damage, INDEX_NONE, 100.0f, 25.0f));
			TestFalse(TEXT("no damage, no tear"), TikiLimbs::AddDamage(Damage, 1, 0.0f, 25.0f));
		});

		It("lets the default pistol ball tear an arm, a leg or the head, never one shotgun pellet at 10 m", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestEqual(TEXT("five limbs"), Rules->Limbs.Num(), 5);
			for (const FTikiLimb& Limb : Rules->Limbs)
			{
				TArray<float> Ball;
				TestTrue(*FString::Printf(TEXT("pistol ball (35) tears %s"), *Limb.Part.ToString()), TikiLimbs::AddDamage(Ball, 0, 35.0f, Limb.Health));
				TArray<float> Pellet;
				// one pellet at 10 m: 16 x 0.35 x 2.5 = 14
				TestFalse(*FString::Printf(TEXT("one pellet at 10 m does not tear %s"), *Limb.Part.ToString()), TikiLimbs::AddDamage(Pellet, 0, 14.0f, Limb.Health));
			}
			TestTrue(TEXT("the head kills"), Rules->Limbs[0].Loss == ETikiLimbLoss::Kills);
		});
	});

	Describe("Loss", [this]()
	{
		It("slows a creature with one leg, more with two", [this]()
		{
			TestEqual(TEXT("whole"), TikiLimbs::SpeedScale(0, 0.35f, 0.12f), 1.0f);
			TestEqual(TEXT("one leg"), TikiLimbs::SpeedScale(1, 0.35f, 0.12f), 0.35f);
			TestEqual(TEXT("two legs"), TikiLimbs::SpeedScale(2, 0.35f, 0.12f), 0.12f);
		});

		It("shreds what the hits of the shot have not torn already", [this]()
		{
			TestEqual(TEXT("3 wanted, 1 torn"), TikiLimbs::ShredLeft(3, 1), 2);
			TestEqual(TEXT("2 wanted, 3 torn"), TikiLimbs::ShredLeft(2, 3), 0);
		});
	});
}

#endif
