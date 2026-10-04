// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TapaoUISubsystem.generated.h"

class FTapaoStyle;
class IInputProcessor;
class SWidget;
class UTapaoSaveGame;
class UTapaoUISkin;
class UWorld;
struct FActorsInitializedParams;

/**
 *  Screens of TAPAO: Rush Island around the game (owner request 2026-10-04, maquette ui-tapao and menu flow of the
 *  second PC, menu_flow.json): title screen, main menu on L_MainMenu (JOUER continues the campaign or starts one,
 *  CONTINUER, NOUVELLE PARTIE with "overwrite?", OPTIONS, CRÉDITS, QUITTER with "quit?"), loading curtain, in game
 *  HUD, pause (Escape; P in the editor, where Escape stops the play session), NAUFRAGÉ ! when the local player dies,
 *  ÎLE CONQUISE ! on NotifyVictory, and the campaign save (start of an island, every AutosaveSeconds, back to the
 *  menu, quit). Slate, skinned by DA_TapaoUISkin.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UTapaoUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Opens or closes the pause */
	void TogglePause();

	/** Pause shown */
	bool IsPauseOpen() const { return Pause.IsValid(); }

	/** The main menu is shown (Escape does not pause there) */
	bool IsMainMenuOpen() const { return MainMenu.IsValid(); }

	/** A question or an end screen holds the input: Escape is theirs */
	bool IsModalOpen() const { return Confirm.IsValid() || EndScreen.IsValid() || Splash.IsValid(); }

	/** The TAPAO HUD draws the gauges (the provisional canvas bars of AIslanderHUD step aside) */
	bool IsHudShown() const { return Hud.IsValid(); }

	/** Gives the input back to the screen still open, after the settings menu or a question closed */
	void RefocusScreen();

	/** A local player is in a game world with the viewport focused */
	bool CanPause() const;

	/** The island is conquered: shows ÎLE CONQUISE ! and saves (called by the game rules) */
	UFUNCTION(BlueprintCallable, Category="TAPAO")
	void NotifyVictory();

	/** Skin of the screens (DA_TapaoUISkin) */
	UPROPERTY(Config)
	FSoftObjectPath SkinPath = FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin.DA_TapaoUISkin"));

	/** Slot of the campaign save */
	UPROPERTY(Config)
	FString SaveSlot = TEXT("TapaoCampaign");

	/** Title screen: a key leaves it after the first time, it leaves by itself at the second (s) */
	UPROPERTY(Config)
	FVector2D SplashSeconds = FVector2D(2.5, 6.0);

	/** Time between two automatic saves in game (s) */
	UPROPERTY(Config)
	float AutosaveSeconds = 30.0f;

	/** NAUFRAGÉ ! appears this long after the death (s) */
	UPROPERTY(Config)
	float GameOverDelay = 1.5f;

private:
	friend class FTapaoInputSpec;

	void HandlePreLoadMap(const FString& MapName);
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	void ShowSplash();
	void ShowMainMenu();
	void HideMainMenu();
	void ShowCurtain();
	void HideCurtain();
	void ShowHud();
	void OpenPause();
	void ClosePause();
	void ShowEndScreen(bool bVictory);

	/** A question over the current screen; OnYes runs after it closed */
	void AskConfirm(const FText& Question, const FText& Yes, const FText& No, FSimpleDelegate OnYes);
	void CloseConfirm();

	void OnPlay();
	void OnContinue();
	void OnNewGame();
	void OnOptions();
	void OnMainMenu();
	void OnQuit();
	void StartNewGame();
	void LeaveToMainMenu();
	void QuitNow();

	/** Opens the play map (L_Boot hosts and draws the island) */
	void TravelToPlay();

	/** In game, twice a second: death of the local player */
	void PollGame();
	void Autosave();

	/** Campaign save, loaded or new */
	UTapaoSaveGame* GetSave(bool bCreate);
	void WriteSave();
	bool HasSave() const;

	/** World of this game instance is the main menu map */
	bool IsMenuWorld(const UWorld* World) const;
	bool IsPlayWorld(const UWorld* World) const;

	void AddToViewport(const TSharedRef<SWidget>& Widget, int32 ZOrder, bool bUIOnly);
	void RemoveFromViewport(TSharedPtr<SWidget>& Widget);

	/** UI input and mouse cursor for a screen, once the local player controller exists */
	void FocusUI(TWeakPtr<SWidget> Widget, int32 Attempt);

	/** Game input back, when no screen holds it */
	void RestoreGameInput();

	UPROPERTY(Transient)
	TObjectPtr<UTapaoUISkin> Skin;

	UPROPERTY(Transient)
	TObjectPtr<UTapaoSaveGame> Save;

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<SWidget> Splash;
	TSharedPtr<SWidget> MainMenu;
	TSharedPtr<SWidget> Pause;
	TSharedPtr<SWidget> Curtain;
	TSharedPtr<SWidget> Confirm;
	TSharedPtr<SWidget> EndScreen;
	TSharedPtr<SWidget> Hud;
	TSharedPtr<IInputProcessor> InputProcessor;
	FDelegateHandle PreLoadHandle;
	FDelegateHandle ActorsHandle;
	FTimerHandle CurtainTimer;
	FTimerHandle GameTimer;
	FTimerHandle AutosaveTimer;
	double DeathTime = -1.0;
	bool bSplashDone = false;
	bool bPausedGame = false;
	bool bInGame = false;
};
