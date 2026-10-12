// monke_island

#include "UI/Tapao/TapaoUISubsystem.h"
#include "UI/Tapao/STapaoHUD.h"
#include "UI/Tapao/STapaoInventory.h"
#include "UI/Tapao/STapaoPlayMenu.h"
#include "Run/RunSubsystem.h"
#include "UI/Tapao/STapaoScreens.h"
#include "UI/Tapao/TapaoDayEndText.h"
#include "UI/Tapao/TapaoSaveGame.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Death/DeathSpectatorComponent.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Execution/ExecutionScene.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunStateComponent.h"
#include "Run/SaveSlotFile.h"
#include "Settings/SettingsSubsystem.h"
#include "Session/CrewSessionSubsystem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoviePlayer.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"

#define LOCTEXT_NAMESPACE "TapaoUI"

namespace
{
	/** Slot of the campaign save: a constant, never a config value nor a typed name (security.md §3, SEC-04, m2) */
	const TCHAR* CampaignSlot = TEXT("TapaoCampaign");

	/** Subject of the messages about this save */
	FText CampaignSaveName()
	{
		return LOCTEXT("CampaignSaveName", "La sauvegarde de la campagne");
	}

	/** Longest address REJOINDRE opens (a host name is at most 253 characters, the port adds 6) */
	constexpr int32 MaxJoinAddressChars = 255;
}

/** Escape (P in the editor) opens and closes the pause */
class FTapaoPauseInputProcessor : public IInputProcessor
{
public:

	explicit FTapaoPauseInputProcessor(UTapaoUISubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UTapaoUISubsystem* Subsystem = Owner.Get();
		if (!Subsystem || InKeyEvent.IsRepeat() || Subsystem->IsModalOpen())
		{
			return false;
		}
		const FKey Key = InKeyEvent.GetKey();
		// Escape closes the inventory screen first (in the editor too, where it would stop the play session)
		if (Key == EKeys::Escape && Subsystem->IsInventoryOpen())
		{
			Subsystem->CloseInventory();
			return true;
		}
		// in the editor Escape stops the play session
		const bool bPauseKey = Key == EKeys::Escape ? !GIsEditor || Subsystem->IsPauseOpen() : (GIsEditor && Key == EKeys::P);
		if (!bPauseKey)
		{
			return false;
		}
		// the settings menu over the pause closes first (USettingsSubsystem)
		const UGameInstance* GameInstance = Subsystem->GetGameInstance();
		const USettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<USettingsSubsystem>() : nullptr;
		if (Settings && Settings->IsMenuOpen())
		{
			return false;
		}
		if (!Subsystem->IsPauseOpen() && !Subsystem->CanPause())
		{
			return false;
		}
		Subsystem->TogglePause();
		return true;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TapaoPause"); }

private:

	TWeakObjectPtr<UTapaoUISubsystem> Owner;
};

bool UTapaoUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void UTapaoUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<USettingsSubsystem>();
	Super::Initialize(Collection);

	Skin = Cast<UTapaoUISkin>(SkinPath.TryLoad());
	if (!Skin)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: %s not found, the TAPAO screens are off"), *SkinPath.ToString());
		return;
	}
	Style = MakeShared<FTapaoStyle>(Skin);

	PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UTapaoUISubsystem::HandlePreLoadMap);
	ActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UTapaoUISubsystem::HandleWorldActorsInitialized);
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FTapaoPauseInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
	InventoryRequestHandle = UIslanderInventoryComponent::OnScreenRequested.AddUObject(this, &UTapaoUISubsystem::ToggleInventory);
}

void UTapaoUISubsystem::Deinitialize()
{
	if (bInGame)
	{
		Autosave();
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearAllTimersForObject(this);
	}
	UIslanderInventoryComponent::OnScreenRequested.Remove(InventoryRequestHandle);
	CloseInventory();
	RemoveFromViewport(Confirm);
	RemoveFromViewport(EndScreen);
	RemoveFromViewport(Pause);
	RemoveFromViewport(Hud);
	RemoveFromViewport(Splash);
	RemoveFromViewport(MainMenu);
	RemoveFromViewport(Curtain);
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadHandle);
	FWorldDelegates::OnWorldInitializedActors.Remove(ActorsHandle);
	Style.Reset();
	Super::Deinitialize();
}

// ---------------------------------------------------------------- maps

bool UTapaoUISubsystem::IsMenuWorld(const UWorld* World) const
{
	if (!World || !Skin || Skin->MainMenuMap.IsNull())
	{
		return false;
	}
	return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == Skin->MainMenuMap.ToSoftObjectPath().GetLongPackageName();
}

bool UTapaoUISubsystem::IsPlayWorld(const UWorld* World) const
{
	if (!World || !Skin || Skin->PlayMap.IsNull())
	{
		return false;
	}
	return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == Skin->PlayMap.ToSoftObjectPath().GetLongPackageName();
}

void UTapaoUISubsystem::HandlePreLoadMap(const FString& MapName)
{
	// the screens of the world that goes away; the curtain stays over the load
	if (bInGame)
	{
		Autosave();
	}
	bInGame = false;
	DeathTime = -1.0;
	WipeTime = -1.0;
	bPausedGame = false;
	GetGameInstance()->GetTimerManager().ClearTimer(CurtainTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(GameTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(AutosaveTimer);
	CloseInventory();
	RemoveFromViewport(Confirm);
	RemoveFromViewport(EndScreen);
	RemoveFromViewport(Pause);
	RemoveFromViewport(Hud);
	RemoveFromViewport(Splash);
	RemoveFromViewport(MainMenu);
	// a real loading screen in the game (the editor has no movie player)
	if (!GIsEditor && Style.IsValid() && IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.MinimumLoadingScreenDisplayTime = Skin->MinLoadingSeconds;
		// the logo and the gauge only (owner 2026-10-07: no day nor quota while loading; the quota shows at the end of the day)
		Attributes.WidgetLoadingScreen = SNew(STapaoLoading).Style(Style);
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
	}
}

void UTapaoUISubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	UWorld* World = Params.World;
	if (!World || World->GetGameInstance() != GetGameInstance() || !World->IsGameWorld())
	{
		return;
	}
	if (IsMenuWorld(World))
	{
		HideCurtain();
		if (bSplashDone)
		{
			ShowMainMenu();
		}
		else
		{
			ShowSplash();
		}
		return;
	}
	HideMainMenu();
	ShowCurtain();
	if (IsPlayWorld(World))
	{
		// the boot map only draws the island and travels on
		return;
	}
	// an island: the curtain covers the first seconds (camps, marae, villages and loot appear), the campaign advances
	bInGame = true;
	ShowHud();
	FTimerManager& Timers = GetGameInstance()->GetTimerManager();
	Timers.SetTimer(GameTimer, this, &UTapaoUISubsystem::PollGame, 0.5f, true);
	if (World->GetNetMode() != NM_Client)
	{
		if (UTapaoSaveGame* Data = GetSave(true))
		{
			++Data->Expeditions;
			Data->LastIsland = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
			WriteSave();
		}
		if (AutosaveSeconds > 0.0f)
		{
			Timers.SetTimer(AutosaveTimer, this, &UTapaoUISubsystem::Autosave, AutosaveSeconds, true);
		}
	}
}

// ---------------------------------------------------------------- screens

void UTapaoUISubsystem::AddToViewport(const TSharedRef<SWidget>& Widget, int32 ZOrder, bool bUIOnly)
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport)
	{
		return;
	}
	Viewport->AddViewportWidgetContent(Widget, ZOrder);
	if (bUIOnly)
	{
		FocusUI(Widget, 0);
	}
}

void UTapaoUISubsystem::FocusUI(TWeakPtr<SWidget> Widget, int32 Attempt)
{
	const TSharedPtr<SWidget> Pinned = Widget.Pin();
	UGameInstance* GameInstance = GetGameInstance();
	if (!Pinned.IsValid() || !GameInstance)
	{
		return;
	}
	// the screens come with the world, before its player controller: wait for it, or the viewport keeps the mouse
	APlayerController* Controller = GameInstance->GetFirstLocalPlayerController();
	if (!Controller)
	{
		if (Attempt < 100)
		{
			GameInstance->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UTapaoUISubsystem::FocusUI, Widget, Attempt + 1));
		}
		return;
	}
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Pinned);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(Mode);
	Controller->SetShowMouseCursor(true);
	FSlateApplication::Get().SetKeyboardFocus(Pinned);
}

void UTapaoUISubsystem::RestoreGameInput()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr)
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
}

void UTapaoUISubsystem::RemoveFromViewport(TSharedPtr<SWidget>& Widget)
{
	if (!Widget.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();
}

void UTapaoUISubsystem::RefocusScreen()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const USettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<USettingsSubsystem>() : nullptr;
	if (Settings && Settings->IsMenuOpen())
	{
		return;
	}
	for (const TSharedPtr<SWidget>* Screen : { &Confirm, &EndScreen, &Pause, &Splash, &PlayMenu, &MainMenu })
	{
		if (Screen->IsValid())
		{
			FocusUI(*Screen, 0);
			return;
		}
	}
	if (InventoryScreen.IsValid())
	{
		FocusUI(InventoryScreen, 0);
		return;
	}
	RestoreGameInput();
}

// ---------------------------------------------------------------- inventory screen (T-166)

void UTapaoUISubsystem::ToggleInventory(UIslanderInventoryComponent* Inventory)
{
	if (InventoryScreen.IsValid())
	{
		CloseInventory();
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	// the inventory of this game instance's player only, in game, over no other screen
	if (!Style.IsValid() || !Inventory || !Controller || Controller->GetPawn() != Inventory->GetOwner()
		|| !Inventory->GetWorld() || Inventory->GetWorld()->GetGameInstance() != GameInstance)
	{
		return;
	}
	const USettingsSubsystem* Settings = GameInstance->GetSubsystem<USettingsSubsystem>();
	if (IsModalOpen() || Pause.IsValid() || MainMenu.IsValid() || (Settings && Settings->IsMenuOpen()))
	{
		return;
	}
	if (const UIslanderHealthComponent* Health = Inventory->GetOwner()->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return;
	}

	// the keys that close it and the keys of the bar cells, as the player's mapping has them
	TArray<FKey> CloseKeys;
	TArray<TArray<FKey>> BarKeys;
	if (const UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		if (Inventory->InventoryScreenAction)
		{
			CloseKeys = Input->QueryKeysMappedToAction(Inventory->InventoryScreenAction);
		}
		for (const TObjectPtr<UInputAction>& Action : Inventory->SlotActions)
		{
			BarKeys.Add(Action ? Input->QueryKeysMappedToAction(Action) : TArray<FKey>());
		}
	}
	CloseKeys.AddUnique(EKeys::E);
	const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	BarKeys.SetNum(FMath::Max(BarKeys.Num(), FMath::Min(Inventory->GetBarNum(), 9)));
	for (int32 Bar = 0; Bar < BarKeys.Num() && Bar < 9; ++Bar)
	{
		if (BarKeys[Bar].Num() == 0)
		{
			BarKeys[Bar].Add(Digits[Bar]);
		}
	}

	InventoryScreen = SNew(STapaoInventoryScreen)
		.Style(Style)
		.Inventory(Inventory)
		.CloseKeys(CloseKeys)
		.BarKeys(BarKeys)
		.OnClose(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::CloseInventory));
	// the keys held now (a walk forward) must not stay down while the screen has the input
	Controller->FlushPressedKeys();
	AddToViewport(InventoryScreen.ToSharedRef(), 850, true);
}

void UTapaoUISubsystem::CloseInventory()
{
	if (!InventoryScreen.IsValid())
	{
		return;
	}
	TSharedPtr<SWidget> Screen = InventoryScreen;
	InventoryScreen.Reset();
	RemoveFromViewport(Screen);
	RefocusScreen();
}

void UTapaoUISubsystem::ShowSplash()
{
	if (Splash.IsValid() || !Style.IsValid())
	{
		return;
	}
	Splash = SNew(STapaoSplash)
		.Style(Style)
		.MinSeconds(static_cast<float>(SplashSeconds.X))
		.MaxSeconds(static_cast<float>(SplashSeconds.Y))
		.OnDone(FSimpleDelegate::CreateWeakLambda(this, [this]()
		{
			bSplashDone = true;
			// the splash is still handling its key: it goes on the next tick
			GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RemoveFromViewport(Splash);
				ShowMainMenu();
			}));
		}));
	AddToViewport(Splash.ToSharedRef(), 800, true);
}

void UTapaoUISubsystem::ShowMainMenu()
{
	if (MainMenu.IsValid() || !Style.IsValid())
	{
		return;
	}
	const UTapaoSaveGame* Data = GetSave(false);
	FText Hint = Data && Data->Expeditions > 0
		? FText::Format(LOCTEXT("ContinueHint", "Campagne : {0} expédition(s), dernière le {1}"), Data->Expeditions, FText::AsDate(Data->LastPlayed))
		: FText::GetEmpty();
	// T-302: a damaged, too new or unreadable save is told here (error-handling.md §3), never fixed in silence
	if (const FText Notice = GetSaveNotice(); !Notice.IsEmpty())
	{
		Hint = Hint.IsEmpty() ? Notice : FText::Format(LOCTEXT("HintWithNotice", "{0}\n{1}"), Notice, Hint);
	}
	MainMenu = SNew(STapaoMainMenu)
		.Style(Style)
		.CanContinue_UObject(this, &UTapaoUISubsystem::HasSave)
		.ContinueHint(Hint)
		.OnPlay(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnPlay))
		.OnContinue(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnContinue))
		.OnNewGame(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnNewGame))
		.OnOptions(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnOptions))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnQuit));
	AddToViewport(MainMenu.ToSharedRef(), 800, true);
	// T-315: back at the menu after a join the host refused (expedition under way, full session) or that failed: the
	// player is told why, once (cahier §19: no screen without a reason)
	if (UCrewSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrewSessionSubsystem>() : nullptr)
	{
		if (Session->HasJoinFailure())
		{
			AskConfirm(Session->ConsumeJoinFailure(), LOCTEXT("JoinFailureOk", "Compris"), FText::GetEmpty(), FSimpleDelegate());
		}
	}
}

void UTapaoUISubsystem::HideMainMenu()
{
	RemoveFromViewport(PlayMenu);
	RemoveFromViewport(Splash);
	RemoveFromViewport(MainMenu);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadingScreenCommand(
	TEXT("monke.UI.Loading"),
	TEXT("Shows the loading screen for its minimum time (tests of its layout)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UTapaoUISubsystem* Screens = GameInstance ? GameInstance->GetSubsystem<UTapaoUISubsystem>() : nullptr)
		{
			Screens->DebugShowLoading();
		}
	}));
#endif

void UTapaoUISubsystem::ShowCurtain()
{
	if (!Style.IsValid())
	{
		return;
	}
	if (!Curtain.IsValid())
	{
		Curtain = SNew(STapaoLoading).Style(Style);
		AddToViewport(Curtain.ToSharedRef(), 1100, false);
	}
	GetGameInstance()->GetTimerManager().SetTimer(CurtainTimer, this, &UTapaoUISubsystem::HideCurtain, FMath::Max(Skin->MinLoadingSeconds, 0.1f), false);
}

void UTapaoUISubsystem::HideCurtain()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(CurtainTimer);
		// A client can finish loading before its local controller arrives. Keep the curtain until input can follow.
		if (bInGame && !GameInstance->GetFirstLocalPlayerController())
		{
			CurtainTimer = GameInstance->GetTimerManager().SetTimerForNextTick(this, &UTapaoUISubsystem::HideCurtain);
			return;
		}
	}
	RemoveFromViewport(Curtain);
	if (bInGame)
	{
		// Map travel preserves the viewport's UI-only mode. Restore capture after loading, just like closing pause.
		RefocusScreen();
	}
}

void UTapaoUISubsystem::ShowHud()
{
	if (Hud.IsValid() || !Style.IsValid())
	{
		return;
	}
	Hud = SNew(STapaoHUD).Style(Style).GameInstance(GetGameInstance());
	AddToViewport(Hud.ToSharedRef(), 100, false);
}

bool UTapaoUISubsystem::CanPause() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || !bInGame || IsModalOpen() || MainMenu.IsValid() || !GameInstance->GetFirstLocalPlayerController())
	{
		return false;
	}
	const UGameViewportClient* Viewport = GameInstance->GetGameViewportClient();
	const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	return Widget.IsValid() && Widget->HasAnyUserFocusOrFocusedDescendants();
}

void UTapaoUISubsystem::TogglePause()
{
	if (IsPauseOpen())
	{
		ClosePause();
	}
	else
	{
		OpenPause();
	}
}

void UTapaoUISubsystem::OpenPause()
{
	if (Pause.IsValid() || !Style.IsValid())
	{
		return;
	}
	// the pause comes over a closed inventory
	CloseInventory();
	Pause = SNew(STapaoPause)
		.Style(Style)
		.OnResume(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::ClosePause))
		.OnOptions(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnOptions))
		.OnMainMenu(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnMainMenu))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::OnQuit));
	AddToViewport(Pause.ToSharedRef(), 900, true);
	// the world stops only for a player alone (a session with others goes on)
	UWorld* World = GetGameInstance()->GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (World && World->GetNetMode() != NM_Client && (!GameState || GameState->PlayerArray.Num() <= 1))
	{
		bPausedGame = UGameplayStatics::SetGamePaused(World, true);
	}
}

void UTapaoUISubsystem::ClosePause()
{
	if (!Pause.IsValid())
	{
		return;
	}
	RemoveFromViewport(Confirm);
	RemoveFromViewport(Pause);
	UGameInstance* GameInstance = GetGameInstance();
	if (bPausedGame && GameInstance && GameInstance->GetWorld())
	{
		UGameplayStatics::SetGamePaused(GameInstance->GetWorld(), false);
	}
	bPausedGame = false;
	RefocusScreen();
}

void UTapaoUISubsystem::AskConfirm(const FText& Question, const FText& Yes, const FText& No, FSimpleDelegate OnYes)
{
	if (!Style.IsValid())
	{
		OnYes.ExecuteIfBound();
		return;
	}
	RemoveFromViewport(Confirm);
	Confirm = SNew(STapaoConfirm)
		.Style(Style)
		.Question(Question)
		.Yes(Yes)
		.No(No)
		.OnYes(FSimpleDelegate::CreateWeakLambda(this, [this, OnYes]()
		{
			CloseConfirm();
			OnYes.ExecuteIfBound();
		}))
		.OnNo(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::CloseConfirm));
	AddToViewport(Confirm.ToSharedRef(), 950, true);
}

void UTapaoUISubsystem::CloseConfirm()
{
	RemoveFromViewport(Confirm);
	RefocusScreen();
}

void UTapaoUISubsystem::ShowEndScreen(bool bVictory, bool bExecuted)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	TArray<FText> Lines;
	if (bExecuted)
	{
		// the Bureau's notice of the lost campaign (T-173): the contract, the crowns missing, the crew declared traitor
		const URunStateComponent* Run = URunStateComponent::Get(World);
		Lines = TapaoDayEndText::LostCampaignLines(Run ? Run->GetLastDayReport() : FRunDayReport(), Run ? Run->GetZoneName() : FText::GetEmpty(),
			Run ? Run->GetDaysPlayed() : 0);
	}
	else if (!bVictory && GameInstance)
	{
		// how the local player died (cahier §19: a short cause, T-135)
		const APlayerController* Controller = GameInstance->GetFirstLocalPlayerController();
		const UDeathSpectatorComponent* Spectator = Controller ? Controller->FindComponentByClass<UDeathSpectatorComponent>() : nullptr;
		const FText Cause = Spectator ? Spectator->DescribeOwnerDeath() : FText::GetEmpty();
		if (!Cause.IsEmpty())
		{
			Lines.Add(FText::Format(LOCTEXT("WreckCause", "Tu es mort : {0}"), Cause));
		}
	}
	ShowEndScreenLines(bVictory, bExecuted, Lines);
}

void UTapaoUISubsystem::DebugShowLostCampaign(const FRunDayReport& Report, const FText& ZoneName, int32 CampaignDay)
{
	RemoveFromViewport(EndScreen);
	ShowEndScreenLines(false, true, TapaoDayEndText::LostCampaignLines(Report, ZoneName, CampaignDay));
}

void UTapaoUISubsystem::DebugCloseEndScreen()
{
	if (EndScreen.IsValid())
	{
		RemoveFromViewport(EndScreen);
		RefocusScreen();
	}
}

void UTapaoUISubsystem::ShowEndScreenLines(bool bVictory, bool bExecuted, const TArray<FText>& LeadLines)
{
	if (EndScreen.IsValid() || !Style.IsValid())
	{
		return;
	}
	ClosePause();
	CloseInventory();
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance->GetWorld();
	const bool bHost = World && World->GetNetMode() != NM_Client;
	const UTapaoSaveGame* Data = GetSave(false);
	TArray<FText> Lines = LeadLines;
	if (Data)
	{
		Lines.Add(FText::Format(LOCTEXT("StatExpeditions", "Expéditions : {0}"), Data->Expeditions));
		Lines.Add(FText::Format(LOCTEXT("StatWrecks", "Naufrages : {0}"), Data->Wrecks));
		Lines.Add(FText::Format(LOCTEXT("StatIslands", "Îles conquises : {0}"), Data->Islands));
	}
	EndScreen = SNew(STapaoEndScreen)
		.Style(Style)
		.Ribbon(bVictory ? TEXT("ribbon_ile_conquise") : (bExecuted && Style->Size(TEXT("ribbon_blank")).X > 0.0f ? TEXT("ribbon_blank") : TEXT("ribbon_naufrage")))
		.RibbonText(bExecuted && Style->Size(TEXT("ribbon_blank")).X > 0.0f ? LOCTEXT("ExecutedRibbon", "CONTRAT ÉCHOUÉ") : FText::GetEmpty())
		.Lines(Lines)
		.FirstButton(bVictory ? TEXT("btn_continuer") : TEXT("btn_recommencer"))
		// only the host starts a new island
		.FirstEnabled(bHost)
		.OnFirst(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::RestartPlay))
		.OnMainMenu(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::LeaveToMainMenu));
	AddToViewport(EndScreen.ToSharedRef(), 920, true);
}

void UTapaoUISubsystem::NotifyVictory()
{
	if (!bInGame || EndScreen.IsValid())
	{
		return;
	}
	if (UTapaoSaveGame* Data = GetSave(true))
	{
		++Data->Islands;
		WriteSave();
	}
	ShowEndScreen(true);
}

void UTapaoUISubsystem::PollGame()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	// the inventory screen goes with its pawn (death, respawn, possession of another pawn)
	if (InventoryScreen.IsValid())
	{
		const UIslanderInventoryComponent* Shown = InventoryScreen->GetInventory();
		if (!Shown || Shown->GetOwner() != Pawn || (Health && Health->IsDead()))
		{
			CloseInventory();
		}
	}
	// the day is survived (ADR-006: alive aboard the galleon at the end of the deluge): ÎLE CONQUISE ! for the living
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const AExpeditionGameState* Expedition = World ? World->GetGameState<AExpeditionGameState>() : nullptr;
	// no expedition in this world (test map L_Test_Flat, its own game mode): no end screen, the game mode respawns (the
	// death screen with its cause shows meanwhile, UTapaoDeathUISubsystem)
	if (!Expedition)
	{
		DeathTime = -1.0;
		WipeTime = -1.0;
		return;
	}
	// the campaign lost on a contract failed on its milestone day (owner 2026-10-07): the crew is executed, the scene plays
	// to its end, then the end screen of the lost campaign (CONTRAT ÉCHOUÉ; RECOMMENCER starts a new one). A crew wiped
	// that day has nobody left to execute (T-173): no scene, JOUR N PERDU first (UTapaoDayUISubsystem), then the end
	// screen LostCampaignScreenSeconds after the wipe
	if (Expedition->IsExecution())
	{
		const double Now = FPlatformTime::Seconds();
		const AExecutionScene* Scene = AExecutionScene::Get(World);
		if (Expedition->IsTeamWiped() && WipeTime < 0.0)
		{
			WipeTime = Now;
		}
		const bool bReady = Scene ? Scene->IsOver() : (Expedition->IsTeamWiped() && Now - WipeTime >= LostCampaignScreenSeconds);
		if (!EndScreen.IsValid() && bReady)
		{
			if (DeathTime < 0.0)
			{
				DeathTime = Now;
				if (UTapaoSaveGame* Data = GetSave(true))
				{
					++Data->Wrecks;
					WriteSave();
				}
			}
			ShowEndScreen(false, true);
		}
		return;
	}
	if (Expedition->IsDaySurvived() && Health && !Health->IsDead() && !EndScreen.IsValid())
	{
		// a day of the campaign: the island is counted, no end screen over the departure scene and the day summary
		// (UDayTransitionSubsystem, UTapaoDayUISubsystem); the travel to the next island follows on its own. R-03: the
		// run of the host, as the game state carries it to every machine (never the run this machine keeps on its disk)
		const URunStateComponent* Run = URunStateComponent::Get(World);
		if (Run && Run->IsRunActive())
		{
			if (VictoryWorld.Get() != World)
			{
				VictoryWorld = World;
				if (UTapaoSaveGame* Data = GetSave(true))
				{
					++Data->Islands;
					WriteSave();
				}
			}
			return;
		}
		NotifyVictory();
		return;
	}
	if (!Health || !Health->IsDead() || EndScreen.IsValid())
	{
		if (Health && !Health->IsDead())
		{
			DeathTime = -1.0;
			WipeTime = -1.0;
		}
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (DeathTime < 0.0)
	{
		DeathTime = Now;
		if (UTapaoSaveGame* Data = GetSave(true))
		{
			++Data->Wrecks;
			WriteSave();
		}
	}
	// T-135 (cahier §4, §19): NAUFRAGÉ only once the whole crew is dead, as the server decides it (solo: at the death);
	// while a teammate lives the dead player watches him (UTapaoDeathUISubsystem, UDeathSpectatorComponent)
	const EDeathScreen Screen = UDeathSpectatorComponent::DecideScreen(true, Expedition->IsTeamWiped(), Expedition->IsDaySurvived(), false);
	if (Screen != EDeathScreen::Wreck)
	{
		WipeTime = -1.0;
		return;
	}
	if (WipeTime < 0.0)
	{
		WipeTime = Now;
	}
	// T-173 (ADR-007 §14): in a campaign the whole crew dead loses the day, not the game: JOUR N PERDU
	// (UTapaoDayUISubsystem), then the travel to the next island, which the host may hurry; NAUFRAGÉ only on a map
	// without a campaign
	const URunStateComponent* RunState = URunStateComponent::Get(World);
	if (RunState && TapaoDayEndText::IsCampaignUnderWay(RunState->GetQuota(), RunState->GetDaysPlayed(), RunState->GetLastDayReport().Verdict))
	{
		return;
	}
	if (Now - WipeTime >= GameOverDelay)
	{
		ShowEndScreen(false);
	}
}

// ---------------------------------------------------------------- actions

void UTapaoUISubsystem::OnPlay()
{
	// JOUER goes on with the campaign when there is one, else starts one
	if (!HasSave())
	{
		GetSave(true);
		WriteSave();
	}
	ShowPlayMenu(false);
}

void UTapaoUISubsystem::OnContinue()
{
	if (HasSave())
	{
		ShowPlayMenu(false);
	}
}

void UTapaoUISubsystem::ShowPlayMenu(bool bNewRun)
{
	if (!Style.IsValid())
	{
		return;
	}
	bPendingNewRun = bNewRun;
	RemoveFromViewport(PlayMenu);
	FText RunLine = LOCTEXT("RunNew", "Nouvelle partie : jour 1");
	// the main menu: this machine's own run (read here; a client in a game never reads it, R-03)
	const URunSubsystem* Run = URunSubsystem::Get(GetGameInstance()->GetWorld());
	if (!bNewRun && Run && Run->GetState().bActive && Run->GetState().Outcome == ERunOutcome::InProgress)
	{
		const FRunContractState& State = Run->GetState();
		RunLine = FText::Format(LOCTEXT("RunGoesOn", "Partie en cours : {0}, jour {1}, quota {2} / {3}, trésorerie {4}"),
			Run->GetZoneName(), State.DayInContract, FText::AsNumber(State.ContractValue), FText::AsNumber(State.Quota), FText::AsNumber(State.Treasury));
	}
	// T-302: what happened to the save of the run is told before the player goes on with it
	if (Run && !Run->GetLoadNotice().IsEmpty())
	{
		RunLine = FText::Format(LOCTEXT("RunLineWithNotice", "{0}\n{1}"), Run->GetLoadNotice(), RunLine);
	}
	PlayMenu = SNew(STapaoPlayMenu)
		.Style(Style)
		.RunLine(RunLine)
		.DefaultAddress(JoinAddress)
		.OnSolo(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::StartSolo))
		.OnHost(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::StartHost))
		.OnJoin(FOnTapaoJoin::CreateUObject(this, &UTapaoUISubsystem::StartJoin))
		.OnBack(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::ClosePlayMenu));
	AddToViewport(PlayMenu.ToSharedRef(), 820, true);
}

void UTapaoUISubsystem::ClosePlayMenu()
{
	RemoveFromViewport(PlayMenu);
	bPendingNewRun = false;
	RefocusScreen();
}

void UTapaoUISubsystem::ApplyPendingNewRun()
{
	if (bPendingNewRun)
	{
		if (URunSubsystem* Run = URunSubsystem::Get(GetGameInstance()->GetWorld()))
		{
			Run->StartNewRun();
		}
		bPendingNewRun = false;
	}
}

void UTapaoUISubsystem::StartSolo()
{
	ApplyPendingNewRun();
	RemoveFromViewport(PlayMenu);
	TravelToPlay(FString());
}

void UTapaoUISubsystem::StartHost()
{
	ApplyPendingNewRun();
	RemoveFromViewport(PlayMenu);
	TravelToPlay(MakePlayOptions(true, GetHostedMaxPlayers()));
}

void UTapaoUISubsystem::StartJoin(const FString& Address)
{
	// m5: an address that is not host[:port] never reaches OpenLevel (FName stops at 1023 characters, URL options)
	const FString Trimmed = Address.TrimStartAndEnd();
	if (!IsValidJoinAddress(Trimmed))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: join refused, the address is not host[:port] (%d characters)"), Trimmed.Len());
		AskConfirm(LOCTEXT("JoinBadAddress", "Adresse invalide : saisis l'adresse IP de l'hôte, par exemple 192.168.1.20 ou 192.168.1.20:7777."),
			LOCTEXT("JoinBackToMenu", "MENU"), LOCTEXT("JoinFix", "CORRIGER"),
			FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::ClosePlayMenu));
		return;
	}
	// the host's run counts: this machine only follows it (its own run is kept for later, untouched, R-03)
	bPendingNewRun = false;
	JoinAddress = Trimmed;
	RemoveFromViewport(PlayMenu);
	ClosePause();
	RemoveFromViewport(EndScreen);
	HideMainMenu();
	ShowCurtain();
	// no address in the logs (security.md §6, M7)
	UE_LOG(Logmonke_island, Log, TEXT("TapaoUI: joining a host"));
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Trimmed), true);
}

FString UTapaoUISubsystem::MakePlayOptions(bool bHosted, int32 MaxPlayers)
{
	return bHosted ? FString::Printf(TEXT("listen?MaxPlayers=%d"), FMath::Clamp(MaxPlayers, 1, 16)) : FString();
}

TOptional<FString> UTapaoUISubsystem::MakeRestartOptions(ENetMode NetMode, int32 MaxPlayers)
{
	switch (NetMode)
	{
	case NM_Client:
		return TOptional<FString>();
	case NM_ListenServer:
		return MakePlayOptions(true, MaxPlayers);
	default:
		// M4: a game alone (or a dedicated server, which has no screens) is started again without listening
		return MakePlayOptions(false, MaxPlayers);
	}
}

bool UTapaoUISubsystem::IsValidJoinAddress(const FString& Address)
{
	if (Address.IsEmpty() || Address.Len() > MaxJoinAddressChars)
	{
		return false;
	}
	FString Host = Address;
	FString Port;
	if (Address.Split(TEXT(":"), &Host, &Port, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		if (Port.IsEmpty() || Port.Len() > 5)
		{
			return false;
		}
		for (const TCHAR Char : Port)
		{
			if (Char < TEXT('0') || Char > TEXT('9'))
			{
				return false;
			}
		}
		const int32 PortNumber = FCString::Atoi(*Port);
		if (PortNumber < 1 || PortNumber > 65535)
		{
			return false;
		}
	}
	if (Host.IsEmpty() || Host.Len() > 253 || Host.StartsWith(TEXT(".")) || Host.StartsWith(TEXT("-")) || Host.EndsWith(TEXT("."))
		|| Host.EndsWith(TEXT("-")))
	{
		return false;
	}
	for (const TCHAR Char : Host)
	{
		const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('A') && Char <= TEXT('Z'))
			|| (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('.') || Char == TEXT('-');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

int32 UTapaoUISubsystem::GetHostedMaxPlayers() const
{
	return (Skin ? Skin.Get() : GetDefault<UTapaoUISkin>())->MaxPlayers;
}

void UTapaoUISubsystem::OnNewGame()
{
	if (HasSave())
	{
		AskConfirm(LOCTEXT("ConfirmOverwrite", "Écraser la partie en cours ?"), LOCTEXT("Overwrite", "ÉCRASER"), LOCTEXT("Cancel", "ANNULER"),
			FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::StartNewGame));
		return;
	}
	StartNewGame();
}

void UTapaoUISubsystem::StartNewGame()
{
	// the player said "overwrite": a fresh campaign is written over the slot as a transaction, the old one stays as its
	// last valid copy (.bak) instead of being deleted
	if (!bSaveRead)
	{
		ReadSave();
	}
	Save = Cast<UTapaoSaveGame>(UGameplayStatics::CreateSaveGameObject(UTapaoSaveGame::StaticClass()));
	WriteSave();
	ShowPlayMenu(true);
}

void UTapaoUISubsystem::OnOptions()
{
	if (USettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<USettingsSubsystem>(); Settings && !Settings->IsMenuOpen())
	{
		Settings->ToggleMenu();
	}
}

void UTapaoUISubsystem::OnMainMenu()
{
	AskConfirm(LOCTEXT("ConfirmLeave", "Revenir au menu principal ? La partie est sauvegardée."), LOCTEXT("Menu", "MENU"), LOCTEXT("Cancel", "ANNULER"),
		FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::LeaveToMainMenu));
}

void UTapaoUISubsystem::LeaveToMainMenu()
{
	ClosePause();
	if (Skin && !Skin->MainMenuMap.IsNull())
	{
		ShowCurtain();
		UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Skin->MainMenuMap, true);
	}
}

void UTapaoUISubsystem::OnQuit()
{
	AskConfirm(LOCTEXT("ConfirmQuit", "Quitter TAPAO : Rush Island ?"), LOCTEXT("Quit", "QUITTER"), LOCTEXT("Cancel", "ANNULER"),
		FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::QuitNow));
}

void UTapaoUISubsystem::QuitNow()
{
	if (bInGame)
	{
		Autosave();
	}
	UKismetSystemLibrary::QuitGame(GetGameInstance(), GetGameInstance()->GetFirstLocalPlayerController(), EQuitPreference::Quit, false);
}

void UTapaoUISubsystem::RestartPlay()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	// M4: the same kind of game again; a game alone never becomes a server open to the local network
	const TOptional<FString> Options = MakeRestartOptions(World ? World->GetNetMode() : NM_Standalone, GetHostedMaxPlayers());
	if (!Options.IsSet())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: only the host starts an island"));
		return;
	}
	TravelToPlay(Options.GetValue());
}

void UTapaoUISubsystem::TravelToPlay(const FString& Options)
{
	if (!Skin || Skin->PlayMap.IsNull())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: no play map in DA_TapaoUISkin"));
		return;
	}
	ClosePause();
	RemoveFromViewport(EndScreen);
	HideMainMenu();
	ShowCurtain();
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Skin->PlayMap, true, Options);
}

// ---------------------------------------------------------------- save

bool UTapaoUISubsystem::HasSave() const
{
	return Save != nullptr || UGameplayStatics::DoesSaveGameExist(CampaignSlot, 0);
}

bool UTapaoUISubsystem::OwnsCampaignSave() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	return !World || World->GetNetMode() != NM_Client;
}

FText UTapaoUISubsystem::GetSaveNotice() const
{
	// the main menu: this machine's own run (read there; a client in a game never reads it, R-03)
	const UGameInstance* GameInstance = GetGameInstance();
	const URunSubsystem* Run = GameInstance ? URunSubsystem::Get(GameInstance->GetWorld()) : nullptr;
	const FText RunNotice = Run ? Run->GetLoadNotice() : FText::GetEmpty();
	if (CampaignNotice.IsEmpty() || RunNotice.IsEmpty())
	{
		return CampaignNotice.IsEmpty() ? RunNotice : CampaignNotice;
	}
	return FText::Format(LOCTEXT("TwoNotices", "{0}\n{1}"), CampaignNotice, RunNotice);
}

void UTapaoUISubsystem::ReadSave()
{
	bSaveRead = true;
	const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Paths = MonkeSaveSlot::MakePaths(MonkeSaveSlot::DefaultDirectory(), CampaignSlot);
	if (Paths.HasError())
	{
		bCampaignWritable = false;
		UE_LOG(Logmonke_island, Error, TEXT("TapaoUI: the folder of the campaign save is refused (%s)"), LexToString(Paths.GetError()));
		return;
	}
	int32 Fixes = 0;
	const MonkeSaveSlot::FReadResult Result = MonkeSaveSlot::Read(Paths.GetValue(), UTapaoSaveGame::StaticClass(), UTapaoSaveGame::CurrentVersion,
		[&Fixes](USaveGame& Loaded)
		{
			return CastChecked<UTapaoSaveGame>(&Loaded)->SanitizeLoaded(Fixes);
		}, FDateTime::Now());
	bCampaignWritable = Result.bWritable;
	CampaignNotice = MonkeSaveSlot::DescribeForPlayer(Result, CampaignSaveName(), LOCTEXT("CampaignFresh", "une nouvelle campagne commencera"));
	Save = Cast<UTapaoSaveGame>(Result.Object);
	if (Fixes > 0)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: %d value(s) of the campaign save were out of bounds and were fixed"), Fixes);
	}
	if (!Save && Result.MainError != ESaveSlotError::NotFound)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: no valid campaign save (%s), a new campaign starts"), LexToString(Result.MainError));
	}
}

UTapaoSaveGame* UTapaoUISubsystem::GetSave(bool bCreate)
{
	// bCreate asks for a save to count in: a client gets none, the campaign it plays is the host's (cahier §16); its
	// own save is only read (end screen statistics), so nothing counted there leaks into a later write at the menu
	if (bCreate && !OwnsCampaignSave())
	{
		return nullptr;
	}
	// the slot is read by the main menu, or by the host: a client in a host's game never reads its own files (R-03)
	if (!Save && !bSaveRead && OwnsCampaignSave())
	{
		ReadSave();
	}
	if (!Save && bCreate)
	{
		Save = Cast<UTapaoSaveGame>(UGameplayStatics::CreateSaveGameObject(UTapaoSaveGame::StaticClass()));
	}
	return Save;
}

void UTapaoUISubsystem::Autosave()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !GetSave(false))
	{
		return;
	}
	Save->PlaySeconds += AutosaveSeconds;
	WriteSave();
}

void UTapaoUISubsystem::WriteSave()
{
	if (!Save)
	{
		return;
	}
	// a client never writes the campaign (R-03, cahier §16); a slot that could not be read is never written over
	if (!OwnsCampaignSave() || !bCampaignWritable)
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("TapaoUI: campaign save not written (client, or slot left untouched)"));
		return;
	}
	const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Paths = MonkeSaveSlot::MakePaths(MonkeSaveSlot::DefaultDirectory(), CampaignSlot);
	if (Paths.HasError())
	{
		return;
	}
	Save->Version = UTapaoSaveGame::CurrentVersion;
	Save->LastPlayed = FDateTime::Now();
	const ESaveSlotError Error = MonkeSaveSlot::Write(Paths.GetValue(), *Save, UTapaoSaveGame::CurrentVersion, FDateTime::Now());
	if (Error != ESaveSlotError::None)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: the campaign save could not be written (%s), its last valid copy is kept"), LexToString(Error));
		CampaignNotice = MonkeSaveSlot::DescribeWriteFailureForPlayer(CampaignSaveName());
		return;
	}
	CampaignNotice = FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
