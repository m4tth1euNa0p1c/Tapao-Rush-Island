// monke_island

#include "Misc/AutomationTest.h"
#include "Fort/FortSoldierCorpseMath.h"
#include "Fort/FortSoldierRules.h"
#include <limits>

#if WITH_AUTOMATION_TESTS

namespace
{
	/** A body that rests in every way: dead, its clip over, still on the ground, its ragdoll asleep, nothing to come */
	FortSoldierCorpse::FRestFacts RestingBody()
	{
		FortSoldierCorpse::FRestFacts Facts;
		Facts.bDead = true;
		Facts.bClipDone = true;
		Facts.Speed = 0.0f;
		return Facts;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Fort.Corpse
// The body of a dead fort soldier stops animating once at rest (T-323): its death clip over, still on the ground, its
// ragdoll asleep, no look to come, for a while and two ticks at least; a frozen body looks at its detail level
BEGIN_DEFINE_SPEC(FFortSoldierCorpseSpec, "monke_island.Fort.Corpse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFortSoldierCorpseSpec)

void FFortSoldierCorpseSpec::Define()
{
	Describe("Rules", [this]()
	{
		It("freezes a resting body by default, soon, and looks at its detail level often (DA_FortSoldierRules defaults)", [this]()
		{
			const UFortSoldierRules* Rules = GetDefault<UFortSoldierRules>();
			TestTrue(TEXT("on by default"), Rules->bFreezeRestingCorpse);
			TestTrue(TEXT("rests a few seconds at most"), Rules->CorpseRestSeconds >= 0.0f && Rules->CorpseRestSeconds <= 5.0f);
			TestTrue(TEXT("a crawling soldier is never still"), Rules->CorpseStillSpeed < Rules->CrawlSpeed);
			TestTrue(TEXT("a new detail level within a second"), Rules->CorpseLODCheckSeconds >= 0.05f && Rules->CorpseLODCheckSeconds <= 1.0f);
			TestTrue(TEXT("frozen before its body goes"), Rules->DeadLifetimeSeconds <= 0.0f || Rules->CorpseRestSeconds < Rules->DeadLifetimeSeconds);
		});
	});

	Describe("Clip", [this]()
	{
		It("waits for the death clip to end and to blend in fully, and for the upper layer to fade out", [this]()
		{
			TestFalse(TEXT("mid-clip"), FortSoldierCorpse::IsClipDone(true, 0.5f, 2.0f, 1.0f, 0.0f));
			TestTrue(TEXT("at its end"), FortSoldierCorpse::IsClipDone(true, 2.0f, 2.0f, 1.0f, 0.0f));
			TestTrue(TEXT("long after"), FortSoldierCorpse::IsClipDone(true, 30.0f, 2.0f, 1.0f, 0.0f));
			TestFalse(TEXT("still blending in"), FortSoldierCorpse::IsClipDone(true, 3.0f, 2.0f, 0.6f, 0.0f));
			TestFalse(TEXT("the upper layer still in"), FortSoldierCorpse::IsClipDone(true, 3.0f, 2.0f, 1.0f, 0.4f));
			TestTrue(TEXT("no death clip: nothing to wait for"), FortSoldierCorpse::IsClipDone(false, 0.0f, 0.0f, 0.0f, 0.0f));
			TestFalse(TEXT("no death clip, the upper layer still in"), FortSoldierCorpse::IsClipDone(false, 0.0f, 0.0f, 0.0f, 0.5f));
			TestFalse(TEXT("a time that is not a number"), FortSoldierCorpse::IsClipDone(true, std::numeric_limits<float>::quiet_NaN(), 2.0f, 1.0f, 0.0f));
		});
	});

	Describe("Rest", [this]()
	{
		It("rests only dead, its clip over, still on the ground, its ragdoll asleep, no look to come", [this]()
		{
			TestTrue(TEXT("at rest"), FortSoldierCorpse::IsAtRest(RestingBody(), 5.0f));

			FortSoldierCorpse::FRestFacts Facts = RestingBody();
			Facts.bDead = false;
			TestFalse(TEXT("alive"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.bClipDone = false;
			TestFalse(TEXT("its clip still playing"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.bFalling = true;
			TestFalse(TEXT("falling"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.bCarried = true;
			TestFalse(TEXT("carried"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.Speed = 40.0f;
			TestFalse(TEXT("moving"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));
			Facts.Speed = 5.0f;
			TestTrue(TEXT("at the still speed"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));
			Facts.Speed = std::numeric_limits<float>::quiet_NaN();
			TestFalse(TEXT("a speed that is not a number"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.bRagdollAwake = true;
			TestFalse(TEXT("its ragdoll awake"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.bLookPending = true;
			TestFalse(TEXT("a look to come"), FortSoldierCorpse::IsAtRest(Facts, 5.0f));

			Facts = RestingBody();
			Facts.Speed = 1.0f;
			TestFalse(TEXT("a negative still speed counts as 0"), FortSoldierCorpse::IsAtRest(Facts, -10.0f));
		});

		It("freezes after the rest delay and two ticks at least, and starts over when the body moves", [this]()
		{
			FortSoldierCorpse::FRestClock Clock;
			TestFalse(TEXT("no rest yet"), FortSoldierCorpse::ShouldFreeze(true, Clock, 1.0f));
			Clock.Advance(true, 0.5f);
			TestFalse(TEXT("half the delay"), FortSoldierCorpse::ShouldFreeze(true, Clock, 1.0f));
			Clock.Advance(true, 0.5f);
			TestTrue(TEXT("the delay"), FortSoldierCorpse::ShouldFreeze(true, Clock, 1.0f));
			TestFalse(TEXT("not allowed (rules or console off)"), FortSoldierCorpse::ShouldFreeze(false, Clock, 1.0f));
			TestFalse(TEXT("a delay that is not a number"), FortSoldierCorpse::ShouldFreeze(true, Clock, std::numeric_limits<float>::quiet_NaN()));
			Clock.Advance(false, 0.5f);
			TestFalse(TEXT("moved: starts over"), FortSoldierCorpse::ShouldFreeze(true, Clock, 1.0f));
			TestEqual(TEXT("moved: no time"), Clock.Seconds, 0.0f);
			TestEqual(TEXT("moved: no tick"), Clock.Ticks, 0);

			// without a delay, two ticks still: the skeleton refreshed once after the last change
			Clock.Advance(true, 0.0f);
			TestFalse(TEXT("one tick without a delay"), FortSoldierCorpse::ShouldFreeze(true, Clock, 0.0f));
			Clock.Advance(true, 0.0f);
			TestTrue(TEXT("two ticks without a delay"), FortSoldierCorpse::ShouldFreeze(true, Clock, 0.0f));
			TestTrue(TEXT("a negative delay counts as 0"), FortSoldierCorpse::ShouldFreeze(true, Clock, -3.0f));

			for (int32 Tick = 0; Tick < 100; ++Tick)
			{
				Clock.Advance(true, std::numeric_limits<float>::quiet_NaN());
			}
			TestEqual(TEXT("ticks held at the minimum"), Clock.Ticks, FortSoldierCorpse::MinRestTicks);
			TestEqual(TEXT("a step that is not a number adds no time"), Clock.Seconds, 0.0f);
		});

		It("lets a body fall, land and play its clip, then freezes it once rested, and again after a wake", [this]()
		{
			// a quarter of a second per frame: falling for 0.75 s, a death clip of 2 s, 1 s of rest asked
			constexpr float Step = 0.25f;
			constexpr float ClipLength = 2.0f;
			constexpr float RestSeconds = 1.0f;
			FortSoldierCorpse::FRestClock Clock;
			int32 FrozenAt = INDEX_NONE;
			for (int32 Frame = 1; Frame <= 40 && FrozenAt == INDEX_NONE; ++Frame)
			{
				const float Time = Frame * Step;
				FortSoldierCorpse::FRestFacts Facts = RestingBody();
				Facts.bFalling = Time <= 0.75f;
				Facts.Speed = Facts.bFalling ? 500.0f : 0.0f;
				Facts.bClipDone = FortSoldierCorpse::IsClipDone(true, Time, ClipLength, 1.0f, 0.0f);
				Clock.Advance(FortSoldierCorpse::IsAtRest(Facts, 5.0f), Step);
				if (FortSoldierCorpse::ShouldFreeze(true, Clock, RestSeconds))
				{
					FrozenAt = Frame;
				}
			}
			// the clip ends at frame 8 (2 s), at rest from there: 4 frames of a quarter make the second asked
			TestEqual(TEXT("frozen one second after the clip ended"), FrozenAt, 11);

			// a wake (a limb torn off, a new detail level): the rest starts over, the same second again
			Clock.Reset();
			int32 Again = 0;
			while (!FortSoldierCorpse::ShouldFreeze(true, Clock, RestSeconds) && Again < 40)
			{
				Clock.Advance(FortSoldierCorpse::IsAtRest(RestingBody(), 5.0f), Step);
				++Again;
			}
			TestEqual(TEXT("frozen again one second after the wake"), Again, 4);
		});
	});

	Describe("Watch", [this]()
	{
		It("looks at a frozen body between 0.05 and 5 s", [this]()
		{
			TestEqual(TEXT("as asked"), FortSoldierCorpse::WatchDelay(0.25f), 0.25f);
			TestEqual(TEXT("not too often"), FortSoldierCorpse::WatchDelay(0.0f), 0.05f);
			TestEqual(TEXT("not too seldom"), FortSoldierCorpse::WatchDelay(100.0f), 5.0f);
			TestEqual(TEXT("not a number"), FortSoldierCorpse::WatchDelay(std::numeric_limits<float>::quiet_NaN()), 0.25f);
		});
	});

	Describe("Bounds (T-332)", [this]()
	{
		It("gives a body lying on the ground a larger box, never a smaller one", [this]()
		{
			TestEqual(TEXT("standing: the blueprint's"), FortSoldierCorpse::BodyBoundsScale(false, 1.8f, 2.6f), 1.8f);
			TestEqual(TEXT("lying: the rules'"), FortSoldierCorpse::BodyBoundsScale(true, 1.8f, 2.6f), 2.6f);
			TestEqual(TEXT("lying, the rules off (0)"), FortSoldierCorpse::BodyBoundsScale(true, 1.8f, 0.0f), 1.8f);
			TestEqual(TEXT("lying, never below the standing box"), FortSoldierCorpse::BodyBoundsScale(true, 3.0f, 2.6f), 3.0f);
			TestEqual(TEXT("a standing scale that is not a number"), FortSoldierCorpse::BodyBoundsScale(false, std::numeric_limits<float>::quiet_NaN(), 2.6f), 1.0f);
			TestEqual(TEXT("a lying scale that is not a number"), FortSoldierCorpse::BodyBoundsScale(true, 1.8f, std::numeric_limits<float>::quiet_NaN()), 1.8f);
			TestTrue(TEXT("on by default, larger than the standing box of the soldier (1.8)"), GetDefault<UFortSoldierRules>()->LyingBoundsScale > 1.8f);
		});
	});
}

#endif
