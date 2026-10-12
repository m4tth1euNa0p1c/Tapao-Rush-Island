// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GalleonDeparture.generated.h"

class APlayerState;

/** Stage of the departure from the galleon's wheel (cahier §24.2 step 7, §6.5), decided by the server */
UENUM(BlueprintType)
enum class EGalleonDepartureStage : uint8
{
	/** Nothing under way: a living player aboard may hold "Lever l'ancre" at the wheel */
	None,
	/** A vote of the crew is open: the galleon sails once the majority of the living agrees, somebody being aboard */
	Vote,
	/** The departure is decided: the notice runs, cancellable at the wheel; at its end the galleon sails */
	Notice
};

/** Why the last vote or notice ended, told to the whole crew for a moment */
UENUM(BlueprintType)
enum class EGalleonDepartureEnd : uint8
{
	/** Nothing ended yet */
	None,
	/** Cancelled at the wheel */
	Cancelled,
	/** The vote ran out of time without a majority */
	Lapsed,
	/** Nobody alive aboard any more: the vote or the notice is void (cahier §6.5) */
	NobodyAboard,
	/** The notice ran out: the galleon sailed */
	Sailed,
	/** The day ended otherwise (end of the submersion, the crew wiped) */
	Overtaken
};

/** Sound of the departure, played on every machine (AGalleonActor::PlayDepartureCue) */
enum class EGalleonDepartureCue : uint8
{
	/** Ship's bell: a vote opens, a notice starts */
	Bell,
	/** Foghorn: a notice starts */
	Horn,
	/** One second of the notice's last ones */
	Tick
};

/** Replicated state of the departure (UGalleonDepartureComponent, on the expedition game state) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FGalleonDepartureState
{
	GENERATED_BODY()

	/** Stage under way */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	EGalleonDepartureStage Stage = EGalleonDepartureStage::None;

	/** The notice follows the "every living player aboard" rule (the short one), not a vote */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	bool bAllAboard = false;

	/** Living players for the departure, the one who raised the anchor first; dead or gone players are dropped */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	TArray<TObjectPtr<APlayerState>> Voters;

	/** Living players at the server's last count (the electorate: dead, spectators and players still loading left out) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	uint8 Alive = 0;

	/** Living players aboard at the server's last count */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	uint8 Aboard = 0;

	/** Votes needed at the server's last count (DepartureMajorityFraction of Alive) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	uint8 Needed = 0;

	/** Server time the current stage began (s) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	double StageStartTime = -1.0;

	/** Server time the vote lapses (Vote) or the galleon sails (Notice) (s) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	double StageEndTime = -1.0;

	/** Server time before which no new vote opens, after one failed (s) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	double NextVoteTime = -1.0;

	/** Why the last vote or notice ended */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	EGalleonDepartureEnd LastEnd = EGalleonDepartureEnd::None;

	/** Server time it ended (s) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	double LastEndTime = -1.0;
};

/** "Lever l'ancre" held at the wheel, replicated so the holder sees the hold fill */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FGalleonHelmHold
{
	GENERATED_BODY()

	/** Player holding the wheel's key, nullptr when nobody */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	TObjectPtr<APlayerState> Holder;

	/** Server time the hold began (s) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	double StartTime = -1.0;

	/** Server time the hold completes (s) */
	UPROPERTY(BlueprintReadOnly, Category="Galleon|Departure")
	double EndTime = -1.0;
};

/** Counts of the living crew taken by the server, the input of the rules below */
struct FGalleonCrewCount
{
	/** Living players (the electorate) */
	int32 Alive = 0;

	/** Living players aboard the galleon */
	int32 Aboard = 0;

	/** Living players who voted for the departure */
	int32 Yes = 0;
};

/**
 *  Pure rules of the departure from the wheel (cahier §24.2 step 7, §6.5, §24.5), shared by the server component and
 *  the tests (monke_island.Galleon.Departure). Times are server world times in seconds.
 *  - A living player aboard holds "Lever l'ancre" at the wheel (HelmHoldSeconds).
 *  - Every living player aboard: the galleon sails after the short notice (DepartureAllAboardNoticeSeconds).
 *  - Otherwise a vote of the living opens; it passes with strictly more than DepartureMajorityFraction of them and
 *    somebody aboard, then the long notice runs (DepartureVoteNoticeSeconds), cancellable at the wheel.
 *  - Nobody alive aboard: the vote or the notice is void. A vote without majority lapses after DepartureVoteSeconds.
 */
namespace GalleonDeparture
{
	/** What a hold completed at the wheel starts */
	enum class EHoldOutcome : uint8
	{
		/** Every living player is aboard: the short notice */
		AllAboardNotice,
		/** A vote of the crew, the holder's vote counted */
		Vote,
		/** A vote is needed but the last one failed a moment ago: nothing */
		CoolingDown,
		/** Nobody alive aboard (the holder died or left meanwhile): nothing */
		NobodyAboard
	};

	/** What becomes of an open vote now */
	enum class EVoteVerdict : uint8
	{
		/** Not enough votes yet, time left */
		Wait,
		/** The majority agrees, somebody is aboard: the notice starts */
		Notice,
		/** Nobody alive aboard: void */
		Void,
		/** Out of time without a majority */
		Lapse
	};

	/**
	 *  Votes needed among Alive living players: the smallest count strictly above Fraction of them (Fraction 0.5 gives
	 *  1 of 1, 2 of 2 or 3, 3 of 4; 0 gives 1, the holder alone decides). Fraction is clamped to [0, 0.99]; 0 when
	 *  nobody is alive.
	 */
	MONKE_ISLAND_API int32 VotesNeeded(int32 Alive, float Fraction);

	/** True when Yes votes reach VotesNeeded(Alive, Fraction), somebody being alive */
	MONKE_ISLAND_API bool HasMajority(int32 Yes, int32 Alive, float Fraction);

	/** What a hold completed at the wheel starts: the short notice when every living player is aboard, else a vote
	 *  unless one failed before NextVoteTime */
	MONKE_ISLAND_API EHoldOutcome OnHoldCompleted(const FGalleonCrewCount& Count, double Now, double NextVoteTime);

	/** What becomes of an open vote: void without anybody alive aboard first, then the majority, then the time */
	MONKE_ISLAND_API EVoteVerdict EvaluateVote(const FGalleonCrewCount& Count, float Fraction, double Now, double VoteEndTime);

	/** Whether a notice still holds: somebody alive aboard to sail with (else void, cahier §6.5) */
	MONKE_ISLAND_API bool NoticeHolds(const FGalleonCrewCount& Count);

	/** Length of a notice: the short one when every living player was aboard, the long one after a vote (s, >= 0) */
	MONKE_ISLAND_API float NoticeSeconds(bool bAllAboard, float AllAboardSeconds, float VoteNoticeSeconds);

	/** Second of a countdown to tick at Left seconds, 0 when none: the seconds TickFrom..1 each tick once, as Left
	 *  crosses them */
	MONKE_ISLAND_API int32 CountdownSecond(double Left, int32 TickFrom);
}
