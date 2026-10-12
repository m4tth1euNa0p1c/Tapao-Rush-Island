// monke_island

#pragma once

#include "CoreMinimal.h"
#include "TikiCombat.generated.h"

class UAnimSequenceBase;

/** Combat reaction clip of a creature struck by the bean (T-150): 8 clips per mob, 30 fps, contact at frame 12 */
UENUM(BlueprintType)
enum class ETikiCombatClip : uint8
{
	None,
	/** Musket shot taken, not lethal (the mob is already reacting) */
	MusketImpact,
	/** Lethal shot that bursts the body or takes a limb off */
	MusketBurst,
	/** Stagger then back on guard: the only reaction that does not kill */
	MusketStaggerRecover,
	/** Lethal shot, realistic fall; the held item leaves the hand at frame 27 */
	MusketArticulatedFall,
	/** Lethal horizontal cut, the blade going from the victim's left to its right */
	SabreLeftToRight,
	/** Lethal horizontal cut, the blade going from the victim's right to its left */
	SabreRightToLeft,
	/** Lethal vertical cut */
	SabreDown,
	/** Lethal cut at the legs */
	SabreLow,
	/** Lethal shot in the head (A_<Mob>_Death_Headshot, the GoreBox clips of the realistic mobs, owner 2026-10-08) */
	HeadshotDeath,
	/** Lethal blow that tore the head off (A_<Mob>_Death_Decapitated: it staggers, then collapses) */
	DecapitatedDeath,
	/** Death of a creature crawling without a leg, whatever the blow but a burst (A_<Mob>_Legless_Crawl_Death, Anims/Gore;
	 *  owner 2026-10-09) */
	LeglessCrawlDeath,
	/** Lethal blade at the waist, drawn by SabreBisectChance: it falls in two (A_<Mob>_Death_Bisected; the two halves
	 *  come with the corpse) */
	BisectedDeath
};

/** Clips of the combat reactions of a creature (A_<Mob>_<Clip>, /Game/MonkeIsland/Characters/<Mob>/Anims/Combat);
 *  any clip left empty falls back on the next rule, then on the behaviour before T-150 */
USTRUCT(BlueprintType)
struct FTikiCombatReactions
{
	GENERATED_BODY()

	/** Shot taken while already reacting (A_<Mob>_Musket_Impact, 90 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> MusketImpact;

	/** Lethal shot that bursts or dismembers (A_<Mob>_Musket_Burst, 90 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> MusketBurst;

	/** Stagger then back on guard, not lethal (A_<Mob>_Musket_StaggerRecover, 150 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> MusketStaggerRecover;

	/** Lethal realistic fall, the weapon dropped at frame 27 (A_<Mob>_Musket_ArticulatedFall, 150 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> MusketArticulatedFall;

	/** Lethal horizontal cut from the victim's left (A_<Mob>_Sabre_LeftToRight, 90 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> SabreLeftToRight;

	/** Lethal horizontal cut from the victim's right (A_<Mob>_Sabre_RightToLeft, 90 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> SabreRightToLeft;

	/** Lethal vertical cut (A_<Mob>_Sabre_Down, 90 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> SabreDown;

	/** Lethal cut at the legs (A_<Mob>_Sabre_Low, 90 frames) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> SabreLow;

	/** Lethal shot in the head (A_<Mob>_Death_Headshot, Anims/Gore) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> HeadshotDeath;

	/** Lethal blow that tore the head off (A_<Mob>_Death_Decapitated, Anims/Gore) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> DecapitatedDeath;

	/** Death of a creature crawling without a leg, whatever the lethal blow but a burst (A_<Mob>_Legless_Crawl_Death,
	 *  Anims/Gore; owner 2026-10-09) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> LeglessCrawlDeath;

	/** Lethal sabre blow in the torso drawn by SabreBisectChance: it falls in two (A_<Mob>_Death_Bisected, Anims/Gore;
	 *  the waist bleeds at the contact, the two halves come with the corpse) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CombatReactions")
	TObjectPtr<UAnimSequenceBase> BisectedDeath;

	/** Clip of a reaction, nullptr when none is assigned */
	UAnimSequenceBase* Get(ETikiCombatClip Clip) const
	{
		switch (Clip)
		{
		case ETikiCombatClip::MusketImpact: return MusketImpact;
		case ETikiCombatClip::MusketBurst: return MusketBurst;
		case ETikiCombatClip::MusketStaggerRecover: return MusketStaggerRecover;
		case ETikiCombatClip::MusketArticulatedFall: return MusketArticulatedFall;
		case ETikiCombatClip::SabreLeftToRight: return SabreLeftToRight;
		case ETikiCombatClip::SabreRightToLeft: return SabreRightToLeft;
		case ETikiCombatClip::SabreDown: return SabreDown;
		case ETikiCombatClip::SabreLow: return SabreLow;
		case ETikiCombatClip::HeadshotDeath: return HeadshotDeath;
		case ETikiCombatClip::DecapitatedDeath: return DecapitatedDeath;
		case ETikiCombatClip::LeglessCrawlDeath: return LeglessCrawlDeath;
		case ETikiCombatClip::BisectedDeath: return BisectedDeath;
		default: return nullptr;
		}
	}

	/** Bit (1 << clip) set for each assigned clip */
	uint32 AvailableMask() const
	{
		uint32 Mask = 0;
		for (uint8 Index = 1; Index <= static_cast<uint8>(ETikiCombatClip::BisectedDeath); ++Index)
		{
			if (Get(static_cast<ETikiCombatClip>(Index)))
			{
				Mask |= 1u << Index;
			}
		}
		return Mask;
	}
};

/** Reaction last started by the server, replicated: Serial changes at each reaction so the same clip restarts */
USTRUCT()
struct FTikiCombatReactionState
{
	GENERATED_BODY()

	/** Clip of the reaction */
	UPROPERTY()
	ETikiCombatClip Clip = ETikiCombatClip::None;

	/** Bumped by the server at each reaction */
	UPROPERTY()
	uint8 Serial = 0;

	/** Server world time the reaction started (s): a client joining later does not replay it */
	UPROPERTY()
	float StartServerTime = 0.0f;

	/** The mob is replaced by the baked demo of Clip (T-168): every machine spawns it locally */
	UPROPERTY()
	bool bShredDemo = false;

	/** Yaw of the demo (FRotator::CompressAxisToShort): the victim faces where the blow came from */
	UPROPERTY()
	uint16 DemoYaw = 0;
};

/** Pure rules of the combat reactions, tested without a world */
namespace TikiCombat
{
	/** Weapon of the bean that dealt a blow */
	enum class EWeapon : uint8
	{
		Other,
		Musket,
		Sabre,
		/** Pirate pistol (T-156): reacts like the musket */
		Pistol,
		/** Pirate shotgun (T-156): reacts like the musket, bursts at close range (FBlow::CloseBurstDistance) */
		Shotgun,
		/** A mob's spear or club picked up by the bean (T-161, UCutlassRules::bEdged off): it bleeds, but no sabre
		 *  reaction, demo nor cut: a plain hit, a plain death */
		Blunt
	};

	/** Weapon of a melee blow of the bean's cutlass class: the sabre, or a weapon without an edge (T-161) */
	inline EWeapon MeleeWeapon(bool bEdged)
	{
		return bEdged ? EWeapon::Sabre : EWeapon::Blunt;
	}

	/** A gun shot (musket, pistol, shotgun): the musket reactions */
	inline bool IsFirearm(EWeapon Weapon)
	{
		return Weapon == EWeapon::Musket || Weapon == EWeapon::Pistol || Weapon == EWeapon::Shotgun;
	}

	/** Stroke of a sabre blow, from the move of the cutlass */
	enum class EStroke : uint8
	{
		/** No reliable sense (thrust) */
		None,
		/** Horizontal: the blade sweeps along BladeSweep */
		Horizontal,
		/** From above (finisher) */
		Vertical
	};

	/** What the server knows of a blow */
	struct FBlow
	{
		EWeapon Weapon = EWeapon::Other;
		bool bLethal = false;
		/** The creature still plays a combat reaction */
		bool bInReaction = false;
		float Damage = 0.0f;
		/** Distance of the shot (cm), 0 when unknown */
		float Distance = 0.0f;
		/** A lethal shot from at most this distance bursts (cm, the shotgun's DA_ShotgunRules); 0 = Tuning only */
		float CloseBurstDistance = 0.0f;
		/** Uniform draw in [0, 1) for the weighted choice */
		float Roll = 1.0f;
		EStroke Stroke = EStroke::None;
		/** World direction the blade travels in, for a horizontal stroke */
		FVector BladeSweep = FVector::ZeroVector;
		/** Right of the victim in the world */
		FVector VictimRight = FVector::RightVector;
		/** Pitch of the attacker's aim (degrees, negative looking down) */
		float AimPitch = 0.0f;
		/** The blow struck the head (the bone of the shot, HitZones) */
		bool bHeadHit = false;
		/** The blow tore the head off (the limb health of the head, T-167) */
		bool bHeadTorn = false;
		/** The blow struck the torso (HitZones), or a blade whose bone is unknown (owner 2026-10-09) */
		bool bTorsoHit = false;
		/** The creature crawls already: a leg torn off before this blow (T-167) */
		bool bCrawling = false;
	};

	/** Tuning read from DA_TikiRules (CombatReactions) */
	struct FTuning
	{
		/** A shot dealing at least this damage bursts; 0 = never by damage */
		float BurstDamage = 0.0f;
		/** A shot from at most this distance bursts (cm); 0 = never by distance */
		float BurstDistance = 0.0f;
		/** Otherwise, chance a lethal shot bursts */
		float BurstChance = 0.0f;
		/** A sabre blow aimed lower than this below the horizon is a blow at the legs (degrees) */
		float LowAimPitch = 25.0f;
		/** The sides of the horizontal cuts are swapped (if the clips read the other way in game) */
		bool bMirrorSides = false;
		/** Chance a lethal sabre blow in the torso cuts the body in two (BisectedDeath); 0 = never */
		float BisectChance = 0.0f;
	};

	/** True for a clip after which the creature is dead */
	inline bool IsLethal(ETikiCombatClip Clip)
	{
		return Clip != ETikiCombatClip::None && Clip != ETikiCombatClip::MusketImpact && Clip != ETikiCombatClip::MusketStaggerRecover;
	}

	/** True for a clip of the sabre */
	inline bool IsSabre(ETikiCombatClip Clip)
	{
		return Clip >= ETikiCombatClip::SabreLeftToRight && Clip <= ETikiCombatClip::SabreLow;
	}

	inline bool Has(uint32 Mask, ETikiCombatClip Clip)
	{
		return Clip != ETikiCombatClip::None && (Mask & (1u << static_cast<uint8>(Clip))) != 0;
	}

	/** First of the wanted clips that is assigned, None when none is */
	inline ETikiCombatClip FirstAvailable(uint32 Mask, std::initializer_list<ETikiCombatClip> Wanted)
	{
		for (const ETikiCombatClip Clip : Wanted)
		{
			if (Has(Mask, Clip))
			{
				return Clip;
			}
		}
		return ETikiCombatClip::None;
	}

	/** Side clip of a horizontal cut: the blade going toward the victim's right came from its left. None when the
	 *  sweep is too much along the victim's front to tell (a sense one cannot trust) */
	inline ETikiCombatClip SabreSide(const FVector& BladeSweep, const FVector& VictimRight, bool bMirror)
	{
		const float Side = static_cast<float>(FVector::DotProduct(BladeSweep.GetSafeNormal2D(), VictimRight.GetSafeNormal2D()));
		if (FMath::Abs(Side) < 0.25f)
		{
			return ETikiCombatClip::None;
		}
		return (Side > 0.0f) != bMirror ? ETikiCombatClip::SabreLeftToRight : ETikiCombatClip::SabreRightToLeft;
	}

	/** Reaction to a blow of the bean, among the assigned clips (Mask, from FTikiCombatReactions::AvailableMask).
	 *  None keeps the behaviour without combat clips: hit clip, or ragdoll at death */
	inline ETikiCombatClip ChooseClip(const FBlow& Blow, const FTuning& Tuning, uint32 Mask)
	{
		using enum ETikiCombatClip;
		// the head torn off has its own death, by a shot or a blade (owner 2026-10-08); a body crawling without a leg
		// dies crawling instead, whatever the blow but a burst (owner 2026-10-09)
		if (Blow.bLethal && Blow.bHeadTorn && !Blow.bCrawling && Has(Mask, DecapitatedDeath))
		{
			return DecapitatedDeath;
		}
		if (IsFirearm(Blow.Weapon))
		{
			if (!Blow.bLethal)
			{
				return Blow.bInReaction ? FirstAvailable(Mask, { MusketImpact, MusketStaggerRecover })
					: FirstAvailable(Mask, { MusketStaggerRecover, MusketImpact });
			}
			const bool bStrong = (Tuning.BurstDamage > 0.0f && Blow.Damage >= Tuning.BurstDamage)
				|| (Tuning.BurstDistance > 0.0f && Blow.Distance > 0.0f && Blow.Distance <= Tuning.BurstDistance)
				|| (Blow.CloseBurstDistance > 0.0f && Blow.Distance > 0.0f && Blow.Distance <= Blow.CloseBurstDistance);
			// a strong or close shot bursts whatever the body; then the crawl death; then a shot in the head has its own
			if (bStrong)
			{
				return FirstAvailable(Mask, { MusketBurst, MusketArticulatedFall });
			}
			if (Blow.bCrawling && Has(Mask, LeglessCrawlDeath))
			{
				return LeglessCrawlDeath;
			}
			if (Blow.bHeadTorn && Has(Mask, DecapitatedDeath))
			{
				return DecapitatedDeath;
			}
			if (Blow.bHeadHit && Has(Mask, HeadshotDeath))
			{
				return HeadshotDeath;
			}
			const bool bBurst = Blow.Roll < Tuning.BurstChance;
			return bBurst ? FirstAvailable(Mask, { MusketBurst, MusketArticulatedFall })
				: FirstAvailable(Mask, { MusketArticulatedFall, MusketBurst });
		}
		// a crawling creature dies crawling of any other lethal blow
		if (Blow.bLethal && Blow.bCrawling && Has(Mask, LeglessCrawlDeath))
		{
			return LeglessCrawlDeath;
		}
		if (Blow.bLethal && Blow.bHeadTorn && Has(Mask, DecapitatedDeath))
		{
			return DecapitatedDeath;
		}
		if (Blow.Weapon == EWeapon::Sabre && Blow.bLethal)
		{
			// a blade at the waist cuts the body in two, one time in BisectChance (owner 2026-10-09)
			if (Blow.bTorsoHit && Blow.Roll < Tuning.BisectChance && Has(Mask, BisectedDeath))
			{
				return BisectedDeath;
			}
			ETikiCombatClip Wanted = SabreDown;
			if (Blow.AimPitch <= -Tuning.LowAimPitch)
			{
				Wanted = SabreLow;
			}
			else if (Blow.Stroke == EStroke::Horizontal)
			{
				const ETikiCombatClip Side = SabreSide(Blow.BladeSweep, Blow.VictimRight, Tuning.bMirrorSides);
				Wanted = Side != None ? Side : SabreDown;
			}
			return FirstAvailable(Mask, { Wanted, SabreDown });
		}
		// a sabre blow that does not kill keeps the hit clip; other weapons have no combat clip
		return None;
	}
}
