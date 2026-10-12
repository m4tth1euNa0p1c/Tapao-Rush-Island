// monke_island

#include "UI/Tapao/STapaoSpellSlot.h"
#include "UI/Tapao/TapaoSpellMath.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/SlateResourceHandle.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** The picture sits this far inside the frame (px) */
	constexpr float PictureInset = 2.0f;

	/** The seconds of the recharge and the key, as shares of the side of the slot */
	constexpr float SecondsFontShare = 0.42f;
	constexpr float KeyFontShare = 0.24f;
	constexpr float KeyCapShare = 0.36f;

	/** Dark veil of the recharge, and the picture of a spell that cannot be cast now */
	const FLinearColor VeilColor(0.0f, 0.0f, 0.0f, 0.66f);
	const FLinearColor DimColor(0.42f, 0.42f, 0.42f, 1.0f);

	/** The outline that runs out while the spell plays (px), and the glow that breathes over the picture meanwhile */
	constexpr float ActiveOutline = 3.0f;
	constexpr float GlowAlpha = 0.1f;
	constexpr float GlowPulse = 0.07f;
	constexpr float GlowPeriod = 1.2f;

	/** Alpha of the ready flash at its start */
	constexpr float FlashAlpha = 0.6f;
}

void STapaoSpellSlot::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Icon = InArgs._Icon;
	KeyText = InArgs._KeyText;
	CooldownFraction = InArgs._CooldownFraction;
	CooldownSeconds = InArgs._CooldownSeconds;
	ActiveFraction = InArgs._ActiveFraction;
	Usable = InArgs._Usable;
	Size = FMath::Max(InArgs._Size, 8.0f);
	FlashSeconds = FMath::Max(InArgs._FlashSeconds, 0.0f);
	// the TAPAO palette (gold frame, cream text) when the skin is there
	if (const UTapaoUISkin* Skin = Style.IsValid() ? Style->GetSkin() : nullptr)
	{
		Gold = Skin->Gold;
		Cream = Skin->Cream;
		Frame.OutlineSettings.Color = Gold;
		KeyCap.OutlineSettings.Color = Gold;
	}
	// the recharge, its seconds and the outline change every frame
	ForceVolatile(true);
	SetCanTick(false);
}

FVector2D STapaoSpellSlot::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(Size, Size);
}

void STapaoSpellSlot::PaintSweep(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer, float Fraction, const FLinearColor& Color) const
{
	const FVector2f Local = UE::Slate::CastToVector2f(Geometry.GetLocalSize());
	const float Side = FMath::Min(Local.X, Local.Y);
	TapaoSpellMath::SquareSweep(Side, Fraction, Border);
	if (Border.Num() < 2)
	{
		return;
	}
	// a fan from the centre over the border still to run, in the colour of its vertices (no texture: Slate's white)
	const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
	const FColor Packed = Color.ToFColorSRGB();
	FanVertices.Reset();
	FanIndices.Reset();
	FanVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(0.5f * Side, 0.5f * Side), FVector2f::ZeroVector, Packed));
	for (const FVector2f& Point : Border)
	{
		FanVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Point, FVector2f::ZeroVector, Packed));
	}
	for (int32 Index = 1; Index + 1 < FanVertices.Num(); ++Index)
	{
		FanIndices.Add(0);
		FanIndices.Add(static_cast<SlateIndex>(Index));
		FanIndices.Add(static_cast<SlateIndex>(Index + 1));
	}
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, FSlateResourceHandle(), FanVertices, FanIndices, nullptr, 0, 0);
}

void STapaoSpellSlot::PaintBorderRun(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer, float Fraction, const FLinearColor& Color, float Thickness) const
{
	const FVector2f Local = UE::Slate::CastToVector2f(Geometry.GetLocalSize());
	const float Side = FMath::Min(Local.X, Local.Y);
	// the line runs on the square shrunk by half its thickness, so that it stays inside the slot
	TapaoSpellMath::SquareSweep(Side - Thickness, Fraction, Border);
	if (Border.Num() < 2)
	{
		return;
	}
	TArray<FVector2f> Points;
	Points.Reserve(Border.Num());
	const FVector2f Offset(0.5f * Thickness, 0.5f * Thickness);
	for (const FVector2f& Point : Border)
	{
		Points.Add(Point + Offset);
	}
	FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, true, Thickness);
}

int32 STapaoSpellSlot::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const ESlateDrawEffect Effects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FVector2f Local = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float Side = FMath::Min(Local.X, Local.Y);
	const float Active = ActiveFraction.Get(-1.0f);
	const bool bActive = Active >= 0.0f;
	const float Fraction = FMath::Clamp(CooldownFraction.Get(0.0f), 0.0f, 1.0f);
	const float Seconds = CooldownSeconds.Get(0.0f);
	const bool bRecharging = !bActive && Fraction > 0.0f && Seconds > 0.0f;
	const bool bDimmed = !bActive && !bRecharging && !Usable.Get(true);
	const double Now = FSlateApplication::Get().GetCurrentTime();

	// the ready flash once the recharge is over
	if (bWasRecharging && !bRecharging && !bActive)
	{
		ReadyAt = Now;
	}
	bWasRecharging = bRecharging;

	int32 Layer = LayerId;
	// the dark square, then the picture (dimmed while the spell cannot be cast)
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), &Back, Effects, Back.GetTint(InWidgetStyle) * Tint);
	if (const FSlateBrush* Picture = Icon.Get(nullptr); Picture && Picture->DrawAs != ESlateBrushDrawType::NoDrawType)
	{
		const FVector2f PictureSize(FMath::Max(Side - 2.0f * PictureInset, 1.0f), FMath::Max(Side - 2.0f * PictureInset, 1.0f));
		FSlateDrawElement::MakeBox(OutDrawElements, ++Layer, AllottedGeometry.ToPaintGeometry(PictureSize, FSlateLayoutTransform(FVector2f(PictureInset, PictureInset))),
			Picture, Effects, Picture->GetTint(InWidgetStyle) * Tint * (bDimmed ? DimColor : FLinearColor::White));
	}

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto DrawCentredText = [&](const FText& Text, const FSlateFontInfo& Font, const FVector2f& Centre, const FLinearColor& Color, int32 TextLayer)
	{
		const FVector2f TextSize = UE::Slate::CastToVector2f(Measure->Measure(Text, Font));
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Centre - 0.5f * TextSize)),
			Text, Font, Effects, Color);
	};

	if (bRecharging)
	{
		// the veil turns away clockwise from 12 o'clock as the recharge runs out, the seconds left in the middle
		PaintSweep(AllottedGeometry, OutDrawElements, ++Layer, Fraction, VeilColor * Tint);
		const FSlateFontInfo SecondsFont = Style.IsValid() ? Style->Font(ETapaoFont::UI, FMath::RoundToInt32(Side * SecondsFontShare), 2)
			: FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt32(Side * SecondsFontShare));
		DrawCentredText(FText::AsNumber(FMath::CeilToInt32(Seconds)), SecondsFont, FVector2f(0.5f * Side, 0.5f * Side), Cream * Tint, ++Layer);
	}
	if (bActive)
	{
		// it plays: a soft glow breathes over the picture and the outline runs out with the time left
		const float Breath = GlowAlpha + GlowPulse * FMath::Sin(2.0f * UE_PI * static_cast<float>(FMath::Fmod(Now, static_cast<double>(GlowPeriod))) / GlowPeriod);
		FSlateDrawElement::MakeBox(OutDrawElements, ++Layer, AllottedGeometry.ToPaintGeometry(), &Flash, Effects,
			FLinearColor(Cream.R, Cream.G, Cream.B, FMath::Clamp(Breath, 0.0f, 1.0f)) * Tint);
	}
	// the gold frame (fainter on a spell that cannot be cast): a rounded box takes its fill from the tint (none here) and
	// draws its outline in its own colour
	FLinearColor FrameColour = Gold * Tint;
	FrameColour.A *= bDimmed ? 0.5f : 1.0f;
	Frame.OutlineSettings.Color = FSlateColor(FrameColour);
	FSlateDrawElement::MakeBox(OutDrawElements, ++Layer, AllottedGeometry.ToPaintGeometry(), &Frame, Effects, FLinearColor::Transparent);
	if (bActive)
	{
		PaintBorderRun(AllottedGeometry, OutDrawElements, ++Layer, Active, FLinearColor(Cream.R, Cream.G, Cream.B, 1.0f) * Tint, ActiveOutline);
	}
	// the flash of a spell ready again
	if (FlashSeconds > 0.0f && Now - ReadyAt < FlashSeconds)
	{
		const float Alpha = FlashAlpha * (1.0f - static_cast<float>((Now - ReadyAt) / FlashSeconds));
		FSlateDrawElement::MakeBox(OutDrawElements, ++Layer, AllottedGeometry.ToPaintGeometry(), &Flash, Effects, FLinearColor(1.0f, 1.0f, 1.0f, Alpha) * Tint);
	}
	// the key, bottom left, in its plate
	const FText Key = KeyText.Get(FText::GetEmpty());
	if (!Key.IsEmpty())
	{
		const float Cap = FMath::RoundToFloat(Side * KeyCapShare);
		const FVector2f CapCorner(0.0f, Side - Cap);
		FSlateDrawElement::MakeBox(OutDrawElements, ++Layer, AllottedGeometry.ToPaintGeometry(FVector2f(Cap, Cap), FSlateLayoutTransform(CapCorner)),
			&KeyCap, Effects, KeyCap.GetTint(InWidgetStyle) * Tint);
		const FSlateFontInfo KeyFont = Style.IsValid() ? Style->Font(ETapaoFont::UI, FMath::RoundToInt32(Side * KeyFontShare), 1)
			: FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt32(Side * KeyFontShare));
		DrawCentredText(Key, KeyFont, CapCorner + FVector2f(0.5f * Cap, 0.5f * Cap), Cream * Tint, ++Layer);
	}
	return Layer;
}
