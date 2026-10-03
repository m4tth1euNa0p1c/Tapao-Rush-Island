// monke_island

#include "UI/FloodTimerSubsystem.h"
#include "UI/FloodTimerWidget.h"
#include "UI/HudTimerRules.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "monke_island.h"

bool UFloodTimerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && (!World || World->GetNetMode() != NM_DedicatedServer);
}

bool UFloodTimerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFloodTimerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Small data asset and widget class, loaded once while the level starts (not during play)
	Rules = RulesAsset.IsNull() ? nullptr : RulesAsset.LoadSynchronous();
	if (!RulesAsset.IsNull() && !Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("FloodTimer: rules %s not found, using the defaults"), *RulesAsset.ToString());
	}
	ResolvedWidgetClass = WidgetClass.IsNull() ? nullptr : WidgetClass.LoadSynchronous();
	if (!ResolvedWidgetClass)
	{
		if (!WidgetClass.IsNull())
		{
			UE_LOG(Logmonke_island, Warning, TEXT("FloodTimer: widget class %s not found, using the native timer"), *WidgetClass.ToString());
		}
		ResolvedWidgetClass = UFloodTimerWidget::StaticClass();
	}

	const UHudTimerRules& R = Rules ? *Rules : *GetDefault<UHudTimerRules>();
	InWorld.GetTimerManager().SetTimer(DiscoveryTimer, FTimerDelegate::CreateUObject(this, &UFloodTimerSubsystem::Discover),
		FMath::Max(0.1f, R.DiscoveryInterval), true, 0.0f);
}

void UFloodTimerSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DiscoveryTimer);
	}
	for (UFloodTimerWidget* Widget : Widgets)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}
	Widgets.Reset();

	Super::Deinitialize();
}

void UFloodTimerSubsystem::Discover()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UHudTimerRules& R = Rules ? *Rules : *GetDefault<UHudTimerRules>();
	Probe.Resolve(World, R.bShowStandaloneFloodSource);
	if (Probe.GetKind() == EFloodTimerSourceKind::None)
	{
		// No expedition running and no flood actor (yet): nothing on screen
		return;
	}

	Widgets.RemoveAll([](const TObjectPtr<UFloodTimerWidget>& Widget) { return !IsValid(Widget); });

	int32 LocalControllers = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalPlayerController() || !Controller->GetLocalPlayer())
		{
			continue;
		}
		++LocalControllers;
		const bool bHasTimer = Widgets.ContainsByPredicate([Controller](const TObjectPtr<UFloodTimerWidget>& Widget)
		{
			return Widget->GetOwningPlayer() == Controller;
		});
		if (bHasTimer)
		{
			continue;
		}

		UFloodTimerWidget* Widget = CreateWidget<UFloodTimerWidget>(Controller, ResolvedWidgetClass);
		if (!Widget)
		{
			UE_LOG(Logmonke_island, Error, TEXT("FloodTimer: could not create %s for %s"), *GetNameSafe(ResolvedWidgetClass), *Controller->GetName());
			continue;
		}
		if (Rules)
		{
			Widget->SetRules(Rules);
		}
		Widget->AddToPlayerScreen(R.ZOrder);
		Widgets.Add(Widget);
		UE_LOG(Logmonke_island, Log, TEXT("FloodTimer: timer shown for %s (%s clock)"), *Controller->GetName(),
			Probe.GetKind() == EFloodTimerSourceKind::Expedition ? TEXT("expedition") : TEXT("flood actor"));
	}

	// Every local player has its timer: the widgets follow the source themselves, the discovery can stop
	if (LocalControllers > 0 && Widgets.Num() >= LocalControllers)
	{
		World->GetTimerManager().ClearTimer(DiscoveryTimer);
	}
}
