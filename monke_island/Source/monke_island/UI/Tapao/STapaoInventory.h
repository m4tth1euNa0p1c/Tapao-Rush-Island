// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"

class AInventoryItemActor;
class FTapaoStyle;
class UInventoryItemDefinition;
class UIslanderInventoryComponent;
struct FSlateBrush;

/** What a cell shows, copied from the inventory when it changes (never read every frame from the game) */
struct FTapaoInventoryCellView
{
	const UInventoryItemDefinition* Definition = nullptr;
	int32 Count = 0;
	TWeakObjectPtr<AInventoryItemActor> Item;

	bool IsEmpty() const { return Definition == nullptr || Count <= 0; }
};

/**
 *  The inventory of the local player as the TAPAO widgets see it (T-166): a copy of its cells, refreshed by
 *  UIslanderInventoryComponent::OnInventoryChangedNative (no polling of the cells).
 */
class MONKE_ISLAND_API FTapaoInventoryModel : public TSharedFromThis<FTapaoInventoryModel>
{
public:

	~FTapaoInventoryModel();

	/** Follows an inventory (null to stop); nothing happens when it is already the one followed */
	void Bind(UIslanderInventoryComponent* InInventory);

	/** Inventory followed, null once gone */
	UIslanderInventoryComponent* GetInventory() const { return Inventory.Get(); }

	/** Copies the cells again */
	void Refresh();

	/** Cell view, empty out of range */
	const FTapaoInventoryCellView& GetCell(int32 Index) const;

	TArray<FTapaoInventoryCellView> Cells;
	int32 BarNum = 0;
	int32 ActiveSlot = INDEX_NONE;
	int32 BagColumns = 6;

	/** The cells changed (after the copy) */
	FSimpleMulticastDelegate OnChanged;

private:

	/** Stops following the inventory */
	void Unbind();

	TWeakObjectPtr<UIslanderInventoryComponent> Inventory;
	FDelegateHandle ChangedHandle;
};

/** Sober brushes of the inventory (dark translucent squares, fine border), built from the TAPAO colours */
struct MONKE_ISLAND_API FTapaoInventoryBrushes
{
	explicit FTapaoInventoryBrushes(const FTapaoStyle& Style);

	FSlateRoundedBoxBrush Cell;
	FSlateRoundedBoxBrush CellHovered;
	FSlateRoundedBoxBrush CellInHand;
	FSlateRoundedBoxBrush Panel;
	FSlateRoundedBoxBrush Tip;

	/** Frame of a key in the hints (the F badge of the pickup prompt: dark, a fine gold border) */
	FSlateRoundedBoxBrush KeyCap;
	FSlateColorBrush Veil;
	FLinearColor Text;
	FLinearColor Dim;
	FLinearColor Accent;
};

/** Picture of an item: the definition's Icon, else the kit picture of its class, else none (the short name is written) */
MONKE_ISLAND_API const FSlateBrush* GetTapaoItemBrush(const FTapaoStyle& Style, const UInventoryItemDefinition* Definition, const AInventoryItemActor* Item);

/** Short label of an item without a picture */
MONKE_ISLAND_API FText GetTapaoItemShortName(const UInventoryItemDefinition* Definition);

/** Tooltip of an item: name, then its state (balls, rounds, fuel, edge) or the count */
MONKE_ISLAND_API FText DescribeTapaoItem(const FTapaoInventoryCellView& Cell);

DECLARE_DELEGATE_ThreeParams(FOnTapaoCellPressed, int32 /*Index*/, FKey /*Button*/, bool /*bShift*/);
DECLARE_DELEGATE_OneParam(FOnTapaoCellIndex, int32);

/** One square of the inventory: picture or short name, count bottom right, key number top left, the hand outlined */
class MONKE_ISLAND_API STapaoInventoryCell : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoInventoryCell) : _Index(0), _Size(64.0f), _bInteractive(false), _KeyNumber(0) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(TSharedPtr<FTapaoInventoryBrushes>, Brushes)
		SLATE_ARGUMENT(TSharedPtr<FTapaoInventoryModel>, Model)
		SLATE_ARGUMENT(int32, Index)
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(bool, bInteractive)
		/** Number written top left (key of a bar cell), 0 for none */
		SLATE_ARGUMENT(int32, KeyNumber)
		/** Items lifted off this cell by the cursor (not shown here) */
		SLATE_ATTRIBUTE(int32, LiftedCount)
		SLATE_EVENT(FOnTapaoCellPressed, OnPressed)
		SLATE_EVENT(FOnTapaoCellIndex, OnDoubleClicked)
		SLATE_EVENT(FOnTapaoCellIndex, OnHovered)
		SLATE_EVENT(FOnTapaoCellIndex, OnUnhovered)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:

	/** Count shown: the cell's minus what the cursor lifted */
	int32 GetShownCount() const;

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<FTapaoInventoryBrushes> Brushes;
	TSharedPtr<FTapaoInventoryModel> Model;
	int32 Index = 0;
	bool bInteractive = false;
	TAttribute<int32> LiftedCount;
	FOnTapaoCellPressed OnPressed;
	FOnTapaoCellIndex OnDoubleClicked;
	FOnTapaoCellIndex OnHovered;
	FOnTapaoCellIndex OnUnhovered;
	TSharedPtr<class SToolTip> Tip;
};

/**
 *  Inventory screen of TAPAO (T-166, Minecraft-like, sober): the bag (3 x 6) over the quick bar (1-4), in a dark
 *  panel over the game, which goes on (multiplayer). The cursor carries a stack: left click takes or puts a stack
 *  (swaps with another item), right click takes half or puts one, shift + click sends between bar and bag, double
 *  click gathers identical items, keys 1-4 over a cell swap it with that bar cell, a click outside the panel drops
 *  what the cursor carries. The cursor only points at a cell: nothing moves before the server says so.
 */
class MONKE_ISLAND_API STapaoInventoryScreen : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoInventoryScreen) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(TWeakObjectPtr<UIslanderInventoryComponent>, Inventory)
		/** Keys that close the screen besides Escape (those of the inventory action, E by default) */
		SLATE_ARGUMENT(TArray<FKey>, CloseKeys)
		/** Keys of the bar cells (index 0 = key 1) */
		SLATE_ARGUMENT(TArray<TArray<FKey>>, BarKeys)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~STapaoInventoryScreen() override;

	/** The inventory shown, null once gone */
	UIslanderInventoryComponent* GetInventory() const;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:

	TSharedRef<SWidget> MakeGrid(int32 First, int32 Num, int32 Columns, bool bBar);
	void HandleCellPressed(int32 Index, FKey Button, bool bShift);
	void HandleCellDoubleClicked(int32 Index);
	void HandleHovered(int32 Index);
	void HandleUnhovered(int32 Index);
	void HandleModelChanged();
	void ClearHeld();
	void Close();

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<FTapaoInventoryBrushes> Brushes;
	TSharedPtr<FTapaoInventoryModel> Model;
	TArray<FKey> CloseKeys;
	TArray<TArray<FKey>> BarKeys;
	FSimpleDelegate OnClose;
	FDelegateHandle ModelHandle;

	/** Cell the cursor carries items from (INDEX_NONE = empty cursor), and how many */
	int32 HeldFrom = INDEX_NONE;
	int32 HeldCount = 0;

	/** Cell under the mouse */
	int32 Hovered = INDEX_NONE;

	/** Mouse in the screen's space */
	FVector2D CursorPos = FVector2D::ZeroVector;

	float CellSize = 64.0f;
};
