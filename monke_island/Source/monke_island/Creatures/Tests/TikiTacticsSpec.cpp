// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiTacticsMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.Tactics
BEGIN_DEFINE_SPEC(FTikiTacticsSpec, "monke_island.Tiki.Tactics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiTacticsSpec)

void FTikiTacticsSpec::Define()
{
	Describe("Archetype", [this]()
	{
		It("gives each mob of the island its role from its class name", [this]()
		{
			const TArray<FTikiArchetypeMatch>& Matches = GetDefault<UTikiRules>()->Pack.Archetypes;
			TestTrue(TEXT("Guerrier Coco: stalker"), TikiTactics::ResolveArchetype(TEXT("BP_TikiCocoWarrior_C"), Matches) == ETikiArchetype::Stalker);
			TestTrue(TEXT("Gardien Crabe: bulwark"), TikiTactics::ResolveArchetype(TEXT("BP_TikiCrabGuardian_C"), Matches) == ETikiArchetype::Bulwark);
			TestTrue(TEXT("Veilleur des Cendres: watcher"), TikiTactics::ResolveArchetype(TEXT("BP_TikiAshWatcher_C"), Matches) == ETikiArchetype::Watcher);
			TestTrue(TEXT("any case"), TikiTactics::ResolveArchetype(TEXT("bp_tikicocowarrior_c"), Matches) == ETikiArchetype::Stalker);
		});

		It("keeps the classic melee for a class it does not know", [this]()
		{
			const TArray<FTikiArchetypeMatch>& Matches = GetDefault<UTikiRules>()->Pack.Archetypes;
			TestTrue(TEXT("unknown class"), TikiTactics::ResolveArchetype(TEXT("BP_TikiBrambleStalker_C"), Matches) == ETikiArchetype::Brawler);
			TestTrue(TEXT("no keyword"), TikiTactics::ResolveArchetype(TEXT("BP_TikiCocoWarrior_C"), {}) == ETikiArchetype::Brawler);
			const TArray<FTikiArchetypeMatch> Odd = { { TEXT(""), ETikiArchetype::Watcher }, { TEXT("Coco"), ETikiArchetype::Auto } };
			TestTrue(TEXT("empty keyword and Auto ignored"), TikiTactics::ResolveArchetype(TEXT("BP_TikiCocoWarrior_C"), Odd) == ETikiArchetype::Brawler);
		});
	});

	Describe("Pack", [this]()
	{
		It("never lets more than the allowed attackers commit to one foe", [this]()
		{
			TestTrue(TEXT("nobody attacks"), TikiTactics::HasAttackToken(0, 2));
			TestTrue(TEXT("one attacks"), TikiTactics::HasAttackToken(1, 2));
			TestFalse(TEXT("two attack"), TikiTactics::HasAttackToken(2, 2));
			TestTrue(TEXT("a cap of 0 still lets one"), TikiTactics::HasAttackToken(0, 0));
			TestFalse(TEXT("a cap of 0 lets only one"), TikiTactics::HasAttackToken(1, 0));
		});

		It("gives each hunter a stable rank among those of its role", [this]()
		{
			const TArray<uint32> Others = { 20u, 30u };
			const TArray<uint32> Nobody;
			TestEqual(TEXT("first"), TikiTactics::PackRank(10u, Others), 0);
			TestEqual(TEXT("middle"), TikiTactics::PackRank(25u, Others), 1);
			TestEqual(TEXT("last"), TikiTactics::PackRank(40u, Others), 2);
			TestEqual(TEXT("alone"), TikiTactics::PackRank(40u, Nobody), 0);
		});

		It("puts the first place behind the foe and spreads the others evenly", [this]()
		{
			TestTrue(TEXT("first behind a foe looking along +X"), FMath::IsNearlyEqual(FMath::Abs(TikiTactics::EncircleSlotYaw(0, 3, 0.0f)), 180.0f, 0.01f));
			TestTrue(TEXT("second a third of a turn further"), FMath::IsNearlyEqual(TikiTactics::EncircleSlotYaw(1, 3, 0.0f), -60.0f, 0.01f));
			TestTrue(TEXT("third"), FMath::IsNearlyEqual(TikiTactics::EncircleSlotYaw(2, 3, 0.0f), 60.0f, 0.01f));
			TestTrue(TEXT("follows the view"), FMath::IsNearlyEqual(TikiTactics::EncircleSlotYaw(0, 1, 90.0f), -90.0f, 0.01f));
			const FVector Slot = TikiTactics::RingPoint(FVector(100.0f, 0.0f, 50.0f), 180.0f, 550.0f);
			TestTrue(TEXT("on the ring behind"), Slot.Equals(FVector(-450.0f, 0.0f, 50.0f), 0.5f));
		});
	});

	Describe("Stalker", [this]()
	{
		It("knows when its foe looks at it", [this]()
		{
			const FVector Eye(0.0f, 0.0f, 0.0f);
			TestTrue(TEXT("in front"), TikiTactics::IsInView(Eye, FVector::ForwardVector, FVector(800.0f, 100.0f, 0.0f), 35.0f));
			TestFalse(TEXT("at its side"), TikiTactics::IsInView(Eye, FVector::ForwardVector, FVector(0.0f, 800.0f, 0.0f), 35.0f));
			TestFalse(TEXT("behind"), TikiTactics::IsInView(Eye, FVector::ForwardVector, FVector(-800.0f, 0.0f, 0.0f), 35.0f));
			TestTrue(TEXT("height ignored"), TikiTactics::IsInView(Eye, FVector(1.0f, 0.0f, -0.9f), FVector(800.0f, 0.0f, 500.0f), 35.0f));
		});

		It("circles far while looked at, near when not, never inside the fear of a torch", [this]()
		{
			TestEqual(TEXT("looked at"), TikiTactics::StalkRadius(true, 700.0f, 1100.0f, 0.0f), 1100.0f);
			TestEqual(TEXT("not looked at"), TikiTactics::StalkRadius(false, 700.0f, 1100.0f, 0.0f), 700.0f);
			TestEqual(TEXT("bounds swapped"), TikiTactics::StalkRadius(false, 1100.0f, 700.0f, 0.0f), 700.0f);
			TestEqual(TEXT("torch"), TikiTactics::StalkRadius(false, 300.0f, 1100.0f, 600.0f), 600.0f);
		});

		It("steps around its foe on the ring, one way or the other", [this]()
		{
			const FVector Foe(0.0f, 0.0f, 0.0f);
			const FVector Self(1000.0f, 0.0f, 30.0f);
			const FVector Plus = TikiTactics::OrbitPoint(Foe, Self, 700.0f, 25.0f, 1);
			const FVector Minus = TikiTactics::OrbitPoint(Foe, Self, 700.0f, 25.0f, -1);
			TestTrue(TEXT("on the ring"), FMath::IsNearlyEqual(FVector::Dist2D(Plus, Foe), 700.0, 0.5));
			TestTrue(TEXT("turned one way"), Plus.Y > 0.0);
			TestTrue(TEXT("turned the other"), Minus.Y < 0.0);
			TestTrue(TEXT("its own height kept"), FMath::IsNearlyEqual(Plus.Z, 30.0, 0.01));
		});

		It("runs away only wounded and alone", [this]()
		{
			TestTrue(TEXT("wounded, alone"), TikiTactics::ShouldFlee(0.3f, 0.35f, 0));
			TestFalse(TEXT("wounded, an ally near"), TikiTactics::ShouldFlee(0.3f, 0.35f, 1));
			TestFalse(TEXT("healthy, alone"), TikiTactics::ShouldFlee(0.8f, 0.35f, 0));
			TestFalse(TEXT("dead"), TikiTactics::ShouldFlee(0.0f, 0.35f, 0));
		});

		It("keeps away from a lit torch a while, farther when it is raised", [this]()
		{
			TestEqual(TEXT("unlit"), TikiTactics::TorchFearRadius(false, true, 400.0f, 600.0f), 0.0f);
			TestEqual(TEXT("lit"), TikiTactics::TorchFearRadius(true, false, 400.0f, 600.0f), 400.0f);
			TestEqual(TEXT("raised"), TikiTactics::TorchFearRadius(true, true, 400.0f, 600.0f), 600.0f);
			TestTrue(TEXT("first seconds"), TikiTactics::IsTorchFearHolding(10.0, 12.0, 10.0f, 1, 3));
			TestFalse(TEXT("the courage comes back"), TikiTactics::IsTorchFearHolding(10.0, 20.5, 10.0f, 1, 3));
			TestFalse(TEXT("enough of the pack"), TikiTactics::IsTorchFearHolding(10.0, 12.0, 10.0f, 3, 3));
		});
	});

	Describe("Bulwark", [this]()
	{
		It("guards against blows and shots, never a blast nor a burn", [this]()
		{
			using TikiTactics::EGuardable;
			TestTrue(TEXT("grenade"), TikiTactics::ClassifyBlow(false, true, false, false) == EGuardable::None);
			TestTrue(TEXT("musket"), TikiTactics::ClassifyBlow(true, false, true, false) == EGuardable::Shot);
			TestTrue(TEXT("sabre"), TikiTactics::ClassifyBlow(true, false, false, false) == EGuardable::Melee);
			TestTrue(TEXT("fist"), TikiTactics::ClassifyBlow(false, false, false, true) == EGuardable::Melee);
			TestTrue(TEXT("burn of a torch"), TikiTactics::ClassifyBlow(false, false, false, false) == EGuardable::None);
		});

		It("covers the front only: a blow from the side or the back gets through", [this]()
		{
			const FVector Here(0.0f, 0.0f, 0.0f);
			TestTrue(TEXT("front"), TikiTactics::IsInGuardArc(Here, FVector::ForwardVector, FVector(300.0f, 50.0f, 0.0f), 70.0f));
			TestTrue(TEXT("front, a little aside"), TikiTactics::IsInGuardArc(Here, FVector::ForwardVector, FVector(300.0f, 300.0f, 0.0f), 70.0f));
			TestFalse(TEXT("side"), TikiTactics::IsInGuardArc(Here, FVector::ForwardVector, FVector(0.0f, 300.0f, 0.0f), 70.0f));
			TestFalse(TEXT("back"), TikiTactics::IsInGuardArc(Here, FVector::ForwardVector, FVector(-300.0f, 0.0f, 0.0f), 70.0f));
			TestFalse(TEXT("on itself"), TikiTactics::IsInGuardArc(Here, FVector::ForwardVector, Here, 70.0f));
		});

		It("lets through the share of the rules", [this]()
		{
			using TikiTactics::EGuardable;
			TestEqual(TEXT("melee blocked"), TikiTactics::GuardedDamage(35.0f, EGuardable::Melee, 0.0f, 0.2f), 0.0f);
			TestEqual(TEXT("shot partly"), TikiTactics::GuardedDamage(50.0f, EGuardable::Shot, 0.0f, 0.2f), 10.0f);
			TestEqual(TEXT("not guarded"), TikiTactics::GuardedDamage(50.0f, EGuardable::None, 0.0f, 0.0f), 50.0f);
			TestEqual(TEXT("scale bounded"), TikiTactics::GuardedDamage(50.0f, EGuardable::Melee, 2.0f, 0.0f), 50.0f);
		});

		It("breaks under heavy blows, a series or a single strong one", [this]()
		{
			TikiTactics::FGuardPoise Poise;
			TestFalse(TEXT("one slash"), Poise.Add(35.0f, 10.0, 4.0f, 70.0f, 40.0f));
			TestTrue(TEXT("a second slash soon after"), Poise.Add(35.0f, 11.0, 4.0f, 70.0f, 40.0f));
			TestFalse(TEXT("reset after it broke"), Poise.Add(35.0f, 12.0, 4.0f, 70.0f, 40.0f));
			TestFalse(TEXT("the window passed"), Poise.Add(35.0f, 17.0, 4.0f, 70.0f, 40.0f));
			TestTrue(TEXT("a musket ball"), Poise.Add(10000.0f, 18.0, 4.0f, 70.0f, 40.0f));
		});

		It("stands between its ward and the foe", [this]()
		{
			const FVector Spot = TikiTactics::InterposePoint(FVector(0.0f, 0.0f, 10.0f), FVector(1000.0f, 0.0f, 0.0f), 250.0f);
			TestTrue(TEXT("toward the foe"), Spot.Equals(FVector(250.0f, 0.0f, 10.0f), 0.5f));
			const FVector Close = TikiTactics::InterposePoint(FVector::ZeroVector, FVector(200.0f, 0.0f, 0.0f), 250.0f);
			TestTrue(TEXT("never past the middle"), Close.Equals(FVector(100.0f, 0.0f, 0.0f), 0.5f));
		});

		It("covers a watcher or a wounded ally its foe comes at, nobody else", [this]()
		{
			TestEqual(TEXT("foe far"), TikiTactics::WardUrgency(true, 1.0f, 1200.0f, 900.0f, 0.5f), 0.0f);
			TestEqual(TEXT("healthy stalker"), TikiTactics::WardUrgency(false, 0.9f, 300.0f, 900.0f, 0.5f), 0.0f);
			const float Watcher = TikiTactics::WardUrgency(true, 1.0f, 600.0f, 900.0f, 0.5f);
			const float Wounded = TikiTactics::WardUrgency(false, 0.3f, 600.0f, 900.0f, 0.5f);
			TestTrue(TEXT("watcher"), Watcher > 0.0f);
			TestTrue(TEXT("wounded"), Wounded > 0.0f);
			TestTrue(TEXT("the watcher first"), Watcher > Wounded);
			TestTrue(TEXT("a closer foe is more urgent"), TikiTactics::WardUrgency(true, 1.0f, 100.0f, 900.0f, 0.5f) > Watcher);
		});

		It("comes at its foe sideways, then the other side", [this]()
		{
			const FVector Right = TikiTactics::SideApproach(FVector(500.0f, 0.0f, 0.0f), 1, 40.0f);
			const FVector Left = TikiTactics::SideApproach(FVector(500.0f, 0.0f, 0.0f), -1, 40.0f);
			TestTrue(TEXT("unit"), FMath::IsNearlyEqual(Right.Size(), 1.0, 0.001));
			TestTrue(TEXT("still toward the foe"), Right.X > 0.7);
			TestTrue(TEXT("to one side"), Right.Y > 0.6);
			TestTrue(TEXT("to the other"), Left.Y < -0.6);
		});

		It("throws a seized foe to the side, away from its ward", [this]()
		{
			const FVector Throw = TikiTactics::ThrowVelocity(FVector::ForwardVector, 1, 70.0f, 750.0f, 420.0f);
			TestTrue(TEXT("to its right"), Throw.Y > 600.0);
			TestTrue(TEXT("a little forward"), Throw.X > 0.0);
			TestEqual(TEXT("upward"), Throw.Z, 420.0);
			TestEqual(TEXT("no ward: right"), TikiTactics::ThrowSide(FVector::ForwardVector, FVector::ZeroVector, false, FVector::ZeroVector), 1);
			TestEqual(TEXT("ward on its right: left"), TikiTactics::ThrowSide(FVector::ForwardVector, FVector::ZeroVector, true, FVector(0.0f, 300.0f, 0.0f)), -1);
			TestEqual(TEXT("ward on its left: right"), TikiTactics::ThrowSide(FVector::ForwardVector, FVector::ZeroVector, true, FVector(0.0f, -300.0f, 0.0f)), 1);
		});
	});

	Describe("Watcher", [this]()
	{
		It("stands behind a bush from its foe", [this]()
		{
			const FVector Cover = TikiTactics::CoverPoint(FVector(1000.0f, 0.0f, 20.0f), FVector::ZeroVector, 130.0f);
			TestTrue(TEXT("beyond the bush"), Cover.Equals(FVector(1130.0f, 0.0f, 20.0f), 0.5f));
		});

		It("keeps its foe between its distances, on the line through itself", [this]()
		{
			TestTrue(TEXT("too close: backs away"), TikiTactics::KeepAwayPoint(FVector(300.0f, 0.0f, 5.0f), FVector::ZeroVector, 900.0f, 1600.0f).Equals(FVector(900.0f, 0.0f, 5.0f), 0.5f));
			TestTrue(TEXT("too far: comes closer"), TikiTactics::KeepAwayPoint(FVector(0.0f, 3000.0f, 0.0f), FVector::ZeroVector, 900.0f, 1600.0f).Equals(FVector(0.0f, 1600.0f, 0.0f), 0.5f));
			TestTrue(TEXT("in its band: stays"), TikiTactics::KeepAwayPoint(FVector(1200.0f, 0.0f, 0.0f), FVector::ZeroVector, 900.0f, 1600.0f).Equals(FVector(1200.0f, 0.0f, 0.0f), 0.5f));
		});

		It("aims a little ahead of its foe, never far", [this]()
		{
			TestTrue(TEXT("lead"), TikiTactics::LeadAim(FVector::ZeroVector, FVector(300.0f, 0.0f, 0.0f), 0.35f, 300.0f).Equals(FVector(105.0f, 0.0f, 0.0f), 0.5f));
			TestTrue(TEXT("bounded"), TikiTactics::LeadAim(FVector::ZeroVector, FVector(3000.0f, 0.0f, 0.0f), 0.35f, 300.0f).Equals(FVector(300.0f, 0.0f, 0.0f), 0.5f));
			TestTrue(TEXT("a fall is not led"), TikiTactics::LeadAim(FVector::ZeroVector, FVector(0.0f, 0.0f, -900.0f), 0.35f, 300.0f).Equals(FVector::ZeroVector, 0.5f));
		});

		It("throws the lump on an arc that lands where it was aimed", [this]()
		{
			const FVector From(0.0f, 0.0f, 100.0f);
			const FVector To(1000.0f, 0.0f, 0.0f);
			TestTrue(TEXT("leaves the hand"), TikiTactics::ArcPoint(From, To, 250.0f, 0.0f).Equals(From, 0.5f));
			TestTrue(TEXT("lands there"), TikiTactics::ArcPoint(From, To, 250.0f, 1.0f).Equals(To, 0.5f));
			TestTrue(TEXT("top in the middle"), FMath::IsNearlyEqual(TikiTactics::ArcPoint(From, To, 250.0f, 0.5f).Z, 300.0, 0.5));
			TestTrue(TEXT("never past the end"), TikiTactics::ArcPoint(From, To, 250.0f, 3.0f).Equals(To, 0.5f));
			TestTrue(TEXT("flight time"), FMath::IsNearlyEqual(TikiTactics::FlightSeconds(1300.0f, 1300.0f, 0.4f), 1.0f, 0.001f));
			TestTrue(TEXT("never too fast to see"), FMath::IsNearlyEqual(TikiTactics::FlightSeconds(100.0f, 1300.0f, 0.4f), 0.4f, 0.001f));
		});

		It("blinds or burns only who stands in the cloud or the embers", [this]()
		{
			const FVector Center(0.0f, 0.0f, 0.0f);
			TestTrue(TEXT("inside"), TikiTactics::IsInsideBurst(Center, 320.0f, 260.0f, FVector(200.0f, 100.0f, 60.0f)));
			TestFalse(TEXT("beside"), TikiTactics::IsInsideBurst(Center, 320.0f, 260.0f, FVector(400.0f, 0.0f, 60.0f)));
			TestFalse(TEXT("above"), TikiTactics::IsInsideBurst(Center, 320.0f, 260.0f, FVector(0.0f, 0.0f, 400.0f)));
		});
	});

	Describe("Torch", [this]()
	{
		It("shows a standing player at night, not a crouched one, and changes nothing by day", [this]()
		{
			TestTrue(TEXT("night, standing, lit"), TikiTactics::TorchReveals(true, false, true, 2000.0f, 2500.0f));
			TestFalse(TEXT("crouched"), TikiTactics::TorchReveals(true, true, true, 2000.0f, 2500.0f));
			TestFalse(TEXT("unlit"), TikiTactics::TorchReveals(true, false, false, 2000.0f, 2500.0f));
			TestFalse(TEXT("too far"), TikiTactics::TorchReveals(true, false, true, 3000.0f, 2500.0f));
			TestFalse(TEXT("by day the rules of T-262 decide"), TikiTactics::TorchReveals(false, false, true, 2000.0f, 2500.0f));
		});
	});

	Describe("Locomotion", [this]()
	{
		It("picks the side step or the walk backward of a body facing its foe", [this]()
		{
			using TikiTactics::EStrafeClip;
			const FVector Front = FVector::ForwardVector;
			TestTrue(TEXT("backward"), TikiTactics::StrafeClipFor(FVector(-100.0f, 10.0f, 0.0f), Front) == EStrafeClip::Back);
			TestTrue(TEXT("right"), TikiTactics::StrafeClipFor(FVector(0.0f, 100.0f, 0.0f), Front) == EStrafeClip::Right);
			TestTrue(TEXT("left"), TikiTactics::StrafeClipFor(FVector(0.0f, -100.0f, 0.0f), Front) == EStrafeClip::Left);
			TestTrue(TEXT("forward"), TikiTactics::StrafeClipFor(FVector(100.0f, 20.0f, 0.0f), Front) == EStrafeClip::Forward);
			TestTrue(TEXT("still"), TikiTactics::StrafeClipFor(FVector::ZeroVector, Front) == EStrafeClip::Forward);
			TestTrue(TEXT("stalking faces the foe"), TikiTactics::IsStrafeState(ETikiTacticState::Stalk));
			TestFalse(TEXT("a charge turns to its run"), TikiTactics::IsStrafeState(ETikiTacticState::Charge));
		});
	});

	Describe("Fairness", [this]()
	{
		It("announces every heavy move long enough to step aside", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("heavy blow"), Rules->Stalker.HeavyWindup >= 0.6f);
			TestTrue(TEXT("grab"), Rules->Bulwark.GrabWindup >= 0.6f);
			TestTrue(TEXT("cloud"), Rules->Watcher.CastWindup >= 0.6f);
			TestTrue(TEXT("the lump can be seen flying"), Rules->Watcher.Ash.MinFlightSeconds >= 0.3f);
		});

		It("never swarms, never strikes a player lying down, never blinds for long", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("a few attackers at once"), Rules->Pack.MaxAttackersPerFoe >= 1 && Rules->Pack.MaxAttackersPerFoe <= 3);
			TestTrue(TEXT("a player down is spared"), Rules->Pack.bSpareDownedFoes);
			TestTrue(TEXT("a short veil"), Rules->Watcher.Ash.BlindSeconds <= 3.0f);
			TestTrue(TEXT("shapes still show through"), Rules->Watcher.Ash.VeilOpacity < 1.0f);
		});

		It("leaves ways around each role", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("the crab turns slower behind its guard than a stalker circles"), Rules->Bulwark.GuardTurnRate < Rules->Pack.StrafeTurnRate);
			TestTrue(TEXT("its guard leaves the sides open"), Rules->Bulwark.GuardHalfArc < 90.0f);
			TestTrue(TEXT("a stalker's fear of the torch ends"), Rules->Stalker.TorchCourageSeconds > 0.0f);
			TestTrue(TEXT("the watcher backs away before it strikes"), Rules->Watcher.RetreatDistance > Rules->AttackRange);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
