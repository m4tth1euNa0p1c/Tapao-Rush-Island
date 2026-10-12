// monke_island

#pragma once

#include "CoreMinimal.h"
#include "StealthTypes.generated.h"

class AActor;

/** Stealth log (T-262): noises reported, foes spotted or lost, theft unseen */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeStealth, Log, All);

/** Kind of a noise the mobs and the villagers may hear (T-262, docs/features/furtivite.md) */
UENUM(BlueprintType)
enum class EStealthNoise : uint8
{
	/** A running step on the ground (walking and crouching are silent) */
	Footstep,
	/** A player speaking in the proximity voice chat (T-263) */
	Voice,
	/** A shot of a firearm */
	Gunshot,
	/** An explosion or a heavy impact (grenade) */
	Impact
};

/** A noise made in the world (server): who made it, where, how far it carries and when */
USTRUCT(BlueprintType)
struct FStealthNoise
{
	GENERATED_BODY()

	/** Who made it (the player pawn, usually); invalid for a noise of nobody */
	UPROPERTY(BlueprintReadOnly, Category="Stealth")
	TWeakObjectPtr<AActor> Instigator;

	/** Where it was made */
	UPROPERTY(BlueprintReadOnly, Category="Stealth")
	FVector Location = FVector::ZeroVector;

	/** How far it carries for a listener of hearing scale 1 (cm) */
	UPROPERTY(BlueprintReadOnly, Category="Stealth")
	float RadiusCm = 0.0f;

	/** What it is */
	UPROPERTY(BlueprintReadOnly, Category="Stealth")
	EStealthNoise Kind = EStealthNoise::Footstep;

	/** World time it was made (s) */
	UPROPERTY(BlueprintReadOnly, Category="Stealth")
	double Time = 0.0;
};

/** How exposed a player is (the HUD indicator, T-262) */
UENUM(BlueprintType)
enum class EStealthExposure : uint8
{
	/** Seen as usual (standing in daylight) */
	Visible,
	/** Unseen but at very close contact: crouched, or at night */
	Hidden,
	/** Running: every step is heard */
	Noisy
};

/** Sight rules of the stealth (DA_StealthRules.Sight) */
USTRUCT(BlueprintType)
struct FStealthSightRules
{
	GENERATED_BODY()

	/** A crouched player is unseen by every mob and villager, day and night, carrying an object or not, unless at
	 *  contact (owner 2026-10-09); off: only crouched in a bush (T-207) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight")
	bool bCrouchHides = true;

	/** At night (EDayPhase::Night) a standing player is unseen unless at contact: only the hearing works */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight")
	bool bNightHidesStanding = true;

	/** Contact: a hidden player is seen when the gap between the two bodies (collision cylinders) is this small (cm);
	 *  100 = about 1.5 m between a tiki and the bean */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight", meta=(ClampMin="0", Units="cm"))
	float ContactGapCm = 100.0f;

	/** A crouched player this close to a bush of the island is in the bush (T-207, the bush grid of ATikiDirector) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight|Bush", meta=(ClampMin="0", Units="cm"))
	float BushRadiusCm = 150.0f;

	/** Crouched in a bush the contact is closer still: a gap of this much (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight|Bush", meta=(ClampMin="0", Units="cm"))
	float BushContactGapCm = 50.0f;
};

/** Noise rules of the stealth (DA_StealthRules.Hearing) */
USTRUCT(BlueprintType)
struct FStealthHearingRules
{
	GENERATED_BODY()

	/** A running step carries this far (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0", Units="cm"))
	float RunStepRadiusCm = 1500.0f;

	/** Ground run between two noisy steps (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="20", Units="cm"))
	float RunStrideCm = 170.0f;

	/** A voice carries this far (cm), before the loudness the voice chat gives it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0", Units="cm"))
	float VoiceRadiusCm = 1800.0f;

	/** A shot carries this far (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0", Units="cm"))
	float GunshotRadiusCm = 5000.0f;

	/** An explosion carries this far (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0", Units="cm"))
	float ImpactRadiusCm = 6000.0f;

	/** No noise carries farther, whatever the caller asks (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0", Units="cm"))
	float MaxNoiseRadiusCm = 20000.0f;

	/** A noise stays audible this long after it was made, so a listener that thinks now and then hears it (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0.05", Units="s"))
	float NoiseWindowSeconds = 1.0f;

	/** Noises kept at once; the oldest is dropped beyond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="4", ClampMax="512"))
	int32 MaxNoises = 64;

	/** Radius of a kind of noise (cm) */
	float RadiusOf(EStealthNoise Kind) const
	{
		switch (Kind)
		{
		case EStealthNoise::Footstep: return RunStepRadiusCm;
		case EStealthNoise::Voice: return VoiceRadiusCm;
		case EStealthNoise::Gunshot: return GunshotRadiusCm;
		case EStealthNoise::Impact: return ImpactRadiusCm;
		default: return 0.0f;
		}
	}
};
