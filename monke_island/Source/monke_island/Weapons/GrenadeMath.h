// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the bean's grenade (owner 2026-10-09), shared by the weapon, the projectile and the tests (no world) */
namespace GrenadeMath
{
	/** The grenade bag: one grenade in hand or not, the rest in the bag */
	struct FGrenadeState
	{
		bool bInHand = false;
		int32 Bag = 0;

		int32 Total() const { return Bag + (bInHand ? 1 : 0); }
	};

	/** What a press on fire does */
	enum class ETrigger : uint8
	{
		/** Busy: nothing */
		None,
		/** A grenade in hand: it is thrown */
		Throw,
		/** Empty hand with grenades in the bag: the next one is taken (Reload) */
		TakeNext,
		/** Empty hand, empty bag: nothing to throw (DryFire) */
		DryFire
	};

	/** At spawn: MaxGrenades at most, Start of them, one in hand when asked */
	inline FGrenadeState Starting(int32 MaxGrenades, int32 Start, bool bStartInHand)
	{
		FGrenadeState State;
		const int32 Count = FMath::Clamp(Start, 0, FMath::Max(MaxGrenades, 0));
		State.bInHand = bStartInHand && Count > 0;
		State.Bag = Count - (State.bInHand ? 1 : 0);
		return State;
	}

	/** Press on fire (bAutoTakeNext: an empty hand takes the next grenade on the press) */
	inline ETrigger OnTrigger(const FGrenadeState& State, bool bBusy, bool bAutoTakeNext)
	{
		if (bBusy)
		{
			return ETrigger::None;
		}
		if (State.bInHand)
		{
			return ETrigger::Throw;
		}
		return State.Bag > 0 && bAutoTakeNext ? ETrigger::TakeNext : ETrigger::DryFire;
	}

	/** Press on reload: an empty hand takes the next grenade when the bag has one */
	inline ETrigger OnReloadKey(const FGrenadeState& State, bool bBusy)
	{
		return !bBusy && !State.bInHand && State.Bag > 0 ? ETrigger::TakeNext : ETrigger::None;
	}

	/** Throw: the hand is empty */
	inline void Throw(FGrenadeState& State)
	{
		State.bInHand = false;
	}

	/** Next grenade taken from the bag into the hand; false when the hand is full or the bag empty */
	inline bool TakeNext(FGrenadeState& State)
	{
		if (State.bInHand || State.Bag <= 0)
		{
			return false;
		}
		--State.Bag;
		State.bInHand = true;
		return true;
	}

	/** Damage at Distance from the blast: CenterDamage up to FullRadius, down to 0 at BlastRadius (linear), 0 beyond */
	inline float DamageAtDistance(float CenterDamage, float Distance, float FullRadius, float BlastRadius)
	{
		if (CenterDamage <= 0.0f || Distance >= BlastRadius)
		{
			return 0.0f;
		}
		if (Distance <= FullRadius || BlastRadius <= FullRadius)
		{
			return CenterDamage;
		}
		return CenterDamage * (1.0f - (Distance - FullRadius) / (BlastRadius - FullRadius));
	}

	/** Velocity of the throw: Speed along the aim lifted by PitchUpDegrees (a lob), the aim kept level otherwise */
	inline FVector ThrowVelocity(const FVector& Aim, float Speed, float PitchUpDegrees)
	{
		FRotator Direction = Aim.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector).Rotation();
		Direction.Pitch = FMath::Clamp(Direction.Pitch + PitchUpDegrees, -89.0f, 89.0f);
		return Direction.Vector() * FMath::Max(Speed, 0.0f);
	}

	/** Fuse left when the grenade leaves the hand: FuseSeconds less the time since it was lit, never under MinAfterThrow
	 *  (a grenade held too long still flies a little before it goes off) */
	inline float FuseLeft(float FuseSeconds, float LitSince, float MinAfterThrow)
	{
		const float Since = FMath::Max(LitSince, 0.0f);
		return FMath::Max(FuseSeconds - Since, FMath::Max(MinAfterThrow, 0.0f));
	}

	/** Velocity a player caught in the blast is knocked out at: away from the blast, half as much up */
	inline FVector KnockoutVelocity(const FVector& FromBlast, float Speed)
	{
		const FVector Away = FromBlast.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
		return Away * Speed + FVector::UpVector * Speed * 0.5f;
	}
}
