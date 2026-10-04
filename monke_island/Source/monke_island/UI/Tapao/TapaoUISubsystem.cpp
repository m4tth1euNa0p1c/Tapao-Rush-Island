// monke_island

#include "UI/Tapao/TapaoUISubsystem.h"
#include "UI/Tapao/STapaoHUD.h"
#include "UI/Tapao/STapaoScreens.h"
#include "UI/Tapao/TapaoSaveGame.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunStateComponent.h"
#include "Settings/SettingsSubsystem.h"
#include "monke_island.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoviePlayer.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"

#define LOCTEXT_NAMESPACE "TapaoUI"

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
		// in the editor Escape stops the play session
		const FKey Key = InKeyEvent.GetKey();
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

namespace
{
	/** Line of the loading screens: the day in its period (URunStateComponent of the game state, ADR-006); the first
	 *  day of the first period while no run state is there yet */
	FText MakeDayLine(const UGameInstance* GameInstance)
	{
		const URunStateComponent* Run = GameInstance ? URunStateComponent::Get(GameInstance->GetWorld()) : nullptr;
		const int32 Day = Run ? FMath::Max(Run->GetDayIndex(), 1) : 1;
		const int32 Period = Run ? FMath::Max(Run->GetPeriodIndex(), 1) : 1;
		return FText::Format(NSLOCTEXT("TapaoUI", "DayLine", "Jour {0} / Période {1}"), Day, Period);
	}
}

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
	bPausedGame = false;
	GetGameInstance()->GetTimerManager().ClearTimer(CurtainTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(GameTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(AutosaveTimer);
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
		Attributes.WidgetLoadingScreen = SNew(STapaoLoading).Style(Style).Tip(MakeDayLine(GetGameInstance()));
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
	for (const TSharedPtr<SWidget>* Screen : { &Confirm, &EndScreen, &Pause, &Splash, &MainMenu })
	{
		if (Screen->IsValid())
		{
			FocusUI(*Screen, 0);
			return;
		}
	}
	RestoreGameInput();
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
	const FText Hint = Data && Data->Expeditions > 0
		? FText::Format(LOCTEXT("ContinueHint", "Campagne : {0} expédition(s), dernière le {1}"), Data->Expeditions, FText::AsDate(Data->LastPlayed))
		: FText::GetEmpty();
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
}

void UTapaoUISubsystem::HideMainMenu()
{
	RemoveFromViewport(Splash);
	RemoveFromViewport(MainMenu);
}

void UTapaoUISubsystem::ShowCurtain()
{
	if (!Style.IsValid())
	{
		return;
	}
	if (!Curtain.IsValid())
	{
		Curtain = SNew(STapaoLoading).Style(Style).Tip(MakeDayLine(GetGameInstance()));
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

void UTapaoUISubsystem::ShowEndScreen(bool bVictory)
{
	if (EndScreen.IsValid() || !Style.IsValid())
	{
		return;
	}
	ClosePause();
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance->GetWorld();
	const bool bHost = World && World->GetNetMode() != NM_Client;
	const UTapaoSaveGame* Data = GetSave(false);
	TArray<FText> Lines;
	if (Data)
	{
		Lines.Add(FText::Format(LOCTEXT("StatExpeditions", "Expéditions : {0}"), Data->Expeditions));
		Lines.Add(FText::Format(LOCTEXT("StatWrecks", "Naufrages : {0}"), Data->Wrecks));
		Lines.Add(FText::Format(LOCTEXT("StatIslands", "Îles conquises : {0}"), Data->Islands));
	}
	EndScreen = SNew(STapaoEndScreen)
		.Style(Style)
		.Ribbon(bVictory ? TEXT("ribbon_ile_conquise") : TEXT("ribbon_naufrage"))
		.Lines(Lines)
		.FirstButton(bVictory ? TEXT("btn_continuer") : TEXT("btn_recommencer"))
		// only the host starts a new island
		.FirstEnabled(bHost)
		.OnFirst(FSimpleDelegate::CreateUObject(this, &UTapaoUISubsystem::TravelToPlay))
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
	// the day is survived (ADR-006: alive aboard the galleon at the end of the deluge): ÎLE CONQUISE ! for the living
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const AExpeditionGameState* Expedition = World ? World->GetGameState<AExpeditionGameState>() : nullptr;
	if (Expedition && Expedition->IsDaySurvived() && Health && !Health->IsDead() && !EndScreen.IsValid())
	{
		NotifyVictory();
		return;
	}
	if (!Health || !Health->IsDead() || EndScreen.IsValid())
	{
		if (Health && !Health->IsDead())
		{
			DeathTime = -1.0;
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
	else if (Now - DeathTime >= GameOverDelay)
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
	TravelToPlay();
}

void UTapaoUISubsystem::OnContinue()
{
	if (HasSave())
	{
		TravelToPlay();
	}
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
	UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
	Save = nullptr;
	GetSave(true);
	WriteSave();
	TravelToPlay();
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

void UTapaoUISubsystem::TravelToPlay()
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
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Skin->PlayMap, true, Skin->PlayOptions);
}

// ---------------------------------------------------------------- save

bool UTapaoUISubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlot, 0);
}

UTapaoSaveGame* UTapaoUISubsystem::GetSave(bool bCreate)
{
	if (!Save && HasSave())
	{
		Save = Cast<UTapaoSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
		if (!Save)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: the save %s cannot be read, a new campaign starts"), *SaveSlot);
		}
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
	Save->LastPlayed = FDateTime::Now();
	if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoUI: the save %s could not be written"), *SaveSlot);
	}
}

#undef LOCTEXT_NAMESPACE
