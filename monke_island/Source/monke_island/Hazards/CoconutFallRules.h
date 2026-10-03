// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CoconutFallRules.generated.h"

class UStaticMesh;

/**
 *  Tunable rules of the coconuts falling from the palms (DA_CoconutFallRules): how often they fall around each
 *  player (more under the storm wind), and when a nut knocks a player out (ragdoll on the ground).
 *  A green coconut weighs 1.5 to 2.5 kg and falls from 10 to 25 m: it lands at 14 to 22 m/s.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UCoconutFallRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Mean time between two falls around a player in calm weather (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0.1", Units="s"))
	float CalmIntervalSeconds = 7.0f;

	/** Mean time between two falls around a player under the full storm wind (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0.1", Units="s"))
	float StormIntervalSeconds = 1.2f;

	/** Radius around a player where palms drop their nuts (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="100", Units="cm"))
	float SearchRadiusCm = 2500.0f;

	/** Chance that the palm closest to the player is the one that drops (0-1): nuts often land near the player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0", ClampMax="1"))
	float ClosestPalmChance = 0.4f;

	/** Chance that a fall is aimed: a palm close to the player drops its nut right on the player's head (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0", ClampMax="1"))
	float AimedFallChance = 0.3f;

	/** Farthest crown (horizontally, cm) that can aim a nut at the player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0", Units="cm"))
	float MaxAimDistanceCm = 900.0f;

	/** Only palms at least this tall drop nuts (cm): no nuts from sprouts and juveniles */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0", Units="cm"))
	float MinPalmHeightCm = 600.0f;

	/** Text a palm mesh name must contain */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls")
	FString PalmMeshNameFilter = TEXT("CoconutPalm");

	/** Only palms whose mesh name contains one of these drop nuts: mature trees with a full green crown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls")
	TArray<FString> CrownedPalmTokens = {TEXT("Straight"), TEXT("Leaning"), TEXT("Sabre"), TEXT("SCurve")};

	/** Fewest nuts a crowned palm holds; once dropped they never grow back during the expedition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0"))
	int32 MinNutsPerPalm = 2;

	/** Most nuts a crowned palm holds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0"))
	int32 MaxNutsPerPalm = 5;

	/** Whether fallen nuts stay on the ground for the whole expedition (no despawn); MaxLiveCoconuts still caps them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Coconut")
	bool bFallenNutsStay = true;

	/** Nuts a palm instance holds at the start, stable for a given instance (same tree, same count) */
	int32 NutsForPalm(uint32 PalmKey) const
	{
		const int32 Low = FMath::Min(MinNutsPerPalm, MaxNutsPerPalm);
		const int32 High = FMath::Max(MinNutsPerPalm, MaxNutsPerPalm);
		return Low + static_cast<int32>(PalmKey % static_cast<uint32>(High - Low + 1));
	}

	/** Nuts start this far under the top of the palm (cm), inside the crown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0", Units="cm"))
	float CrownDepthCm = 120.0f;

	/** Random spread of the start point inside the crown (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="0", Units="cm"))
	float CrownSpreadCm = 80.0f;

	/** Most fallen nuts alive at once in the level */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="1"))
	int32 MaxLiveCoconuts = 40;

	/** Seconds a fallen nut stays before it disappears */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Falls", meta=(ClampMin="1", Units="s"))
	float CoconutLifeSpan = 90.0f;

	/** Mass of a nut (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Coconut", meta=(ClampMin="0.1"))
	float CoconutMassKg = 1.8f;

	/** Meshes of the nuts, one picked at random */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Coconut")
	TArray<TSoftObjectPtr<UStaticMesh>> CoconutMeshes;

	/** Impact speed from which a nut knocks a player out (cm/s): about a 3 m fall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knockout", meta=(ClampMin="0", Units="cm/s"))
	float KnockoutSpeed = 750.0f;

	/** Health lost when knocked out by a nut */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knockout", meta=(ClampMin="0"))
	float KnockoutDamage = 15.0f;

	/** Share of the nut velocity given to the falling body (the player crumples where it stands) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knockout", meta=(ClampMin="0", ClampMax="1"))
	float KnockoutVelocityShare = 0.15f;

	/** Mean time between falls for a wind strength (0 calm, 1 full storm; gusts above 1 drop even more) */
	float IntervalFor(float WindStrength) const
	{
		const float Alpha = FMath::Clamp(WindStrength, 0.0f, 1.5f) / 1.0f;
		return FMath::Max(0.1f, FMath::Lerp(CalmIntervalSeconds, StormIntervalSeconds, FMath::Min(Alpha, 1.0f)) / FMath::Max(1.0f, Alpha));
	}

	/** Whether a nut hitting at this speed knocks a player out */
	bool KnocksOut(float ImpactSpeed) const { return ImpactSpeed >= KnockoutSpeed; }
};
