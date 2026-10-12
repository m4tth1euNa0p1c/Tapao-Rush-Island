// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Fort/FortSoldierRules.h"

/** Pure rules of the GoreBox deaths (owner 2026-10-08) and wounds (limbs torn off, owner 2026-10-09) of the French
 *  soldiers, tested without a world (monke_island.Fort.SoldierGore) */
namespace FortSoldierGore
{
	/** Clips the rules hold, for the choice of a death */
	struct FDeathClips
	{
		bool bHeadshot = false;
		bool bDecapitated = false;
		bool bExplode = false;
		/** DeathBisected and LeglessCrawlDeath (2026-10-09) */
		bool bBisected = false;
		bool bCrawlDeath = false;
	};

	/**
	 *  The death a lethal blow causes: a shot from at most ExplodeRange (cm, 0 = never) bursts the body whatever it
	 *  struck; a blade in the head takes it off; a shot in the head takes it off when the decapitation is drawn, else
	 *  it is the headshot death; everything else is the plain death. A death whose clip is missing falls back on the
	 *  next one (decapitated, then headshot, then plain).
	 */
	inline EFortSoldierDeath DeathOf(bool bHead, bool bShot, float ShotDistance, float ExplodeRange, bool bDecapitationDrawn, const FDeathClips& Clips)
	{
		if (bShot && Clips.bExplode && ExplodeRange > 0.0f && ShotDistance > 0.0f && ShotDistance <= ExplodeRange)
		{
			return EFortSoldierDeath::Exploded;
		}
		if (!bHead)
		{
			return EFortSoldierDeath::Plain;
		}
		const bool bTakesHead = Clips.bDecapitated && (!bShot || bDecapitationDrawn);
		if (bTakesHead)
		{
			return EFortSoldierDeath::Decapitated;
		}
		return bShot && Clips.bHeadshot ? EFortSoldierDeath::Headshot : EFortSoldierDeath::Plain;
	}

	// ------------------------------------------------------------------ limbs (2026-10-09)

	/** Limbs a soldier can lose: the order of its limb damage (an arm and a leg of each side) */
	enum ELimb : int32
	{
		ArmL = 0,
		ArmR = 1,
		LegL = 2,
		LegR = 3,
		LimbCount = 4
	};

	/** Bits of FFortSoldierActionRep::LostLimbs: 1 << ELimb */
	enum ELimbBit : uint8
	{
		ArmLBit = 1,
		ArmRBit = 2,
		LegLBit = 4,
		LegRBit = 8,
		ArmBits = ArmLBit | ArmRBit,
		LegBits = LegLBit | LegRBit
	};

	/** Limb of a zone of the body (HitZones::EZone: 2 ArmL, 3 ArmR, 4 LegL, 5 LegR), INDEX_NONE for the head, the torso
	 *  and an unknown bone */
	inline int32 LimbOfZone(uint8 Zone)
	{
		switch (Zone)
		{
		case 2: return ArmL;
		case 3: return ArmR;
		case 4: return LegL;
		case 5: return LegR;
		default: return INDEX_NONE;
		}
	}

	/** Bit of a limb in the lost mask, 0 for no limb */
	inline uint8 BitOfLimb(int32 Limb)
	{
		return Limb >= 0 && Limb < LimbCount ? static_cast<uint8>(1 << Limb) : 0;
	}

	inline bool IsArm(int32 Limb)
	{
		return Limb == ArmL || Limb == ArmR;
	}

	inline bool IsLeft(int32 Limb)
	{
		return Limb == ArmL || Limb == LegL;
	}

	/** Health of a limb (the rules: LimbHealthArm, LimbHealthLeg) */
	inline float LimbHealth(int32 Limb, float ArmHealth, float LegHealth)
	{
		return IsArm(Limb) ? ArmHealth : LegHealth;
	}

	/** What a soldier does with the limbs it lost */
	enum class EStance : uint8
	{
		/** Whole */
		Standing,
		/** Without its left arm: the musket dropped, it fights on at the sabre with the right one (ArmlessFightOn) */
		OneArm,
		/** Without its right arm, or both: nothing to hold, it runs from its foes in a panic (ArmlessPanicLoop, FleeLoop) */
		Panic,
		/** Without a leg: on the ground, it drags itself (LeglessCrawlLoop), whatever its arms */
		Crawl
	};

	/** Stance of a soldier from its lost limbs (ELimbBit mask): a leg lost beats the arms (it cannot run) */
	inline EStance StanceOf(uint8 Lost)
	{
		if (Lost & LegBits)
		{
			return EStance::Crawl;
		}
		if (Lost & ArmRBit)
		{
			return EStance::Panic;
		}
		if (Lost & ArmLBit)
		{
			return EStance::OneArm;
		}
		return EStance::Standing;
	}

	/** Whether the body lies on the ground in the crawl stance (low capsule, mesh at its feet; T-332): a living soldier
	 *  without a leg; a dead one keeps the stance it died in (a leg torn off by the very blow that kills a standing
	 *  soldier leaves it standing: its movement stops at its death, and a crawl stance then hung the body 25 cm over the
	 *  ground) */
	inline bool IsCrawlStance(uint8 Lost, bool bDead, bool bDiedCrawling)
	{
		return bDead ? bDiedCrawling : StanceOf(Lost) == EStance::Crawl;
	}

	/** Height of the mesh in the capsule for a stance (cm): the standing height raised by what the capsule lost under
	 *  its centre, plus Extra (CrawlMeshOffset), so that the feet stay at the bottom of the capsule */
	inline double StanceMeshZ(double StandingMeshZ, float StandingHalfHeight, float HalfHeight, float Extra)
	{
		return StandingMeshZ + (static_cast<double>(StandingHalfHeight) - HalfHeight) + Extra;
	}

	/** The musket leaves with the first arm lost (one hand cannot hold it): no shot, no bayonet */
	inline bool MusketGone(uint8 Lost)
	{
		return (Lost & ArmBits) != 0;
	}

	/** The sabre leaves with the right arm when it was in that hand; sheathed, it stays on the hip */
	inline bool SabreGone(uint8 Lost, bool bSabreInHand)
	{
		return bSabreInHand && (Lost & ArmRBit) != 0;
	}

	/** What is known of a blow, for the death it would cause */
	struct FBlowFacts
	{
		/** Zone of the body it struck (HitZones) */
		bool bHead = false;
		bool bTorso = false;
		/** A gun shot, from how far (cm, 0 unknown) */
		bool bShot = false;
		float ShotDistance = 0.0f;
		/** A blade: the player's cutlass when its move cuts, the sabre of another soldier */
		bool bBlade = false;
		/** The soldier crawls already (a leg lost before this blow) */
		bool bCrawling = false;
		/** Draws of the rules (DecapitationChance, BisectChance) */
		bool bDecapitationDrawn = false;
		bool bBisectDrawn = false;
	};

	/**
	 *  The death a lethal blow causes to a soldier, wounded or not (2026-10-09): a shot from point blank bursts the
	 *  body whatever its state; a crawling soldier dies its crawl death; a blade in the torso cuts the body in two
	 *  when the bisection is drawn; everything else is DeathOf. A missing clip falls back on DeathOf.
	 */
	inline EFortSoldierDeath DeathOfBlow(const FBlowFacts& Blow, float ExplodeRange, const FDeathClips& Clips)
	{
		const EFortSoldierDeath Standing = DeathOf(Blow.bHead, Blow.bShot, Blow.ShotDistance, ExplodeRange, Blow.bDecapitationDrawn, Clips);
		if (Standing == EFortSoldierDeath::Exploded)
		{
			return Standing;
		}
		if (Blow.bCrawling && Clips.bCrawlDeath)
		{
			return EFortSoldierDeath::CrawlDeath;
		}
		if (Blow.bBlade && Blow.bTorso && Blow.bBisectDrawn && Clips.bBisected)
		{
			return EFortSoldierDeath::Bisected;
		}
		return Standing;
	}
}
