// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Shop/ShopCatalog.h"
#include "Shop/ShopTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class AShopkeeperActor;
class FTapaoStyle;
class SScrollBox;
class UIslanderInventoryComponent;
struct FTapaoInventoryBrushes;
struct FTapaoShopBrushes;

/**
 *  Holds the shop window at a size fitted to the screen (owner request 2026-10-11: "adapter la vue"):
 *  FShopScreenLayout::Fit of the size it is given (the viewport in Slate units, its DPI scale applied) and of the
 *  window's own height, centred. Squeezed, the window keeps its header and its footer, and its list of offers scrolls.
 */
class MONKE_ISLAND_API STapaoShopFrame : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoShopFrame) {}
		SLATE_ARGUMENT(FShopScreenLayout, Layout)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:

	FShopScreenLayout Layout;
};

/**
 *  Shop screen of the galleon merchant (owner requests 2026-10-06 and 2026-10-11, ADR-007 §8, cahier §19), sober like
 *  the inventory, in a window fitted to the screen (STapaoShopFrame). Fixed on top: the title and the crew's treasury.
 *  In the middle, a list that scrolls (wheel, TAPAO scroll bar): the offers of DA_ShopCatalog grouped by category in a
 *  fixed order (Armes à feu, Armes blanches, Explosifs, Munitions, Outils et objets, Consommables), cheapest first,
 *  each with its picture, name, line, price and an "Acheter" button that says why when it cannot buy ("Trop cher",
 *  "il manque N c"). Fixed below: the chosen offer before / after (treasury), the server's answer, the keys, "Fermer".
 *  One offer is chosen: arrows, pad and stick move it and scroll to it, page keys and shoulders jump a group, Enter or
 *  pad A buy it, the mouse chooses the line it moves over. It only asks: UIslanderInventoryComponent::RequestBuy with
 *  the catalog index, judged by AShopkeeperActor::Sell on the server (only the host confirms, cahier §7); the treasury
 *  shown is the replicated one, never lowered here before the answer. Échap or F closes it (UTapaoShopUISubsystem),
 *  pad B too.
 */
class MONKE_ISLAND_API STapaoShopScreen : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoShopScreen) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(TWeakObjectPtr<AShopkeeperActor>, Shop)
		SLATE_ARGUMENT(TWeakObjectPtr<UIslanderInventoryComponent>, Inventory)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The server answered a purchase (Offer: catalog index) */
	void ShowResult(EShopResult Result, int32 Offer);

	/** Reads the treasury again: the replicated run state changed, or a purchase was answered */
	void RefreshTreasury();

	/** Money of the crew as this machine sees it (crowns) */
	int32 GetTreasury() const;

	/** Catalog index of the chosen offer, INDEX_NONE without offers */
	int32 GetSelectedOffer() const;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:

	/** Title and treasury */
	TSharedRef<SWidget> MakeHeader();

	/** The groups and their lines in a scroll box */
	TSharedRef<SWidget> MakeList(const TArray<FShopSection>& Sections);

	/** Title of a group with its count and a rule */
	TSharedRef<SWidget> MakeSectionTitle(const FShopSection& Section);

	/** Line of the offer at a position of the screen's order */
	TSharedRef<SWidget> MakeOfferRow(int32 Position);

	/** Keys and "Fermer" */
	TSharedRef<SWidget> MakeFooter();

	/** "Acheter" pressed (catalog index) */
	FReply HandleBuy(int32 Offer);

	/** Enter or pad A: buys the chosen offer, or says why not */
	void BuySelected();

	/** Chooses the offer at Position of the screen's order; bScroll brings it into view (keys, pad), at the top of the list with bToTop */
	void Select(int32 Position, bool bScroll, bool bToTop = false);

	/** Delta 1: first offer of the next group; -1: first offer of this group, else of the previous one */
	void JumpSection(int32 Delta);

	/** Writes the line of the chosen offer again (before / after) */
	void UpdateSelectionText();

	/** Offer at a position of the screen's order, null out of range or once the catalog is gone */
	const FShopOffer* GetOfferAt(int32 Position) const;

	TSharedPtr<FTapaoStyle> Style;

	/** Sober brushes of the inventory (cells, panel, key caps), and the shop's own (buttons, rules, mark of the chosen line) */
	TSharedPtr<FTapaoInventoryBrushes> Brushes;
	TSharedPtr<FTapaoShopBrushes> ShopBrushes;

	TWeakObjectPtr<AShopkeeperActor> Shop;
	TWeakObjectPtr<UIslanderInventoryComponent> Inventory;

	/** Catalog shown, kept alive while the screen is open (the shopkeeper's soft default holds nothing; its icons are drawn) */
	TStrongObjectPtr<UShopCatalog> Catalog;
	FSimpleDelegate OnClose;

	/** Size and scrolling of the window, from the catalog (DA_ShopCatalog > Screen) */
	FShopScreenLayout Layout;

	/** Catalog indices in the order of the screen (groups, then price) */
	TArray<int32> Order;

	/** Position in Order of the first offer of each group */
	TArray<int32> SectionStarts;

	/** Line of each position, scrolled into view when chosen */
	TArray<TSharedPtr<SWidget>> Rows;

	/** "il manque N c" of each position, written when the treasury changes */
	TArray<FText> MissingTexts;

	/** The list of offers */
	TSharedPtr<SScrollBox> List;

	/** Position in Order of the chosen offer */
	int32 Selected = INDEX_NONE;

	/** Treasury as last read (RefreshTreasury), and its line */
	int32 Treasury = 0;
	FText TreasuryText;

	/** Chosen offer before / after */
	FText SelectionText;

	/** Labels of the buy buttons, made once */
	FText BuyLabel;
	FText TooDearLabel;

	/** Last answer of the server, under the list */
	FText Status;
	FLinearColor StatusColor = FLinearColor::White;
};
