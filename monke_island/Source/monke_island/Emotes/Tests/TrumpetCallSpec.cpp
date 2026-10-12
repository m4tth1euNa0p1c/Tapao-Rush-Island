// monke_island

#include "Misc/AutomationTest.h"
#include "Emotes/TrumpetCallMath.h"
#include "Emotes/TrumpetEmoteRules.h"
#include "UI/Tapao/TapaoSpellMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Emotes.TrumpetCall
BEGIN_DEFINE_SPEC(FTrumpetCallSpec, "monke_island.Emotes.TrumpetCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTrumpetCallSpec)

void FTrumpetCallSpec::Define()
{
	Describe("Rules", [this]()
	{
		It("asks 10 s of call, a minute of cooldown, a full bar of stamina and the key A (owner 2026-10-11)", [this]()
		{
			const UTrumpetEmoteRules* Rules = GetDefault<UTrumpetEmoteRules>();
			TestEqual(TEXT("call"), Rules->CallSeconds, 10.0f);
			TestEqual(TEXT("cooldown"), Rules->CallCooldown, 60.0f);
			TestEqual(TEXT("stamina"), Rules->CallStaminaFraction, 1.0f);
			TestEqual(TEXT("over the whole loop"), Rules->CallStaminaSeconds, 0.0f);
			TestTrue(TEXT("key A"), Rules->CallKey == EKeys::A);
		});
	});

	Describe("Duration", [this]()
	{
		It("fills the 10 s with the equip, the play loop and the put away", [this]()
		{
			const UTrumpetEmoteRules* Rules = GetDefault<UTrumpetEmoteRules>();
			const float Loop = TrumpetCallMath::LoopSeconds(Rules->CallSeconds, Rules->EquipSeconds, Rules->UnequipSeconds);
			TestEqual(TEXT("loop between the clips"), Loop, 8.416f, 0.0001f);
			TestEqual(TEXT("whole call"), Rules->EquipSeconds + Loop + Rules->UnequipSeconds, 10.0f, 0.0001f);
			TestEqual(TEXT("never negative"), TrumpetCallMath::LoopSeconds(1.0f, 0.792f, 0.792f), 0.0f);
		});
	});

	Describe("Cooldown", [this]()
	{
		It("counts a minute from the start of the call", [this]()
		{
			TestTrue(TEXT("never called: ready"), TrumpetCallMath::IsReady(5.0, -1.0e9, 60.0f));
			TestEqual(TEXT("right after the start"), TrumpetCallMath::CooldownLeft(100.0, 100.0, 60.0f), 60.0f);
			TestEqual(TEXT("at the end of the call"), TrumpetCallMath::CooldownLeft(110.0, 100.0, 60.0f), 50.0f, 0.0001f);
			TestEqual(TEXT("share of the veil at the end of the call"), TrumpetCallMath::CooldownFraction(110.0, 100.0, 60.0f), 50.0f / 60.0f, 0.0001f);
			TestEqual(TEXT("over"), TrumpetCallMath::CooldownLeft(161.0, 100.0, 60.0f), 0.0f);
		});

		It("refuses a call while the cooldown runs and takes it once over", [this]()
		{
			TestFalse(TEXT("during the call"), TrumpetCallMath::IsReady(105.0, 100.0, 60.0f));
			TestFalse(TEXT("half a second before the minute"), TrumpetCallMath::IsReady(159.5, 100.0, 60.0f));
			TestTrue(TEXT("at the minute"), TrumpetCallMath::IsReady(160.0, 100.0, 60.0f));
			TestTrue(TEXT("no cooldown"), TrumpetCallMath::IsReady(100.0, 100.0, 0.0f));
			TestEqual(TEXT("no cooldown, no veil"), TrumpetCallMath::CooldownFraction(100.0, 100.0, 0.0f), 0.0f);
		});
	});

	Describe("Stamina", [this]()
	{
		It("gives a full bar back over the play loop, nothing while the trumpet comes out", [this]()
		{
			const float Loop = TrumpetCallMath::LoopSeconds(10.0f, 0.792f, 0.792f);
			TestEqual(TEXT("during the equip"), TrumpetCallMath::StaminaGiven(0.5f, 0.792f, Loop, 0.0f, 100.0f), 0.0f);
			TestEqual(TEXT("half way through the loop"), TrumpetCallMath::StaminaGiven(0.792f + 0.5f * Loop, 0.792f, Loop, 0.0f, 100.0f), 50.0f, 0.01f);
			TestEqual(TEXT("at the end of the loop"), TrumpetCallMath::StaminaGiven(0.792f + Loop, 0.792f, Loop, 0.0f, 100.0f), 100.0f, 0.001f);
			TestEqual(TEXT("never more"), TrumpetCallMath::StaminaGiven(10.0f, 0.792f, Loop, 0.0f, 100.0f), 100.0f, 0.001f);
		});

		It("gives it faster over a shorter time, and only its share of the bar", [this]()
		{
			TestEqual(TEXT("over 4 s, after 2 s"), TrumpetCallMath::StaminaGiven(2.792f, 0.792f, 8.416f, 4.0f, 100.0f), 50.0f, 0.01f);
			TestEqual(TEXT("over 4 s, after 5 s"), TrumpetCallMath::StaminaGiven(5.792f, 0.792f, 8.416f, 4.0f, 100.0f), 100.0f, 0.001f);
			TestEqual(TEXT("a time longer than the loop is the loop"), TrumpetCallMath::StaminaGiven(0.792f + 8.416f, 0.792f, 8.416f, 30.0f, 100.0f), 100.0f, 0.001f);
			TestEqual(TEXT("half a bar"), TrumpetCallMath::StaminaGiven(9.208f, 0.792f, 8.416f, 0.0f, 50.0f), 50.0f, 0.001f);
			TestEqual(TEXT("nothing to give"), TrumpetCallMath::StaminaGiven(5.0f, 0.792f, 8.416f, 0.0f, 0.0f), 0.0f);
		});

		It("stops where a broken call stopped (the component holds the elapsed time at the cut)", [this]()
		{
			const float AtCut = TrumpetCallMath::StaminaGiven(0.792f + 2.0f, 0.792f, 8.416f, 0.0f, 100.0f);
			TestEqual(TEXT("the share of the loop played"), AtCut, 100.0f * 2.0f / 8.416f, 0.01f);
		});
	});

	Describe("Icon", [this]()
	{
		It("veils the whole square as the recharge starts, nothing once ready", [this]()
		{
			TArray<FVector2f> Border;
			TapaoSpellMath::SquareSweep(48.0f, 1.0f, Border);
			TestEqual(TEXT("12 o'clock, the 4 corners, 12 o'clock"), Border.Num(), 6);
			if (Border.Num() == 6)
			{
				TestTrue(TEXT("from 12 o'clock"), Border[0].Equals(FVector2f(24.0f, 0.0f), 0.01f));
				TestTrue(TEXT("first corner top right"), Border[1].Equals(FVector2f(48.0f, 0.0f), 0.01f));
				TestTrue(TEXT("to 12 o'clock"), Border.Last().Equals(FVector2f(24.0f, 0.0f), 0.01f));
			}
			TapaoSpellMath::SquareSweep(48.0f, 0.0f, Border);
			TestEqual(TEXT("ready: no veil"), Border.Num(), 0);
		});

		It("turns away clockwise: a quarter left veils the top left only", [this]()
		{
			TArray<FVector2f> Border;
			TapaoSpellMath::SquareSweep(48.0f, 0.25f, Border);
			TestEqual(TEXT("9 o'clock, the top left corner, 12 o'clock"), Border.Num(), 3);
			if (Border.Num() == 3)
			{
				TestTrue(TEXT("from 9 o'clock"), Border[0].Equals(FVector2f(0.0f, 24.0f), 0.01f));
				TestTrue(TEXT("top left corner"), Border[1].Equals(FVector2f(0.0f, 0.0f), 0.01f));
				TestTrue(TEXT("to 12 o'clock"), Border[2].Equals(FVector2f(24.0f, 0.0f), 0.01f));
			}
		});
	});
}

#endif
