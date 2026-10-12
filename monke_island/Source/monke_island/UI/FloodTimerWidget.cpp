// monke_island

#include "UI/FloodTimerWidget.h"
#include "UI/HudTimerRules.h"
#include "UI/SFloodTimer.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Expedition/Core/DayTransitionSubsystem.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "TimerManager.h"
#include "monke_island.h"

// T-141, ADR-006: the day of the run comes from Run/ (URunStateComponent, T-109/T-113 of monk-island-78), not yet
// committed. Optional on purpose: without Run/ the timer builds and simply shows no day.
#if __has_include("Run/RunStateComponent.h")
#include "Run/RunStateComponent.h"
#define MONKE_FLOOD_TIMER_HAS_RUN 1
#else
#define MONKE_FLOOD_TIMER_HAS_RUN 0
#endif

#define LOCTEXT_NAMESPACE "MonkeFloodTimer"

void UFloodTimerWidget::SetRules(UHudTimerRules* InRules)
{
	Rules = InRules;
}

const UHudTimerRules& UFloodTimerWidget::GetRules() const
{
	return Rules ? *Rules : *GetDefault<UHudTimerRules>();
}

TSharedRef<SWidget> UFloodTimerWidget::RebuildWidget()
{
	TSharedRef<SWidget> Designed = Super::RebuildWidget();
	if (WidgetTree && WidgetTree->RootWidget)
	{
		// A Blueprint child drew its own layout: it is dressed by the Blueprint events
		return Designed;
	}

	const UHudTimerRules& R = GetRules();
	FFloodTimerStyle Style;
	Style.TopMargin = R.TopMargin;
	Style.ClockFontSize = R.ClockFontSize;
	Style.NotificationFontSize = R.NotificationFontSize;
	Style.PulseDuration = R.PulseDuration;
	Style.PulseScale = R.PulseScale;
	Style.CountdownColor = R.CountdownColor;
	Style.UrgentColor = R.UrgentColor;
	Style.EvacuationColor = R.EvacuationColor;
	Style.PanelColor = R.PanelColor;
	Style.EvacuationPanelColor = R.EvacuationPanelColor;
	Style.ZoneSecuredColor = R.ZoneSecuredColor;
	Style.PulseColor = R.PulseColor;

	View = SNew(SFloodTimer).Style(Style);
	View->SetWaterIcon(WaterIconTexture);
	View->SetClock(Snapshot.Mode, ClockText, false);
	return View.ToSharedRef();
}

void UFloodTimerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	View.Reset();
}

void UFloodTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const UHudTimerRules& R = GetRules();
	Presenter.Configure(R.AlertThresholds, R.bRearmAlertsAfterExtension);
	LoadAssets();
	Refresh();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UFloodTimerWidget::Refresh),
			FMath::Clamp(R.RefreshInterval, 0.05f, 1.0f), true);
	}
}

void UFloodTimerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
		World->GetTimerManager().ClearTimer(NotificationTimer);
	}
	if (AExpeditionGameState* GameState = BoundExpedition.Get())
	{
		GameState->OnFloodDeadlineChanged.RemoveDynamic(this, &UFloodTimerWidget::HandleFloodDeadlineChanged);
		GameState->OnPhaseChanged.RemoveDynamic(this, &UFloodTimerWidget::HandlePhaseChanged);
	}
	BoundExpedition.Reset();
	if (AssetsHandle.IsValid())
	{
		AssetsHandle->CancelHandle();
		AssetsHandle.Reset();
	}

	Super::NativeDestruct();
}

void UFloodTimerWidget::LoadAssets()
{
	const UHudTimerRules& R = GetRules();
	TArray<FSoftObjectPath> Paths;
	for (const FSoftObjectPath& Path : { R.AlertSound.ToSoftObjectPath(), R.EvacuationSound.ToSoftObjectPath(),
		R.ZoneSecuredSound.ToSoftObjectPath(), R.SoundClass.ToSoftObjectPath(), R.WaterIcon.ToSoftObjectPath() })
	{
		if (Path.IsValid())
		{
			Paths.Add(Path);
		}
	}
	if (Paths.IsEmpty())
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("%s: no timer sound nor icon in the rules, the timer stays silent"), *GetName());
		return;
	}

	TWeakObjectPtr<UFloodTimerWidget> WeakThis(this);
	AssetsHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateLambda([WeakThis]()
	{
		UFloodTimerWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}
		This->WaterIconTexture = This->GetRules().WaterIcon.Get();
		if (This->View.IsValid())
		{
			This->View->SetWaterIcon(This->WaterIconTexture);
		}
	}));
}

void UFloodTimerWidget::ResolveSource()
{
	Source.Resolve(GetWorld(), GetRules().bShowStandaloneFloodSource);

	AExpeditionGameState* GameState = Source.GetExpedition();
	if (GameState != BoundExpedition.Get())
	{
		if (AExpeditionGameState* Previous = BoundExpedition.Get())
		{
			Previous->OnFloodDeadlineChanged.RemoveDynamic(this, &UFloodTimerWidget::HandleFloodDeadlineChanged);
			Previous->OnPhaseChanged.RemoveDynamic(this, &UFloodTimerWidget::HandlePhaseChanged);
		}
		BoundExpedition = GameState;
		if (GameState)
		{
			GameState->OnFloodDeadlineChanged.AddUniqueDynamic(this, &UFloodTimerWidget::HandleFloodDeadlineChanged);
			GameState->OnPhaseChanged.AddUniqueDynamic(this, &UFloodTimerWidget::HandlePhaseChanged);
		}
	}

	if (!bConflictLogged && Source.HasConflictingSources())
	{
		bConflictLogged = true;
		UE_LOG(Logmonke_island, Warning, TEXT("%s: the expedition clock runs while an AFloodSimulationActor floods on the day / night clock; "
			"the timer shows the expedition clock, the water may rise at another time"), *GetName());
	}
}

FText UFloodTimerWidget::MakeClockText(const FFloodTimerSnapshot& InSnapshot)
{
	// the clock alone, top right (owner request 2026-10-04): the panel colour and the water icon tell the evacuation
	const FText Time = FloodTimerMath::FormatClock(InSnapshot.SecondsLeft);
	switch (InSnapshot.Mode)
	{
	case EFloodTimerMode::Countdown:
		return FText::Format(LOCTEXT("CountdownBare", "{0}"), Time);
	case EFloodTimerMode::Evacuation:
		return FText::Format(LOCTEXT("EvacuationBare", "{0}"), Time);
	case EFloodTimerMode::Over:
		return LOCTEXT("Over", "Île submergée : l'eau monte encore, montez dans la barque !");
	case EFloodTimerMode::GameOver:
		return FText::Format(LOCTEXT("GameOverBare", "{0}"), Time);
	default:
		return FText::GetEmpty();
	}
}

FText UFloodTimerWidget::ReadDayText() const
{
#if MONKE_FLOOD_TIMER_HAS_RUN
	if (const URunStateComponent* Run = URunStateComponent::Get(this))
	{
		// ADR-007: the day of the contract, then the value brought back against the quota
		const FText Day = FloodTimerMath::FormatDay(Run->GetDayIndex(), Run->GetDaysPerPeriod());
		if (Run->GetQuota() <= 0)
		{
			return Day;
		}
		return FText::Format(NSLOCTEXT("FloodTimer", "DayQuota", "{0} · Quota {1} / {2}{3}"), Day, FText::AsNumber(Run->GetHoldValue()),
			FText::AsNumber(Run->GetQuota()), Run->IsGreedDay() ? NSLOCTEXT("FloodTimer", "Greed", " · jour de cupidité") : FText::GetEmpty());
	}
#endif
	return FText::GetEmpty();
}

void UFloodTimerWidget::Refresh()
{
	ResolveSource();

	const EFloodTimerSourceKind Kind = Source.GetKind();
	const AExpeditionGameState* GameState = BoundExpedition.Get();
	const FGuid ExpeditionId = Kind == EFloodTimerSourceKind::Expedition && GameState ? GameState->GetExpeditionId() : FGuid();
	if (Kind != SourceKind || ExpeditionId != AlertsExpeditionId)
	{
		// Another clock or another expedition: its alerts start afresh
		Presenter.Reset();
		SourceKind = Kind;
		AlertsExpeditionId = ExpeditionId;
	}

	Snapshot = FloodTimerMath::Evaluate(Source.Read());
	const FFloodTimerEvents Events = Presenter.Update(Snapshot);
	const UHudTimerRules& R = GetRules();
	// the day of the contract and the quota (ADR-007), read from the replicated state at each refresh; written before the
	// clock only when the rules ask: by default the timer is the clock alone (T-283), the TAPAO HUD shows the day top left
	DayText = ReadDayText();
	const FText Clock = MakeClockText(Snapshot);
	ClockText = Snapshot.Mode == EFloodTimerMode::Hidden || !R.bShowRunDayInClock ? Clock : FloodTimerMath::ComposeClockLine(DayText, Clock);

	const bool bUrgent = Snapshot.Mode == EFloodTimerMode::Countdown && Snapshot.SecondsLeft <= R.UrgentSeconds;
	if (View.IsValid())
	{
		View->SetClock(Snapshot.Mode, ClockText, bUrgent);
	}
	// no clock during the arrival and departure scenes (owner 2026-10-07: no player UI in a scene)
	const UDayTransitionSubsystem* Transition = UDayTransitionSubsystem::Get(this);
	SetVisibility(Transition && Transition->IsScenePlaying() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	if (Events.bModeChanged)
	{
		if (Snapshot.Mode == EFloodTimerMode::Evacuation && Events.PreviousMode == EFloodTimerMode::Countdown)
		{
			PlayTimerSound(R.EvacuationSound);
			if (View.IsValid())
			{
				View->Pulse();
			}
		}
		if (Snapshot.Mode == EFloodTimerMode::Hidden)
		{
			HideNotification();
		}
		OnFloodTimerModeChanged(Snapshot.Mode, Events.PreviousMode);
	}
	if (Events.HasAlert())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: flood alert at %.0f s (%s left)"), *GetName(), Events.AlertThreshold, *ClockText.ToString());
		PlayTimerSound(R.AlertSound);
		if (View.IsValid())
		{
			View->Pulse();
		}
		OnFloodAlert(Events.AlertThreshold);
	}
	OnFloodTimerUpdated(Snapshot.Mode, static_cast<float>(Snapshot.SecondsLeft), ClockText);
}

void UFloodTimerWidget::HandleFloodDeadlineChanged(double NewFloodStartTime, double PreviousFloodStartTime)
{
	if (FloodTimerMath::IsZoneBonus(NewFloodStartTime, PreviousFloodStartTime))
	{
		const double Bonus = NewFloodStartTime - PreviousFloodStartTime;
		const FText Text = FText::Format(LOCTEXT("ZoneSecured", "Zone sécurisée : +{0}"), FloodTimerMath::FormatClock(Bonus));
		const UHudTimerRules& R = GetRules();
		if (View.IsValid())
		{
			View->SetNotification(Text, true);
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(NotificationTimer, FTimerDelegate::CreateUObject(this, &UFloodTimerWidget::HideNotification),
				FMath::Max(0.5f, R.ZoneSecuredDuration), false);
		}
		PlayTimerSound(R.ZoneSecuredSound);
		OnZoneSecured(static_cast<float>(Bonus), Text);
	}
	Refresh();
}

void UFloodTimerWidget::HandlePhaseChanged(EExpeditionPhase NewPhase, EExpeditionPhase PreviousPhase)
{
	Refresh();
}

void UFloodTimerWidget::HideNotification()
{
	if (View.IsValid())
	{
		View->SetNotification(FText::GetEmpty(), false);
	}
}

void UFloodTimerWidget::PlayTimerSound(const TSoftObjectPtr<USoundBase>& Sound)
{
	USoundBase* Loaded = Sound.Get();
	if (!Loaded)
	{
		return;
	}
	if (UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Loaded))
	{
		if (USoundClass* SoundClass = GetRules().SoundClass.Get())
		{
			Audio->SoundClassOverride = SoundClass;
		}
		Audio->Play();
	}
}

#undef LOCTEXT_NAMESPACE
