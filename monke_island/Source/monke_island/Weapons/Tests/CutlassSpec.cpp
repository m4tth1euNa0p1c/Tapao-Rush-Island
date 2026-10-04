// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/CutlassMath.h"
#include "Weapons/CutlassRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons
BEGIN_DEFINE_SPEC(FCutlassSpec, "monke_island.Weapons.Cutlass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UCutlassRules* Rules = nullptr;
END_DEFINE_SPEC(FCutlassSpec)

void FCutlassSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UCutlassRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	It("costs 11 % of the stamina per use and refuses a blow below that", [this]()
	{
		TestEqual(TEXT("cost"), CutlassMath::StaminaCost(100.0f, Rules->StaminaCostFraction), 11.0f, 0.001f);
		TestTrue(TEXT("11 left"), CutlassMath::HasStaminaFor(11.0f, 100.0f, Rules->StaminaCostFraction));
		TestFalse(TEXT("10 left"), CutlassMath::HasStaminaFor(10.0f, 100.0f, Rules->StaminaCostFraction));
		// a full bar gives 9 blows
		float Stamina = 100.0f;
		int32 Blows = 0;
		while (CutlassMath::HasStaminaFor(Stamina, 100.0f, Rules->StaminaCostFraction))
		{
			Stamina -= CutlassMath::StaminaCost(100.0f, Rules->StaminaCostFraction);
			++Blows;
		}
		TestEqual(TEXT("blows per bar"), Blows, 9);
	});

	It("chains right cut, backhand cut and thrust, and restarts after the window", [this]()
	{
		const float Window = Rules->ComboWindowSeconds;
		TestEqual(TEXT("first"), CutlassMath::NextComboMove(ECutlassMove::None, 0.0, Window), ECutlassMove::SlashRight);
		TestEqual(TEXT("second"), CutlassMath::NextComboMove(ECutlassMove::SlashRight, 0.1, Window), ECutlassMove::SlashLeft);
		TestEqual(TEXT("third"), CutlassMath::NextComboMove(ECutlassMove::SlashLeft, 0.1, Window), ECutlassMove::Thrust);
		TestEqual(TEXT("again"), CutlassMath::NextComboMove(ECutlassMove::Thrust, 0.1, Window), ECutlassMove::SlashRight);
		TestEqual(TEXT("late"), CutlassMath::NextComboMove(ECutlassMove::SlashRight, Window + 0.1, Window), ECutlassMove::SlashRight);
	});

	It("uses the damage of the maquette and kills a Coco Warrior (80) with a combo", [this]()
	{
		TestEqual(TEXT("right"), Rules->SlashRight.Damage, 35.0f);
		TestEqual(TEXT("left"), Rules->SlashLeft.Damage, 35.0f);
		TestEqual(TEXT("thrust"), Rules->Thrust.Damage, 45.0f);
		TestEqual(TEXT("finisher"), Rules->Finisher.Damage, 70.0f);
		TestFalse(TEXT("the thrust does not cut in two"), Rules->Thrust.bCut);
		TestTrue(TEXT("two cuts and a thrust"), Rules->SlashRight.Damage + Rules->SlashLeft.Damage + Rules->Thrust.Damage >= 80.0f);
		TestTrue(TEXT("a cut and the finisher"), Rules->SlashRight.Damage + Rules->Finisher.Damage >= 80.0f);
	});

	It("dulls under half sharpness", [this]()
	{
		TestEqual(TEXT("sharp"), CutlassMath::BlowDamage(35.0f, 1.0f, Rules->DullThreshold, Rules->DullDamageMultiplier), 35.0f, 0.001f);
		TestEqual(TEXT("dull"), CutlassMath::BlowDamage(35.0f, 0.4f, Rules->DullThreshold, Rules->DullDamageMultiplier), 28.0f, 0.001f);
	});

	It("parries 80 % of a frontal hit while there is stamina", [this]()
	{
		TestEqual(TEXT("front"), CutlassMath::ParriedDamage(20.0f, true, 1.0f, Rules->ParryFrontDot, Rules->ParryReduction, true), 4.0f, 0.001f);
		TestEqual(TEXT("behind"), CutlassMath::ParriedDamage(20.0f, true, -1.0f, Rules->ParryFrontDot, Rules->ParryReduction, true), 20.0f, 0.001f);
		TestEqual(TEXT("exhausted"), CutlassMath::ParriedDamage(20.0f, true, 1.0f, Rules->ParryFrontDot, Rules->ParryReduction, false), 20.0f, 0.001f);
		TestEqual(TEXT("lowered"), CutlassMath::ParriedDamage(20.0f, false, 1.0f, Rules->ParryFrontDot, Rules->ParryReduction, true), 20.0f, 0.001f);
	});

	It("sweeps the arc in front within the reach only", [this]()
	{
		const FVector Forward(1.0, 0.0, 0.0);
		TestTrue(TEXT("in front"), CutlassMath::IsInSweep(Forward, FVector(60.0, 0.0, 0.0), 85.0f, 110.0f));
		TestTrue(TEXT("at 50 degrees"), CutlassMath::IsInSweep(Forward, FVector(50.0, 59.0, 0.0), 85.0f, 110.0f));
		TestFalse(TEXT("at 70 degrees"), CutlassMath::IsInSweep(Forward, FVector(25.0, 70.0, 0.0), 85.0f, 110.0f));
		TestFalse(TEXT("behind"), CutlassMath::IsInSweep(Forward, FVector(-60.0, 0.0, 0.0), 85.0f, 110.0f));
		TestFalse(TEXT("too far"), CutlassMath::IsInSweep(Forward, FVector(120.0, 0.0, 0.0), 85.0f, 110.0f));
	});

	It("plays the second PC's tracks in order, the hit inside each blow", [this]()
	{
		for (const ECutlassMove Move : { ECutlassMove::SlashRight, ECutlassMove::SlashLeft, ECutlassMove::Thrust, ECutlassMove::Finisher })
		{
			const FCutlassAttack* Attack = Rules->GetAttack(Move);
			if (!TestNotNull(TEXT("attack"), Attack) || !TestTrue(TEXT("keys"), Attack->Keys.Num() >= 3))
			{
				return;
			}
			for (int32 Index = 1; Index < Attack->Keys.Num(); ++Index)
			{
				TestTrue(TEXT("sorted"), Attack->Keys[Index].Time >= Attack->Keys[Index - 1].Time);
			}
			TestTrue(TEXT("hit inside the blow"), Attack->HitTime > 0.0f && Attack->HitTime < Attack->Keys.Last().Time);
		}
		TestTrue(TEXT("draw track"), Rules->DrawKeys.Num() >= 3);
		TestTrue(TEXT("sheathe track"), Rules->SheatheKeys.Num() >= 3);
		TestEqual(TEXT("draw time"), Rules->GetMoveDuration(ECutlassMove::Draw), 0.5f, 0.001f);
		TestEqual(TEXT("sheathe time"), Rules->GetMoveDuration(ECutlassMove::Sheathe), 0.6f, 0.001f);
	});

	It("keeps the blade in front of the bean and the scabbard on its left hip", [this]()
	{
		// mesh space: +Y forward, -X right
		TestTrue(TEXT("guard in front"), Rules->GuardKey.GripLocation.Y > 0.0);
		TestTrue(TEXT("thrust points forward"), FRotationMatrix(Rules->Thrust.Keys[2].GripRotation).GetUnitAxis(EAxis::X).Y > 0.9);
		TestTrue(TEXT("scabbard on the left"), Rules->ScabbardRest.GetLocation().X > 0.0);
	});

	It("reaches a target with a two-bone arm", [this]()
	{
		const FVector Shoulder(0.0, 0.0, 0.0);
		const FVector Target(30.0, 0.0, 0.0);
		const FVector Elbow = CutlassMath::SolveElbow(Shoulder, Target, 20.0f, 20.0f, FVector(0.0, 0.0, -1.0));
		TestEqual(TEXT("upper length"), FVector::Dist(Shoulder, Elbow), 20.0, 0.01);
		TestEqual(TEXT("lower length"), FVector::Dist(Elbow, Target), 20.0, 0.01);
		TestTrue(TEXT("bends toward the pole"), Elbow.Z < 0.0);
		const FVector Far = CutlassMath::SolveElbow(Shoulder, FVector(100.0, 0.0, 0.0), 20.0f, 20.0f, FVector(0.0, 0.0, -1.0));
		TestEqual(TEXT("out of reach: straight"), Far.X, 20.0, 0.1);
	});
}

#endif
