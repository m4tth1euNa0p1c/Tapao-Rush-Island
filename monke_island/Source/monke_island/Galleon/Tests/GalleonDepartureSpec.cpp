// monke_island

#include "Misc/AutomationTest.h"
#include "Galleon/GalleonDeparture.h"
#include "Galleon/GalleonRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** Enums compared as numbers in the reports */
	int32 HoldCode(GalleonDeparture::EHoldOutcome Outcome) { return static_cast<int32>(Outcome); }
	int32 VoteCode(GalleonDeparture::EVoteVerdict Verdict) { return static_cast<int32>(Verdict); }

	FGalleonCrewCount Crew(int32 Alive, int32 Aboard, int32 Yes)
	{
		FGalleonCrewCount Count;
		Count.Alive = Alive;
		Count.Aboard = Aboard;
		Count.Yes = Yes;
		return Count;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Galleon.Departure
// Cahier §24.2 step 7, §6.5, §24.5, §24.6; network review T-268 R-01 (T-286). Times are server times in seconds.
BEGIN_DEFINE_SPEC(FGalleonDepartureSpec, "monke_island.Galleon.Departure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGalleonDepartureSpec)

void FGalleonDepartureSpec::Define()
{
	Describe("Cahier defaults (DA_GalleonRules)", [this]()
	{
		It("holds 2 s, gives 5 s all aboard and 10 s after a vote, the majority of the living", [this]()
		{
			const UGalleonRules* Rules = GetDefault<UGalleonRules>();
			TestEqual(TEXT("hold"), Rules->HelmHoldSeconds, 2.0f);
			TestEqual(TEXT("notice all aboard"), Rules->DepartureAllAboardNoticeSeconds, 5.0f);
			TestEqual(TEXT("notice after a vote"), Rules->DepartureVoteNoticeSeconds, 10.0f);
			TestEqual(TEXT("strictly more than half"), Rules->DepartureMajorityFraction, 0.5f);
			TestTrue(TEXT("the vote lapses"), Rules->DepartureVoteSeconds > 0.0f);
			TestTrue(TEXT("the wheel's key is the interaction key F"), Rules->HelmKeys.Contains(EKeys::F));
			TestTrue(TEXT("a vote key"), Rules->VoteKeys.Num() > 0 && !Rules->VoteKeys.Contains(EKeys::F));
			TestTrue(TEXT("requests rate limited"), Rules->DepartureRequestMinInterval > 0.0f);
		});
	});

	Describe("VotesNeeded", [this]()
	{
		It("asks strictly more than half of the living", [this]()
		{
			TestEqual(TEXT("1 of 1"), GalleonDeparture::VotesNeeded(1, 0.5f), 1);
			TestEqual(TEXT("2 of 2"), GalleonDeparture::VotesNeeded(2, 0.5f), 2);
			TestEqual(TEXT("2 of 3"), GalleonDeparture::VotesNeeded(3, 0.5f), 2);
			TestEqual(TEXT("3 of 4"), GalleonDeparture::VotesNeeded(4, 0.5f), 3);
			TestEqual(TEXT("nobody alive"), GalleonDeparture::VotesNeeded(0, 0.5f), 0);
		});

		It("follows the share of the rules, clamped", [this]()
		{
			TestEqual(TEXT("0: the holder alone"), GalleonDeparture::VotesNeeded(4, 0.0f), 1);
			TestEqual(TEXT("0.99: unanimity"), GalleonDeparture::VotesNeeded(4, 0.99f), 4);
			TestEqual(TEXT("above 0.99 clamped: still possible"), GalleonDeparture::VotesNeeded(3, 1.5f), 3);
			TestEqual(TEXT("below 0 clamped"), GalleonDeparture::VotesNeeded(3, -1.0f), 1);
			TestEqual(TEXT("0.7 of 10 is 7: 8 needed (no rounding error)"), GalleonDeparture::VotesNeeded(10, 0.7f), 8);
		});

		It("counts a majority only among somebody alive", [this]()
		{
			TestFalse(TEXT("1 of 2"), GalleonDeparture::HasMajority(1, 2, 0.5f));
			TestTrue(TEXT("2 of 2"), GalleonDeparture::HasMajority(2, 2, 0.5f));
			TestTrue(TEXT("2 of 3"), GalleonDeparture::HasMajority(2, 3, 0.5f));
			TestFalse(TEXT("2 of 4"), GalleonDeparture::HasMajority(2, 4, 0.5f));
			TestTrue(TEXT("3 of 4"), GalleonDeparture::HasMajority(3, 4, 0.5f));
			TestFalse(TEXT("nobody alive"), GalleonDeparture::HasMajority(0, 0, 0.5f));
		});
	});

	Describe("OnHoldCompleted", [this]()
	{
		It("gives the short notice when every living player is aboard, solo included", [this]()
		{
			TestEqual(TEXT("solo"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(1, 1, 0), 100.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::AllAboardNotice));
			TestEqual(TEXT("4 of 4 aboard"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(4, 4, 0), 100.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::AllAboardNotice));
			TestEqual(TEXT("all aboard, a vote failed a moment ago: no vote needed"),
				HoldCode(GalleonDeparture::OnHoldCompleted(Crew(2, 2, 0), 100.0, 105.0)), HoldCode(GalleonDeparture::EHoldOutcome::AllAboardNotice));
		});

		It("opens a vote when somebody alive is ashore (R-01: one press no longer ends the day)", [this]()
		{
			TestEqual(TEXT("host ashore"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(2, 1, 0), 100.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::Vote));
			TestEqual(TEXT("cooldown over"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(3, 1, 0), 100.0, 100.0)), HoldCode(GalleonDeparture::EHoldOutcome::Vote));
		});

		It("opens no vote during the cooldown, nor with nobody alive aboard", [this]()
		{
			TestEqual(TEXT("cooldown"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(2, 1, 0), 100.0, 104.0)), HoldCode(GalleonDeparture::EHoldOutcome::CoolingDown));
			TestEqual(TEXT("nobody aboard"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(2, 0, 0), 100.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::NobodyAboard));
			TestEqual(TEXT("nobody alive"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(0, 0, 0), 100.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::NobodyAboard));
		});
	});

	Describe("EvaluateVote", [this]()
	{
		const double End = 130.0;

		It("waits for the majority while time is left", [this, End]()
		{
			TestEqual(TEXT("1 of 2"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 1, 1), 0.5f, 110.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Wait));
		});

		It("starts the notice once the majority of the living agrees, somebody aboard", [this, End]()
		{
			TestEqual(TEXT("2 of 2, one ashore"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 1, 2), 0.5f, 110.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Notice));
			TestEqual(TEXT("2 of 3"), VoteCode(GalleonDeparture::EvaluateVote(Crew(3, 1, 2), 0.5f, 110.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Notice));
			TestEqual(TEXT("majority at the deadline wins"), VoteCode(GalleonDeparture::EvaluateVote(Crew(3, 1, 2), 0.5f, End, End)), VoteCode(GalleonDeparture::EVoteVerdict::Notice));
		});

		It("voids the vote once nobody alive is aboard, even with the votes (cahier §6.5)", [this, End]()
		{
			TestEqual(TEXT("everybody ashore"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 0, 2), 0.5f, 110.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Void));
			TestEqual(TEXT("everybody dead"), VoteCode(GalleonDeparture::EvaluateVote(Crew(0, 0, 0), 0.5f, 110.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Void));
		});

		It("lapses without a majority at the deadline", [this, End]()
		{
			TestEqual(TEXT("1 of 2 at the end"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 1, 1), 0.5f, End, End)), VoteCode(GalleonDeparture::EVoteVerdict::Lapse));
		});

		It("lets the voters' death change the majority: 1 of 1 left alive passes", [this, End]()
		{
			// three alive, one vote; two die ashore: the one left aboard is the majority
			TestEqual(TEXT("1 of 3"), VoteCode(GalleonDeparture::EvaluateVote(Crew(3, 1, 1), 0.5f, 110.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Wait));
			TestEqual(TEXT("1 of 1"), VoteCode(GalleonDeparture::EvaluateVote(Crew(1, 1, 1), 0.5f, 111.0, End)), VoteCode(GalleonDeparture::EVoteVerdict::Notice));
		});
	});

	Describe("Notice", [this]()
	{
		It("holds while somebody alive is aboard", [this]()
		{
			TestTrue(TEXT("one aboard"), GalleonDeparture::NoticeHolds(Crew(2, 1, 0)));
			TestFalse(TEXT("nobody aboard"), GalleonDeparture::NoticeHolds(Crew(2, 0, 0)));
			TestFalse(TEXT("nobody alive"), GalleonDeparture::NoticeHolds(Crew(0, 0, 0)));
		});

		It("lasts the short notice all aboard, the long one after a vote", [this]()
		{
			TestEqual(TEXT("all aboard"), GalleonDeparture::NoticeSeconds(true, 5.0f, 10.0f), 5.0f);
			TestEqual(TEXT("vote"), GalleonDeparture::NoticeSeconds(false, 5.0f, 10.0f), 10.0f);
			TestEqual(TEXT("never negative"), GalleonDeparture::NoticeSeconds(false, 5.0f, -3.0f), 0.0f);
		});

		It("ticks each of its last seconds once", [this]()
		{
			TestEqual(TEXT("10 s left, ticks from 5"), GalleonDeparture::CountdownSecond(10.0, 5), 0);
			TestEqual(TEXT("5 s left"), GalleonDeparture::CountdownSecond(5.0, 5), 5);
			TestEqual(TEXT("4.2 s left"), GalleonDeparture::CountdownSecond(4.2, 5), 5);
			TestEqual(TEXT("4 s left"), GalleonDeparture::CountdownSecond(4.0, 5), 4);
			TestEqual(TEXT("0.3 s left"), GalleonDeparture::CountdownSecond(0.3, 5), 1);
			TestEqual(TEXT("over"), GalleonDeparture::CountdownSecond(0.0, 5), 0);
			TestEqual(TEXT("past"), GalleonDeparture::CountdownSecond(-1.0, 5), 0);
			TestEqual(TEXT("no tick"), GalleonDeparture::CountdownSecond(3.0, 0), 0);
		});
	});

	Describe("Scenarios", [this]()
	{
		It("solo: hold, every living player aboard, the short notice", [this]()
		{
			const FGalleonCrewCount Solo = Crew(1, 1, 0);
			TestEqual(TEXT("short notice"), HoldCode(GalleonDeparture::OnHoldCompleted(Solo, 50.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::AllAboardNotice));
			TestEqual(TEXT("5 s"), GalleonDeparture::NoticeSeconds(true, 5.0f, 10.0f), 5.0f);
		});

		It("R-01: the client at the wheel, the host ashore: a vote, the host's yes, then the long notice", [this]()
		{
			// the client holds the wheel: a vote with his yes (1 of 2)
			TestEqual(TEXT("vote"), HoldCode(GalleonDeparture::OnHoldCompleted(Crew(2, 1, 0), 300.0, -1.0)), HoldCode(GalleonDeparture::EHoldOutcome::Vote));
			TestEqual(TEXT("1 of 2 waits"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 1, 1), 0.5f, 300.0, 330.0)), VoteCode(GalleonDeparture::EVoteVerdict::Wait));
			// without the host's vote the departure never comes: the vote lapses
			TestEqual(TEXT("lapses"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 1, 1), 0.5f, 330.0, 330.0)), VoteCode(GalleonDeparture::EVoteVerdict::Lapse));
			// with it: the notice of 10 s
			TestEqual(TEXT("2 of 2"), VoteCode(GalleonDeparture::EvaluateVote(Crew(2, 1, 2), 0.5f, 310.0, 330.0)), VoteCode(GalleonDeparture::EVoteVerdict::Notice));
			TestEqual(TEXT("10 s"), GalleonDeparture::NoticeSeconds(false, 5.0f, 10.0f), 10.0f);
		});
	});
}

#endif
