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
#include "Expedition/Core/ExpeditionGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "TimerManager.h"
#include "monke_island.h"

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
	const FText Time = FloodTimerMath::FormatClock(InSnapshot.SecondsLeft);
	switch (InSnapshot.Mode)
	{
	case EFloodTimerMode::Countdown:
		return FText::Format(LOCTEXT("Countdown", "Montée des eaux dans {0}"), Time);
	case EFloodTimerMode::Evacuation:
		return FText::Format(LOCTEXT("Evacuation", "Évacuation ! Extraction dans {0}"), Time);
	case EFloodTimerMode::Over:
		return LOCTEXT("Over", "Île submergée");
	default:
		return FText::GetEmpty();
	}
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
	ClockText = MakeClockText(Snapshot);

	const UHudTimerRules& R = GetRules();
	const bool bUrgent = Snapshot.Mode == EFloodTimerMode::Countdown && Snapshot.SecondsLeft <= R.UrgentSeconds;
	if (View.IsValid())
	{
		View->SetClock(Snapshot.Mode, ClockText, bUrgent);
	}

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
