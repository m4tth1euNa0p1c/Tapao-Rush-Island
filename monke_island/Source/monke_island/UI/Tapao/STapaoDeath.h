// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Death/DeathSpectatorComponent.h"
#include "Widgets/SCompoundWidget.h"

class FTapaoStyle;

/** What the death screen tells (T-135), refreshed by UTapaoDeathUISubsystem from the replicated state */
struct FTapaoDeathView
{
	/** Dead (the cause, big) or Following (a bar at the bottom) */
	EDeathScreen Screen = EDeathScreen::Dead;

	/** Short cause of the death (cahier §19) */
	FText Cause;

	/** Name of the followed teammate */
	FText Followed;

	/** Names of the living teammates one may follow */
	TArray<FText> Observables;
};

/**
 *  Death of the local player, over the game (T-135, cahier §4, §19; §21 WBP_Spectator in Slate, as the other TAPAO
 *  screens): "TU ES MORT" and the short cause over a light veil (the game stays visible, never a black screen), then,
 *  while the camera follows a living teammate, a bar at the bottom: whom one follows, the clicks that change him, the
 *  cause, the living teammates. Reads nothing itself: UTapaoDeathUISubsystem gives it its values. No input (the clicks
 *  go through the subsystem's input processor). Texts 20 px and more at 1080p, light on a dark plate.
 */
class MONKE_ISLAND_API STapaoDeath : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoDeath) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** New values to show */
	void SetView(const FTapaoDeathView& InView);

private:

	TSharedPtr<FTapaoStyle> Style;
	FTapaoDeathView View;

	/** "A · B", built when the view changes */
	FText ObservablesText;
};
