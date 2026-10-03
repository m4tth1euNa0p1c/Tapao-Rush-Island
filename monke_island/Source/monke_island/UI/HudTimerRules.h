// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HudTimerRules.generated.h"

class USoundBase;
class USoundClass;
class UTexture2D;

/**
 *  Tunable values of the flood timer of the HUD (DA_HudTimerRules, cahier §6.1, §19): alert thresholds, durations,
 *  sizes at 1080p, colours and sounds. Without an asset the subsystem uses the class defaults (no sound).
 *  Sizes are Slate units at 1080p; the engine DPI curve scales them for other resolutions (720p = x0.67).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UHudTimerRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UHudTimerRules();

	/** Seconds before the rise at which a single alert plays (cahier §19: 60 / 30 / 10 s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Alerts")
	TArray<float> AlertThresholds;

	/**
	 *  Off (default, cahier §19): a threshold alerts once per expedition, even when a zone bonus pushes the deadline
	 *  back above it. On: a bonus re-arms the thresholds now above the time left, each fires again once.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Alerts")
	bool bRearmAlertsAfterExtension = false;

	/** Below this time left the countdown takes the urgent colour (s); the text and the pulse carry it too */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Alerts", meta=(ClampMin="0", Units="s"))
	float UrgentSeconds = 30.0f;

	/** Duration of the pulse of an alert (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Alerts", meta=(ClampMin="0.05", Units="s"))
	float PulseDuration = 0.6f;

	/** Scale reached at the top of the pulse */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Alerts", meta=(ClampMin="1"))
	float PulseScale = 1.15f;

	/** How long "Zone sécurisée : +2:00" stays on screen (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Notification", meta=(ClampMin="0.5", Units="s"))
	float ZoneSecuredDuration = 3.0f;

	/** Period of the refresh of the displayed time (s); the text only changes once a second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Refresh", meta=(ClampMin="0.05", ClampMax="1", Units="s"))
	float RefreshInterval = 0.2f;

	/** Period of the search for a flood source while none is active (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Refresh", meta=(ClampMin="0.1", Units="s"))
	float DiscoveryInterval = 0.5f;

	/**
	 *  Shows the timer of a standalone AFloodSimulationActor (day / night clock) when no expedition runs,
	 *  so the timer matches the water that really rises in a level without the expedition clock.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Source")
	bool bShowStandaloneFloodSource = true;

	/** Z order of the timer on the player screen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout")
	int32 ZOrder = 10;

	/** Distance from the top of the safe zone at 1080p */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float TopMargin = 24.0f;

	/** Font size of the clock line (points: 24 pt = 32 px at 1080p, 21 px at 720p) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout", meta=(ClampMin="14"))
	int32 ClockFontSize = 24;

	/** Font size of the notification line (points: 18 pt = 24 px at 1080p) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout", meta=(ClampMin="14"))
	int32 NotificationFontSize = 18;

	/** Text of the countdown before the rise */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor CountdownColor = FLinearColor(0.96f, 0.93f, 0.86f, 1.0f);

	/** Text of the countdown under UrgentSeconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor UrgentColor = FLinearColor(1.0f, 0.66f, 0.22f, 1.0f);

	/** Text of the evacuation countdown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor EvacuationColor = FLinearColor(0.55f, 0.85f, 1.0f, 1.0f);

	/** Panel behind the countdown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor PanelColor = FLinearColor(0.02f, 0.03f, 0.05f, 0.72f);

	/** Panel behind the evacuation countdown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor EvacuationPanelColor = FLinearColor(0.01f, 0.10f, 0.24f, 0.88f);

	/** Text of "Zone sécurisée" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor ZoneSecuredColor = FLinearColor(0.62f, 0.96f, 0.62f, 1.0f);

	/** Colour the text flashes to during an alert pulse */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Colors")
	FLinearColor PulseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** Short sound of a 60 / 30 / 10 s alert */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> AlertSound;

	/** Sound of the start of the evacuation (the submersion starts) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> EvacuationSound;

	/** Short sound of a zone bonus (cahier §19: "Bonus +2 min accompagné d'un son bref") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundBase> ZoneSecuredSound;

	/** Sound class the timer sounds play in (the Effects class of DA_SettingsRules, T-073), so the volume menu drives them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TSoftObjectPtr<USoundClass> SoundClass;

	/** Water icon shown during the evacuation; a "~" glyph stands in when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout")
	TSoftObjectPtr<UTexture2D> WaterIcon;
};
