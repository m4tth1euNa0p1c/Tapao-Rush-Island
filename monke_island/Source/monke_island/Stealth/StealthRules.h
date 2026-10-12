// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Stealth/StealthTypes.h"
#include "StealthRules.generated.h"

/**
 *  Rules of the stealth (DA_StealthRules, T-262, owner request 2026-10-09; docs/features/furtivite.md): a crouched
 *  player (left Ctrl held) is unseen by the mobs and the villagers, day and night, unless at contact; at night nobody
 *  is seen but at contact and the mobs only hear: the running steps, the voice (T-263), the shots and the explosions.
 *  A mob or a villager that hears a noise goes to look, attacks or panics if it then sees a player, and forgets after
 *  NoiseMemorySeconds. Loaded by path by UStealthSubsystem; the class defaults stand in when the asset is missing.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UStealthRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Who is seen, and when */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight")
	FStealthSightRules Sight;

	/** What is heard, and how far */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing")
	FStealthHearingRules Hearing;

	/** A mob that heard a noise (or lost the foe it hunted) looks for it this long, then forgets (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reactions", meta=(ClampMin="0", Units="s"))
	float NoiseMemorySeconds = 10.0f;

	/** A hunted foe out of sight (crouched, the night) is still followed this long before the mob only goes to the
	 *  place it saw it last (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reactions", meta=(ClampMin="0", Units="s"))
	float LoseSightGraceSeconds = 1.0f;

	/** Time between two listens of a mob or a villager (s); under NoiseWindowSeconds so that no noise is missed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reactions", meta=(ClampMin="0", Units="s"))
	float ListenIntervalSeconds = 0.2f;

	/** A mob looking for a noise is there within this distance of its place (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reactions", meta=(ClampMin="10", Units="cm"))
	float ArriveRadiusCm = 120.0f;

	/** Once there, it turns its head toward a new direction this often (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reactions", meta=(ClampMin="0.2", Units="s"))
	float SearchTurnSeconds = 1.6f;

	/** Hearing of the tiki warriors: the radius of a noise times this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Listeners", meta=(ClampMin="0"))
	float WarriorHearingScale = 1.0f;

	/** Hearing of the villagers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Listeners", meta=(ClampMin="0"))
	float VillagerHearingScale = 0.8f;

	/** A villager looking for a noise sees a player up to this distance (cm; crouched or at night: only at contact) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Listeners", meta=(ClampMin="0", Units="cm"))
	float VillagerSightCm = 2000.0f;

	/** A villager of a house minds the noises in its house and this close around it (cm), not the others */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Listeners", meta=(ClampMin="0", Units="cm"))
	float HouseNoiseMarginCm = 300.0f;

	/** Hearing of the fort soldiers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Listeners", meta=(ClampMin="0"))
	float SoldierHearingScale = 1.0f;
};
