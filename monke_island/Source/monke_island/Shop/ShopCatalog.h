// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shop/ShopTypes.h"
#include "ShopCatalog.generated.h"

/**
 *  Where the shop screen sits on the screen (owner request 2026-10-11: "adapter la vue et défiler vers le bas"). Slate
 *  units of the 1920 x 1080 reference: the viewport's DPI scale applies, so the window keeps its proportions from
 *  1280 x 720 to 2560 x 1440. The window hugs its content up to MaxHeightFraction of the screen; the list of offers
 *  scrolls beyond, the header (title, treasury) and the footer (keys, close) stay fixed.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FShopScreenLayout
{
	GENERATED_BODY()

	/** Width of the window when the screen has room */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screen", meta=(ClampMin="320"))
	float Width = 900.0f;

	/** Largest share of the screen's width the window takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screen", meta=(ClampMin="0.3", ClampMax="1"))
	float MaxWidthFraction = 0.92f;

	/** Largest share of the screen's height the window takes; the list of offers scrolls beyond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screen", meta=(ClampMin="0.3", ClampMax="1"))
	float MaxHeightFraction = 0.86f;

	/** Room kept around the chosen offer when the keys or the pad scroll the list to it (the title of its group stays in view) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screen", meta=(ClampMin="0"))
	float NavigationPadding = 64.0f;

	/** Speed of the mouse wheel over the list (1 = Slate's step of 32 units a notch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screen", meta=(ClampMin="0.1"))
	float WheelScrollMultiplier = 2.0f;

	/** Size of the window on a screen of Available (Slate units) for a content ContentHeight tall: Width, at most
	 *  MaxWidthFraction of the screen's width, and the content's height, at most MaxHeightFraction of the screen's height */
	FVector2D Fit(const FVector2D& Available, double ContentHeight) const
	{
		const double MaxWidth = FMath::Max(Available.X, 0.0) * FMath::Clamp(static_cast<double>(MaxWidthFraction), 0.1, 1.0);
		const double MaxHeight = FMath::Max(Available.Y, 0.0) * FMath::Clamp(static_cast<double>(MaxHeightFraction), 0.1, 1.0);
		return FVector2D(FMath::Min(FMath::Max(static_cast<double>(Width), 0.0), MaxWidth), FMath::Min(FMath::Max(ContentHeight, 0.0), MaxHeight));
	}
};

/** What the merchant of the galleon sells and how (DA_ShopCatalog, ADR-007 §8): offers, prices, reach, rate */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UShopCatalog : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Title of the shop screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	FText Title = NSLOCTEXT("Shop", "Title", "Le marchand du galion");

	/** Offers; the screen groups them by category (EShopCategory order), cheapest first, whatever their order here */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	TArray<FShopOffer> Offers;

	/** Server: farthest a buyer may stand from the merchant (cm, interaction reach + latency) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop", meta=(ClampMin="0", Units="cm"))
	float BuyReach = 450.0f;

	/** Server: shortest time between two purchases of one player (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop", meta=(ClampMin="0", Units="s"))
	float BuyCooldown = 0.3f;

	/** Size and scrolling of the shop screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screen")
	FShopScreenLayout Screen;

	/** Group of an offer on the shop screen (ResolveCategory), Tool out of range */
	UFUNCTION(BlueprintPure, Category="Shop")
	EShopCategory GetOfferCategory(int32 Offer) const;

	/**
	 *  Group of an offer: its Category, else deduced from what it gives (what the screen shows: the item when there is
	 *  one, else the packet of ammunition): ammunition; a grenade (an AFirearmWeapon in the code) is an explosive; a
	 *  firearm (AFirearmWeapon, AMusketWeapon); a blade (ACutlassWeapon); an item class named *Consumable*; else a tool
	 */
	static EShopCategory ResolveCategory(const FShopOffer& Offer);
};
