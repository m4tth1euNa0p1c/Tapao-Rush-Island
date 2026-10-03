// monke_island

#include "Misc/AutomationTest.h"
#include "Items/ConsumablePickupActor.h"
#include "Player/IslanderHealthComponent.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Items.Consumable
BEGIN_DEFINE_SPEC(FConsumableSpec, "monke_island.Items.Consumable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FConsumableSpec)

void FConsumableSpec::Define()
{
	Describe("Heal", [this]()
	{
		It("restores the amount when there is room", [this]()
		{
			TestEqual(TEXT("healed"), UIslanderHealthComponent::ComputeHeal(40.0f, 100.0f, 25.0f), 25.0f);
		});

		It("never goes above MaxHealth", [this]()
		{
			TestEqual(TEXT("healed"), UIslanderHealthComponent::ComputeHeal(90.0f, 100.0f, 25.0f), 10.0f);
		});

		It("restores nothing at full health or with a non positive amount", [this]()
		{
			TestEqual(TEXT("full"), UIslanderHealthComponent::ComputeHeal(100.0f, 100.0f, 25.0f), 0.0f);
			TestEqual(TEXT("zero"), UIslanderHealthComponent::ComputeHeal(50.0f, 100.0f, 0.0f), 0.0f);
			TestEqual(TEXT("negative"), UIslanderHealthComponent::ComputeHeal(50.0f, 100.0f, -10.0f), 0.0f);
		});
	});

	Describe("Pickup", [this]()
	{
		It("is consumable by a hurt living player", [this]()
		{
			TestTrue(TEXT("hurt"), AConsumablePickupActor::CanConsume(false, false, 60.0f, 100.0f, false));
		});

		It("is kept at full health unless usable at full health", [this]()
		{
			TestFalse(TEXT("full, kept"), AConsumablePickupActor::CanConsume(false, false, 100.0f, 100.0f, false));
			TestTrue(TEXT("full, allowed"), AConsumablePickupActor::CanConsume(false, false, 100.0f, 100.0f, true));
		});

		It("is consumed only once and never by a dead player", [this]()
		{
			TestFalse(TEXT("already consumed"), AConsumablePickupActor::CanConsume(true, false, 10.0f, 100.0f, true));
			TestFalse(TEXT("dead"), AConsumablePickupActor::CanConsume(false, true, 0.0f, 100.0f, true));
		});
	});
}

#endif
