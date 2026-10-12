// monke_island

#include "Misc/AutomationTest.h"
#include "Shop/ShopCatalog.h"
#include "Shop/ShopTypes.h"
#include "UObject/Package.h"
#include "Weapons/AmmoDefinition.h"
#include "Weapons/CutlassWeapon.h"
#include "Weapons/GrenadeWeapon.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/MusketWeapon.h"
#include "Weapons/PirateMusketRules.h"
#include "Weapons/PistolRules.h"
#include "Weapons/PistolWeapon.h"
#include "Weapons/ShotgunRules.h"
#include "Weapons/TorchItem.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Shop
BEGIN_DEFINE_SPEC(FShopSpec, "monke_island.Shop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** Categories of the sections, in order */
	static TArray<EShopCategory> CategoriesOf(const TArray<FShopSection>& Sections)
	{
		TArray<EShopCategory> Out;
		for (const FShopSection& Section : Sections)
		{
			Out.Add(Section.Category);
		}
		return Out;
	}

	/** An item definition whose actor is Class (no asset) */
	static UInventoryItemDefinition* ItemOf(UClass* Class)
	{
		UInventoryItemDefinition* Item = NewObject<UInventoryItemDefinition>(GetTransientPackage());
		Item->ItemClass = Class;
		return Item;
	}

END_DEFINE_SPEC(FShopSpec)

void FShopSpec::Define()
{
	Describe("Judge", [this]()
	{
		It("sells when the offer exists, the buyer is near, has the money and the room", [this]()
		{
			TestEqual(TEXT("bought"), ShopRules::Judge(true, true, 300, 300, true), EShopResult::Bought);
		});

		It("checks the offer, the reach, the money, then the room, in that order", [this]()
		{
			TestEqual(TEXT("offer"), ShopRules::Judge(false, false, 999, 0, false), EShopResult::InvalidOffer);
			TestEqual(TEXT("reach"), ShopRules::Judge(true, false, 999, 0, false), EShopResult::TooFar);
			TestEqual(TEXT("money"), ShopRules::Judge(true, true, 301, 300, false), EShopResult::NotEnoughMoney);
			TestEqual(TEXT("room"), ShopRules::Judge(true, true, 300, 300, false), EShopResult::InventoryFull);
		});
	});

	Describe("Ammo", [this]()
	{
		It("gives 15 rounds to each firearm (owner request 2026-10-06)", [this]()
		{
			TestEqual(TEXT("pistol"), GetDefault<UPistolRules>()->MaxCharges, 15);
			TestEqual(TEXT("shotgun"), GetDefault<UShotgunRules>()->MaxShells, 15);
			TestEqual(TEXT("musket"), GetDefault<UPirateMusketRules>()->MaxBalls, 15);
			TestFalse(TEXT("kept when empty: it reloads (owner 2026-10-11, T-329)"), GetDefault<UPistolRules>()->bDiscardWhenEmpty);
		});
	});

	// owner request 2026-10-11 (T-320): "organiser la boutique"
	Describe("Arrange", [this]()
	{
		It("shows the groups in a fixed order whatever the catalog order: firearms, blades, explosives, ammunition, tools, consumables", [this]()
		{
			const TArray<EShopCategory> Categories = {EShopCategory::Consumable, EShopCategory::Ammunition, EShopCategory::Tool,
				EShopCategory::Firearm, EShopCategory::Explosive, EShopCategory::Blade};
			const TArray<int32> Prices = {10, 20, 30, 40, 50, 60};
			const TArray<EShopCategory> Expected = {EShopCategory::Firearm, EShopCategory::Blade, EShopCategory::Explosive,
				EShopCategory::Ammunition, EShopCategory::Tool, EShopCategory::Consumable};
			TestTrue(TEXT("fixed order of the groups"), CategoriesOf(ShopRules::Arrange(Categories, Prices)) == Expected);
		});

		It("lists each group cheapest first, equal prices in catalog order", [this]()
		{
			// the catalog of the galleon in its order: musket 400, pistol 250, cutlass 300, shotgun 600, torch 120, double pistol 400
			const TArray<EShopCategory> Categories = {EShopCategory::Firearm, EShopCategory::Firearm, EShopCategory::Blade,
				EShopCategory::Firearm, EShopCategory::Tool, EShopCategory::Firearm};
			const TArray<int32> Prices = {400, 250, 300, 600, 120, 400};
			const TArray<FShopSection> Sections = ShopRules::Arrange(Categories, Prices);
			if (!TestEqual(TEXT("three groups"), Sections.Num(), 3))
			{
				return;
			}
			TestTrue(TEXT("firearms: pistol, musket, double pistol, shotgun"), Sections[0].Offers == TArray<int32>({1, 0, 5, 3}));
			TestTrue(TEXT("blades: cutlass"), Sections[1].Offers == TArray<int32>({2}));
			TestTrue(TEXT("tools: torch"), Sections[2].Offers == TArray<int32>({4}));
		});

		It("leaves empty groups out, counts Auto as a tool and keeps every catalog index once", [this]()
		{
			const TArray<EShopCategory> Categories = {EShopCategory::Auto, EShopCategory::Ammunition, EShopCategory::Tool, EShopCategory::Ammunition};
			const TArray<int32> Prices = {5, 90, 3, 40};
			const TArray<FShopSection> Sections = ShopRules::Arrange(Categories, Prices);
			TestTrue(TEXT("ammunition then tools only"), CategoriesOf(Sections) == TArray<EShopCategory>({EShopCategory::Ammunition, EShopCategory::Tool}));
			TArray<int32> Seen;
			for (const FShopSection& Section : Sections)
			{
				Seen.Append(Section.Offers);
			}
			Seen.Sort();
			TestTrue(TEXT("every offer once (a purchase names its catalog index)"), Seen == TArray<int32>({0, 1, 2, 3}));
			if (Sections.Num() == 2)
			{
				TestTrue(TEXT("Auto among the tools, by price"), Sections[1].Offers == TArray<int32>({2, 0}));
			}
		});

		It("shows nothing for an empty catalog and bounds the offers by the shorter list", [this]()
		{
			TestEqual(TEXT("empty"), ShopRules::Arrange(TArray<EShopCategory>(), TArray<int32>()).Num(), 0);
			const TArray<FShopSection> Sections = ShopRules::Arrange(TArray<EShopCategory>({EShopCategory::Firearm, EShopCategory::Blade}), TArray<int32>({100}));
			TestEqual(TEXT("one offer"), Sections.Num(), 1);
		});
	});

	Describe("Category", [this]()
	{
		It("deduces the group from what the offer gives", [this]()
		{
			FShopOffer Offer;
			Offer.Item = ItemOf(APistolWeapon::StaticClass());
			TestEqual(TEXT("pistol: firearm"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Firearm);
			Offer.Item = ItemOf(AMusketWeapon::StaticClass());
			TestEqual(TEXT("first musket: firearm"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Firearm);
			Offer.Item = ItemOf(AGrenadeWeapon::StaticClass());
			TestEqual(TEXT("grenade: explosive, though a firearm in the code"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Explosive);
			Offer.Item = ItemOf(ACutlassWeapon::StaticClass());
			TestEqual(TEXT("cutlass: blade"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Blade);
			Offer.Item = ItemOf(ATorchItem::StaticClass());
			TestEqual(TEXT("torch: tool"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Tool);
			Offer.Item = ItemOf(nullptr);
			TestEqual(TEXT("item without actor: tool"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Tool);

			FShopOffer Ammo;
			Ammo.Ammo = NewObject<UAmmoDefinition>(GetTransientPackage());
			TestEqual(TEXT("packet of ammunition"), UShopCatalog::ResolveCategory(Ammo), EShopCategory::Ammunition);
			TestEqual(TEXT("nothing given: tool"), UShopCatalog::ResolveCategory(FShopOffer()), EShopCategory::Tool);
		});

		It("keeps a category set in the catalog", [this]()
		{
			FShopOffer Offer;
			Offer.Item = ItemOf(APistolWeapon::StaticClass());
			Offer.Category = EShopCategory::Consumable;
			TestEqual(TEXT("set by hand"), UShopCatalog::ResolveCategory(Offer), EShopCategory::Consumable);
			UShopCatalog* Catalog = NewObject<UShopCatalog>(GetTransientPackage());
			Catalog->Offers.Add(Offer);
			TestEqual(TEXT("through the catalog"), Catalog->GetOfferCategory(0), EShopCategory::Consumable);
			TestEqual(TEXT("out of range"), Catalog->GetOfferCategory(3), EShopCategory::Tool);
		});
	});

	// owner request 2026-10-11 (T-320): "pouvoir adapter la vue et défiler vers le bas"
	Describe("Screen", [this]()
	{
		It("keeps its width and its content's height when the screen has room", [this]()
		{
			const FShopScreenLayout Layout{};
			const FVector2D Size = Layout.Fit(FVector2D(1920.0, 1080.0), 600.0);
			TestEqual(TEXT("width"), Size.X, static_cast<double>(Layout.Width), 0.01);
			TestEqual(TEXT("height of a short list"), Size.Y, 600.0, 0.01);
		});

		It("caps the height at a share of the screen: the list scrolls beyond", [this]()
		{
			const FShopScreenLayout Layout{};
			const FVector2D Size = Layout.Fit(FVector2D(1920.0, 1080.0), 2400.0);
			TestEqual(TEXT("height"), Size.Y, 1080.0 * Layout.MaxHeightFraction, 0.01);
			TestTrue(TEXT("room left above and below"), Size.Y < 1080.0);
		});

		It("narrows on a narrow screen and draws nothing without one", [this]()
		{
			const FShopScreenLayout Layout{};
			TestEqual(TEXT("width on 800 units"), Layout.Fit(FVector2D(800.0, 1200.0), 300.0).X, 800.0 * Layout.MaxWidthFraction, 0.01);
			const FVector2D None = Layout.Fit(FVector2D::ZeroVector, 500.0);
			TestTrue(TEXT("zero"), None.IsNearlyZero());
		});
	});
}

#endif
