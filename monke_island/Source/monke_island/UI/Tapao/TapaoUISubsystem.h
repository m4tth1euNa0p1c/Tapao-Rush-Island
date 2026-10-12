// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TapaoUISubsystem.generated.h"

class FTapaoStyle;
class IInputProcessor;
class SWidget;
class UTapaoSaveGame;
class UTapaoUISkin;
class UIslanderInventoryComponent;
class UWorld;
struct FActorsInitializedParams;
struct FRunDayReport;

/**
 *  Screens of TAPAO: Rush Island around the game (owner request 2026-10-04, maquette ui-tapao and menu flow of the
 *  second PC, menu_flow.json): title screen, main menu on L_MainMenu (JOUER continues the campaign or starts one,
 *  CONTINUER, NOUVELLE PARTIE with "overwrite?", OPTIONS, CRÉDITS, QUITTER with "quit?"), loading curtain, in game
 *  HUD, pause (Escape; P in the editor, where Escape stops the play session), NAUFRAGÉ ! once the whole crew is dead on
 *  a map without a campaign (in a campaign the day is lost, not the game: JOUR N PERDU, UTapaoDayUISubsystem, T-173;
 *  while a teammate lives the dead one watches him, UTapaoDeathUISubsystem, T-135), CONTRAT ÉCHOUÉ once the campaign is
 *  lost (after the execution, or after a crew wiped on its milestone day), ÎLE CONQUISE ! on NotifyVictory, and the
 *  campaign save (start of an island, every AutosaveSeconds, back to the menu, quit). Slate, skinned by DA_TapaoUISkin.
 *  Inventory screen (T-166): the inventory key (E) of the local player opens STapaoInventoryScreen over the game,
 *  which goes on; the mouse shows, the view and the walk stop (UI-only input) until E or Escape closes it.
 *  R-03: in game the run is read from URunStateComponent (the host's run), never from this machine's URunSubsystem;
 *  a client never writes the campaign save (the campaign is the host's). T-297 (M4): a game alone never listens, after
 *  the end screen either; a hosted game takes DA_TapaoUISkin.MaxPlayers places. T-302: the campaign save is read and
 *  written through MonkeSaveSlot; what went wrong with a save is told under the main menu and in the play menu.
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

	/** Opens the inventory screen of a local player's inventory, or closes it when open */
	void ToggleInventory(UIslanderInventoryComponent* Inventory);

	/** Closes the inventory screen (nothing carried by its cursor is lost: nothing moved before the server said so) */
	void CloseInventory();

	/** The inventory screen is shown */
	bool IsInventoryOpen() const { return InventoryScreen.IsValid(); }

	/** The TAPAO HUD draws the gauges (the provisional canvas bars of AIslanderHUD step aside) */
	bool IsHudShown() const { return Hud.IsValid(); }

	/** The loading screen still covers the game (the arrival scene of a day waits for it, T-214) */
	bool IsCurtainShown() const { return Curtain.IsValid(); }

	/** Tests (monke.UI.Loading): the loading screen for its minimum time */
	void DebugShowLoading() { ShowCurtain(); }

	/** Gives the input back to the screen still open, after the settings menu or a question closed */
	void RefocusScreen();

	/** A local player is in a game world with the viewport focused */
	bool CanPause() const;

	/** The island is conquered: shows ÎLE CONQUISE ! and saves (called by the game rules) */
	UFUNCTION(BlueprintCallable, Category="TAPAO")
	void NotifyVictory();

	/** Tests (monke.UI.DayEnd Executed): the end screen of a lost campaign (CONTRAT ÉCHOUÉ) for a made-up failed day */
	void DebugShowLostCampaign(const FRunDayReport& Report, const FText& ZoneName, int32 CampaignDay);

	/** Tests (monke.UI.DayEnd Hide): closes an end screen shown by a test */
	void DebugCloseEndScreen();

	/** URL options of the play map: "" for a game alone (it never listens, M4), "listen?MaxPlayers=N" for a hosted game */
	static FString MakePlayOptions(bool bHosted, int32 MaxPlayers);

	/** Options to start the same kind of game again from the end screen (a game alone stays closed to the network);
	 *  unset on a client: only the host starts an island */
	static TOptional<FString> MakeRestartOptions(ENetMode NetMode, int32 MaxPlayers);

	/** An address REJOINDRE may open: a host name or an IPv4 address, letters, digits, dots and dashes, with an optional
	 *  port (1-65535); no URL option, no space, at most 255 characters (m5: a longer one crashes FName) */
	static bool IsValidJoinAddress(const FString& Address);

	/** Skin of the screens (DA_TapaoUISkin) */
	UPROPERTY(Config)
	FSoftObjectPath SkinPath = FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin.DA_TapaoUISkin"));

	/** Title screen: a key leaves it after the first time, it leaves by itself at the second (s) */
	UPROPERTY(Config)
	FVector2D SplashSeconds = FVector2D(2.5, 6.0);

	/** Time between two automatic saves in game (s) */
	UPROPERTY(Config)
	float AutosaveSeconds = 30.0f;

	/** Address proposed by REJOINDRE (the host's IP, port 7777 when none is given) */
	UPROPERTY(Config)
	FString JoinAddress = TEXT("127.0.0.1");

	/** NAUFRAGÉ ! appears this long after the whole crew is seen dead (s), on a map without a campaign only: in a
	 *  campaign the day is lost, not the game (JOUR N PERDU, UTapaoDayUISubsystem, T-173) */
	UPROPERTY(Config)
	float GameOverDelay = 1.5f;

	/** A crew wiped on the milestone day of a contract without its quota loses the campaign with nobody left to execute
	 *  (no scene): its end screen (CONTRAT ÉCHOUÉ) comes this long after the wipe, once JOUR N PERDU was read (s, T-173) */
	UPROPERTY(Config)
	float LostCampaignScreenSeconds = 8.0f;

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
	void ShowEndScreen(bool bVictory, bool bExecuted = false);

	/** The end screen with its first lines given (the notice of a lost campaign, the cause of a death), then the stats */
	void ShowEndScreenLines(bool bVictory, bool bExecuted, const TArray<FText>& LeadLines);

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

	/** End screen (CONTINUER / RECOMMENCER): the play map again, the same kind of game (a game alone does not listen, M4) */
	void RestartPlay();

	/** Opens the play map with these URL options ("" solo, "listen?MaxPlayers=4" hosted) */
	void TravelToPlay(const FString& Options);

	/** Places of a hosted game (DA_TapaoUISkin) */
	int32 GetHostedMaxPlayers() const;

	/** SOLO / CRÉER UNE PARTIE / REJOINDRE (ADR-007 §1); bNewRun: the run of days starts again */
	void ShowPlayMenu(bool bNewRun);
	void ClosePlayMenu();
	void StartSolo();
	void StartHost();
	void StartJoin(const FString& Address);

	/** Starts the run of days again when the play menu was opened by NOUVELLE PARTIE */
	void ApplyPendingNewRun();

	/** In game, twice a second: death of the local player */
	void PollGame();
	void Autosave();

	/** Campaign save, loaded or new (a client creates none: the campaign is the host's) */
	UTapaoSaveGame* GetSave(bool bCreate);
	void WriteSave();
	bool HasSave() const;

	/** Reads the campaign slot once (MonkeSaveSlot: checks, bounds, damaged file put aside, notice) */
	void ReadSave();

	/** This machine may write the campaign save (not a client) */
	bool OwnsCampaignSave() const;

	/** What went wrong with the saves (campaign and run), for the main menu */
	FText GetSaveNotice() const;

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

	/** The campaign slot was read this session */
	bool bSaveRead = false;

	/** The campaign slot may be written (false when it could neither be read nor put aside) */
	bool bCampaignWritable = true;

	/** What went wrong with the campaign save, for the player */
	FText CampaignNotice;

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<SWidget> Splash;
	TSharedPtr<SWidget> MainMenu;
	TSharedPtr<SWidget> PlayMenu;
	bool bPendingNewRun = false;
	TSharedPtr<SWidget> Pause;
	TSharedPtr<SWidget> Curtain;
	TSharedPtr<SWidget> Confirm;
	TSharedPtr<SWidget> EndScreen;
	TSharedPtr<SWidget> Hud;
	TSharedPtr<class STapaoInventoryScreen> InventoryScreen;
	TSharedPtr<IInputProcessor> InputProcessor;
	FDelegateHandle InventoryRequestHandle;
	FDelegateHandle PreLoadHandle;
	FDelegateHandle ActorsHandle;
	FTimerHandle CurtainTimer;
	FTimerHandle GameTimer;
	FTimerHandle AutosaveTimer;
	double DeathTime = -1.0;

	/** Time the whole crew was seen dead (NAUFRAGÉ follows after GameOverDelay), negative before */
	double WipeTime = -1.0;

	/** World whose survived day was counted in the save (campaign: no end screen, the departure scene plays) */
	TWeakObjectPtr<const UWorld> VictoryWorld;
	bool bSplashDone = false;
	bool bPausedGame = false;
	bool bInGame = false;
};
