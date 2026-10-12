// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/HitZoneMath.h"
#include "Creatures/TikiLimbs.h"
#include "Fort/FortSoldierGoreMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Fort.SoldierGore
BEGIN_DEFINE_SPEC(FFortSoldierGoreSpec, "monke_island.Fort.SoldierGore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	/** The death as a number, for TestEqual */
	static uint8 Kind(EFortSoldierDeath Death) { return static_cast<uint8>(Death); }
	/** The stance as a number */
	static uint8 Stance(uint8 Lost) { return static_cast<uint8>(FortSoldierGore::StanceOf(Lost)); }
	static uint8 Stance(FortSoldierGore::EStance Wanted) { return static_cast<uint8>(Wanted); }
	/** A blade in the torso of a standing soldier, the bisection drawn */
	static FortSoldierGore::FBlowFacts BladeAtWaist()
	{
		FortSoldierGore::FBlowFacts Blow;
		Blow.bBlade = true;
		Blow.bTorso = true;
		Blow.bBisectDrawn = true;
		return Blow;
	}
END_DEFINE_SPEC(FFortSoldierGoreSpec)

void FFortSoldierGoreSpec::Define()
{
	It("bursts the body of a shot from point blank, whatever it struck (owner 2026-10-08)", [this]()
	{
		const FortSoldierGore::FDeathClips All{ true, true, true };
		const FortSoldierGore::FDeathClips NoExplode{ true, true, false };
		TestEqual(TEXT("torso"), Kind(FortSoldierGore::DeathOf(false, true, 90.0f, 120.0f, false, All)), Kind(EFortSoldierDeath::Exploded));
		TestEqual(TEXT("head too"), Kind(FortSoldierGore::DeathOf(true, true, 120.0f, 120.0f, true, All)), Kind(EFortSoldierDeath::Exploded));
		TestEqual(TEXT("farther"), Kind(FortSoldierGore::DeathOf(false, true, 121.0f, 120.0f, false, All)), Kind(EFortSoldierDeath::Plain));
		TestEqual(TEXT("unknown distance"), Kind(FortSoldierGore::DeathOf(false, true, 0.0f, 120.0f, false, All)), Kind(EFortSoldierDeath::Plain));
		TestEqual(TEXT("a blade never bursts"), Kind(FortSoldierGore::DeathOf(false, false, 50.0f, 120.0f, false, All)), Kind(EFortSoldierDeath::Plain));
		TestEqual(TEXT("without its clip"), Kind(FortSoldierGore::DeathOf(false, true, 90.0f, 120.0f, false, NoExplode)), Kind(EFortSoldierDeath::Plain));
		TestEqual(TEXT("range off"), Kind(FortSoldierGore::DeathOf(false, true, 90.0f, 0.0f, false, All)), Kind(EFortSoldierDeath::Plain));
	});

	It("takes the head off with a blade, or with a shot when the decapitation is drawn", [this]()
	{
		const FortSoldierGore::FDeathClips All{ true, true, true };
		const FortSoldierGore::FDeathClips NoDecapitation{ true, false, true };
		const FortSoldierGore::FDeathClips None;
		TestEqual(TEXT("blade"), Kind(FortSoldierGore::DeathOf(true, false, 60.0f, 120.0f, false, All)), Kind(EFortSoldierDeath::Decapitated));
		TestEqual(TEXT("shot drawn"), Kind(FortSoldierGore::DeathOf(true, true, 600.0f, 120.0f, true, All)), Kind(EFortSoldierDeath::Decapitated));
		TestEqual(TEXT("shot not drawn"), Kind(FortSoldierGore::DeathOf(true, true, 600.0f, 120.0f, false, All)), Kind(EFortSoldierDeath::Headshot));
		TestEqual(TEXT("blade, no decapitation clip"), Kind(FortSoldierGore::DeathOf(true, false, 60.0f, 120.0f, true, NoDecapitation)), Kind(EFortSoldierDeath::Plain));
		TestEqual(TEXT("shot, no decapitation clip"), Kind(FortSoldierGore::DeathOf(true, true, 600.0f, 120.0f, true, NoDecapitation)), Kind(EFortSoldierDeath::Headshot));
		TestEqual(TEXT("no clips at all"), Kind(FortSoldierGore::DeathOf(true, true, 600.0f, 120.0f, false, None)), Kind(EFortSoldierDeath::Plain));
		TestEqual(TEXT("body"), Kind(FortSoldierGore::DeathOf(false, false, 60.0f, 120.0f, true, All)), Kind(EFortSoldierDeath::Plain));
	});

	Describe("Limbs (owner 2026-10-09)", [this]()
	{
		It("wears an arm or a leg down with the blows on its bones and tears it past its health, once", [this]()
		{
			using namespace FortSoldierGore;
			TestEqual(TEXT("upperarm_l is the left arm"), LimbOfZone(HitZones::ZoneOfBone(TEXT("upperarm_l"))), static_cast<int32>(ArmL));
			TestEqual(TEXT("hand_r is the right arm"), LimbOfZone(HitZones::ZoneOfBone(TEXT("hand_r"))), static_cast<int32>(ArmR));
			TestEqual(TEXT("calf_l is the left leg"), LimbOfZone(HitZones::ZoneOfBone(TEXT("calf_l"))), static_cast<int32>(LegL));
			TestEqual(TEXT("foot_r is the right leg"), LimbOfZone(HitZones::ZoneOfBone(TEXT("foot_r"))), static_cast<int32>(LegR));
			TestEqual(TEXT("the head is no limb"), LimbOfZone(HitZones::ZoneOfBone(TEXT("head"))), static_cast<int32>(INDEX_NONE));
			TestEqual(TEXT("the torso is no limb"), LimbOfZone(HitZones::ZoneOfBone(TEXT("spine_02"))), static_cast<int32>(INDEX_NONE));
			TestEqual(TEXT("no bone"), LimbOfZone(HitZones::None), static_cast<int32>(INDEX_NONE));
			TestEqual(TEXT("arm health"), LimbHealth(ArmR, 25.0f, 30.0f), 25.0f);
			TestEqual(TEXT("leg health"), LimbHealth(LegL, 25.0f, 30.0f), 30.0f);
			TestEqual(TEXT("bits"), static_cast<int32>(BitOfLimb(ArmL) | BitOfLimb(ArmR) | BitOfLimb(LegL) | BitOfLimb(LegR)), static_cast<int32>(ArmBits | LegBits));
			TestEqual(TEXT("no bit for no limb"), static_cast<int32>(BitOfLimb(INDEX_NONE)), 0);
			// a musket ball of 35 tears an arm at once; two sabre cuts of 15 wear a leg (30) down
			TArray<float> Damage;
			TestTrue(TEXT("a ball tears the arm"), TikiLimbs::AddDamage(Damage, ArmL, 35.0f, LimbHealth(ArmL, 25.0f, 30.0f)));
			TestFalse(TEXT("a first cut on the leg"), TikiLimbs::AddDamage(Damage, LegR, 15.0f, LimbHealth(LegR, 25.0f, 30.0f)));
			TestTrue(TEXT("the second tears it"), TikiLimbs::AddDamage(Damage, LegR, 15.0f, LimbHealth(LegR, 25.0f, 30.0f)));
			TestFalse(TEXT("never twice"), TikiLimbs::AddDamage(Damage, LegR, 15.0f, LimbHealth(LegR, 25.0f, 30.0f)));
		});

		It("chooses the stance of what is left: one arm fights on, no right arm panics, no leg crawls", [this]()
		{
			using namespace FortSoldierGore;
			TestEqual(TEXT("whole"), Stance(0), Stance(EStance::Standing));
			TestEqual(TEXT("left arm gone: the sabre in the right hand"), Stance(ArmLBit), Stance(EStance::OneArm));
			TestEqual(TEXT("right arm gone: nothing to hold"), Stance(ArmRBit), Stance(EStance::Panic));
			TestEqual(TEXT("both arms gone"), Stance(ArmLBit | ArmRBit), Stance(EStance::Panic));
			TestEqual(TEXT("a leg gone"), Stance(LegLBit), Stance(EStance::Crawl));
			TestEqual(TEXT("both legs gone"), Stance(LegLBit | LegRBit), Stance(EStance::Crawl));
			TestEqual(TEXT("a leg and the arms gone: it still crawls"), Stance(LegRBit | ArmLBit | ArmRBit), Stance(EStance::Crawl));
		});

		It("lays a living soldier without a leg on the ground, and keeps a body in the stance it died in (T-332)", [this]()
		{
			using namespace FortSoldierGore;
			TestFalse(TEXT("whole, alive"), IsCrawlStance(0, false, false));
			TestTrue(TEXT("a leg gone, alive: crawls"), IsCrawlStance(LegRBit, false, false));
			TestFalse(TEXT("an arm gone, alive: stands"), IsCrawlStance(ArmRBit, false, false));
			TestTrue(TEXT("died crawling: lies crawling"), IsCrawlStance(LegLBit | LegRBit, true, true));
			// the machine gun kills a standing soldier and tears its leg off with the same ball: the body falls standing
			TestFalse(TEXT("killed standing, the leg torn by the same blow"), IsCrawlStance(LegRBit, true, false));
			TestFalse(TEXT("killed standing, whole"), IsCrawlStance(0, true, false));
		});

		It("keeps the feet at the bottom of the capsule whatever its height (the mesh offset cached for the network smoothing)", [this]()
		{
			using namespace FortSoldierGore;
			// the bean: a capsule of 55, the mesh at -55; crawling, a capsule of 30: the mesh 25 cm higher in it
			TestEqual(TEXT("standing"), StanceMeshZ(-55.0, 55.0f, 55.0f, 0.0f), -55.0);
			TestEqual(TEXT("crawling"), StanceMeshZ(-55.0, 55.0f, 30.0f, 0.0f), -30.0);
			TestEqual(TEXT("crawling, lifted by the rules"), StanceMeshZ(-55.0, 55.0f, 30.0f, 4.0f), -26.0);
			// the feet: the capsule centre plus the mesh height, at the bottom of the capsule (centre - half height)
			const double CrawlCentre = 30.0;
			TestEqual(TEXT("crawling feet on the floor"), CrawlCentre + StanceMeshZ(-55.0, 55.0f, 30.0f, 0.0f), 0.0);
		});

		It("drops the musket with the first arm, the sabre with the right arm when it was in hand", [this]()
		{
			using namespace FortSoldierGore;
			TestFalse(TEXT("whole"), MusketGone(0));
			TestTrue(TEXT("left arm"), MusketGone(ArmLBit));
			TestTrue(TEXT("right arm"), MusketGone(ArmRBit));
			TestFalse(TEXT("a leg keeps the musket"), MusketGone(LegLBit | LegRBit));
			TestTrue(TEXT("sabre in the right hand"), SabreGone(ArmRBit, true));
			TestFalse(TEXT("sabre sheathed stays on the hip"), SabreGone(ArmRBit, false));
			TestFalse(TEXT("left arm keeps the sabre"), SabreGone(ArmLBit, true));
		});

		It("dies crawling of the crawl death, cut in two by a blade at the waist when drawn, bursts at point blank still", [this]()
		{
			using namespace FortSoldierGore;
			FDeathClips All{ true, true, true };
			All.bBisected = true;
			All.bCrawlDeath = true;
			const FDeathClips Standing{ true, true, true };
			// a blade at the waist
			TestEqual(TEXT("bisected"), Kind(DeathOfBlow(BladeAtWaist(), 120.0f, All)), Kind(EFortSoldierDeath::Bisected));
			FBlowFacts NotDrawn = BladeAtWaist();
			NotDrawn.bBisectDrawn = false;
			TestEqual(TEXT("not drawn"), Kind(DeathOfBlow(NotDrawn, 120.0f, All)), Kind(EFortSoldierDeath::Plain));
			FBlowFacts Arm = BladeAtWaist();
			Arm.bTorso = false;
			TestEqual(TEXT("not the torso"), Kind(DeathOfBlow(Arm, 120.0f, All)), Kind(EFortSoldierDeath::Plain));
			FBlowFacts Shot = BladeAtWaist();
			Shot.bBlade = false;
			Shot.bShot = true;
			Shot.ShotDistance = 600.0f;
			TestEqual(TEXT("a ball in the torso never cuts"), Kind(DeathOfBlow(Shot, 120.0f, All)), Kind(EFortSoldierDeath::Plain));
			TestEqual(TEXT("without its clip"), Kind(DeathOfBlow(BladeAtWaist(), 120.0f, Standing)), Kind(EFortSoldierDeath::Plain));
			FBlowFacts Head = BladeAtWaist();
			Head.bTorso = false;
			Head.bHead = true;
			TestEqual(TEXT("a blade in the head still takes it off"), Kind(DeathOfBlow(Head, 120.0f, All)), Kind(EFortSoldierDeath::Decapitated));
			// crawling: its own death, whatever the blow, but the burst
			FBlowFacts Crawling = BladeAtWaist();
			Crawling.bCrawling = true;
			TestEqual(TEXT("crawling, cut"), Kind(DeathOfBlow(Crawling, 120.0f, All)), Kind(EFortSoldierDeath::CrawlDeath));
			FBlowFacts CrawlingHead;
			CrawlingHead.bCrawling = true;
			CrawlingHead.bHead = true;
			CrawlingHead.bShot = true;
			CrawlingHead.ShotDistance = 600.0f;
			TestEqual(TEXT("crawling, shot in the head"), Kind(DeathOfBlow(CrawlingHead, 120.0f, All)), Kind(EFortSoldierDeath::CrawlDeath));
			TestEqual(TEXT("crawling without its clip"), Kind(DeathOfBlow(CrawlingHead, 120.0f, Standing)), Kind(EFortSoldierDeath::Headshot));
			FBlowFacts CrawlingClose = CrawlingHead;
			CrawlingClose.ShotDistance = 90.0f;
			TestEqual(TEXT("crawling, point blank: bursts"), Kind(DeathOfBlow(CrawlingClose, 120.0f, All)), Kind(EFortSoldierDeath::Exploded));
		});
	});
}

#endif
