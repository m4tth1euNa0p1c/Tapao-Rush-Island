// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"

/**
 *  Pure rules of the bolt-action marksman rifle (T-335), shared by ABoltRifleWeapon and the tests
 *  (monke_island.Weapons.BoltRifle): rounds in the rifle (magazine and chamber), a round chambered only once the bolt
 *  closed on it, the stripper clip reload from the pouch, the spread of the scope. No world, no time source.
 */
namespace BoltRifleMath
{
	/** The rifle: rounds in it (the chambered one included), a live round under a closed bolt, rounds left in the pouch */
	struct FState
	{
		int32 Loaded = 0;
		bool bChambered = false;
		int32 Reserve = 0;

		int32 Total() const { return Loaded + Reserve; }
	};

	/** What a press on fire does */
	enum class ETrigger : uint8
	{
		/** A gesture plays that the press may not cut: nothing */
		None,
		/** A round is chambered: the shot leaves */
		Fire,
		/** Rounds in the rifle, none chambered (the case of the last shot still in): the bolt is worked */
		CycleBolt,
		/** Empty, rounds in the pouch: the stripper clip */
		Reload,
		/** Empty, pouch empty: the striker falls on nothing */
		DryFire
	};

	/** At spawn: MaxRounds in all, StartLoaded of them in the rifle (at most Capacity), the first one chambered */
	inline FState StartingRifle(int32 MaxRounds, int32 Capacity, int32 StartLoaded)
	{
		FState State;
		const int32 Max = FMath::Max(MaxRounds, 0);
		State.Loaded = FMath::Clamp(StartLoaded, 0, FMath::Min(FMath::Max(Capacity, 0), Max));
		State.bChambered = State.Loaded > 0;
		State.Reserve = Max - State.Loaded;
		return State;
	}

	/** Press on fire (bBusy: a gesture plays that the press may not cut; bAutoReload: an empty rifle reloads) */
	inline ETrigger OnTrigger(const FState& State, bool bBusy, bool bAutoReload)
	{
		if (bBusy)
		{
			return ETrigger::None;
		}
		if (State.bChambered && State.Loaded > 0)
		{
			return ETrigger::Fire;
		}
		if (State.Loaded > 0)
		{
			return ETrigger::CycleBolt;
		}
		return State.Reserve > 0 && bAutoReload ? ETrigger::Reload : ETrigger::DryFire;
	}

	/** Shot: the chambered round leaves, its case stays in until the bolt is worked */
	inline void Fire(FState& State)
	{
		State.Loaded = FMath::Max(State.Loaded - 1, 0);
		State.bChambered = false;
	}

	/** The bolt closes (BoltClose): it pushes the next round of the magazine into the chamber; false on an empty rifle */
	inline bool CloseBolt(FState& State)
	{
		State.bChambered = State.Loaded > 0;
		return State.bChambered;
	}

	/** Rounds a stripper clip reload puts in the rifle: up to Capacity, from the pouch */
	inline int32 RoundsToLoad(const FState& State, int32 Capacity)
	{
		return FMath::Max(FMath::Min(FMath::Max(Capacity, 0) - State.Loaded, State.Reserve), 0);
	}

	/** A reload is worth it: room in the rifle and rounds in the pouch */
	inline bool CanReload(const FState& State, int32 Capacity)
	{
		return RoundsToLoad(State, Capacity) > 0;
	}

	/** The clip is pressed in (MagIn): its rounds go from the pouch into the rifle; false when nothing went in */
	inline bool LoadClip(FState& State, int32 Capacity)
	{
		const int32 Given = RoundsToLoad(State, Capacity);
		State.Loaded += Given;
		State.Reserve -= Given;
		return Given > 0;
	}

	/** What follows a shot once its clip played: the bolt worked while rounds are left (bAutoCycle), the reload once the
	 *  rifle is empty and the pouch is not (bAutoReload), else nothing (the next press does it) */
	inline EFirearmGesture AfterShot(const FState& State, bool bAutoCycle, bool bAutoReload)
	{
		if (State.Loaded > 0)
		{
			return bAutoCycle && !State.bChambered ? EFirearmGesture::BoltCycle : EFirearmGesture::None;
		}
		return bAutoReload && State.Reserve > 0 ? EFirearmGesture::Reload : EFirearmGesture::None;
	}

	/** A press on fire may cut the gesture playing and shoot at once: the end of the bolt cycle or of the reload, once the
	 *  bolt closed on a round. Never before BoltClose */
	inline bool CanCutForShot(EFirearmGesture Playing, const FState& State)
	{
		return (Playing == EFirearmGesture::BoltCycle || Playing == EFirearmGesture::Reload) && State.bChambered && State.Loaded > 0;
	}

	/** Spread of a shot (deg): through the scope and steady the tightest, steady without the scope wider, on the move
	 *  the widest (the scope does not steady a running carrier) */
	inline float Spread(bool bScoped, bool bMoving, float ScopedSpread, float NoScopeSpread, float MovingSpread)
	{
		if (bMoving)
		{
			return FMath::Max(MovingSpread, 0.0f);
		}
		return FMath::Max(bScoped ? ScopedSpread : NoScopeSpread, 0.0f);
	}

	/** Play rate of a gesture stretched to last Seconds (0: its own length) */
	inline float StretchRate(float ClipSeconds, float Seconds)
	{
		return Seconds > UE_KINDA_SMALL_NUMBER && ClipSeconds > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(ClipSeconds / Seconds, 0.1f, 4.0f) : 1.0f;
	}
}
