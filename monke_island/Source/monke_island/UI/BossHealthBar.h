// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Creatures/GiantCrabBoss.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class APawn;
class FTapaoStyle;
class UGameInstance;

/**
 *  Boss bar of the TAPAO HUD (T-261, owner 2026-10-09: "boss Dark Souls"): the name of the boss over a long thin bar at
 *  the bottom of the screen, a pale chunk that lingers on the damage just dealt, marks where its phases begin, a burning
 *  red in its frenzy; "GRAND ENNEMI ABATTU" in the middle of the screen at its death. Shown to the local player while
 *  the boss fights it (UBossBarSubsystem); reads replicated state only, decides nothing.
 */
class MONKE_ISLAND_API SBossHealthBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBossHealthBar) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(TWeakObjectPtr<UGameInstance>, GameInstance)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	/** Pawn of the local player, or nullptr */
	const APawn* GetPawn() const;

	/** Colour of the bar: deep red, burning in a frenzy */
	FSlateColor GetFillColor() const;

	/** Left margin of the mark of a later phase (hidden past the marks of the boss) */
	FMargin GetMarkPadding(int32 Index) const;

	EVisibility GetMarkVisibility(int32 Index) const;

	TSharedPtr<FTapaoStyle> Style;
	TWeakObjectPtr<UGameInstance> GameInstance;

	/** What the bar shows (the last boss seen while it shows) */
	FBossBarView View;

	/** A boss fights the local player now */
	bool bEngaged = false;

	/** Opacity of the bar (fades in and out) */
	float Alpha = 0.0f;

	/** Health shown, and the pale chunk behind it with the time it holds still */
	float Fill = 1.0f;
	float Trail = 1.0f;
	float TrailHold = 0.0f;

	/** Seconds since the boss fell (the banner), negative when none */
	float BannerTime = -1.0f;

	/** Time for the pulse of the frenzy */
	float Pulse = 0.0f;

	FSlateColorBrush BackgroundBrush = FSlateColorBrush(FLinearColor(0.015f, 0.01f, 0.01f, 0.85f));
	FSlateColorBrush FillBrush = FSlateColorBrush(FLinearColor::White);
	FSlateColorBrush MarkBrush = FSlateColorBrush(FLinearColor(1.0f, 0.95f, 0.85f, 0.6f));
	FSlateNoResource NoBrush;
	FProgressBarStyle BarStyle;
};
