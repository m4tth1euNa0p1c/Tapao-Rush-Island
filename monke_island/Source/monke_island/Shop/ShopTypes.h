// monke_island

#pragma once

#include "CoreMinimal.h"
#include "ShopTypes.generated.h"

class UAmmoDefinition;
class UInventoryItemDefinition;

/** Answer of the server to a purchase at the shop (ADR-007 §8) */
UENUM(BlueprintType)
enum class EShopResult : uint8
{
	/** Paid and given */
	Bought,
	/** No such offer */
	InvalidOffer,
	/** The buyer is too far from the merchant, dead or not a player */
	TooFar,
	/** Not enough money in the treasury */
	NotEnoughMoney,
	/** No free cell in the inventory */
	InventoryFull,
	/** Too many purchases at once */
	TooFast
};

/**
 *  Group of the shop screen an offer is listed in (owner request 2026-10-11: "organiser la boutique"); the groups are
 *  shown in the order of this enum. Auto: deduced from what the offer gives (UShopCatalog::ResolveCategory)
 */
UENUM(BlueprintType)
enum class EShopCategory : uint8
{
	/** Deduced from what the offer gives: ammunition, firearm, explosive, blade, consumable, else tools and objects */
	Auto,
	/** Muskets, pistols, shotguns, blunderbuss, rifle, machine gun ("Armes à feu") */
	Firearm,
	/** Cutlass and the other blades ("Armes blanches") */
	Blade,
	/** Grenades and the other things thrown that explode ("Explosifs") */
	Explosive,
	/** Packets of ammunition for the weapons ("Munitions") */
	Ammunition,
	/** Torch and the other tools or objects ("Outils et objets") */
	Tool,
	/** What is eaten, drunk or used up ("Consommables") */
	Consumable
};

/** One thing the merchant sells */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FShopOffer
{
	GENERATED_BODY()

	/** Item given (DA_Item_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<UInventoryItemDefinition> Item;

	/** Ammunition given instead of an item (DA_Ammo_*, Item left empty; owner 2026-10-09): poured into the pouches of
	 *  the buyer's weapons of its kinds; refused (InventoryFull) without such a weapon with room */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<UAmmoDefinition> Ammo;

	/** Price (crowns), taken from the crew's treasury */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop", meta=(ClampMin="0"))
	int32 Price = 100;

	/** Line under the name ("15 balles", "4 cartouches"...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	FText Description;

	/** Group of the shop screen (owner 2026-10-11); Auto deduces it from Item or Ammo (UShopCatalog::ResolveCategory) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	EShopCategory Category = EShopCategory::Auto;
};

/** One group of the shop screen: its category and its offers (indices in UShopCatalog::Offers), in the order shown */
struct FShopSection
{
	EShopCategory Category = EShopCategory::Tool;
	TArray<int32> Offers;
};

/** Pure rules of the shop, tested by monke_island.Shop */
namespace ShopRules
{
	/**
	 *  Order of the shop screen (owner request 2026-10-11): the offers grouped by category in the order of EShopCategory,
	 *  each group cheapest first, equal prices in catalog order, empty groups left out. Categories and Prices are indexed
	 *  like the catalog's offers (the shorter one bounds them); Auto counts as Tool (resolve it first). The sections only
	 *  hold catalog indices: a purchase still names the catalog index the server judges.
	 */
	inline TArray<FShopSection> Arrange(TConstArrayView<EShopCategory> Categories, TConstArrayView<int32> Prices)
	{
		const int32 Num = FMath::Min(Categories.Num(), Prices.Num());
		const auto GroupOf = [&Categories](int32 Index)
		{
			return Categories[Index] == EShopCategory::Auto ? EShopCategory::Tool : Categories[Index];
		};
		TArray<int32> Order;
		Order.Reserve(Num);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Order.Add(Index);
		}
		Order.Sort([&GroupOf, &Prices](int32 A, int32 B)
		{
			if (GroupOf(A) != GroupOf(B))
			{
				return GroupOf(A) < GroupOf(B);
			}
			return Prices[A] != Prices[B] ? Prices[A] < Prices[B] : A < B;
		});
		TArray<FShopSection> Sections;
		for (const int32 Index : Order)
		{
			if (Sections.IsEmpty() || Sections.Last().Category != GroupOf(Index))
			{
				Sections.AddDefaulted_GetRef().Category = GroupOf(Index);
			}
			Sections.Last().Offers.Add(Index);
		}
		return Sections;
	}

	/** What the server answers, the checks in order: offer, reach, money, room */
	inline EShopResult Judge(bool bValidOffer, bool bInReach, int32 Price, int32 Treasury, bool bHasRoom)
	{
		if (!bValidOffer)
		{
			return EShopResult::InvalidOffer;
		}
		if (!bInReach)
		{
			return EShopResult::TooFar;
		}
		if (Price > Treasury)
		{
			return EShopResult::NotEnoughMoney;
		}
		return bHasRoom ? EShopResult::Bought : EShopResult::InventoryFull;
	}
}
