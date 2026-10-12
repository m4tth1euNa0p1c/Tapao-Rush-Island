// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "TapaoUISkin.generated.h"

class UFont;
class USoundBase;
class UTexture2D;
class UWorld;

/** One sprite of the TAPAO kit (maquette ui-tapao of the second PC, ui_manifest.json) with its button states */
USTRUCT(BlueprintType)
struct FTapaoSprite
{
	GENERATED_BODY()

	/** Normal image */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprite")
	TObjectPtr<UTexture2D> Texture;

	/** Hovered state (buttons, icons, tabs, slots) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprite")
	TObjectPtr<UTexture2D> Hover;

	/** Pressed state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprite")
	TObjectPtr<UTexture2D> Pressed;

	/** Disabled state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprite")
	TObjectPtr<UTexture2D> Disabled;

	/** 9-slice margin (fractions, manifest unreal_margin); zero = drawn as a plain image */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprite")
	FMargin Margin;

	/** Size of the source image (px, @2x) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprite")
	FVector2D SizePx = FVector2D::ZeroVector;
};

/**
 *  Skin of the game interface TAPAO: Rush Island (owner request 2026-10-04: full UI from the ui-tapao maquette).
 *  Sprites, fills, item icons, fonts, colours, backgrounds, maps and texts of the screens, all drawn by the Slate
 *  screens of UTapaoUISubsystem. Filled by scripts/ui/import_tapao_ui.py from art/ui/tapao.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTapaoUISkin : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Kit sprites by manifest name (btn_jouer, panel_parchment, hud_heart...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprites")
	TMap<FName, FTapaoSprite> Sprites;

	/** Gauge fills (fill_health, fill_stamina, fill_fuel, fill_gold, fill_background) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprites")
	TMap<FName, TObjectPtr<UTexture2D>> Fills;

	/** Item thumbnails (item_torch, item_cutlass...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprites")
	TMap<FName, TObjectPtr<UTexture2D>> Items;

	/** Sprites are rendered @2x: on screen at this scale (1920 x 1080 reference) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprites", meta=(ClampMin="0.05"))
	float SpriteScale = 0.5f;

	/** Background of the main menu (a blurred view of the island) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backgrounds")
	TObjectPtr<UTexture2D> MenuBackground;

	/** Background of the loading screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backgrounds")
	TObjectPtr<UTexture2D> LoadingBackground;

	/** Titles (Pirata One): a Font, or the FontFace imported from the TTF */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fonts", meta=(AllowedClasses="/Script/Engine.Font,/Script/Engine.FontFace"))
	TObjectPtr<UObject> TitleFont;

	/** Buttons and counters (Alegreya SC Black): a Font, or the FontFace imported from the TTF */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fonts", meta=(AllowedClasses="/Script/Engine.Font,/Script/Engine.FontFace"))
	TObjectPtr<UObject> UIFont;

	/** Texts (Alegreya SC Bold): a Font, or the FontFace imported from the TTF */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fonts", meta=(AllowedClasses="/Script/Engine.Font,/Script/Engine.FontFace"))
	TObjectPtr<UObject> TextFont;

	/** Colours of the kit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Colours")
	FLinearColor Cream = FLinearColor::FromSRGBColor(FColor(0xFA, 0xE8, 0xBE));

	/** See Cream */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Colours")
	FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x28, 0x16, 0x0C));

	/** See Cream */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Colours")
	FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(0xFF, 0xC4, 0x50));

	/** Veil behind the pause and the end screens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Colours")
	FLinearColor Veil = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	/** Menu shown when this map is loaded (L_MainMenu) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow")
	TSoftObjectPtr<UWorld> MainMenuMap;

	/** Map opened by JOUER / CONTINUER / NOUVELLE PARTIE (L_Boot: it draws the island and hosts) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow")
	TSoftObjectPtr<UWorld> PlayMap;

	/**
	 *  Places of a hosted game, the host included (cahier §16: lobby of 4 places), sent as ?MaxPlayers with "listen".
	 *  A game alone (SOLO, and CONTINUER / RECOMMENCER after it) never listens (T-297, M4)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow", meta=(ClampMin="1", ClampMax="16"))
	int32 MaxPlayers = 4;

	/** Shortest time the loading screen stays (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow", meta=(ClampMin="0", Units="s"))
	float MinLoadingSeconds = 1.5f;

	/** Version shown under the main menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText VersionText;

	/** Tips drawn at random on the loading screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	TArray<FText> LoadingTips;

	/** Credits (one line each) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	TArray<FText> Credits;

	/** Played when a button is hovered */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundBase> HoverSound;

	/** Played when a button is clicked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundBase> ClickSound;
};
