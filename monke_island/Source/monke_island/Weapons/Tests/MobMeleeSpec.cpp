// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiCombat.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiShredDemo.h"
#include "Weapons/CutlassMath.h"
#include "Weapons/CutlassRules.h"
#include "Weapons/MobWeaponPickup.h"
#include "Items/ValuableComponent.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	using enum ETikiCombatClip;

	/** Every reaction clip assigned, the gore deaths included */
	uint32 AllClips()
	{
		uint32 Mask = 0;
		for (uint8 Index = 1; Index <= static_cast<uint8>(BisectedDeath); ++Index)
		{
			Mask |= 1u << Index;
		}
		return Mask;
	}

	/** Tuning of DA_TikiRules defaults */
	TikiCombat::FTuning DefaultTuning()
	{
		const UTikiRules* Rules = GetDefault<UTikiRules>();
		TikiCombat::FTuning Tuning;
		Tuning.BurstDamage = Rules->MusketBurstDamage;
		Tuning.BurstDistance = Rules->MusketBurstDistance;
		Tuning.BurstChance = Rules->MusketBurstChance;
		Tuning.LowAimPitch = Rules->SabreLowAimPitch;
		Tuning.bMirrorSides = Rules->bMirrorSabreSides;
		Tuning.BisectChance = Rules->SabreBisectChance;
		return Tuning;
	}

	/** A melee blow of the bean's cutlass class, horizontal, in the victim's torso */
	TikiCombat::FBlow Melee(bool bEdged, bool bLethal)
	{
		TikiCombat::FBlow Blow;
		Blow.Weapon = TikiCombat::MeleeWeapon(bEdged);
		Blow.bLethal = bLethal;
		Blow.Damage = 50.0f;
		Blow.Stroke = TikiCombat::EStroke::Horizontal;
		Blow.BladeSweep = FVector(0.0, 1.0, 0.0);
		Blow.VictimRight = FVector(0.0, 1.0, 0.0);
		Blow.AimPitch = -5.0f;
		Blow.bTorsoHit = true;
		Blow.Roll = 0.99f;
		return Blow;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.MobMelee
BEGIN_DEFINE_SPEC(FMobMeleeSpec, "monke_island.Weapons.MobMelee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FMobMeleeSpec)

void FMobMeleeSpec::Define()
{
	Describe("Edge", [this]()
	{
		It("keeps the cutlass as it was: an edge, shown in its scabbard, its mesh straight on the grip", [this]()
		{
			const UCutlassRules* Rules = GetDefault<UCutlassRules>();
			TestTrue(TEXT("bEdged"), Rules->bEdged);
			TestTrue(TEXT("bShowWhenPutAway"), Rules->bShowWhenPutAway);
			TestTrue(TEXT("MeshInGrip identity"), Rules->MeshInGrip.Equals(FTransform::Identity));
		});

		It("cuts limbs only with an edge and an attack that cuts", [this]()
		{
			TestTrue(TEXT("cutlass cut"), CutlassMath::CutsLimbs(true, true));
			TestFalse(TEXT("cutlass thrust"), CutlassMath::CutsLimbs(true, false));
			TestFalse(TEXT("spear swing with bCut left on"), CutlassMath::CutsLimbs(false, true));
			TestFalse(TEXT("club thrust"), CutlassMath::CutsLimbs(false, false));
		});

		It("counts every kill of a weapon without an edge as a kill, the cutlass only its cuts", [this]()
		{
			TestTrue(TEXT("cutlass cut kill"), CutlassMath::IsKillBlow(true, true, true));
			TestFalse(TEXT("cutlass thrust kill stays quiet"), CutlassMath::IsKillBlow(true, true, false));
			TestTrue(TEXT("club kill"), CutlassMath::IsKillBlow(true, false, false));
			TestFalse(TEXT("no kill"), CutlassMath::IsKillBlow(false, false, true));
		});
	});

	Describe("Creatures", [this]()
	{
		It("takes a melee blow without an edge as a blunt one", [this]()
		{
			TestTrue(TEXT("edged"), TikiCombat::MeleeWeapon(true) == TikiCombat::EWeapon::Sabre);
			TestTrue(TEXT("no edge"), TikiCombat::MeleeWeapon(false) == TikiCombat::EWeapon::Blunt);
			TestFalse(TEXT("not a gun"), TikiCombat::IsFirearm(TikiCombat::EWeapon::Blunt));
		});

		It("gives a lethal blunt blow a plain death where the sabre takes a cut", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			TestTrue(TEXT("sabre: a cut reaction"), TikiCombat::IsSabre(TikiCombat::ChooseClip(Melee(true, true), Tuning, AllClips())));
			TestTrue(TEXT("club: no reaction clip"), TikiCombat::ChooseClip(Melee(false, true), Tuning, AllClips()) == None);
			TikiCombat::FBlow Bisect = Melee(false, true);
			Bisect.Roll = 0.0f;
			TestTrue(TEXT("club never cuts in two"), TikiCombat::ChooseClip(Bisect, Tuning, AllClips()) == None);
			TestTrue(TEXT("club, not lethal: the hit clips"), TikiCombat::ChooseClip(Melee(false, false), Tuning, AllClips()) == None);
		});

		It("lets a crawling creature die crawling whatever the weapon", [this]()
		{
			TikiCombat::FBlow Crawl = Melee(false, true);
			Crawl.bCrawling = true;
			TestTrue(TEXT("crawl death"), TikiCombat::ChooseClip(Crawl, DefaultTuning(), AllClips()) == LeglessCrawlDeath);
		});

		It("never replaces a blunt kill by a shredding demo", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TikiShredDemo::FChoice Choice;
			Choice.bEnabled = Rules->bShredDemos;
			Choice.bSabreNeedsCut = Rules->bShredDemoSabreNeedsCut;
			Choice.Clip = TikiCombat::ChooseClip(Melee(false, true), DefaultTuning(), AllClips());
			Choice.DemoMask = AllClips();
			TestTrue(TEXT("no demo"), TikiShredDemo::Choose(Choice) == None);
		});
	});

	Describe("Meshes", [this]()
	{
		It("lays a mob weapon (long axis +Z, grip space) along the blade with a pitch of -90", [this]()
		{
			const FTransform Grip(FRotator(10.0f, 30.0f, -20.0f), FVector(12.0, -4.0, 30.0));
			const FTransform MeshInGrip(FRotator(-90.0f, 0.0f, 0.0f));
			const FTransform Mesh = CutlassMath::MeshOnGrip(Grip, MeshInGrip);
			TestTrue(TEXT("grip kept"), Mesh.GetLocation().Equals(Grip.GetLocation(), 0.01));
			TestTrue(TEXT("long axis on the blade"), Mesh.TransformVectorNoScale(FVector::UpVector).Equals(Grip.TransformVectorNoScale(FVector::ForwardVector), 0.001));
			TestTrue(TEXT("identity: the grip itself"), CutlassMath::MeshOnGrip(Grip, FTransform::Identity).Equals(Grip, 0.001));
		});

		It("hides a mob weapon put away, keeps the sheathed cutlass", [this]()
		{
			TestTrue(TEXT("drawn"), CutlassMath::ShowsThirdPersonMesh(false, true, false));
			TestTrue(TEXT("cutlass sheathed"), CutlassMath::ShowsThirdPersonMesh(false, false, true));
			TestFalse(TEXT("spear put away"), CutlassMath::ShowsThirdPersonMesh(false, false, false));
			TestFalse(TEXT("first person"), CutlassMath::ShowsThirdPersonMesh(true, true, true));
		});
	});

	Describe("Pickup", [this]()
	{
		It("is loot that loses value but never vanishes", [this]()
		{
			const AMobWeaponPickup* Pickup = GetDefault<AMobWeaponPickup>();
			const UValuableComponent* Valuable = Pickup ? Pickup->GetValuable() : nullptr;
			if (!TestNotNull(TEXT("valuable"), Valuable))
			{
				return;
			}
			TestFalse(TEXT("bBreakAtZero"), Valuable->bBreakAtZero);
		});

		It("leaves the dropped weapon out of the shredding demo, a villager's basket in", [this]()
		{
			const FName Weapon = GetDefault<UTikiShredDemoSet>()->WeaponFragmentBone;
			TestEqual(TEXT("pack bone"), Weapon, FName(TEXT("frag_Weapon")));
			TestEqual(TEXT("weapon dropped"), TikiShredDemo::HiddenFragment(Weapon, true), Weapon);
			TestTrue(TEXT("no weapon to drop"), TikiShredDemo::HiddenFragment(Weapon, false).IsNone());
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
