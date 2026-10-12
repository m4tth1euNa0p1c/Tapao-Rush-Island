// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Expedition/Core/ExpeditionTypes.h"
#include "UI/FloodTimerMath.h"
#include "UI/FloodTimerSource.h"
#include "FloodTimerWidget.generated.h"

class AExpeditionGameState;
class SFloodTimer;
class UHudTimerRules;
class USoundBase;
class UTexture2D;
struct FStreamableHandle;

/**
 *  Flood timer of the HUD (cahier §6.1, §19): time before the water rises, then a distinct evacuation countdown,
 *  "Zone sécurisée : +2:00" on each zone bonus, single alerts at 60 / 30 / 10 s (sound + pulse).
 *  Every value is read from the replicated server clock of the source (FFloodTimerSource), never from a local timer,
 *  so the host and the clients show the same time. No tick: a short refresh timer while visible, plus the
 *  OnFloodDeadlineChanged / OnPhaseChanged delegates of the expedition.
 *  Used as is, it draws its own Slate view (SFloodTimer, no asset). A future WBP_HUD / WBP_FloodTimer can inherit:
 *  as soon as it has a widget tree, the Slate view is skipped and the Blueprint events below dress it.
 */
UCLASS(meta=(DisableNativeTick))
class MONKE_ISLAND_API UFloodTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Rules to use (DA_HudTimerRules); call before adding to the screen. Null = the class defaults */
	UFUNCTION(BlueprintCallable, Category="HUD|Flood Timer")
	void SetRules(UHudTimerRules* InRules);

	/** Current display mode */
	UFUNCTION(BlueprintPure, Category="HUD|Flood Timer")
	EFloodTimerMode GetMode() const { return Snapshot.Mode; }

	/** Seconds left in the current mode */
	UFUNCTION(BlueprintPure, Category="HUD|Flood Timer")
	float GetSecondsLeft() const { return static_cast<float>(Snapshot.SecondsLeft); }

	/** Text of the clock line, as displayed */
	UFUNCTION(BlueprintPure, Category="HUD|Flood Timer")
	FText GetClockText() const { return ClockText; }

	/** The day of the contract and the quota ("Jour 2/3 · Quota 9 648 / 5 500", ADR-007), empty without a run; written
	 *  before the clock only with DA_HudTimerRules.bShowRunDayInClock (off by default, T-283) */
	UFUNCTION(BlueprintPure, Category="HUD|Flood Timer")
	FText GetDayText() const { return DayText; }

	/** Whether the timer reads the expedition clock (false: a standalone flood actor, or nothing) */
	UFUNCTION(BlueprintPure, Category="HUD|Flood Timer")
	bool IsReadingExpeditionClock() const { return SourceKind == EFloodTimerSourceKind::Expedition; }

protected:

	/** Rules of the timer (DA_HudTimerRules); a Blueprint child may set them in its defaults */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Flood Timer")
	TObjectPtr<UHudTimerRules> Rules;

	/** Called after every refresh (a few times a second) */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Flood Timer")
	void OnFloodTimerUpdated(EFloodTimerMode Mode, float SecondsLeft, const FText& Text);

	/** Called when the mode changes (countdown -> evacuation...) */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Flood Timer")
	void OnFloodTimerModeChanged(EFloodTimerMode NewMode, EFloodTimerMode PreviousMode);

	/** Called when a zone bonus moves the deadline later ("Zone sécurisée : +2:00") */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Flood Timer")
	void OnZoneSecured(float BonusSeconds, const FText& Text);

	/** Called once per alert threshold (60 / 30 / 10 s) */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Flood Timer")
	void OnFloodAlert(float ThresholdSeconds);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:

	/** Rules in use: the asset, else the class defaults */
	const UHudTimerRules& GetRules() const;

	/** Reads the source and updates the view, the alerts and the events */
	void Refresh();

	/** Finds the source again and (re)binds the expedition delegates */
	void ResolveSource();

	/** Expedition delegate: the deadline moved (start or zone bonus) */
	UFUNCTION()
	void HandleFloodDeadlineChanged(double NewFloodStartTime, double PreviousFloodStartTime);

	/** Expedition delegate: the phase moved */
	UFUNCTION()
	void HandlePhaseChanged(EExpeditionPhase NewPhase, EExpeditionPhase PreviousPhase);

	/** Hides "Zone sécurisée" */
	void HideNotification();

	/** Plays a timer sound in the timer sound class, if it is loaded */
	void PlayTimerSound(const TSoftObjectPtr<USoundBase>& Sound);

	/** Starts the asynchronous load of the sounds, the sound class and the icon */
	void LoadAssets();

	/** Clock line of a snapshot */
	static FText MakeClockText(const FFloodTimerSnapshot& InSnapshot);

	/** Slate view, when the class has no widget tree */
	TSharedPtr<SFloodTimer> View;

	/** Loaded water icon, kept alive for the Slate brush */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WaterIconTexture;

	/** Asynchronous load of the sounds and the icon */
	TSharedPtr<FStreamableHandle> AssetsHandle;

	/** Source of the times */
	FFloodTimerSource Source;

	/** Kind of source of the last refresh */
	EFloodTimerSourceKind SourceKind = EFloodTimerSourceKind::None;

	/** Expedition whose delegates are bound */
	TWeakObjectPtr<AExpeditionGameState> BoundExpedition;

	/** Expedition the alerts belong to */
	FGuid AlertsExpeditionId;

	/** Alerts and modes */
	FFloodTimerPresenter Presenter;

	/** Last snapshot */
	FFloodTimerSnapshot Snapshot;

	/** Last clock line */
	FText ClockText;

	/** Last day line ("Jour 2/3 · Quota …"), empty without a run */
	FText DayText;

	/** Day of the run read from the game state (URunStateComponent), empty when there is none */
	FText ReadDayText() const;

	/** Whether the two-clock warning was logged */
	bool bConflictLogged = false;

	/** Refresh period */
	FTimerHandle RefreshTimer;

	/** End of the "Zone sécurisée" notification */
	FTimerHandle NotificationTimer;
};
