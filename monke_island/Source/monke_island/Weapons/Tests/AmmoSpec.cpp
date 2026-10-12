// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/AmmoDefinition.h"
#include "Weapons/AmmoTypes.h"
#include "Weapons/BlunderbussRules.h"
#include "Weapons/GrenadeRules.h"
#include "Weapons/PirateMusketRules.h"
#include "Weapons/PistolRules.h"
#include "Weapons/RifleRules.h"
#include "Weapons/ShotgunRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Ammo
BEGIN_DEFINE_SPEC(FAmmoSpec, "monke_island.Weapons.Ammo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FAmmoSpec)

void FAmmoSpec::Define()
{
	Describe("Room", [this]()
	{
		It("is the capacity less the rounds loaded and in the pouch, never negative", [this]()
		{
			TestEqual(TEXT("empty weapon"), AmmoMath::Room(15, 0, 0), 15);
			TestEqual(TEXT("one loaded, ten in the pouch"), AmmoMath::Room(15, 1, 10), 4);
			TestEqual(TEXT("full"), AmmoMath::Room(15, 1, 14), 0);
			TestEqual(TEXT("over full (loadout of the day before)"), AmmoMath::Room(15, 1, 20), 0);
		});
	});

	Describe("Distribute", [this]()
	{
		It("fills the first weapon (the one in hand) before the others and leaves the rest in the packet", [this]()
		{
			TArray<int32> Given;
			TestEqual(TEXT("taken"), AmmoMath::Distribute({ 4, 10 }, 6, Given), 6);
			TestEqual(TEXT("first weapon full"), Given[0], 4);
			TestEqual(TEXT("the rest to the second"), Given[1], 2);
			TestEqual(TEXT("more than the rooms"), AmmoMath::Distribute({ 2, 1 }, 6, Given), 3);
			TestEqual(TEXT("three left in the packet"), 6 - 3, 3);
			TestEqual(TEXT("no weapon"), AmmoMath::Distribute({}, 6, Given), 0);
			TestEqual(TEXT("nothing to give"), AmmoMath::Distribute({ 5 }, 0, Given), 0);
			TestEqual(TEXT("negative count"), AmmoMath::Distribute({ 5 }, -3, Given), 0);
		});
	});

	Describe("Judge", [this]()
	{
		It("refuses without a weapon, refuses a full pouch, takes all or part otherwise", [this]()
		{
			TestEqual(TEXT("no weapon"), AmmoMath::Judge(0, 10, 6), AmmoMath::EPickupResult::NoWeapon);
			TestEqual(TEXT("full"), AmmoMath::Judge(1, 0, 6), AmmoMath::EPickupResult::Full);
			TestEqual(TEXT("taken"), AmmoMath::Judge(1, 10, 6), AmmoMath::EPickupResult::Taken);
			TestEqual(TEXT("part"), AmmoMath::Judge(2, 4, 6), AmmoMath::EPickupResult::PartlyTaken);
		});
	});

	Describe("Kinds", [this]()
	{
		It("gives each firearm its ammunition kind so a packet finds its weapon", [this]()
		{
			TestEqual(TEXT("musket"), GetDefault<UPirateMusketRules>()->AmmoKind, EAmmoKind::MusketBall);
			TestEqual(TEXT("pistol"), GetDefault<UPistolRules>()->AmmoKind, EAmmoKind::PistolCharge);
			TestEqual(TEXT("double pistol"), GetDefault<UDoublePistolRules>()->AmmoKind, EAmmoKind::PistolCharge);
			TestEqual(TEXT("shotgun"), GetDefault<UShotgunRules>()->AmmoKind, EAmmoKind::Shell);
			TestEqual(TEXT("blunderbuss"), GetDefault<UBlunderbussRules>()->AmmoKind, EAmmoKind::Shot);
			TestEqual(TEXT("rifle"), GetDefault<URifleRules>()->AmmoKind, EAmmoKind::RifleCartridge);
			TestEqual(TEXT("grenade"), GetDefault<UGrenadeRules>()->AmmoKind, EAmmoKind::Grenade);
			TestEqual(TEXT("the base takes nothing"), GetDefault<UFirearmRules>()->AmmoKind, EAmmoKind::None);
		});
	});

	Describe("Definition", [this]()
	{
		It("counts the rounds of a kind over its lots (a mixed crate)", [this]()
		{
			UAmmoDefinition* Crate = NewObject<UAmmoDefinition>();
			FAmmoLot Balls;
			Balls.Kind = EAmmoKind::MusketBall;
			Balls.Count = 12;
			FAmmoLot Shells;
			Shells.Kind = EAmmoKind::Shell;
			Shells.Count = 8;
			FAmmoLot MoreBalls;
			MoreBalls.Kind = EAmmoKind::MusketBall;
			MoreBalls.Count = 3;
			Crate->Lots = { Balls, Shells, MoreBalls };
			TestEqual(TEXT("balls"), Crate->CountOf(EAmmoKind::MusketBall), 15);
			TestEqual(TEXT("shells"), Crate->CountOf(EAmmoKind::Shell), 8);
			TestEqual(TEXT("none of the rest"), Crate->CountOf(EAmmoKind::Grenade), 0);
		});
	});
}

#endif
