// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stealth/StealthTypes.h"

/** One look of an observer at a target, as UStealthSubsystem::CanSee gathers it */
struct FStealthSight
{
	/** Distance between the two (cm) */
	float DistanceCm = 0.0f;

	/** Sight range of the observer (cm); 0 or less: no limit */
	float RangeCm = 0.0f;

	/** Gap between the two bodies: the distance less both collision radii, never below 0 (cm) */
	float GapCm = 0.0f;

	/** The target is a player: only the players hide */
	bool bTargetIsPlayer = false;

	/** The target is crouched */
	bool bCrouched = false;

	/** It is night on the island (EDayPhase::Night) */
	bool bNight = false;

	/** The target is in a bush (looked up only when StealthMath::NeedsBushCheck says it matters) */
	bool bInBush = false;
};

/** A noise as UStealthSubsystem keeps it */
struct FStealthNoiseRecord
{
	FStealthNoise Noise;

	/** A player made it: only the noises of the players alert the mobs and the villagers */
	bool bFromPlayer = false;
};

/** Pure rules of the stealth (T-262), without a world: tested by monke_island.Stealth.* */
namespace StealthMath
{
	/** Gap between two bodies whose centres are Distance apart (cm), never below 0 */
	inline float BodyGap(float Distance, float RadiusA, float RadiusB)
	{
		return FMath::Max(0.0f, Distance - FMath::Max(RadiusA, 0.0f) - FMath::Max(RadiusB, 0.0f));
	}

	/** True within the range; a range of 0 or less has no limit */
	inline bool IsInRange(float DistanceCm, float RangeCm)
	{
		return RangeCm <= 0.0f || DistanceCm <= RangeCm;
	}

	/** True when the bodies touch or nearly (the contact that gives a hidden player away) */
	inline bool IsContact(float GapCm, float ContactGapCm)
	{
		return GapCm <= ContactGapCm;
	}

	/** True when the observer sees the target: in range, and a player crouched (in a bush: closer still, T-207) or at
	 *  night only at contact; any other target is seen in range as before (the mobs see each other) */
	inline bool CanSee(const FStealthSight& Sight, const FStealthSightRules& Rules)
	{
		if (!IsInRange(Sight.DistanceCm, Sight.RangeCm))
		{
			return false;
		}
		if (!Sight.bTargetIsPlayer)
		{
			return true;
		}
		if (Sight.bCrouched && Sight.bInBush)
		{
			return IsContact(Sight.GapCm, Rules.BushContactGapCm);
		}
		if ((Sight.bCrouched && Rules.bCrouchHides) || (Sight.bNight && Rules.bNightHidesStanding))
		{
			return IsContact(Sight.GapCm, Rules.ContactGapCm);
		}
		return true;
	}

	/** True when the answer of CanSee depends on the bush: only then is the bush grid looked up */
	inline bool NeedsBushCheck(const FStealthSight& Sight, const FStealthSightRules& Rules)
	{
		FStealthSight InBush = Sight;
		InBush.bInBush = true;
		FStealthSight OutOfBush = Sight;
		OutOfBush.bInBush = false;
		return CanSee(InBush, Rules) != CanSee(OutOfBush, Rules);
	}

	/** True when a noise of RadiusCm made DistanceCm away is heard by a listener of this hearing scale */
	inline bool CanHear(float DistanceCm, float RadiusCm, float HearingScale)
	{
		return HearingScale > 0.0f && RadiusCm > 0.0f && DistanceCm <= RadiusCm * HearingScale;
	}

	/** True while a noise made at NoiseTime is still audible at Now */
	inline bool IsFresh(double NoiseTime, double Now, float WindowSeconds)
	{
		return Now - NoiseTime <= WindowSeconds;
	}

	/** True once a clue (seen there, heard there) at ClueTime is forgotten at Now */
	inline bool IsForgotten(double ClueTime, double Now, float MemorySeconds)
	{
		return Now - ClueTime > MemorySeconds;
	}

	/** Radius a noise is kept with: the asked one clamped to [0, MaxRadiusCm]; 0 for a value that is not a number */
	inline float ClampNoiseRadius(float RadiusCm, float MaxRadiusCm)
	{
		return FMath::IsFinite(RadiusCm) ? FMath::Clamp(RadiusCm, 0.0f, FMath::Max(MaxRadiusCm, 0.0f)) : 0.0f;
	}

	/** Index of the noise a listener at Listener hears: a noise of a player (never one of its own), still fresh, within
	 *  its radius times the hearing scale; the newest, the nearest of the same age; INDEX_NONE when none */
	inline int32 PickHeardNoise(TConstArrayView<FStealthNoiseRecord> Noises, const FVector& Listener, const AActor* ListenerActor, float HearingScale,
		double Now, float WindowSeconds)
	{
		int32 Best = INDEX_NONE;
		double BestTime = 0.0;
		double BestDistSq = 0.0;
		for (int32 Index = 0; Index < Noises.Num(); ++Index)
		{
			const FStealthNoiseRecord& Record = Noises[Index];
			if (!Record.bFromPlayer || !IsFresh(Record.Noise.Time, Now, WindowSeconds))
			{
				continue;
			}
			if (ListenerActor && Record.Noise.Instigator.Get() == ListenerActor)
			{
				continue;
			}
			const double DistSq = FVector::DistSquared(Listener, Record.Noise.Location);
			if (!CanHear(static_cast<float>(FMath::Sqrt(DistSq)), Record.Noise.RadiusCm, HearingScale))
			{
				continue;
			}
			if (Best == INDEX_NONE || Record.Noise.Time > BestTime || (Record.Noise.Time == BestTime && DistSq < BestDistSq))
			{
				Best = Index;
				BestTime = Record.Noise.Time;
				BestDistSq = DistSq;
			}
		}
		return Best;
	}

	/** Exposure of a player for its HUD: running is heard, crouched (or standing at night) is unseen */
	inline EStealthExposure Exposure(bool bCrouched, bool bNight, bool bRunning, const FStealthSightRules& Rules)
	{
		if (bRunning)
		{
			return EStealthExposure::Noisy;
		}
		if ((bCrouched && Rules.bCrouchHides) || (bNight && Rules.bNightHidesStanding))
		{
			return EStealthExposure::Hidden;
		}
		return EStealthExposure::Visible;
	}
}

/**
 *  What a mob or a villager looks for (server, T-262): a foe it lost from sight, or a noise it heard. It goes to the
 *  place (running after a foe it hunted, walking to a noise), looks around there, and forgets NoiseMemorySeconds after
 *  the last clue. Plain data with pure rules, kept by each creature.
 */
struct FStealthLead
{
	/** Who it was, when known (the player whose noise it heard, the foe it lost) */
	TWeakObjectPtr<AActor> Suspect;

	/** Where to look */
	FVector Location = FVector::ZeroVector;

	/** World time of the last clue: seen there, or heard there (s) */
	double ClueTime = -1.0e9;

	/** Time of the newest noise taken: each noise is taken once (kept by Clear) */
	double NoiseTime = -1.0e9;

	/** It hunted that foe (it runs there) rather than heard something (it walks there) */
	bool bHunting = false;

	/** It reached the place and looks around */
	bool bArrived = false;

	/** There is something to look for */
	bool bActive = false;

	/** True while there is something to look for and it is not forgotten yet */
	bool IsActive(double Now, float MemorySeconds) const
	{
		return bActive && !StealthMath::IsForgotten(ClueTime, Now, MemorySeconds);
	}

	/** Nothing to look for any more (the noises already taken stay taken) */
	void Clear()
	{
		const double Taken = NoiseTime;
		*this = FStealthLead();
		NoiseTime = Taken;
	}

	/** A hunted foe went out of sight at Where: it runs there */
	void Hunt(AActor* InSuspect, const FVector& Where, double Now)
	{
		Suspect = InSuspect;
		Location = Where;
		ClueTime = Now;
		bHunting = true;
		bArrived = false;
		bActive = true;
	}

	/** A noise heard: it goes there (a hunt stays a hunt); false when this noise or a newer one was taken already */
	bool TakeNoise(const FStealthNoise& Noise)
	{
		if (Noise.Time <= NoiseTime)
		{
			return false;
		}
		NoiseTime = Noise.Time;
		bHunting = bActive && bHunting;
		Suspect = Noise.Instigator;
		Location = Noise.Location;
		ClueTime = Noise.Time;
		bArrived = false;
		bActive = true;
		return true;
	}
};
