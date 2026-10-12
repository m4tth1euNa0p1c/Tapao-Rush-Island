// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the pirate pistol and shotgun, shared by the weapons and the tests (no world) */
namespace FirearmMath
{
	/** Spread of a shot (deg): the aim spread when aiming or standing steady, the hip spread on the move (no aiming
	 *  with the firearms, owner 2026-10-09: the stance makes the accuracy, like Counter-Strike) */
	inline float SpreadForStance(bool bAiming, bool bMoving, float AimSpread, float HipSpread)
	{
		return bAiming || !bMoving ? AimSpread : HipSpread;
	}

	/**
	 *  The weapon is aimed (network review R-38): its carrier wants it (right click held), the rules allow aiming, the
	 *  weapon is usable and may be aimed now (not thrown away, no gesture that lowers the aim: a reload, the inspection).
	 *  The server and the carrier both apply it to the same replicated gesture: the aim drops on both during a reload
	 *  and comes back on both while the click is still held
	 */
	inline bool IsAimed(bool bWanted, bool bAllowedByRules, bool bUsable, bool bMayAimNow)
	{
		return bWanted && bAllowedByRules && bUsable && bMayAimNow;
	}

	/** Damage of a projectile at Distance: full up to FalloffStart, then down to MinScale at Range (linear) */
	inline float DamageAtDistance(float Damage, float Distance, float FalloffStart, float Range, float MinScale)
	{
		if (FalloffStart <= 0.0f || Distance <= FalloffStart || Range <= FalloffStart)
		{
			return Damage;
		}
		const float Alpha = FMath::Clamp((Distance - FalloffStart) / (Range - FalloffStart), 0.0f, 1.0f);
		return Damage * FMath::Lerp(1.0f, FMath::Clamp(MinScale, 0.0f, 1.0f), Alpha);
	}

	/** Directions of Count projectiles inside a cone of HalfAngleDegrees around Aim (the first one too: no perfect pellet) */
	inline void SpreadDirections(const FVector& Aim, int32 Count, float HalfAngleDegrees, FRandomStream& Stream, TArray<FVector>& Out)
	{
		Out.Reset(FMath::Max(Count, 0));
		const FVector Axis = Aim.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const float HalfAngle = FMath::DegreesToRadians(FMath::Max(HalfAngleDegrees, 0.0f));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Out.Add(HalfAngle > 0.0f ? Stream.VRandCone(Axis, HalfAngle) : Axis);
		}
	}

	// ---------------------------------------------------------------- empty weapon (owner 2026-10-09)

	/** A press on fire with nothing loaded and nothing in the pouch throws the weapon away (bDiscardWhenEmpty), unless a
	 *  gesture is already playing */
	inline bool ShouldDiscard(int32 Loaded, int32 Reserve, bool bBusy, bool bDiscardWhenEmpty)
	{
		return bDiscardWhenEmpty && !bBusy && Loaded <= 0 && Reserve <= 0;
	}

	/** Velocity of the thrown away weapon: Speed along the carrier's right, PitchDegrees below the horizontal */
	inline FVector DiscardToss(const FVector& Right, float Speed, float PitchDegrees)
	{
		const float Pitch = FMath::DegreesToRadians(FMath::Clamp(PitchDegrees, 0.0f, 89.0f));
		return (Right.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::RightVector) * FMath::Cos(Pitch) - FVector::UpVector * FMath::Sin(Pitch)) * FMath::Max(Speed, 0.0f);
	}

	// ---------------------------------------------------------------- pistol

	/** The pistol: charges in its barrels (one, or two for the double pistol), the hammer cocked or down, charges left in
	 *  the pouch */
	struct FPistolState
	{
		/** Charges loaded (0 to Barrels) */
		int32 Loaded = 0;
		bool bCocked = false;
		int32 Reserve = 0;
		/** Barrels of the pistol (UPistolRules::Barrels: 1, 2 for the double pistol) */
		int32 Barrels = 1;

		bool IsLoaded() const { return Loaded > 0; }
		int32 Total() const { return Reserve + Loaded; }
	};

	/** What a press on fire does */
	enum class EPistolTrigger : uint8
	{
		/** Busy: nothing */
		None,
		/** Loaded and cocked: the shot leaves */
		Fire,
		/** Cocked, empty, nothing to load: the hammer falls on nothing */
		DryFire,
		/** Hammer down: cock it first */
		Cock,
		/** Empty with a charge left: reload (which cocks too) */
		Reload
	};

	/** At spawn: MaxCharges in all, the barrels full when the pistol starts loaded */
	inline FPistolState StartingPistol(int32 MaxCharges, bool bStartLoaded, bool bStartCocked, int32 Barrels = 1)
	{
		FPistolState State;
		const int32 Max = FMath::Max(MaxCharges, 0);
		State.Barrels = FMath::Max(Barrels, 1);
		State.Loaded = bStartLoaded ? FMath::Min(State.Barrels, Max) : 0;
		State.bCocked = bStartCocked;
		State.Reserve = Max - State.Loaded;
		return State;
	}

	/** Press on fire (bBusy: another gesture is playing; bAutoCock: a loaded pistol with the hammer down is cocked and
	 *  fires in the same press). A double pistol fires its second barrel on the next press, no reload between */
	inline EPistolTrigger OnTrigger(const FPistolState& State, bool bBusy, bool bAutoReload, bool bAutoCock = false)
	{
		if (bBusy)
		{
			return EPistolTrigger::None;
		}
		if (!State.IsLoaded() && State.Reserve > 0 && bAutoReload)
		{
			return EPistolTrigger::Reload;
		}
		if (!State.bCocked)
		{
			return bAutoCock && State.IsLoaded() ? EPistolTrigger::Fire : EPistolTrigger::Cock;
		}
		return State.IsLoaded() ? EPistolTrigger::Fire : EPistolTrigger::DryFire;
	}

	/** Press on reload: a barrel to fill with a charge left reloads (the double pistol tops up both), loaded with the
	 *  hammer down cocks, else nothing */
	inline EPistolTrigger OnReloadKey(const FPistolState& State, bool bBusy)
	{
		if (bBusy)
		{
			return EPistolTrigger::None;
		}
		if (State.Loaded < State.Barrels && State.Reserve > 0)
		{
			return EPistolTrigger::Reload;
		}
		return State.IsLoaded() && !State.bCocked ? EPistolTrigger::Cock : EPistolTrigger::None;
	}

	/** Shot: one barrel empties, the hammer falls; returns the barrel fired (0 first, 1 the second of a double pistol:
	 *  its muzzle socket) */
	inline int32 Fire(FPistolState& State)
	{
		const int32 Barrel = FMath::Clamp(State.Barrels - State.Loaded, 0, FMath::Max(State.Barrels - 1, 0));
		State.Loaded = FMath::Max(State.Loaded - 1, 0);
		State.bCocked = false;
		return Barrel;
	}

	/** Dry fire: the hammer falls */
	inline void DryFire(FPistolState& State)
	{
		State.bCocked = false;
	}

	/** Cock: the hammer is up */
	inline void Cock(FPistolState& State)
	{
		State.bCocked = true;
	}

	/** Reload done: the empty barrels take a charge each from the pouch (as many as it holds), the hammer cocked; false
	 *  when nothing could be loaded */
	inline bool CompleteReload(FPistolState& State)
	{
		const int32 Given = FMath::Min(State.Barrels - State.Loaded, State.Reserve);
		if (Given <= 0)
		{
			return false;
		}
		State.Reserve -= Given;
		State.Loaded += Given;
		State.bCocked = true;
		return true;
	}

	// ---------------------------------------------------------------- shotgun

	/** The shotgun: shells in the gun (tube and chamber) and in the pouch */
	struct FShotgunState
	{
		int32 Rounds = 0;
		int32 Reserve = 0;

		int32 Total() const { return Rounds + Reserve; }
	};

	/** Step of the shell by shell reload */
	enum class EReloadPhase : uint8
	{
		None,
		/** ReloadStart: the loading port comes up */
		Start,
		/** ReloadInsert: one shell */
		Insert,
		/** ReloadEnd: back to the ready pose */
		End,
		/** ReloadEmpty: three shells then the pump, not interrupted */
		Empty
	};

	/** At spawn: MaxShells in all, StartLoaded of them in the gun (at most Capacity) */
	inline FShotgunState StartingShotgun(int32 MaxShells, int32 Capacity, int32 StartLoaded)
	{
		FShotgunState State;
		const int32 Max = FMath::Max(MaxShells, 0);
		State.Rounds = FMath::Clamp(StartLoaded, 0, FMath::Min(FMath::Max(Capacity, 0), Max));
		State.Reserve = Max - State.Rounds;
		return State;
	}

	/** Shells the reload can still put in */
	inline int32 ShellsToLoad(const FShotgunState& State, int32 Capacity)
	{
		return FMath::Max(FMath::Min(Capacity - State.Rounds, State.Reserve), 0);
	}

	/** A shot needs a shell in the gun and an idle gun */
	inline bool CanFire(const FShotgunState& State, bool bBusy)
	{
		return State.Rounds > 0 && !bBusy;
	}

	/** Shot: one shell is spent (the pump of the shot clip chambers the next one) */
	inline void Fire(FShotgunState& State)
	{
		State.Rounds = FMath::Max(State.Rounds - 1, 0);
	}

	/** First step of a reload: None when full or nothing to load, Empty for an empty gun, else Start. MinEmptyShells
	 *  (T-335): ReloadEmpty shows that many shells going in; an empty gun with fewer to load takes them one by one (Start,
	 *  Insert x N, End) rather than showing shells it does not have (0: always Empty on an empty gun) */
	inline EReloadPhase FirstReloadPhase(const FShotgunState& State, int32 Capacity, int32 MinEmptyShells = 0)
	{
		const int32 ToLoad = ShellsToLoad(State, Capacity);
		if (ToLoad <= 0)
		{
			return EReloadPhase::None;
		}
		return State.Rounds <= 0 && ToLoad >= MinEmptyShells ? EReloadPhase::Empty : EReloadPhase::Start;
	}

	/** One shell from the pouch into the gun; false when full or the pouch is empty */
	inline bool InsertShell(FShotgunState& State, int32 Capacity)
	{
		if (ShellsToLoad(State, Capacity) <= 0)
		{
			return false;
		}
		++State.Rounds;
		--State.Reserve;
		return true;
	}

	/** Step after Phase has played: inserts go on until full, out of shells or asked to stop, then End, then None.
	 *  bTopUpAfterEmpty (T-335): ReloadEmpty (three shells) leaves a gun that holds more with room and shells: the shell by
	 *  shell reload fills it up (Start, Insert, End) instead of ending there */
	inline EReloadPhase NextReloadPhase(EReloadPhase Phase, const FShotgunState& State, int32 Capacity, bool bStopRequested,
		bool bTopUpAfterEmpty = false)
	{
		switch (Phase)
		{
		case EReloadPhase::Start:
		case EReloadPhase::Insert:
			return (bStopRequested || ShellsToLoad(State, Capacity) <= 0) ? EReloadPhase::End : EReloadPhase::Insert;
		case EReloadPhase::Empty:
			return bTopUpAfterEmpty && !bStopRequested && ShellsToLoad(State, Capacity) > 0 ? EReloadPhase::Start : EReloadPhase::None;
		default:
			return EReloadPhase::None;
		}
	}

	/** A press on fire during the reload cuts it and fires at once: only with a shell in the gun, not during ReloadEmpty */
	inline bool CanInterruptReloadForFire(EReloadPhase Phase, const FShotgunState& State)
	{
		return State.Rounds > 0 && (Phase == EReloadPhase::Start || Phase == EReloadPhase::Insert || Phase == EReloadPhase::End);
	}

	// ---- carry loops (idle, walk, run): steady arms and weapon while the speed or the run hesitates (T-330) ----

	/** Speed smoothed toward Speed over about SmoothingSeconds (exponential, frame rate independent) */
	inline float SmoothSpeed(float Smoothed, float Speed, float DeltaSeconds, float SmoothingSeconds)
	{
		if (SmoothingSeconds <= 0.0f || DeltaSeconds <= 0.0f)
		{
			return SmoothingSeconds <= 0.0f ? Speed : Smoothed;
		}
		return Smoothed + (Speed - Smoothed) * (1.0f - FMath::Exp(-DeltaSeconds / SmoothingSeconds));
	}

	/** The carrier moves for the carry loop: it starts walking above Threshold and stops under Threshold * ExitFactor
	 *  (a speed hovering at the threshold no longer flips the loops) */
	inline bool IsMovingForLoop(float SmoothedSpeed, float Threshold, float ExitFactor, bool bMovingLoopNow)
	{
		const float Limit = bMovingLoopNow ? Threshold * FMath::Clamp(ExitFactor, 0.0f, 1.0f) : Threshold;
		return SmoothedSpeed > Limit;
	}

	/** Another carry loop takes the place of the one playing once it was wanted for ConfirmSeconds and the one playing
	 *  played MinPlaySeconds */
	inline bool ShouldSwitchCarryLoop(double Now, double WantedSince, double PlayingSince, float ConfirmSeconds, float MinPlaySeconds)
	{
		return Now - WantedSince >= ConfirmSeconds && Now - PlayingSince >= MinPlaySeconds;
	}

	/** Time in a loop of NextDuration at the same phase as Time in a loop of Duration (walk <-> run: the step goes on) */
	inline float SamePhaseTime(float Time, float Duration, float NextDuration)
	{
		if (Duration <= 0.0f || NextDuration <= 0.0f)
		{
			return 0.0f;
		}
		const float Phase = FMath::Fmod(FMath::Max(Time, 0.0f), Duration) / Duration;
		return FMath::Clamp(Phase, 0.0f, 1.0f) * NextDuration;
	}
}
