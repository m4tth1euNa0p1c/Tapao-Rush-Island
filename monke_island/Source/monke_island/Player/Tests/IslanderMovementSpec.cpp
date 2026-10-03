// monke_island

#include "Misc/AutomationTest.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Items/ValuableComponent.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Player
BEGIN_DEFINE_SPEC(FIslanderMovementSpec, "monke_island.Player.Movement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UIslanderMovementComponent* Move = nullptr;
END_DEFINE_SPEC(FIslanderMovementSpec)

void FIslanderMovementSpec::Define()
{
	Describe("Stamina", [this]()
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

		It("starts full", [this]()
		{
			TestEqual(TEXT("stamina"), Move->GetStamina(), Move->MaxStamina);
			TestFalse(TEXT("exhausted"), Move->IsExhausted());
		});

		It("drains while sprinting at SprintStaminaCost per second", [this]()
		{
			Move->UpdateStamina(1.0f, true, false, true);
			TestEqual(TEXT("stamina after 1 s"), Move->GetStamina(), Move->MaxStamina - Move->SprintStaminaCost, 0.01f);
		});

		It("drains while swimming (cahier §4)", [this]()
		{
			Move->UpdateStamina(2.0f, false, true, true);
			TestEqual(TEXT("stamina after 2 s"), Move->GetStamina(), Move->MaxStamina - 2.0f * Move->SwimStaminaCost, 0.01f);
		});

		It("exhausts at zero and only recovers past ExhaustedRecoverFraction", [this]()
		{
			for (int32 Step = 0; Step < 100 && !Move->IsExhausted(); ++Step)
			{
				Move->UpdateStamina(0.5f, true, false, true);
			}
			TestTrue(TEXT("exhausted"), Move->IsExhausted());
			TestEqual(TEXT("empty"), Move->GetStamina(), 0.0f);

			// regen delay, then a little recovery is not enough
			Move->UpdateStamina(Move->StaminaRegenDelay + 0.01f, false, false, false);
			Move->UpdateStamina(0.5f, false, false, false);
			TestTrue(TEXT("still exhausted"), Move->IsExhausted());

			for (int32 Step = 0; Step < 200 && Move->IsExhausted(); ++Step)
			{
				Move->UpdateStamina(0.1f, false, false, false);
			}
			TestFalse(TEXT("recovered"), Move->IsExhausted());
			TestTrue(TEXT("recovered at the threshold"), Move->GetStaminaFraction() >= Move->ExhaustedRecoverFraction);
		});

		It("restores faster standing still than walking", [this]()
		{
			Move->ConsumeStamina(50.0f);
			Move->UpdateStamina(Move->StaminaRegenDelay + 0.01f, false, false, false);
			const float Start = Move->GetStamina();
			Move->UpdateStamina(1.0f, false, false, true);
			const float Walking = Move->GetStamina() - Start;
			Move->UpdateStamina(1.0f, false, false, false);
			const float Idle = Move->GetStamina() - Start - Walking;
			TestTrue(TEXT("idle regen > walking regen"), Idle > Walking);
		});

		It("refuses a cost larger than what is left", [this]()
		{
			TestTrue(TEXT("first"), Move->ConsumeStamina(90.0f));
			TestFalse(TEXT("second"), Move->ConsumeStamina(20.0f));
			TestEqual(TEXT("unchanged"), Move->GetStamina(), Move->MaxStamina - 90.0f, 0.01f);
		});

		It("costs more stamina when carrying weight", [this]()
		{
			Move->SetCarriedWeight(50.0f);
			Move->UpdateStamina(1.0f, true, false, true);
			TestTrue(TEXT("heavier drain"), Move->GetStamina() < Move->MaxStamina - Move->SprintStaminaCost);
		});
	});

	Describe("Fall damage", [this]()
	{
		It("is zero up to the safe speed", [this]()
		{
			TestEqual(TEXT("safe"), UIslanderHealthComponent::ComputeFallDamage(1000.0f, 1000.0f, 2000.0f, 100.0f), 0.0f);
		});

		It("is linear between safe and lethal", [this]()
		{
			TestEqual(TEXT("half"), UIslanderHealthComponent::ComputeFallDamage(1500.0f, 1000.0f, 2000.0f, 100.0f), 50.0f, 0.01f);
		});

		It("kills from the lethal speed", [this]()
		{
			TestEqual(TEXT("lethal"), UIslanderHealthComponent::ComputeFallDamage(2500.0f, 1000.0f, 2000.0f, 100.0f), 100.0f);
		});
	});

	Describe("Grab strength", [this]()
	{
		It("is full up to the lift mass", [this]()
		{
			TestEqual(TEXT("light"), UIslanderGrabComponent::ComputeStrength(20.0f, 1, 25.0f, 0.12f), 1.0f);
		});

		It("drops for heavier objects", [this]()
		{
			TestEqual(TEXT("double"), UIslanderGrabComponent::ComputeStrength(50.0f, 1, 25.0f, 0.12f), 0.5f, 0.001f);
		});

		It("adds up with more holders (R.E.P.O.)", [this]()
		{
			TestEqual(TEXT("two holders"), UIslanderGrabComponent::ComputeStrength(50.0f, 2, 25.0f, 0.12f), 1.0f);
		});

		It("never goes under the minimum", [this]()
		{
			TestEqual(TEXT("huge"), UIslanderGrabComponent::ComputeStrength(1000.0f, 1, 25.0f, 0.12f), 0.12f, 0.001f);
		});
	});

	Describe("Valuable", [this]()
	{
		It("loses nothing on a soft touch", [this]()
		{
			TestEqual(TEXT("soft"), UValuableComponent::ComputeImpactLoss(200.0f, 250.0f, 1200.0f, 0.35f, 100), 0);
		});

		It("loses at most MaxLossPerHit on a heavy impact", [this]()
		{
			TestEqual(TEXT("heavy"), UValuableComponent::ComputeImpactLoss(5000.0f, 250.0f, 1200.0f, 0.35f, 100), 35);
		});

		It("loses at least 1 on any counted impact", [this]()
		{
			TestEqual(TEXT("light hit"), UValuableComponent::ComputeImpactLoss(251.0f, 250.0f, 1200.0f, 0.35f, 100), 1);
		});
	});

	Describe("Carry", [this]()
	{
		It("tires the carrier of a 10 kg statuette even at a walk", [this]()
		{
			const UIslanderMovementComponent* Move = GetDefault<UIslanderMovementComponent>();
			const float Walking = UIslanderMovementComponent::ComputeCarryStaminaCost(10.0f, true, Move->CarryStaminaPerKgMoving, Move->CarryStaminaPerKgIdle);
			const float Standing = UIslanderMovementComponent::ComputeCarryStaminaCost(10.0f, false, Move->CarryStaminaPerKgMoving, Move->CarryStaminaPerKgIdle);
			TestTrue(TEXT("walking with it costs stamina"), Walking > 0.0f);
			TestTrue(TEXT("standing still costs less"), Standing < Walking);
			TestTrue(TEXT("a full bar lasts between 15 s and 60 s at a walk"), Move->MaxStamina / Walking >= 15.0f && Move->MaxStamina / Walking <= 60.0f);
		});

		It("costs nothing with empty hands", [this]()
		{
			TestEqual(TEXT("empty hands"), UIslanderMovementComponent::ComputeCarryStaminaCost(0.0f, true, 0.5f, 0.15f), 0.0f);
		});
	});

	Describe("Guard", [this]()
	{
		It("reduces a frontal hit", [this]()
		{
			TestEqual(TEXT("blocked"), UIslanderCombatComponent::ComputeGuardedDamage(20.0f, true, 1.0f, 0.2f, 0.25f, true), 5.0f, 0.01f);
		});

		It("does not block a hit from behind", [this]()
		{
			TestEqual(TEXT("from behind"), UIslanderCombatComponent::ComputeGuardedDamage(20.0f, true, -0.8f, 0.2f, 0.25f, true), 20.0f);
		});

		It("breaks without stamina", [this]()
		{
			TestEqual(TEXT("broken"), UIslanderCombatComponent::ComputeGuardedDamage(20.0f, true, 1.0f, 0.2f, 0.25f, false), 20.0f);
		});

		It("does nothing when down", [this]()
		{
			TestEqual(TEXT("down"), UIslanderCombatComponent::ComputeGuardedDamage(20.0f, false, 1.0f, 0.2f, 0.25f, true), 20.0f);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
