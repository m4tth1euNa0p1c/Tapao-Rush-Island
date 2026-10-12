// monke_island

#include "UI/Tapao/STapaoInventory.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Weapons/CutlassWeapon.h"
#include "Weapons/FirearmWeapon.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/MusketWeapon.h"
#include "Weapons/TorchItem.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TapaoUI"

namespace
{
	/** Size of a cell on the screen and in the HUD bar (px at 1080p; the viewport DPI scale follows the resolution) */
	constexpr float InventoryCellSize = 64.0f;

	/** Gap between two cells */
	constexpr float InventoryCellGap = 3.0f;

	/** Smallest text of the inventory (accessibility: 18 px at 1080p) */
	constexpr int32 InventoryTextSize = 18;

	/** Explanation of a key hint, in a plain font, small and white (owner requests 2026-10-07: smaller, sober) */
	constexpr int32 InventoryHintSize = 12;

	/** The key of a hint, framed like the F of the pickup prompt */
	constexpr int32 InventoryHintKeySize = 14;

	const FTapaoInventoryCellView EmptyCell;

	/** Kit thumbnail of an inventory item, from its class name (the pictures of DA_TapaoUISkin) */
	FName KitItemIcon(const UInventoryItemDefinition* Definition, const AInventoryItemActor* Item)
	{
		const UClass* Class = Item ? Item->GetClass() : (Definition ? Definition->ItemClass.Get() : nullptr);
		if (!Class)
		{
			return NAME_None;
		}
		const FString Name = Class->GetName();
		if (Name.Contains(TEXT("Musket"))) { return TEXT("item_musket"); }
		if (Name.Contains(TEXT("Cutlass"))) { return TEXT("item_cutlass"); }
		if (Name.Contains(TEXT("Torch"))) { return TEXT("item_torch"); }
		if (Name.Contains(TEXT("Trumpet"))) { return TEXT("item_trumpet"); }
		return NAME_None;
	}
}

// ---------------------------------------------------------------- helpers

const FSlateBrush* GetTapaoItemBrush(const FTapaoStyle& Style, const UInventoryItemDefinition* Definition, const AInventoryItemActor* Item)
{
	if (Definition && Definition->Icon)
	{
		return Style.TextureBrush(Definition->Icon);
	}
	const FName Kit = KitItemIcon(Definition, Item);
	UTapaoUISkin* Skin = Style.GetSkin();
	UTexture2D* Texture = Skin && !Kit.IsNone() ? Skin->Items.FindRef(Kit).Get() : nullptr;
	return Texture ? Style.TextureBrush(Texture) : nullptr;
}

FText GetTapaoItemShortName(const UInventoryItemDefinition* Definition)
{
	if (!Definition)
	{
		return FText::GetEmpty();
	}
	if (!Definition->ShortName.IsEmpty())
	{
		return Definition->ShortName;
	}
	// four letters of the name fit a 64 px cell at 18 px
	const FString Name = Definition->DisplayName.IsEmpty() ? Definition->GetName() : Definition->DisplayName.ToString();
	return FText::FromString(Name.Left(4));
}

FText DescribeTapaoItem(const FTapaoInventoryCellView& Cell)
{
	if (Cell.IsEmpty())
	{
		return FText::GetEmpty();
	}
	const FText Name = Cell.Definition->DisplayName.IsEmpty() ? FText::FromString(Cell.Definition->GetName()) : Cell.Definition->DisplayName;
	const AInventoryItemActor* Item = Cell.Item.Get();
	FText State;
	if (const AMusketWeapon* Musket = Cast<AMusketWeapon>(Item))
	{
		State = FText::Format(LOCTEXT("InvMusket", "{0} · {1} balles en réserve"),
			Musket->IsLoaded() ? LOCTEXT("InvLoaded", "Chargé") : LOCTEXT("InvEmpty", "Vide"), Musket->GetPouchBalls());
	}
	else if (const AFirearmWeapon* Firearm = Cast<AFirearmWeapon>(Item))
	{
		State = FText::Format(LOCTEXT("InvFirearm", "{0} chargé · {1} en réserve"), Firearm->GetLoadedRounds(), Firearm->GetReserveRounds());
	}
	else if (const ATorchItem* Torch = Cast<ATorchItem>(Item))
	{
		State = FText::Format(LOCTEXT("InvTorch", "Combustible : {0} s"), FMath::CeilToInt32(Torch->GetFuel()));
	}
	else if (const ACutlassWeapon* Cutlass = Cast<ACutlassWeapon>(Item))
	{
		State = FText::Format(LOCTEXT("InvCutlass", "Fil : {0} %"), FMath::RoundToInt32(Cutlass->GetSharpness() * 100.0f));
	}
	if (Cell.Count > 1)
	{
		const FText Count = FText::Format(LOCTEXT("InvCount", "{0} / {1}"), Cell.Count, UInventoryItemDefinition::GetMaxStack(Cell.Definition));
		State = State.IsEmpty() ? Count : FText::Format(LOCTEXT("InvStateCount", "{0} · {1}"), State, Count);
	}
	return State.IsEmpty() ? Name : FText::Format(LOCTEXT("InvTip", "{0}\n{1}"), Name, State);
}

// ---------------------------------------------------------------- model

FTapaoInventoryModel::~FTapaoInventoryModel()
{
	Unbind();
}

void FTapaoInventoryModel::Unbind()
{
	if (UIslanderInventoryComponent* Old = Inventory.Get())
	{
		Old->OnInventoryChangedNative.Remove(ChangedHandle);
	}
	ChangedHandle.Reset();
	Inventory.Reset();
}

void FTapaoInventoryModel::Bind(UIslanderInventoryComponent* InInventory)
{
	// already following it (or following nothing, cleanly)
	if (Inventory.Get() == InInventory && ChangedHandle.IsValid() == (InInventory != nullptr))
	{
		return;
	}
	Unbind();
	Inventory = InInventory;
	if (InInventory)
	{
		ChangedHandle = InInventory->OnInventoryChangedNative.AddSP(this, &FTapaoInventoryModel::Refresh);
	}
	Refresh();
}

void FTapaoInventoryModel::Refresh()
{
	const UIslanderInventoryComponent* Source = Inventory.Get();
	const int32 Num = Source ? Source->GetSlotNum() : 0;
	Cells.SetNum(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FInventorySlot& Slot = Source->GetSlots()[Index];
		Cells[Index].Definition = Slot.IsEmpty() ? nullptr : Slot.Definition.Get();
		Cells[Index].Count = Slot.IsEmpty() ? 0 : Slot.Count;
		Cells[Index].Item = Slot.Item;
	}
	BarNum = Source ? Source->GetBarNum() : 0;
	ActiveSlot = Source ? Source->GetActiveSlot() : INDEX_NONE;
	BagColumns = Source ? FMath::Max(Source->BagColumns, 1) : 6;
	OnChanged.Broadcast();
}

const FTapaoInventoryCellView& FTapaoInventoryModel::GetCell(int32 Index) const
{
	return Cells.IsValidIndex(Index) ? Cells[Index] : EmptyCell;
}

// ---------------------------------------------------------------- brushes

FTapaoInventoryBrushes::FTapaoInventoryBrushes(const FTapaoStyle& Style)
	: Cell(FLinearColor(0.02f, 0.015f, 0.01f, 0.62f), 3.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.22f), 1.0f)
	, CellHovered(FLinearColor(0.06f, 0.05f, 0.04f, 0.78f), 3.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.75f), 1.0f)
	, CellInHand(FLinearColor(0.04f, 0.03f, 0.02f, 0.72f), 3.0f, FLinearColor(1.0f, 0.77f, 0.31f, 1.0f), 2.5f)
	, Panel(FLinearColor(0.02f, 0.017f, 0.014f, 0.86f), 6.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.18f), 1.0f)
	, Tip(FLinearColor(0.02f, 0.017f, 0.014f, 0.94f), 4.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.3f), 1.0f)
	, KeyCap(FLinearColor(0.05f, 0.05f, 0.05f, 0.7f), 3.0f, FLinearColor(1.0f, 0.77f, 0.31f, 1.0f), 1.0f)
	, Veil(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f))
	, Text(FLinearColor::White)
	, Dim(FLinearColor(1.0f, 1.0f, 1.0f, 0.7f))
	, Accent(FLinearColor(1.0f, 0.77f, 0.31f, 1.0f))
{
	// the TAPAO palette (cream text, gold for the hand) when the skin is there
	if (const UTapaoUISkin* Skin = Style.GetSkin())
	{
		Text = Skin->Cream;
		Dim = FLinearColor(Skin->Cream.R, Skin->Cream.G, Skin->Cream.B, 0.72f);
		Accent = Skin->Gold;
		KeyCap.OutlineSettings.Color = Skin->Gold;
		const FLinearColor Line(Skin->Cream.R, Skin->Cream.G, Skin->Cream.B, 0.24f);
		Cell.OutlineSettings.Color = Line;
		CellHovered.OutlineSettings.Color = FLinearColor(Skin->Cream.R, Skin->Cream.G, Skin->Cream.B, 0.8f);
		CellInHand.OutlineSettings.Color = Skin->Gold;
		Panel.OutlineSettings.Color = FLinearColor(Skin->Cream.R, Skin->Cream.G, Skin->Cream.B, 0.2f);
		Tip.OutlineSettings.Color = FLinearColor(Skin->Cream.R, Skin->Cream.G, Skin->Cream.B, 0.35f);
	}
}

// ---------------------------------------------------------------- cell

void STapaoInventoryCell::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Brushes = InArgs._Brushes;
	Model = InArgs._Model;
	Index = InArgs._Index;
	bInteractive = InArgs._bInteractive;
	LiftedCount = InArgs._LiftedCount;
	OnPressed = InArgs._OnPressed;
	OnDoubleClicked = InArgs._OnDoubleClicked;
	OnHovered = InArgs._OnHovered;
	OnUnhovered = InArgs._OnUnhovered;
	const float Size = InArgs._Size;
	const FSlateFontInfo SmallFont = Style->Font(ETapaoFont::UI, InventoryTextSize, 1);

	TSharedRef<SOverlay> Content = SNew(SOverlay)
		// picture of the item
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(Size * 0.72f).HeightOverride(Size * 0.72f)
			[
				SNew(SImage)
				.Visibility_Lambda([this]()
				{
					const FTapaoInventoryCellView& Cell = Model->GetCell(Index);
					return GetShownCount() > 0 && GetTapaoItemBrush(*Style, Cell.Definition, Cell.Item.Get()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				.Image_Lambda([this]()
				{
					const FTapaoInventoryCellView& Cell = Model->GetCell(Index);
					return GetTapaoItemBrush(*Style, Cell.Definition, Cell.Item.Get());
				})
			]
		]
		// short name when there is no picture
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(2.0f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]()
			{
				const FTapaoInventoryCellView& Cell = Model->GetCell(Index);
				return GetShownCount() > 0 && !GetTapaoItemBrush(*Style, Cell.Definition, Cell.Item.Get()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.Text_Lambda([this]() { return GetTapaoItemShortName(Model->GetCell(Index).Definition); })
			.Font(SmallFont)
			.ColorAndOpacity(Brushes->Text)
			.Justification(ETextJustify::Center)
		]
		// count, bottom right (a single item shows none)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GetShownCount() > 1 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text_Lambda([this]() { return FText::AsNumber(GetShownCount()); })
			.Font(SmallFont)
			.ColorAndOpacity(Brushes->Text)
		];
	if (InArgs._KeyNumber > 0)
	{
		Content->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(InArgs._KeyNumber))
				.Font(SmallFont)
				.ColorAndOpacity(Brushes->Dim)
			];
	}

	ChildSlot
	[
		SNew(SBox).WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderImage_Lambda([this]() -> const FSlateBrush*
			{
				// the hand: a thicker gold frame (not the colour alone); the hovered cell: a bright frame
				if (Model->ActiveSlot == Index && !Model->GetCell(Index).IsEmpty())
				{
					return &Brushes->CellInHand;
				}
				return bInteractive && IsHovered() ? &Brushes->CellHovered : &Brushes->Cell;
			})
			[
				Content
			]
		]
	];

	if (bInteractive)
	{
		// a tooltip only over an item
		Tip = SNew(SToolTip)
			.BorderImage(&Brushes->Tip)
			.TextMargin(FMargin(10.0f, 6.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return DescribeTapaoItem(Model->GetCell(Index)); })
				.Font(SmallFont)
				.ColorAndOpacity(Brushes->Text)
			];
		SetToolTip(TAttribute<TSharedPtr<IToolTip>>::CreateLambda([this]() -> TSharedPtr<IToolTip>
		{
			return GetShownCount() > 0 ? Tip : nullptr;
		}));
	}
	else
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}
}

int32 STapaoInventoryCell::GetShownCount() const
{
	return FMath::Max(0, Model->GetCell(Index).Count - LiftedCount.Get(0));
}

FReply STapaoInventoryCell::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive)
	{
		return FReply::Unhandled();
	}
	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton)
	{
		OnPressed.ExecuteIfBound(Index, Button, MouseEvent.IsShiftDown());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply STapaoInventoryCell::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive)
	{
		return FReply::Unhandled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnDoubleClicked.ExecuteIfBound(Index);
		return FReply::Handled();
	}
	// a quick second right click is one more right click
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

void STapaoInventoryCell::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	OnHovered.ExecuteIfBound(Index);
}

void STapaoInventoryCell::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	OnUnhovered.ExecuteIfBound(Index);
}

// ---------------------------------------------------------------- screen

void STapaoInventoryScreen::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Brushes = MakeShared<FTapaoInventoryBrushes>(*Style);
	Model = MakeShared<FTapaoInventoryModel>();
	Model->Bind(InArgs._Inventory.Get());
	ModelHandle = Model->OnChanged.AddSP(this, &STapaoInventoryScreen::HandleModelChanged);
	CloseKeys = InArgs._CloseKeys;
	BarKeys = InArgs._BarKeys;
	OnClose = InArgs._OnClose;
	CellSize = InventoryCellSize;

	const int32 BarNum = Model->BarNum;
	const int32 BagNum = FMath::Max(0, Model->Cells.Num() - BarNum);
	// without a bag (owner request 2026-10-07: E opens the row of the player only) the screen is the bar
	const int32 Columns = BagNum > 0 ? FMath::Max(Model->BagColumns, BarNum) : FMath::Max(BarNum, 1);
	const FSlateFontInfo TitleFont = Style->Font(ETapaoFont::UI, 28, 1);
	// key hints: the key in the font of the game, what it does in a plain font, small and white
	const FSlateFontInfo KeyFont = Style->Font(ETapaoFont::UI, InventoryHintKeySize, 1);
	const FSlateFontInfo SmallFont = Style->Font(ETapaoFont::UI, InventoryTextSize, 1);
	const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Regular", InventoryHintSize);

	// owner choice 2026-10-07: without a bag, E brings the quick bar of the HUD to life where it is, no window, no veil;
	// with a bag, the window of T-166 (title, bag, bar)
	const bool bBarOnly = BagNum == 0;
	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);
	if (!bBarOnly)
	{
		Panel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("InvTitle", "Inventaire")).Font(TitleFont).ColorAndOpacity(Brushes->Text)
			];
		Panel->AddSlot().AutoHeight().HAlign(HAlign_Left)[MakeGrid(BarNum, BagNum, Model->BagColumns, false)];
		Panel->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f);
	}

	TSharedRef<SWrapBox> Hints = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16.0f, 6.0f));
	const auto AddHint = [&Hints, &KeyFont, &HintFont, this](const FText& Key, const FText& What)
	{
		Hints->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(SBorder).BorderImage(&Brushes->KeyCap).Padding(FMargin(5.0f, 1.0f, 5.0f, 2.0f))
				[
					SNew(STextBlock).Text(Key).Font(KeyFont).ColorAndOpacity(Brushes->Accent)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(What).Font(HintFont).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.92f))
				.ShadowOffset(FVector2D(1.0f, 1.0f)).ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]
		];
	};
	AddHint(LOCTEXT("HintClickKey", "Clic"), LOCTEXT("HintClick", "prendre / poser"));
	AddHint(LOCTEXT("HintRightKey", "Clic droit"), LOCTEXT("HintRight", "moitié / un"));
	AddHint(LOCTEXT("HintDoubleKey", "Double-clic"), LOCTEXT("HintDouble", "regrouper"));
	if (BagNum > 0)
	{
		AddHint(LOCTEXT("HintShiftKey", "Maj + clic"), LOCTEXT("HintShift", "barre / sac"));
	}
	AddHint(FText::Format(LOCTEXT("HintBarKey", "1 à {0}"), FText::AsNumber(FMath::Max(BarNum, 1))), LOCTEXT("HintBar", "sur une case : vers cette case"));
	AddHint(LOCTEXT("HintOutKey", "Clic hors du cadre"), LOCTEXT("HintOut", "lâcher"));
	AddHint(LOCTEXT("HintCloseKey", "E / Échap"), LOCTEXT("HintClose", "fermer"));
	// the hints right above the bar when it is alone, under it in the window
	const float HintWidth = FMath::Max((CellSize + 2.0f * InventoryCellGap) * Columns, bBarOnly ? 760.0f : 420.0f);
	if (bBarOnly)
	{
		Panel->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBox).WidthOverride(HintWidth)
				[
					Hints
				]
			];
		Panel->AddSlot().AutoHeight().HAlign(HAlign_Center)[MakeGrid(0, BarNum, Columns, true)];
	}
	else
	{
		Panel->AddSlot().AutoHeight().HAlign(HAlign_Left)[MakeGrid(0, BarNum, Columns, true)];
		Panel->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(HintWidth)
				[
					Hints
				]
			];
	}

	ChildSlot
	[
		SNew(SOverlay)
		// the game stays visible (multiplayer: it goes on), slightly darkened with the window; the bar alone leaves it
		// as it is (the veil then only catches the clicks out of the bar: a drop)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(&Brushes->Veil).ColorAndOpacity(bBarOnly ? FLinearColor::Transparent : FLinearColor::White)
		]
		// the bar alone sits exactly on the quick bar of the HUD (64 px cells, 18 px from the bottom), which hides meanwhile
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(bBarOnly ? VAlign_Bottom : VAlign_Center)
			.Padding(bBarOnly ? FMargin(0.0f, 0.0f, 0.0f, 18.0f - InventoryCellGap) : FMargin(0.0f))
		[
			SNew(SBorder)
			.BorderImage(bBarOnly ? FCoreStyle::Get().GetBrush("NoBorder") : &Brushes->Panel)
			.Padding(bBarOnly ? FMargin(0.0f) : FMargin(22.0f, 16.0f))
			// a click on the panel itself is not a drop
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			[
				Panel
			]
		]
		// what the cursor carries, under the mouse
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(CellSize).HeightOverride(CellSize)
			.Visibility_Lambda([this]() { return HeldFrom != INDEX_NONE && HeldCount > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
			{
				return FSlateRenderTransform(FVector2f(CursorPos - FVector2D(CellSize, CellSize) * 0.5));
			})
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(CellSize * 0.72f).HeightOverride(CellSize * 0.72f)
					[
						SNew(SImage).Image_Lambda([this]()
						{
							const FTapaoInventoryCellView& Cell = Model->GetCell(HeldFrom);
							const FSlateBrush* Brush = GetTapaoItemBrush(*Style, Cell.Definition, Cell.Item.Get());
							return Brush ? Brush : &Brushes->Cell;
						})
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]()
					{
						const FTapaoInventoryCellView& Cell = Model->GetCell(HeldFrom);
						return GetTapaoItemBrush(*Style, Cell.Definition, Cell.Item.Get()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
					})
					.Text_Lambda([this]() { return GetTapaoItemShortName(Model->GetCell(HeldFrom).Definition); })
					.Font(SmallFont)
					.ColorAndOpacity(Brushes->Text)
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return HeldCount > 1 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.Text_Lambda([this]() { return FText::AsNumber(HeldCount); })
					.Font(SmallFont)
					.ColorAndOpacity(Brushes->Text)
				]
			]
		]
	];
}

STapaoInventoryScreen::~STapaoInventoryScreen()
{
	if (Model.IsValid())
	{
		Model->OnChanged.Remove(ModelHandle);
	}
}

UIslanderInventoryComponent* STapaoInventoryScreen::GetInventory() const
{
	return Model.IsValid() ? Model->GetInventory() : nullptr;
}

TSharedRef<SWidget> STapaoInventoryScreen::MakeGrid(int32 First, int32 Num, int32 Columns, bool bBar)
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(InventoryCellGap));
	Columns = FMath::Max(Columns, 1);
	for (int32 Offset = 0; Offset < Num; ++Offset)
	{
		const int32 Index = First + Offset;
		Grid->AddSlot(Offset % Columns, Offset / Columns)
			[
				SNew(STapaoInventoryCell)
				.Style(Style)
				.Brushes(Brushes)
				.Model(Model)
				.Index(Index)
				.Size(CellSize)
				.bInteractive(true)
				.KeyNumber(bBar ? Index + 1 : 0)
				.LiftedCount_Lambda([this, Index]() { return HeldFrom == Index ? HeldCount : 0; })
				.OnPressed(this, &STapaoInventoryScreen::HandleCellPressed)
				.OnDoubleClicked(this, &STapaoInventoryScreen::HandleCellDoubleClicked)
				.OnHovered(this, &STapaoInventoryScreen::HandleHovered)
				.OnUnhovered(this, &STapaoInventoryScreen::HandleUnhovered)
			];
	}
	return Grid;
}

void STapaoInventoryScreen::ClearHeld()
{
	HeldFrom = INDEX_NONE;
	HeldCount = 0;
}

void STapaoInventoryScreen::HandleModelChanged()
{
	// the server's answer is the truth: the cursor never carries more than its cell holds
	if (HeldFrom == INDEX_NONE)
	{
		return;
	}
	const FTapaoInventoryCellView& Source = Model->GetCell(HeldFrom);
	if (Source.IsEmpty())
	{
		ClearHeld();
		return;
	}
	HeldCount = FMath::Clamp(HeldCount, 1, Source.Count);
}

void STapaoInventoryScreen::HandleCellPressed(int32 Index, FKey Button, bool bShift)
{
	UIslanderInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Model->Cells.IsValidIndex(Index))
	{
		return;
	}
	const FTapaoInventoryCellView Target = Model->GetCell(Index);
	const bool bLeft = Button == EKeys::LeftMouseButton;

	if (HeldFrom == INDEX_NONE)
	{
		if (Target.IsEmpty())
		{
			return;
		}
		if (bShift)
		{
			Inventory->RequestQuickMove(Index);
			return;
		}
		// left: the whole stack; right: the bigger half
		HeldFrom = Index;
		HeldCount = bLeft ? Target.Count : (Target.Count + 1) / 2;
		return;
	}

	if (Index == HeldFrom)
	{
		// back where it came from (nothing moved yet)
		if (bLeft)
		{
			ClearHeld();
		}
		return;
	}

	const FTapaoInventoryCellView Source = Model->GetCell(HeldFrom);
	const int32 From = HeldFrom;
	const bool bSameStack = !Target.IsEmpty() && Target.Definition == Source.Definition && UInventoryItemDefinition::GetMaxStack(Target.Definition) > 1;
	const int32 Room = bSameStack ? UInventoryItemDefinition::GetMaxStack(Target.Definition) - Target.Count : 0;

	if (!bLeft)
	{
		// right: one item onto an empty cell or a stack with room
		if (Target.IsEmpty() || Room > 0)
		{
			if (--HeldCount <= 0)
			{
				ClearHeld();
			}
			Inventory->RequestMove(From, Index, 1);
		}
		return;
	}

	const int32 Carried = HeldCount;
	if (Target.IsEmpty())
	{
		ClearHeld();
		Inventory->RequestMove(From, Index, Carried);
	}
	else if (bSameStack)
	{
		const int32 Moved = FMath::Min(Room, Carried);
		if (Moved <= 0)
		{
			return;
		}
		HeldCount -= Moved;
		if (HeldCount <= 0)
		{
			ClearHeld();
		}
		Inventory->RequestMove(From, Index, Moved);
	}
	else if (Carried == Source.Count)
	{
		// another item: they swap, and the cursor now carries the other one (it lands where ours came from)
		HeldCount = Target.Count;
		Inventory->RequestMove(From, Index, Carried);
	}
}

void STapaoInventoryScreen::HandleCellDoubleClicked(int32 Index)
{
	UIslanderInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}
	if (HeldFrom == INDEX_NONE)
	{
		HandleCellPressed(Index, EKeys::LeftMouseButton, false);
		return;
	}
	// gathers the identical items into the carried stack
	const FTapaoInventoryCellView& Source = Model->GetCell(HeldFrom);
	if (Source.IsEmpty() || UInventoryItemDefinition::GetMaxStack(Source.Definition) <= 1)
	{
		return;
	}
	int32 Total = 0;
	for (const FTapaoInventoryCellView& Cell : Model->Cells)
	{
		Total += !Cell.IsEmpty() && Cell.Definition == Source.Definition ? Cell.Count : 0;
	}
	HeldCount = FMath::Min(Total, UInventoryItemDefinition::GetMaxStack(Source.Definition));
	Inventory->RequestGather(HeldFrom);
}

void STapaoInventoryScreen::HandleHovered(int32 Index)
{
	Hovered = Index;
}

void STapaoInventoryScreen::HandleUnhovered(int32 Index)
{
	if (Hovered == Index)
	{
		Hovered = INDEX_NONE;
	}
}

void STapaoInventoryScreen::Close()
{
	// nothing moved for what the cursor carried: it is simply back
	ClearHeld();
	OnClose.ExecuteIfBound();
}

FReply STapaoInventoryScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || CloseKeys.Contains(Key))
	{
		Close();
		return FReply::Handled();
	}
	for (int32 Bar = 0; Bar < BarKeys.Num(); ++Bar)
	{
		if (!BarKeys[Bar].Contains(Key))
		{
			continue;
		}
		// keys 1-4 over a cell: it swaps with that bar cell
		UIslanderInventoryComponent* Inventory = GetInventory();
		const FTapaoInventoryCellView& Cell = Model->GetCell(Hovered);
		if (Inventory && HeldFrom == INDEX_NONE && !Cell.IsEmpty() && Hovered != Bar && Bar < Model->BarNum)
		{
			Inventory->RequestMove(Hovered, Bar, Cell.Count);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply STapaoInventoryScreen::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// a click outside the panel: what the cursor carries falls on the ground (right click: one item)
	UIslanderInventoryComponent* Inventory = GetInventory();
	if (Inventory && HeldFrom != INDEX_NONE && HeldCount > 0)
	{
		const bool bOne = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
		const int32 From = HeldFrom;
		const int32 Count = bOne ? 1 : HeldCount;
		HeldCount -= Count;
		if (HeldCount <= 0)
		{
			ClearHeld();
		}
		Inventory->RequestDrop(From, Count);
	}
	return FReply::Handled();
}

FReply STapaoInventoryScreen::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
