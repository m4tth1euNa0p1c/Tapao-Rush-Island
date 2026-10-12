// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/RenderingCommon.h"
#include "Widgets/SLeafWidget.h"

class FTapaoStyle;

/**
 *  A spell of the player in the HUD, drawn as in League of Legends (owner request 2026-10-11, T-333, the trumpet call):
 *  its square picture in a gold frame, its key in a small plate at the bottom left, a dark veil that turns away clockwise
 *  while it recharges with the seconds left in the middle, a gold outline that runs out while it plays, a flash once it
 *  is ready again, and the picture dimmed while it cannot be cast. Reads its attributes only, changes nothing.
 */
class MONKE_ISLAND_API STapaoSpellSlot : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoSpellSlot)
		: _Size(48.0f)
		, _FlashSeconds(0.35f)
		{}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		/** Picture of the spell (none: the frame alone) */
		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)
		/** Key that casts it ("A") */
		SLATE_ATTRIBUTE(FText, KeyText)
		/** Share of the recharge still to run (1 just cast, 0 ready) */
		SLATE_ATTRIBUTE(float, CooldownFraction)
		/** Seconds of the recharge still to run */
		SLATE_ATTRIBUTE(float, CooldownSeconds)
		/** Share of the spell still to play while it plays, < 0 otherwise */
		SLATE_ATTRIBUTE(float, ActiveFraction)
		/** The spell could be cast now (dimmed otherwise, when it is not recharging) */
		SLATE_ATTRIBUTE(bool, Usable)
		/** Side of the square (px at 1920 x 1080) */
		SLATE_ARGUMENT(float, Size)
		/** Length of the flash once the spell is ready again (s) */
		SLATE_ARGUMENT(float, FlashSeconds)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:

	/** The dark veil of the recharge: a fan from the centre over the part of the square still to run */
	void PaintSweep(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer, float Fraction, const FLinearColor& Color) const;

	/** The part of the square's border still to run, as a polyline */
	void PaintBorderRun(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer, float Fraction, const FLinearColor& Color, float Thickness) const;

	TSharedPtr<FTapaoStyle> Style;
	TAttribute<const FSlateBrush*> Icon;
	TAttribute<FText> KeyText;
	TAttribute<float> CooldownFraction;
	TAttribute<float> CooldownSeconds;
	TAttribute<float> ActiveFraction;
	TAttribute<bool> Usable;
	float Size = 48.0f;
	float FlashSeconds = 0.35f;

	FLinearColor Gold = FLinearColor(1.0f, 0.77f, 0.31f, 1.0f);
	FLinearColor Cream = FLinearColor::White;

	/** Dark square under the picture */
	FSlateRoundedBoxBrush Back = FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.015f, 0.01f, 0.75f), 3.0f);

	/** Gold frame (no fill); its outline colour is set at each paint (dimmed, opacity of the widget) */
	mutable FSlateRoundedBoxBrush Frame = FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.0f, FLinearColor(1.0f, 0.77f, 0.31f, 1.0f), 2.0f);

	/** Plate of the key, as the key caps of the inventory hints */
	FSlateRoundedBoxBrush KeyCap = FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), 3.0f, FLinearColor(1.0f, 0.77f, 0.31f, 1.0f), 1.0f);

	/** Plain square of the ready flash */
	FSlateRoundedBoxBrush Flash = FSlateRoundedBoxBrush(FLinearColor::White, 3.0f);

	/** Recharge state of the last paint, for the ready flash */
	mutable bool bWasRecharging = false;
	mutable double ReadyAt = -1.0e9;

	/** Points reused by every paint (no allocation per frame) */
	mutable TArray<FVector2f> Border;
	mutable TArray<FSlateVertex> FanVertices;
	mutable TArray<SlateIndex> FanIndices;
};
