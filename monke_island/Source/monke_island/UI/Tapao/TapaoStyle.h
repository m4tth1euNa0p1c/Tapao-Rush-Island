// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/GCObject.h"

class UTapaoUISkin;
class UTexture2D;

/** Font roles of the kit */
enum class ETapaoFont : uint8
{
	Title,
	UI,
	Text
};

/**
 *  Slate brushes and styles built once from UTapaoUISkin: 9-slice boxes, buttons with their 4 states, fonts.
 *  Keeps the skin and its textures alive while screens draw them.
 */
class MONKE_ISLAND_API FTapaoStyle : public FGCObject
{
public:

	explicit FTapaoStyle(UTapaoUISkin* InSkin);

	/** Brush of a kit sprite (an empty brush when missing) */
	const FSlateBrush* Brush(FName Sprite) const;

	/** Brush of a plain texture (backgrounds, fills, items) */
	const FSlateBrush* TextureBrush(UTexture2D* Texture) const;

	/** Button style of a kit button sprite (btn_jouer...) with its hover / pressed / disabled images */
	const FButtonStyle& Button(FName Sprite) const;

	/** On screen size of a sprite (px at 1920 x 1080) */
	FVector2D Size(FName Sprite) const;

	/** Font of a role at a size (1920 x 1080 reference) */
	FSlateFontInfo Font(ETapaoFont Role, int32 Size, int32 OutlinePx = 0) const;

	UTapaoUISkin* GetSkin() const { return Skin; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FTapaoStyle"); }

private:

	FSlateBrush MakeBrush(UTexture2D* Texture, const FVector2D& SizePx, const FMargin& Margin) const;

	TObjectPtr<UTapaoUISkin> Skin;
	mutable TMap<FName, TSharedRef<FSlateBrush>> Brushes;
	/** Textures of the skin only (kept by the skin) */
	mutable TMap<const UTexture2D*, TSharedRef<FSlateBrush>> TextureBrushes;
	mutable TMap<FName, TSharedRef<FButtonStyle>> Buttons;
	FSlateBrush Empty;

	/** Composite fonts built from the font faces of the skin */
	mutable TMap<const UObject*, TSharedPtr<const struct FCompositeFont>> Composites;
};
