// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiRules.h"
#include "Weapons/BlunderbussRules.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/FirearmMath.h"
#include "Weapons/PirateMusketRules.h"
#include "Weapons/RifleRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** Damage of one pellet of the blunderbuss at a distance (cm) */
	float PelletAt(const UBlunderbussRules* Rules, float Distance)
	{
		return Rules->Damage * FirearmDamage::ScaleAtDistance(Rules->DamageFalloff, Distance);
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.NewFirearms
BEGIN_DEFINE_SPEC(FNewFirearmsSpec, "monke_island.Weapons.NewFirearms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FNewFirearmsSpec)

void FNewFirearmsSpec::Define()
{
	Describe("Blunderbuss", [this]()
	{
		It("throws ten pellets of shot per charge, wider than the shotgun, and reloads like the musket", [this]()
		{
			const UBlunderbussRules* Rules = GetDefault<UBlunderbussRules>();
			const UPirateMusketRules* Musket = GetDefault<UPirateMusketRules>();
			TestEqual(TEXT("pellets"), Rules->PelletCount, 10);
			TestTrue(TEXT("a wide cone"), Rules->HipSpreadDegrees >= 5.0f && Rules->AimSpreadDegrees >= 3.0f);
			TestEqual(TEXT("charges of shot"), Rules->AmmoKind, EAmmoKind::Shot);
			TestTrue(TEXT("slower reload than the musket"), Rules->ReloadSeconds > Musket->ReloadSeconds);
			TestEqual(TEXT("the musket's reload gesture"), Rules->ReloadGesture, EFirearmGesture::GroundReload);
			TestNotNull(TEXT("ground reload clip"), Rules->FindClip(EFirearmGesture::GroundReload));
			TestNotNull(TEXT("shot clip"), Rules->FindClip(EFirearmGesture::Fire));
			TestTrue(TEXT("shreds at close range"), Rules->CloseBurstDistanceCm > 0.0f && Rules->CloseShredLimbsMax >= Rules->CloseShredLimbsMin);
		});

		It("kills any tiki at 3 m with its pellets on the body, never at 8 m, and scratches beyond 12 m", [this]()
		{
			const UBlunderbussRules* Rules = GetDefault<UBlunderbussRules>();
			const UTikiRules* Tiki = GetDefault<UTikiRules>();
			const float Toughest = FMath::Max(Tiki->VillagerHealth, Tiki->WarriorHealth);
			const float Weakest = FMath::Min(Tiki->VillagerHealth, Tiki->WarriorHealth);
			TestTrue(TEXT("all pellets at 3 m kill the toughest"), PelletAt(Rules, 300.0f) * Rules->PelletCount >= Toughest);
			TestTrue(TEXT("6 of 10 at 3 m kill the toughest"), PelletAt(Rules, 300.0f) * 6.0f >= Toughest);
			TestTrue(TEXT("3 m is a close (shredding) shot"), 300.0f <= Rules->CloseBurstDistanceCm);
			TestTrue(TEXT("all pellets at 8 m do not kill the weakest"), PelletAt(Rules, 800.0f) * Rules->PelletCount < Weakest);
			TestTrue(TEXT("12 m: scratches"), PelletAt(Rules, 1200.0f) * Rules->PelletCount < Weakest * 0.5f);
			TestTrue(TEXT("nothing past the range"), PelletAt(Rules, Rules->RangeCm) <= Rules->Damage * 0.1f);
		});

		It("spreads its pellets inside its cone", [this]()
		{
			const UBlunderbussRules* Rules = GetDefault<UBlunderbussRules>();
			FRandomStream Stream(42);
			TArray<FVector> Directions;
			FirearmMath::SpreadDirections(FVector::ForwardVector, Rules->PelletCount, Rules->HipSpreadDegrees, Stream, Directions);
			TestEqual(TEXT("count"), Directions.Num(), Rules->PelletCount);
			float MaxAngle = 0.0f;
			for (const FVector& Direction : Directions)
			{
				MaxAngle = FMath::Max(MaxAngle, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Direction.X), -1.0f, 1.0f))));
			}
			TestTrue(TEXT("inside the cone"), MaxAngle <= Rules->HipSpreadDegrees + 0.01f);
		});
	});

	Describe("Rifle", [this]()
	{
		It("is a musket with a precise long shot of 90 paid by a longer reload (rules only, no class of its own)", [this]()
		{
			const URifleRules* Rules = GetDefault<URifleRules>();
			const UPirateMusketRules* Musket = GetDefault<UPirateMusketRules>();
			const UTikiRules* Tiki = GetDefault<UTikiRules>();
			TestEqual(TEXT("damage"), Rules->Damage, 90.0f);
			TestEqual(TEXT("range 160 m"), Rules->RangeCm, 16000.0f);
			TestEqual(TEXT("aimed spread"), Rules->AimSpreadDegrees, 0.2f);
			TestTrue(TEXT("tighter than the musket"), Rules->AimSpreadDegrees < Musket->AimSpreadDegrees);
			TestTrue(TEXT("longer reload than the musket"), Rules->ReloadSeconds > Musket->ReloadSeconds);
			TestEqual(TEXT("cartridges"), Rules->AmmoKind, EAmmoKind::RifleCartridge);
			TestTrue(TEXT("one ball kills a warrior on the body"), Rules->Damage >= Tiki->WarriorHealth);
			TestTrue(TEXT("no falloff"), Rules->DamageFalloff.IsEmpty() && Rules->FalloffStartCm <= 0.0f);
			TestNotNull(TEXT("its reload clip"), Rules->FindClip(Rules->ReloadGesture));
			TestTrue(TEXT("trigger then shot"), Rules->FindClip(EFirearmGesture::Fire) && Rules->FindClip(EFirearmGesture::Fire)->FindEventTime(Rules->FireEvent) > Rules->FindClip(EFirearmGesture::Fire)->FindEventTime(Rules->TriggerEvent));
		});
	});

	Describe("Discard", [this]()
	{
		It("throws the weapon away on fire only when nothing is loaded, nothing is left and nothing plays", [this]()
		{
			TestTrue(TEXT("empty: thrown"), FirearmMath::ShouldDiscard(0, 0, false, true));
			TestFalse(TEXT("busy: not yet"), FirearmMath::ShouldDiscard(0, 0, true, true));
			TestFalse(TEXT("a round loaded"), FirearmMath::ShouldDiscard(1, 0, false, true));
			TestFalse(TEXT("a round in the pouch"), FirearmMath::ShouldDiscard(0, 3, false, true));
			TestFalse(TEXT("rule off"), FirearmMath::ShouldDiscard(0, 0, false, false));
			// owner 2026-10-11 (T-329): the normal reload for every weapon, none is thrown away once empty
			TestFalse(TEXT("off by default"), GetDefault<UPirateMusketRules>()->bDiscardWhenEmpty || GetDefault<UBlunderbussRules>()->bDiscardWhenEmpty);
			TestEqual(TEXT("350 cm/s"), GetDefault<UPirateMusketRules>()->DiscardTossSpeed, 350.0f);
			TestEqual(TEXT("release event"), GetDefault<UPirateMusketRules>()->DiscardReleaseEvent, FName(TEXT("Release")));
		});

		It("tosses the weapon to the right and down", [this]()
		{
			const FVector Toss = FirearmMath::DiscardToss(FVector::RightVector, 350.0f, 30.0f);
			TestEqual(TEXT("right"), static_cast<float>(Toss.Y), 350.0f * FMath::Cos(FMath::DegreesToRadians(30.0f)), 0.01f);
			TestEqual(TEXT("down"), static_cast<float>(Toss.Z), -350.0f * FMath::Sin(FMath::DegreesToRadians(30.0f)), 0.01f);
			TestEqual(TEXT("not forward"), static_cast<float>(Toss.X), 0.0f, 0.01f);
			TestEqual(TEXT("speed"), static_cast<float>(Toss.Size()), 350.0f, 0.01f);
			// a tilted right vector is flattened: the throw never goes up
			const FVector Tilted = FirearmMath::DiscardToss(FVector(0.0, 0.7, 0.7), 100.0f, 30.0f);
			TestTrue(TEXT("down whatever the right vector"), Tilted.Z < 0.0);
		});
	});
}

#endif
