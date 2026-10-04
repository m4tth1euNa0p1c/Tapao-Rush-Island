// monke_island

#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

FTapaoStyle::FTapaoStyle(UTapaoUISkin* InSkin)
	: Skin(InSkin)
{
	Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
}

void FTapaoStyle::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Skin);
}

FSlateBrush FTapaoStyle::MakeBrush(UTexture2D* Texture, const FVector2D& SizePx, const FMargin& Margin) const
{
	FSlateBrush Result;
	if (!Texture)
	{
		Result.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Result;
	}
	Result.SetResourceObject(Texture);
	const FVector2D Source = SizePx.IsNearlyZero() ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : SizePx;
	Result.ImageSize = Source * (Skin ? Skin->SpriteScale : 0.5f);
	const bool bBox = Margin.Left > 0.0f || Margin.Top > 0.0f || Margin.Right > 0.0f || Margin.Bottom > 0.0f;
	Result.DrawAs = bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
	Result.Margin = Margin;
	return Result;
}

const FSlateBrush* FTapaoStyle::Brush(FName Sprite) const
{
	if (const TSharedRef<FSlateBrush>* Found = Brushes.Find(Sprite))
	{
		return &Found->Get();
	}
	const FTapaoSprite* Entry = Skin ? Skin->Sprites.Find(Sprite) : nullptr;
	if (!Entry)
	{
		return &Empty;
	}
	return &Brushes.Add(Sprite, MakeShared<FSlateBrush>(MakeBrush(Entry->Texture, Entry->SizePx, Entry->Margin))).Get();
}

const FSlateBrush* FTapaoStyle::TextureBrush(UTexture2D* Texture) const
{
	if (!Texture)
	{
		return &Empty;
	}
	if (const TSharedRef<FSlateBrush>* Found = TextureBrushes.Find(Texture))
	{
		return &Found->Get();
	}
	FSlateBrush Made = MakeBrush(Texture, FVector2D::ZeroVector, FMargin());
	return &TextureBrushes.Add(Texture, MakeShared<FSlateBrush>(Made)).Get();
}

const FButtonStyle& FTapaoStyle::Button(FName Sprite) const
{
	if (const TSharedRef<FButtonStyle>* Found = Buttons.Find(Sprite))
	{
		return Found->Get();
	}
	FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder");
	if (const FTapaoSprite* Entry = Skin ? Skin->Sprites.Find(Sprite) : nullptr)
	{
		// the buttons are drawn whole, scaled (their label is in the picture); the 9-slice is for the panels
		const FMargin None;
		Style.SetNormal(MakeBrush(Entry->Texture, Entry->SizePx, None));
		Style.SetHovered(MakeBrush(Entry->Hover ? Entry->Hover : Entry->Texture, Entry->SizePx, None));
		Style.SetPressed(MakeBrush(Entry->Pressed ? Entry->Pressed : Entry->Texture, Entry->SizePx, None));
		Style.SetDisabled(MakeBrush(Entry->Disabled ? Entry->Disabled : Entry->Texture, Entry->SizePx, None));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
	}
	return Buttons.Add(Sprite, MakeShared<FButtonStyle>(Style)).Get();
}

FVector2D FTapaoStyle::Size(FName Sprite) const
{
	const FTapaoSprite* Entry = Skin ? Skin->Sprites.Find(Sprite) : nullptr;
	if (!Entry || !Entry->Texture)
	{
		return FVector2D::ZeroVector;
	}
	const FVector2D Source = Entry->SizePx.IsNearlyZero() ? FVector2D(Entry->Texture->GetSizeX(), Entry->Texture->GetSizeY()) : Entry->SizePx;
	return Source * Skin->SpriteScale;
}

FSlateFontInfo FTapaoStyle::Font(ETapaoFont Role, int32 Size, int32 OutlinePx) const
{
	const UObject* Face = nullptr;
	if (Skin)
	{
		Face = Role == ETapaoFont::Title ? Skin->TitleFont.Get() : Role == ETapaoFont::UI ? Skin->UIFont.Get() : Skin->TextFont.Get();
	}
	FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("Bold", Size);
	if (const UFont* Font = Cast<UFont>(Face))
	{
		Info = FSlateFontInfo(Font, Size);
	}
	else if (const UFontFace* FontFace = Cast<UFontFace>(Face))
	{
		// a bare font face (imported TTF): one typeface around it
		TSharedPtr<const FCompositeFont>& Composite = Composites.FindOrAdd(FontFace);
		if (!Composite.IsValid())
		{
			TSharedRef<FCompositeFont> Made = MakeShared<FCompositeFont>();
			FTypefaceEntry& Entry = Made->DefaultTypeface.Fonts.Emplace_GetRef(TEXT("Regular"));
			Entry.Font = FFontData(FontFace);
			Composite = Made;
		}
		Info = FSlateFontInfo(Composite, Size);
	}
	if (OutlinePx > 0 && Skin)
	{
		Info.OutlineSettings.OutlineSize = OutlinePx;
		Info.OutlineSettings.OutlineColor = Skin->Ink;
	}
	return Info;
}
