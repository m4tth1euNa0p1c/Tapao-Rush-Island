// monke_island

#include "UI/SFloodTimer.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MonkeFloodTimer"

namespace
{
	FSlateFontInfo TimerFont(int32 Size, const TCHAR* Typeface)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);
		return Font;
	}

	const FSlateBrush* PanelBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 8.0f);
		return &Brush;
	}
}

void SFloodTimer::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	PulseSequence = FCurveSequence();
	PulseCurve = PulseSequence.AddCurve(0.0f, FMath::Max(0.05f, Style.PulseDuration), ECurveEaseFunction::Linear);

	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SSafeZone)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.0f, Style.TopMargin, 0.0f, 0.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBorder)
					.Visibility(this, &SFloodTimer::GetPanelVisibility)
					.BorderImage(PanelBrush())
					.BorderBackgroundColor(this, &SFloodTimer::GetPanelColor)
					.Padding(FMargin(18.0f, 8.0f))
					.RenderTransform(this, &SFloodTimer::GetPulseTransform)
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(SBox)
							.Visibility(this, &SFloodTimer::GetIconVisibility)
							[
								SNew(SOverlay)
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(&Style.WaterIcon)
									.DesiredSizeOverride(FVector2D(Style.ClockFontSize * 1.4f))
									.Visibility_Lambda([this]() { return HasIconTexture() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								]
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::AsCultureInvariant(TEXT("~")))
									.Font(TimerFont(Style.ClockFontSize + 4, TEXT("Bold")))
									.ColorAndOpacity(Style.EvacuationColor)
									.Visibility_Lambda([this]() { return HasIconTexture() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SAssignNew(ClockText, STextBlock)
							.Font(TimerFont(Style.ClockFontSize, TEXT("Bold")))
							.ColorAndOpacity(this, &SFloodTimer::GetClockColor)
							.ShadowOffset(FVector2D(1.0f, 1.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(NotificationText, STextBlock)
					.Visibility(this, &SFloodTimer::GetNotificationVisibility)
					.Font(TimerFont(Style.NotificationFontSize, TEXT("Bold")))
					.ColorAndOpacity(Style.ZoneSecuredColor)
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
				]
			]
		]
	];
}

void SFloodTimer::SetClock(EFloodTimerMode InMode, const FText& Text, bool bInUrgent)
{
	Mode = InMode;
	bUrgent = bInUrgent;
	if (ClockText.IsValid() && !ClockText->GetText().EqualTo(Text))
	{
		ClockText->SetText(Text);
	}
}

void SFloodTimer::SetNotification(const FText& Text, bool bVisible)
{
	bNotificationVisible = bVisible;
	if (NotificationText.IsValid())
	{
		NotificationText->SetText(Text);
	}
}

void SFloodTimer::SetWaterIcon(UObject* Texture)
{
	Style.WaterIcon.SetResourceObject(Texture);
	if (Texture)
	{
		Style.WaterIcon.ImageSize = FVector2D(Style.ClockFontSize * 1.4f);
		Style.WaterIcon.DrawAs = ESlateBrushDrawType::Image;
	}
}

bool SFloodTimer::HasIconTexture() const
{
	return Style.WaterIcon.GetResourceObject() != nullptr;
}

void SFloodTimer::Pulse()
{
	PulseSequence.Play(AsShared());
}

EVisibility SFloodTimer::GetPanelVisibility() const
{
	return Mode == EFloodTimerMode::Hidden ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

EVisibility SFloodTimer::GetIconVisibility() const
{
	return Mode >= EFloodTimerMode::Evacuation ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SFloodTimer::GetNotificationVisibility() const
{
	return bNotificationVisible && Mode != EFloodTimerMode::Hidden ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FSlateColor SFloodTimer::GetClockColor() const
{
	const FLinearColor Base = Mode >= EFloodTimerMode::Evacuation ? Style.EvacuationColor
		: (bUrgent ? Style.UrgentColor : Style.CountdownColor);
	if (!PulseSequence.IsPlaying())
	{
		return Base;
	}
	const float Alpha = FMath::Sin(PI * PulseCurve.GetLerp());
	return FMath::Lerp(Base, Style.PulseColor, Alpha);
}

FSlateColor SFloodTimer::GetPanelColor() const
{
	return Mode >= EFloodTimerMode::Evacuation ? Style.EvacuationPanelColor : Style.PanelColor;
}

TOptional<FSlateRenderTransform> SFloodTimer::GetPulseTransform() const
{
	if (!PulseSequence.IsPlaying())
	{
		return TOptional<FSlateRenderTransform>();
	}
	const float Alpha = FMath::Sin(PI * PulseCurve.GetLerp());
	return FSlateRenderTransform(1.0f + (Style.PulseScale - 1.0f) * Alpha);
}

#undef LOCTEXT_NAMESPACE
