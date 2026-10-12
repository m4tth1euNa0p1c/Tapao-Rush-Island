// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiCombat.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	using ETikiClip = ETikiCombatClip;

	uint32 MaskOf(std::initializer_list<ETikiCombatClip> Clips)
	{
		uint32 Mask = 0;
		for (const ETikiCombatClip Clip : Clips)
		{
			Mask |= 1u << static_cast<uint8>(Clip);
		}
		return Mask;
	}

	/** Every clip assigned */
	const uint32 AllClips = MaskOf({ ETikiClip::MusketImpact, ETikiClip::MusketBurst, ETikiClip::MusketStaggerRecover,
		ETikiClip::MusketArticulatedFall, ETikiClip::SabreLeftToRight, ETikiClip::SabreRightToLeft, ETikiClip::SabreDown, ETikiClip::SabreLow });

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

	TikiCombat::FBlow Shot(bool bLethal, float Distance, float Roll)
	{
		TikiCombat::FBlow Blow;
		Blow.Weapon = TikiCombat::EWeapon::Musket;
		Blow.bLethal = bLethal;
		Blow.Damage = 10000.0f;
		Blow.Distance = Distance;
		Blow.Roll = Roll;
		return Blow;
	}

	/** A sabre blow of a bean facing the victim (+X), the victim facing it (-X): the victim's right is +Y */
	TikiCombat::FBlow Cut(TikiCombat::EStroke Stroke, const FVector& Sweep, float AimPitch = -5.0f)
	{
		TikiCombat::FBlow Blow;
		Blow.Weapon = TikiCombat::EWeapon::Sabre;
		Blow.bLethal = true;
		Blow.Stroke = Stroke;
		Blow.BladeSweep = Sweep;
		Blow.VictimRight = FVector(0, 1, 0);
		Blow.AimPitch = AimPitch;
		return Blow;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.CombatReactions
BEGIN_DEFINE_SPEC(FTikiCombatReactionsSpec, "monke_island.Tiki.CombatReactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiCombatReactionsSpec)

void FTikiCombatReactionsSpec::Define()
{
	Describe("Musket", [this]()
	{
		It("staggers then recovers from a shot it survives, an impact when already reacting", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			TestTrue(TEXT("stagger"), TikiCombat::ChooseClip(Shot(false, 1500.0f, 0.9f), Tuning, AllClips) == ETikiClip::MusketStaggerRecover);
			TikiCombat::FBlow Again = Shot(false, 1500.0f, 0.9f);
			Again.bInReaction = true;
			TestTrue(TEXT("impact"), TikiCombat::ChooseClip(Again, Tuning, AllClips) == ETikiClip::MusketImpact);
			TestFalse(TEXT("not lethal"), TikiCombat::IsLethal(ETikiClip::MusketStaggerRecover) || TikiCombat::IsLethal(ETikiClip::MusketImpact));
		});

		It("falls from a lethal shot, bursts at point blank or on the weighted draw", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			TestTrue(TEXT("fall"), TikiCombat::ChooseClip(Shot(true, 1500.0f, 0.9f), Tuning, AllClips) == ETikiClip::MusketArticulatedFall);
			TestTrue(TEXT("point blank"), TikiCombat::ChooseClip(Shot(true, Tuning.BurstDistance * 0.5f, 0.9f), Tuning, AllClips) == ETikiClip::MusketBurst);
			TestTrue(TEXT("drawn"), TikiCombat::ChooseClip(Shot(true, 1500.0f, Tuning.BurstChance * 0.5f), Tuning, AllClips) == ETikiClip::MusketBurst);
			TikiCombat::FTuning ByDamage = Tuning;
			ByDamage.BurstDamage = 5000.0f;
			TestTrue(TEXT("strong shot"), TikiCombat::ChooseClip(Shot(true, 1500.0f, 0.9f), ByDamage, AllClips) == ETikiClip::MusketBurst);
			TestTrue(TEXT("lethal"), TikiCombat::IsLethal(ETikiClip::MusketArticulatedFall) && TikiCombat::IsLethal(ETikiClip::MusketBurst));
		});

		It("dies of the headshot clip when a lethal shot strikes the head, unless it bursts (owner 2026-10-08)", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			const uint32 WithHeadshot = AllClips | (1u << static_cast<uint8>(ETikiClip::HeadshotDeath));
			TikiCombat::FBlow Head = Shot(true, 1500.0f, 0.9f);
			Head.bHeadHit = true;
			TestTrue(TEXT("headshot"), TikiCombat::ChooseClip(Head, Tuning, WithHeadshot) == ETikiClip::HeadshotDeath);
			TestTrue(TEXT("lethal"), TikiCombat::IsLethal(ETikiClip::HeadshotDeath));
			TestTrue(TEXT("without its clip"), TikiCombat::ChooseClip(Head, Tuning, AllClips) == ETikiClip::MusketArticulatedFall);
			TikiCombat::FBlow Close = Shot(true, Tuning.BurstDistance * 0.5f, 0.9f);
			Close.bHeadHit = true;
			TestTrue(TEXT("point blank still bursts"), TikiCombat::ChooseClip(Close, Tuning, WithHeadshot) == ETikiClip::MusketBurst);
			TikiCombat::FBlow Survived = Head;
			Survived.bLethal = false;
			TestTrue(TEXT("survived"), TikiCombat::ChooseClip(Survived, Tuning, WithHeadshot) == ETikiClip::MusketStaggerRecover);
		});

		It("dies of the decapitated clip when the lethal blow tore the head off, by a shot or a blade (owner 2026-10-08)", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			const uint32 WithBoth = AllClips | (1u << static_cast<uint8>(ETikiClip::HeadshotDeath)) | (1u << static_cast<uint8>(ETikiClip::DecapitatedDeath));
			TikiCombat::FBlow Torn = Shot(true, 1500.0f, 0.9f);
			Torn.bHeadHit = true;
			Torn.bHeadTorn = true;
			TestTrue(TEXT("shot"), TikiCombat::ChooseClip(Torn, Tuning, WithBoth) == ETikiClip::DecapitatedDeath);
			TestTrue(TEXT("lethal"), TikiCombat::IsLethal(ETikiClip::DecapitatedDeath));
			TikiCombat::FBlow Close = Shot(true, Tuning.BurstDistance * 0.5f, 0.9f);
			Close.bHeadTorn = true;
			TestTrue(TEXT("even point blank"), TikiCombat::ChooseClip(Close, Tuning, WithBoth) == ETikiClip::DecapitatedDeath);
			TikiCombat::FBlow Blade;
			Blade.Weapon = TikiCombat::EWeapon::Sabre;
			Blade.bLethal = true;
			Blade.bHeadTorn = true;
			TestTrue(TEXT("blade"), TikiCombat::ChooseClip(Blade, Tuning, WithBoth) == ETikiClip::DecapitatedDeath);
			TestTrue(TEXT("without its clip, the headshot"), TikiCombat::ChooseClip(Torn, Tuning, AllClips | (1u << static_cast<uint8>(ETikiClip::HeadshotDeath))) == ETikiClip::HeadshotDeath);
			TikiCombat::FBlow Survived = Torn;
			Survived.bLethal = false;
			TestTrue(TEXT("survived"), TikiCombat::ChooseClip(Survived, Tuning, WithBoth) == ETikiClip::MusketStaggerRecover);
		});
	});

	Describe("Sabre", [this]()
	{
		It("picks the side clip from the sweep of the blade across the victim", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			// sweeping toward the victim's right: the blade came from its left
			TestTrue(TEXT("left to right"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0)), Tuning, AllClips) == ETikiClip::SabreLeftToRight);
			TestTrue(TEXT("right to left"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Horizontal, FVector(0, -1, 0)), Tuning, AllClips) == ETikiClip::SabreRightToLeft);
			TikiCombat::FTuning Mirrored = Tuning;
			Mirrored.bMirrorSides = true;
			TestTrue(TEXT("mirrored"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0)), Mirrored, AllClips) == ETikiClip::SabreRightToLeft);
		});

		It("cuts down when the sense cannot be trusted, low when the blow aims low", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			TestTrue(TEXT("finisher"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Vertical, FVector::ZeroVector), Tuning, AllClips) == ETikiClip::SabreDown);
			TestTrue(TEXT("thrust"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::None, FVector::ZeroVector), Tuning, AllClips) == ETikiClip::SabreDown);
			TestTrue(TEXT("sweep along the front"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Horizontal, FVector(1, 0, 0)), Tuning, AllClips) == ETikiClip::SabreDown);
			TestTrue(TEXT("low"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0), -Tuning.LowAimPitch - 5.0f), Tuning, AllClips) == ETikiClip::SabreLow);
		});

		It("keeps the hit clip for a blow it survives", [this]()
		{
			TikiCombat::FBlow Blow = Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0));
			Blow.bLethal = false;
			TestTrue(TEXT("none"), TikiCombat::ChooseClip(Blow, DefaultTuning(), AllClips) == ETikiClip::None);
		});
	});

	Describe("Wounds (owner 2026-10-09)", [this]()
	{
		It("dies crawling of any lethal blow once a leg is torn off, but a burst", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			const uint32 WithCrawl = AllClips | MaskOf({ ETikiClip::HeadshotDeath, ETikiClip::DecapitatedDeath, ETikiClip::LeglessCrawlDeath });
			TikiCombat::FBlow Crawling = Shot(true, 1500.0f, 0.9f);
			Crawling.bCrawling = true;
			TestTrue(TEXT("shot"), TikiCombat::ChooseClip(Crawling, Tuning, WithCrawl) == ETikiClip::LeglessCrawlDeath);
			TestTrue(TEXT("lethal"), TikiCombat::IsLethal(ETikiClip::LeglessCrawlDeath));
			TikiCombat::FBlow Head = Crawling;
			Head.bHeadHit = true;
			TestTrue(TEXT("shot in the head"), TikiCombat::ChooseClip(Head, Tuning, WithCrawl) == ETikiClip::LeglessCrawlDeath);
			TikiCombat::FBlow Torn = Head;
			Torn.bHeadTorn = true;
			TestTrue(TEXT("head torn off"), TikiCombat::ChooseClip(Torn, Tuning, WithCrawl) == ETikiClip::LeglessCrawlDeath);
			TikiCombat::FBlow Close = Shot(true, Tuning.BurstDistance * 0.5f, 0.9f);
			Close.bCrawling = true;
			TestTrue(TEXT("point blank still bursts"), TikiCombat::ChooseClip(Close, Tuning, WithCrawl) == ETikiClip::MusketBurst);
			TikiCombat::FBlow Blade = Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0));
			Blade.bCrawling = true;
			TestTrue(TEXT("blade"), TikiCombat::ChooseClip(Blade, Tuning, WithCrawl) == ETikiClip::LeglessCrawlDeath);
			TestTrue(TEXT("without its clip, the fall"), TikiCombat::ChooseClip(Crawling, Tuning, AllClips) == ETikiClip::MusketArticulatedFall);
			TestTrue(TEXT("without its clip, the head torn off"), TikiCombat::ChooseClip(Torn, Tuning, AllClips | MaskOf({ ETikiClip::DecapitatedDeath })) == ETikiClip::DecapitatedDeath);
			TikiCombat::FBlow Survived = Crawling;
			Survived.bLethal = false;
			TestTrue(TEXT("survived"), TikiCombat::ChooseClip(Survived, Tuning, WithCrawl) == ETikiClip::MusketStaggerRecover);
			TikiCombat::FBlow Standing = Shot(true, 1500.0f, 0.9f);
			TestTrue(TEXT("standing, the fall"), TikiCombat::ChooseClip(Standing, Tuning, WithCrawl) == ETikiClip::MusketArticulatedFall);
		});

		It("falls in two when a lethal blade at the waist is drawn", [this]()
		{
			TikiCombat::FTuning Tuning = DefaultTuning();
			TestEqual(TEXT("one time in two by default"), Tuning.BisectChance, 0.5f);
			const uint32 WithBisect = AllClips | MaskOf({ ETikiClip::BisectedDeath });
			TikiCombat::FBlow Waist = Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0));
			Waist.bTorsoHit = true;
			Waist.Roll = 0.2f;
			TestTrue(TEXT("drawn"), TikiCombat::ChooseClip(Waist, Tuning, WithBisect) == ETikiClip::BisectedDeath);
			TestTrue(TEXT("lethal"), TikiCombat::IsLethal(ETikiClip::BisectedDeath));
			TestFalse(TEXT("not a sabre cut of a side"), TikiCombat::IsSabre(ETikiClip::BisectedDeath));
			TikiCombat::FBlow NotDrawn = Waist;
			NotDrawn.Roll = 0.7f;
			TestTrue(TEXT("not drawn: the side cut"), TikiCombat::ChooseClip(NotDrawn, Tuning, WithBisect) == ETikiClip::SabreLeftToRight);
			TikiCombat::FBlow Arm = Waist;
			Arm.bTorsoHit = false;
			TestTrue(TEXT("not the torso"), TikiCombat::ChooseClip(Arm, Tuning, WithBisect) == ETikiClip::SabreLeftToRight);
			TestTrue(TEXT("without its clip"), TikiCombat::ChooseClip(Waist, Tuning, AllClips) == ETikiClip::SabreLeftToRight);
			TikiCombat::FBlow Ball = Shot(true, 1500.0f, 0.2f);
			Ball.bTorsoHit = true;
			TestTrue(TEXT("a ball never cuts"), TikiCombat::ChooseClip(Ball, Tuning, WithBisect) == ETikiClip::MusketBurst);
			Tuning.BisectChance = 0.0f;
			TestTrue(TEXT("chance off"), TikiCombat::ChooseClip(Waist, Tuning, WithBisect) == ETikiClip::SabreLeftToRight);
			TikiCombat::FBlow Survived = Waist;
			Survived.bLethal = false;
			TestTrue(TEXT("survived"), TikiCombat::ChooseClip(Survived, DefaultTuning(), WithBisect) == ETikiClip::None);
		});
	});

	Describe("Fallback", [this]()
	{
		It("falls back on the other clip, then on the behaviour without clips", [this]()
		{
			const TikiCombat::FTuning Tuning = DefaultTuning();
			TestTrue(TEXT("no fall: burst"), TikiCombat::ChooseClip(Shot(true, 1500.0f, 0.9f), Tuning, MaskOf({ ETikiClip::MusketBurst })) == ETikiClip::MusketBurst);
			TestTrue(TEXT("no stagger: impact"), TikiCombat::ChooseClip(Shot(false, 1500.0f, 0.9f), Tuning, MaskOf({ ETikiClip::MusketImpact })) == ETikiClip::MusketImpact);
			TestTrue(TEXT("no side: down"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Horizontal, FVector(0, 1, 0)), Tuning, MaskOf({ ETikiClip::SabreDown })) == ETikiClip::SabreDown);
			TestTrue(TEXT("no clip at all"), TikiCombat::ChooseClip(Shot(true, 100.0f, 0.0f), Tuning, 0) == ETikiClip::None);
			TestTrue(TEXT("sabre without down"), TikiCombat::ChooseClip(Cut(TikiCombat::EStroke::Vertical, FVector::ZeroVector), Tuning, MaskOf({ ETikiClip::SabreLow })) == ETikiClip::None);
			TikiCombat::FBlow Torch;
			Torch.bLethal = true;
			TestTrue(TEXT("other weapon"), TikiCombat::ChooseClip(Torch, Tuning, AllClips) == ETikiClip::None);
		});

		It("finds the assigned clips of a creature", [this]()
		{
			FTikiCombatReactions Reactions;
			TestEqual(TEXT("empty"), Reactions.AvailableMask(), 0u);
			TestNull(TEXT("none"), Reactions.Get(ETikiClip::SabreDown));
		});
	});
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.CorpseSettle
BEGIN_DEFINE_SPEC(FTikiCorpseSettleSpec, "monke_island.Tiki.CorpseSettle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiCorpseSettleSpec)

void FTikiCorpseSettleSpec::Define()
{
	It("bounds the speed of a dead body, direction kept", [this]()
	{
		const FVector Fast(4000, 0, 3000);
		const FVector Bounded = TikiMath::BoundedVelocity(Fast, 300.0f);
		TestTrue(TEXT("bounded"), FMath::IsNearlyEqual(Bounded.Size(), 300.0, 0.01));
		TestTrue(TEXT("same direction"), Bounded.GetSafeNormal().Equals(Fast.GetSafeNormal(), 1.0e-4));
		TestEqual(TEXT("slow kept"), TikiMath::BoundedVelocity(FVector(50, 0, 0), 300.0f), FVector(50, 0, 0));
		TestEqual(TEXT("no speed"), TikiMath::BoundedVelocity(Fast, 0.0f), FVector::ZeroVector);
	});

	It("lays a corpse on the ground: lowered when floating, lifted when sunk, left within the tolerance", [this]()
	{
		TestEqual(TEXT("floating"), TikiMath::GroundSnapOffset(140.0f, 100.0f, 4.0f), -40.0f);
		TestEqual(TEXT("sunk"), TikiMath::GroundSnapOffset(80.0f, 100.0f, 4.0f), 20.0f);
		TestEqual(TEXT("resting"), TikiMath::GroundSnapOffset(102.0f, 100.0f, 4.0f), 0.0f);
	});
}

#endif
