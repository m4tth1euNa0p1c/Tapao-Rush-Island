// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"

/**
 *  Pure rules of the crank machine gun (T-273), shared by AMachineGunWeapon and the tests (monke_island.Weapons.MachineGun):
 *  the cadence follows the crank (it spins up when the trigger is held, down when it is let go), every round heats the
 *  barrel which cools once the fire stops, an overheated gun no longer fires until it cooled, the spread blooms during
 *  a burst, a reload fills the belt from the reserve. No world, no time source: the callers pass the seconds.
 */
namespace MachineGunMath
{
	/** Rounds per second of the crank at CrankSpeed (0: the first turns, MinRpm; 1: full speed, MaxRpm) */
	inline float RoundsPerSecond(float CrankSpeed, float MinRpm, float MaxRpm)
	{
		const float Low = FMath::Max(MinRpm, 1.0f);
		const float High = FMath::Max(MaxRpm, Low);
		return FMath::Lerp(Low, High, FMath::Clamp(CrankSpeed, 0.0f, 1.0f)) / 60.0f;
	}

	/** Crank speed after DeltaSeconds: up to 1 in SpinUpSeconds while it turns, down to 0 in SpinDownSeconds once let go */
	inline float StepCrank(float CrankSpeed, bool bTurning, float DeltaSeconds, float SpinUpSeconds, float SpinDownSeconds)
	{
		const float Step = FMath::Max(DeltaSeconds, 0.0f);
		if (bTurning)
		{
			return SpinUpSeconds <= UE_KINDA_SMALL_NUMBER ? 1.0f : FMath::Min(1.0f, CrankSpeed + Step / SpinUpSeconds);
		}
		return SpinDownSeconds <= UE_KINDA_SMALL_NUMBER ? 0.0f : FMath::Max(0.0f, CrankSpeed - Step / SpinDownSeconds);
	}

	/** Advances the round clock (a round leaves each time it passes 1) by DeltaSeconds at RoundsPerSecond; the rounds
	 *  that leave now, at most MaxRounds (a long hitch never fires a whole belt in one frame: the rest is dropped) */
	inline int32 AdvanceClock(float& Clock, float DeltaSeconds, float InRoundsPerSecond, int32 MaxRounds)
	{
		Clock += FMath::Max(DeltaSeconds, 0.0f) * FMath::Max(InRoundsPerSecond, 0.0f);
		int32 Rounds = 0;
		while (Clock >= 1.0f && Rounds < MaxRounds)
		{
			Clock -= 1.0f;
			++Rounds;
		}
		// rounds beyond the cap are lost, not owed
		Clock = FMath::Min(Clock, 1.0f);
		return Rounds;
	}

	/** Round clock at the start of a burst: the first round leaves at once when the last one is at least a round
	 *  interval (at full speed) old; sooner, it waits for the rest of that interval (no tap fires faster than the gun) */
	inline float BurstStartClock(float SecondsSinceLastRound, float MaxRoundsPerSecond)
	{
		return FMath::Clamp(FMath::Max(SecondsSinceLastRound, 0.0f) * FMath::Max(MaxRoundsPerSecond, 0.0f), 0.0f, 1.0f);
	}

	/** Heat of the barrel after a round (0 cold, 1 white hot) */
	inline float HeatAfterRound(float Heat, float HeatPerRound)
	{
		return FMath::Clamp(Heat + FMath::Max(HeatPerRound, 0.0f), 0.0f, 1.0f);
	}

	/** Heat after DeltaSeconds: the barrel cools at CoolingPerSecond once the last round is CoolingDelay old (the water
	 *  of the jacket keeps it from cooling during a burst) */
	inline float Cool(float Heat, float SecondsSinceLastRound, float DeltaSeconds, float CoolingDelay, float CoolingPerSecond)
	{
		const float Since = FMath::Max(SecondsSinceLastRound, 0.0f);
		const float Step = FMath::Max(DeltaSeconds, 0.0f);
		// only the part of the step past the delay cools
		const float Cooling = FMath::Clamp(Since - FMath::Max(CoolingDelay, 0.0f), 0.0f, Step);
		return FMath::Clamp(Heat - Cooling * FMath::Max(CoolingPerSecond, 0.0f), 0.0f, 1.0f);
	}

	/** The barrel overheats at OverheatHeat: the fire is cut */
	inline bool IsOverheating(float Heat, float OverheatHeat)
	{
		return Heat >= FMath::Clamp(OverheatHeat, 0.01f, 1.0f) - UE_KINDA_SMALL_NUMBER;
	}

	/** An overheated gun fires again once its barrel cooled down to RecoverHeat */
	inline bool HasRecovered(float Heat, float RecoverHeat)
	{
		return Heat <= FMath::Max(RecoverHeat, 0.0f) + UE_KINDA_SMALL_NUMBER;
	}

	/** Glow of the barrel and the jacket (material emission): 0 up to GlowStartHeat, then up to 1 at full heat (smooth) */
	inline float GlowAlpha(float Heat, float GlowStartHeat)
	{
		const float Start = FMath::Clamp(GlowStartHeat, 0.0f, 0.99f);
		const float Alpha = FMath::Clamp((Heat - Start) / (1.0f - Start), 0.0f, 1.0f);
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}

	/** Heat packed for replication (0-255) */
	inline uint8 PackHeat(float Heat)
	{
		return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Heat, 0.0f, 1.0f) * 255.0f));
	}

	/** See PackHeat */
	inline float UnpackHeat(uint8 Packed)
	{
		return static_cast<float>(Packed) / 255.0f;
	}

	/** Bloom (deg) after a round: grows by PerRound up to Max */
	inline float BloomAfterRound(float Bloom, float PerRound, float Max)
	{
		return FMath::Clamp(Bloom + FMath::Max(PerRound, 0.0f), 0.0f, FMath::Max(Max, 0.0f));
	}

	/** Bloom (deg) after DeltaSeconds without a round */
	inline float RecoverBloom(float Bloom, float DeltaSeconds, float PerSecond)
	{
		return FMath::Max(0.0f, Bloom - FMath::Max(DeltaSeconds, 0.0f) * FMath::Max(PerSecond, 0.0f));
	}

	/** Spread half angle of a round (deg): the stance spread (steady or on the move) plus the bloom, wider on the move */
	inline float Spread(float StanceSpread, float Bloom, bool bMoving, float MovingBloomScale)
	{
		return FMath::Max(StanceSpread, 0.0f) + FMath::Max(Bloom, 0.0f) * (bMoving ? FMath::Max(MovingBloomScale, 0.0f) : 1.0f);
	}

	/** Belt in the gun and rounds in the boxes */
	struct FBelt
	{
		int32 Belt = 0;
		int32 Reserve = 0;

		int32 Total() const { return Belt + Reserve; }
	};

	/** At spawn: StartRounds (at most MaxRounds), the belt filled first */
	inline FBelt StartingBelt(int32 MaxRounds, int32 Capacity, int32 StartRounds)
	{
		FBelt State;
		const int32 Rounds = FMath::Clamp(StartRounds, 0, FMath::Max(MaxRounds, 0));
		State.Belt = FMath::Min(Rounds, FMath::Max(Capacity, 0));
		State.Reserve = Rounds - State.Belt;
		return State;
	}

	/** Rounds a reload puts on the belt: up to the capacity, from the reserve */
	inline int32 RoundsToLoad(const FBelt& State, int32 Capacity)
	{
		return FMath::Max(FMath::Min(FMath::Max(Capacity, 0) - State.Belt, State.Reserve), 0);
	}

	/** A reload is worth it: room on the belt and rounds in reserve */
	inline bool CanReload(const FBelt& State, int32 Capacity)
	{
		return RoundsToLoad(State, Capacity) > 0;
	}

	/** The new belt is in: the rounds left on the old one stay, the reserve tops it up; false when nothing went in */
	inline bool LoadBelt(FBelt& State, int32 Capacity)
	{
		const int32 Given = RoundsToLoad(State, Capacity);
		State.Belt += Given;
		State.Reserve -= Given;
		return Given > 0;
	}

	/** Gestures the trigger works through: the crank ones (and none). Any other one (draw, reload, overheat, inspect,
	 *  throw away) lets go of the trigger */
	inline bool AllowsFire(EFirearmGesture Gesture)
	{
		return Gesture == EFirearmGesture::None || Gesture == EFirearmGesture::FireStart || Gesture == EFirearmGesture::FireLoop
			|| Gesture == EFirearmGesture::FireEnd;
	}

	/** A reload may start over this gesture (a burst is cut by it; not over the draw, an overheat or another reload) */
	inline bool AllowsReload(EFirearmGesture Gesture)
	{
		return AllowsFire(Gesture) || Gesture == EFirearmGesture::Inspect;
	}

	/** Play rate of the crank loop so that one clip (a turn of the crank) lasts RoundsPerTurn rounds at RoundsPerSecond */
	inline float CrankClipRate(float ClipSeconds, float InRoundsPerSecond, int32 RoundsPerTurn)
	{
		const float TurnSeconds = static_cast<float>(FMath::Max(RoundsPerTurn, 1)) / FMath::Max(InRoundsPerSecond, 0.1f);
		return FMath::Clamp(FMath::Max(ClipSeconds, 0.01f) / TurnSeconds, 0.1f, 4.0f);
	}

	/** Play rate of a gesture stretched to last Seconds (0: its own length) */
	inline float StretchRate(float ClipSeconds, float Seconds)
	{
		return Seconds > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(ClipSeconds / Seconds, 0.1f, 4.0f) : 1.0f;
	}
}
