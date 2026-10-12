// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExpeditionAudioRules.generated.h"

class USoundBase;

/** One local presentation event. Gameplay authority remains with the existing systems. */
USTRUCT(BlueprintType)
struct FExpeditionAudioCue
{
	GENERATED_BODY()

	/** Recordings to alternate without an immediate repeat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio")
	TArray<TSoftObjectPtr<USoundBase>> Sounds;

	/** Gain in addition to the user's Effects or Music slider. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0", ClampMax="2"))
	float Volume = 0.5f;

	/** Minimum separation of two instances of this event (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0"))
	float Cooldown = 0.2f;
};

/** T-234: sounds for replicated gameplay observations, with no gameplay changes or new RPC. */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UExpeditionAudioRules : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Explicit event names; missing entries stay silent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TMap<FName, FExpeditionAudioCue> Cues;

	/** Lightweight local state sampling; movement/health/shop use their actual delegates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05"))
	float CheckInterval = 0.1f;

	/** World references are cached, never searched every frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.5"))
	float WorldRefreshInterval = 2.0f;

	/** Smooth ambience transitions (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.1"))
	float FadeSeconds = 0.8f;

	/** Range between incidental hull creaks while aboard (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing")
	FVector2D CreakInterval = FVector2D(9.0f, 19.0f);

	/** Thunder interval during the full storm only (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing")
	FVector2D ThunderInterval = FVector2D(18.0f, 36.0f);

	/** A recent replicated inventory increase from a purchase must not also play a loot chime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0"))
	float PurchasePickupSuppression = 0.6f;

	/** Late joins do not replay the start of the expedition (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0"))
	float StartGraceSeconds = 2.0f;

	/** Countdown ticks below the existing 10-second HUD alert. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timing", meta=(ClampMin="1", ClampMax="9"))
	int32 CountdownFrom = 9;

	/** Camera depth required for the muffled underwater bed (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambience", meta=(ClampMin="0"))
	float UnderwaterDepth = 12.0f;

	/** Wind intensity above which the strong layer replaces the light layer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambience", meta=(ClampMin="0"))
	float StrongWindThreshold = 0.5f;
};
