// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/Hostility.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/TikiVillager.h"
#include "Creatures/TikiWarrior.h"
#include "Fort/FortSoldier.h"
#include "Player/IslanderCharacter.h"
#include "GameFramework/Character.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FHostilitySpec, "monke_island.Creatures.Hostility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FHostilitySpec)

void FHostilitySpec::Define()
{
	Describe("FactionOf", [this]()
	{
		It("knows the player, the tiki, the soldier and the crab by their class", [this]()
		{
			TestTrue(TEXT("player"), Hostility::FactionOf(GetDefault<AIslanderCharacter>()) == EFaction::Player);
			TestTrue(TEXT("warrior"), Hostility::FactionOf(GetDefault<ATikiWarrior>()) == EFaction::Tiki);
			TestTrue(TEXT("villager"), Hostility::FactionOf(GetDefault<ATikiVillager>()) == EFaction::Tiki);
			TestTrue(TEXT("soldier"), Hostility::FactionOf(GetDefault<AFortSoldier>()) == EFaction::Soldier);
			TestTrue(TEXT("crab"), Hostility::FactionOf(GetDefault<AGiantCrab>()) == EFaction::Crab);
		});

		It("gives no side to nothing, nor to a plain character", [this]()
		{
			TestTrue(TEXT("null"), Hostility::FactionOf(nullptr) == EFaction::None);
			TestTrue(TEXT("character"), Hostility::FactionOf(GetDefault<ACharacter>()) == EFaction::None);
		});
	});

	Describe("AreFoes", [this]()
	{
		It("sets every side against every other one, both ways", [this]()
		{
			const EFaction Sides[] = { EFaction::Player, EFaction::Tiki, EFaction::Soldier, EFaction::Crab };
			for (const EFaction A : Sides)
			{
				for (const EFaction B : Sides)
				{
					TestTrue(*FString::Printf(TEXT("%d vs %d"), static_cast<int32>(A), static_cast<int32>(B)), Hostility::AreFoes(A, B) == (A != B));
				}
			}
		});

		It("keeps a side at peace with itself and with nobody", [this]()
		{
			TestFalse(TEXT("tiki vs tiki"), Hostility::AreFoes(EFaction::Tiki, EFaction::Tiki));
			TestFalse(TEXT("soldier vs soldier"), Hostility::AreFoes(EFaction::Soldier, EFaction::Soldier));
			TestFalse(TEXT("none vs player"), Hostility::AreFoes(EFaction::None, EFaction::Player));
			TestFalse(TEXT("crab vs none"), Hostility::AreFoes(EFaction::Crab, EFaction::None));
			TestFalse(TEXT("none vs none"), Hostility::AreFoes(EFaction::None, EFaction::None));
		});
	});

	Describe("IsLivingFoe", [this]()
	{
		It("takes a living thing of another side", [this]()
		{
			TestTrue(TEXT("soldier vs crab"), Hostility::IsLivingFoe(EFaction::Soldier, GetDefault<AGiantCrab>()));
			TestTrue(TEXT("warrior vs soldier"), Hostility::IsLivingFoe(GetDefault<ATikiWarrior>(), GetDefault<AFortSoldier>()));
			TestTrue(TEXT("soldier vs player"), Hostility::IsLivingFoe(GetDefault<AFortSoldier>(), GetDefault<AIslanderCharacter>()));
			TestTrue(TEXT("player vs warrior"), Hostility::IsLivingFoe(GetDefault<AIslanderCharacter>(), GetDefault<ATikiWarrior>()));
		});

		It("spares its own side, itself, nothing, and a thing without health", [this]()
		{
			TestFalse(TEXT("soldier vs soldier"), Hostility::IsLivingFoe(EFaction::Soldier, GetDefault<AFortSoldier>()));
			TestFalse(TEXT("warrior vs villager"), Hostility::IsLivingFoe(GetDefault<ATikiWarrior>(), GetDefault<ATikiVillager>()));
			TestFalse(TEXT("itself"), Hostility::IsLivingFoe(GetDefault<AGiantCrab>(), GetDefault<AGiantCrab>()));
			TestFalse(TEXT("null"), Hostility::IsLivingFoe(EFaction::Player, nullptr));
			TestFalse(TEXT("no side"), Hostility::IsLivingFoe(EFaction::None, GetDefault<AGiantCrab>()));
			TestFalse(TEXT("no health"), Hostility::IsLivingFoe(EFaction::Tiki, GetDefault<ACharacter>()));
		});
	});
}

#endif
