// monke_island

#include "Galleon/GalleonDeparture.h"

namespace GalleonDeparture
{
	int32 VotesNeeded(int32 Alive, float Fraction)
	{
		if (Alive <= 0)
		{
			return 0;
		}
		// strictly above Fraction of the living: the next whole vote above Fraction x Alive (the tolerance keeps
		// 0.7 x 10 = 6.9999 from passing at 7)
		const double Share = static_cast<double>(FMath::Clamp(Fraction, 0.0f, 0.99f)) * Alive;
		return FMath::Clamp(FMath::FloorToInt32(Share + 1.0e-6) + 1, 1, Alive);
	}

	bool HasMajority(int32 Yes, int32 Alive, float Fraction)
	{
		return Alive > 0 && Yes >= VotesNeeded(Alive, Fraction);
	}

	EHoldOutcome OnHoldCompleted(const FGalleonCrewCount& Count, double Now, double NextVoteTime)
	{
		if (Count.Aboard <= 0 || Count.Alive <= 0)
		{
			return EHoldOutcome::NobodyAboard;
		}
		if (Count.Aboard >= Count.Alive)
		{
			return EHoldOutcome::AllAboardNotice;
		}
		return Now < NextVoteTime ? EHoldOutcome::CoolingDown : EHoldOutcome::Vote;
	}

	EVoteVerdict EvaluateVote(const FGalleonCrewCount& Count, float Fraction, double Now, double VoteEndTime)
	{
		if (Count.Aboard <= 0 || Count.Alive <= 0)
		{
			return EVoteVerdict::Void;
		}
		if (HasMajority(Count.Yes, Count.Alive, Fraction))
		{
			return EVoteVerdict::Notice;
		}
		return Now >= VoteEndTime ? EVoteVerdict::Lapse : EVoteVerdict::Wait;
	}

	bool NoticeHolds(const FGalleonCrewCount& Count)
	{
		return Count.Alive > 0 && Count.Aboard > 0;
	}

	float NoticeSeconds(bool bAllAboard, float AllAboardSeconds, float VoteNoticeSeconds)
	{
		return FMath::Max(bAllAboard ? AllAboardSeconds : VoteNoticeSeconds, 0.0f);
	}

	int32 CountdownSecond(double Left, int32 TickFrom)
	{
		if (Left <= 0.0 || TickFrom <= 0)
		{
			return 0;
		}
		const int32 Second = FMath::CeilToInt32(Left);
		return Second <= TickFrom ? Second : 0;
	}
}
