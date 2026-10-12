// monke_island

#include "UI/Tapao/TapaoDeathUISubsystem.h"
#include "UI/Tapao/STapaoDeath.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "Death/DeathSpectatorComponent.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Player/IslanderHealthComponent.h"
#include "Settings/SettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Widgets/SViewport.h"

namespace
{
	/** Over the HUD (100), under the end of a day (870), the pause (900), NAUFRAGÉ (920) and the questions (950) */
	constexpr int32 DeathZOrder = 600;

	/** Period of the look at the replicated state (s) */
	constexpr float PollSeconds = 0.2f;
}

/** The dead player's clicks and keys: the teammate followed (UTapaoDeathUISubsystem::HandleCycleInput) */
class FTapaoDeathInputProcessor : public IInputProcessor
{
public:

	explicit FTapaoDeathInputProcessor(UTapaoDeathUISubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		const FKey Button = MouseEvent.GetEffectingButton();
		return Cycle(Button == EKeys::LeftMouseButton ? -1 : (Button == EKeys::RightMouseButton ? 1 : 0));
	}

	/** The second click of a quick double click comes as a double click: it changes the teammate too */
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		return HandleMouseButtonDownEvent(SlateApp, MouseEvent);
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.IsRepeat())
		{
			return false;
		}
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Left || Key == EKeys::Gamepad_LeftShoulder)
		{
			return Cycle(-1);
		}
		if (Key == EKeys::Right || Key == EKeys::Gamepad_RightShoulder)
		{
			return Cycle(1);
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TapaoDeath"); }

private:

	bool Cycle(int32 Direction)
	{
		UTapaoDeathUISubsystem* Subsystem = Owner.Get();
		return Direction != 0 && Subsystem && Subsystem->HandleCycleInput(Direction);
	}

	TWeakObjectPtr<UTapaoDeathUISubsystem> Owner;
};

bool UTapaoDeathUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void UTapaoDeathUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UTapaoUISubsystem>();
	Super::Initialize(Collection);

	Skin = Cast<UTapaoUISkin>(SkinPath.TryLoad());
	if (!Skin)
	{
		// UTapaoUISubsystem already says the TAPAO screens are off
		return;
	}
	Style = MakeShared<FTapaoStyle>(Skin);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTapaoDeathUISubsystem::Poll), PollSeconds);
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FTapaoDeathInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void UTapaoDeathUISubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	Hide();
	Style.Reset();
	Super::Deinitialize();
}

UDeathSpectatorComponent* UTapaoDeathUISubsystem::GetLocalSpectator() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	return Controller ? Controller->FindComponentByClass<UDeathSpectatorComponent>() : nullptr;
}

bool UTapaoDeathUISubsystem::Poll(float DeltaTime)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const UDeathSpectatorComponent* Spectator = GetLocalSpectator();
	const AController* Controller = Spectator ? Cast<AController>(Spectator->GetOwner()) : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	const AExpeditionGameState* Expedition = World ? World->GetGameState<AExpeditionGameState>() : nullptr;

	// what the replicated state says (server decisions: the death, the crew lost, the day over, the teammate followed)
	const EDeathScreen State = UDeathSpectatorComponent::DecideScreen(Health && Health->IsDead(), Expedition && Expedition->IsTeamWiped(),
		Expedition && Expedition->IsDaySurvived(), Spectator && Spectator->GetFollowedPlayer());

	// NAUFRAGÉ, a question, the title screen or the main menu have the screen
	const UTapaoUISubsystem* Tapao = GameInstance ? GameInstance->GetSubsystem<UTapaoUISubsystem>() : nullptr;
	const bool bCovered = Tapao && (Tapao->IsModalOpen() || Tapao->IsMainMenuOpen());
	if (!Spectator || bCovered || (State != EDeathScreen::Dead && State != EDeathScreen::Following))
	{
		Hide();
		return true;
	}

	Show();
	if (Screen.IsValid())
	{
		FTapaoDeathView View;
		View.Screen = State;
		View.Cause = Spectator->DescribeOwnerDeath();
		if (const APlayerState* Followed = Spectator->GetFollowedPlayer())
		{
			View.Followed = FText::FromString(Followed->GetPlayerName());
		}
		for (const APlayerState* Teammate : Spectator->GetObservablePlayers())
		{
			if (Teammate)
			{
				View.Observables.Add(FText::FromString(Teammate->GetPlayerName()));
			}
		}
		Screen->SetView(View);
	}
	return true;
}

void UTapaoDeathUISubsystem::Show()
{
	if (Screen.IsValid() || !Style.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		return;
	}
	Screen = SNew(STapaoDeath).Style(Style);
	// no input taken: the game keeps the mouse, the clicks come through the input processor
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), DeathZOrder);
}

void UTapaoDeathUISubsystem::Hide()
{
	if (!Screen.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	Screen.Reset();
}

bool UTapaoDeathUISubsystem::HandleCycleInput(int32 Direction)
{
	if (!Screen.IsValid())
	{
		return false;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}
	// a menu over the game keeps its clicks (pause, question, settings)
	const UTapaoUISubsystem* Tapao = GameInstance->GetSubsystem<UTapaoUISubsystem>();
	const USettingsSubsystem* Settings = GameInstance->GetSubsystem<USettingsSubsystem>();
	if ((Tapao && (Tapao->IsModalOpen() || Tapao->IsPauseOpen() || Tapao->IsInventoryOpen())) || (Settings && Settings->IsMenuOpen()))
	{
		return false;
	}
	// this game's viewport must have the focus (another play window, an editor panel keep their clicks)
	const UGameViewportClient* Viewport = GameInstance->GetGameViewportClient();
	const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	if (!ViewportWidget.IsValid() || !ViewportWidget->HasAnyUserFocusOrFocusedDescendants())
	{
		return false;
	}
	UDeathSpectatorComponent* Spectator = GetLocalSpectator();
	return Spectator && Spectator->RequestCycle(Direction);
}
