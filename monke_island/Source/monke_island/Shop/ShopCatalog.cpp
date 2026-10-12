// monke_island

#include "Shop/ShopCatalog.h"
#include "Items/Bandage/BandageItem.h"
#include "Weapons/AmmoDefinition.h"
#include "Weapons/CutlassWeapon.h"
#include "Weapons/FirearmWeapon.h"
#include "Weapons/GrenadeWeapon.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/MusketWeapon.h"

EShopCategory UShopCatalog::ResolveCategory(const FShopOffer& Offer)
{
	if (Offer.Category != EShopCategory::Auto)
	{
		return Offer.Category;
	}
	// the screen names and pictures the item when there is one, the packet of ammunition otherwise
	if (!Offer.Item)
	{
		return Offer.Ammo ? EShopCategory::Ammunition : EShopCategory::Tool;
	}
	const UClass* Class = Offer.Item->ItemClass.Get();
	if (!Class)
	{
		return EShopCategory::Tool;
	}
	// the grenade shares the pouch of the firearms in the code; for the buyer it is an explosive
	if (Class->IsChildOf(AGrenadeWeapon::StaticClass()))
	{
		return EShopCategory::Explosive;
	}
	if (Class->IsChildOf(AFirearmWeapon::StaticClass()) || Class->IsChildOf(AMusketWeapon::StaticClass()))
	{
		return EShopCategory::Firearm;
	}
	if (Class->IsChildOf(ACutlassWeapon::StaticClass()))
	{
		return EShopCategory::Blade;
	}
	// the bandages of T-340 heal: a consumable
	if (Class->IsChildOf(ABandageItem::StaticClass()))
	{
		return EShopCategory::Consumable;
	}
	// no other inventory consumable class yet: one named so (AConsumableItem, BP_Consumable_*) is listed with the consumables
	for (const UClass* Walk = Class; Walk && Walk != AInventoryItemActor::StaticClass(); Walk = Walk->GetSuperClass())
	{
		if (Walk->GetName().Contains(TEXT("Consumable")))
		{
			return EShopCategory::Consumable;
		}
	}
	return EShopCategory::Tool;
}

EShopCategory UShopCatalog::GetOfferCategory(int32 Offer) const
{
	return Offers.IsValidIndex(Offer) ? ResolveCategory(Offers[Offer]) : EShopCategory::Tool;
}
