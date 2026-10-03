// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/FloodTimerMath.h"

class STextBlock;

/** Look of the flood timer, copied from UHudTimerRules so the view holds no UObject */
struct MONKE_ISLAND_API FFloodTimerStyle
{
	float TopMargin = 24.0f;
	int32 ClockFontSize = 24;
	int32 NotificationFontSize = 18;
	float PulseDuration = 0.6f;
	float PulseScale = 1.15f;
	FLinearColor CountdownColor = FLinearColor::White;
	FLinearColor UrgentColor = FLinearColor(1.0f, 0.66f, 0.22f, 1.0f);
	FLinearColor EvacuationColor = FLinearColor(0.55f, 0.85f, 1.0f, 1.0f);
	FLinearColor PanelColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);
	FLinearColor EvacuationPanelColor = FLinearColor(0.0f, 0.1f, 0.25f, 0.88f);
	FLinearColor ZoneSecuredColor = FLinearColor(0.62f, 0.96f, 0.62f, 1.0f);
	FLinearColor PulseColor = FLinearColor::White;

	/** Water icon (its texture is kept alive by the owning UFloodTimerWidget); no resource = "~" glyph */
	FSlateBrush WaterIcon;
};

/**
 *  Slate view of the flood timer, top centre of the safe zone (no asset needed):
 *    [ Montée des eaux dans 1:45 ]            countdown, sand text on a dark panel
 *    [ ~ Évacuation ! Extraction dans 0:42 ]  evacuation, blue panel and water icon
 *      Zone sécurisée : +2:00                 notification line, a few seconds
 *  Passive: UFloodTimerWidget pushes the texts; the view only animates the alert pulse (no tick of its own).
 */
class MONKE_ISLAND_API SFloodTimer : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SFloodTimer) {}
		SLATE_ARGUMENT(FFloodTimerStyle, Style)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows a mode with its clock line; Hidden collapses the panel */
	void SetClock(EFloodTimerMode InMode, const FText& Text, bool bUrgent);

	/** Shows or hides the notification line */
	void SetNotification(const FText& Text, bool bVisible);

	/** Sets the water icon texture once it is loaded (the owner keeps it alive); nullptr = "~" glyph */
	void SetWaterIcon(UObject* Texture);

	/** Plays the alert pulse (scale and flash) once */
	void Pulse();

private:

	bool HasIconTexture() const;
	EVisibility GetPanelVisibility() const;
	EVisibility GetIconVisibility() const;
	EVisibility GetNotificationVisibility() const;
	FSlateColor GetClockColor() const;
	FSlateColor GetPanelColor() const;
	TOptional<FSlateRenderTransform> GetPulseTransform() const;

	FFloodTimerStyle Style;
	EFloodTimerMode Mode = EFloodTimerMode::Hidden;
	bool bUrgent = false;
	bool bNotificationVisible = false;
	FCurveSequence PulseSequence;
	FCurveHandle PulseCurve;
	TSharedPtr<STextBlock> ClockText;
	TSharedPtr<STextBlock> NotificationText;
};
