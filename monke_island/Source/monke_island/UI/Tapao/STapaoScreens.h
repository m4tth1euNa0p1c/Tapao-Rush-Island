// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FTapaoStyle;
class SWidgetSwitcher;

/** A kit button (pre-written sprite such as btn_jouer) at a size, with the kit sounds */
TSharedRef<SWidget> MakeTapaoButton(const TSharedRef<FTapaoStyle>& Style, FName Sprite, const FVector2D& Size, FSimpleDelegate OnClicked, TAttribute<bool> Enabled = true);

/** A blank kit button with a written label (confirm dialogs, end screens) */
TSharedRef<SWidget> MakeTapaoTextButton(const TSharedRef<FTapaoStyle>& Style, const FText& Label, const FVector2D& Size, FSimpleDelegate OnClicked, TAttribute<bool> Enabled = true);

/**
 *  Main menu of TAPAO: Rush Island (maquette 01_menu_principal): logo, JOUER, CONTINUER, NOUVELLE PARTIE, OPTIONS,
 *  CRÉDITS, QUITTER over a blurred view of the island; info, settings and sound icons; the credits panel.
 */
class MONKE_ISLAND_API STapaoMainMenu : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoMainMenu) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ATTRIBUTE(bool, CanContinue)
		SLATE_ARGUMENT(FText, ContinueHint)
		SLATE_EVENT(FSimpleDelegate, OnPlay)
		SLATE_EVENT(FSimpleDelegate, OnContinue)
		SLATE_EVENT(FSimpleDelegate, OnNewGame)
		SLATE_EVENT(FSimpleDelegate, OnOptions)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	TSharedRef<SWidget> MakeHome(const FArguments& InArgs);
	TSharedRef<SWidget> MakeCredits();
	void ShowCredits(bool bShow);
	void ToggleSound();

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<SWidgetSwitcher> Pages;
	TSharedPtr<class SButton> SoundButton;
	bool bMuted = false;
};

/** Pause (maquette 04_pause): veil, parchment, PAUSE ribbon, REPRENDRE, OPTIONS, MENU PRINCIPAL, QUITTER */
class MONKE_ISLAND_API STapaoPause : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoPause) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnOptions)
		SLATE_EVENT(FSimpleDelegate, OnMainMenu)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
};

/** Loading (maquette 09_chargement): background, logo, CHARGEMENT ribbon, gold gauge, a tip on parchment */
class MONKE_ISLAND_API STapaoLoading : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoLoading) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(FText, Tip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** The gauge fills in a loop while the map loads (no progress is known) */
	TOptional<float> GetProgress() const;

	double StartTime = 0.0;
};

/** Title screen (menu flow of the second PC): logo and "press a key", left after MinSeconds on a key, or at MaxSeconds */
class MONKE_ISLAND_API STapaoSplash : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoSplash) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(float, MinSeconds)
		SLATE_ARGUMENT(float, MaxSeconds)
		SLATE_EVENT(FSimpleDelegate, OnDone)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:

	FReply TryLeave();
	EActiveTimerReturnType HandleTimeout(double CurrentTime, float DeltaTime);

	FSimpleDelegate OnDone;
	double StartTime = 0.0;
	float MinSeconds = 0.0f;
	bool bDone = false;
};

/** A question over the screen below: parchment, text, yes and no (Escape = no) */
class MONKE_ISLAND_API STapaoConfirm : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoConfirm) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(FText, Question)
		SLATE_ARGUMENT(FText, Yes)
		SLATE_ARGUMENT(FText, No)
		SLATE_EVENT(FSimpleDelegate, OnYes)
		SLATE_EVENT(FSimpleDelegate, OnNo)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	FSimpleDelegate OnNo;
};

/** End of a game (maquettes 07_naufrage, 08_ile_conquise): ribbon, a few lines, two buttons, over the game */
class MONKE_ISLAND_API STapaoEndScreen : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoEndScreen) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(FName, Ribbon)
		SLATE_ARGUMENT(TArray<FText>, Lines)
		SLATE_ARGUMENT(FName, FirstButton)
		SLATE_ATTRIBUTE(bool, FirstEnabled)
		SLATE_EVENT(FSimpleDelegate, OnFirst)
		SLATE_EVENT(FSimpleDelegate, OnMainMenu)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
};
