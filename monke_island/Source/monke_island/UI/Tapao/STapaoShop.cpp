// monke_island

#include "UI/Tapao/STapaoShop.h"
#include "UI/Tapao/STapaoInventory.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Run/RunRules.h"
#include "Run/RunStateComponent.h"
#include "Run/RunSubsystem.h"
#include "Shop/ShopCatalog.h"
#include "Shop/ShopkeeperActor.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SlateSound.h"
#include "Sound/SoundBase.h"
#include "Types/SlateConstants.h"
#include "Weapons/AmmoDefinition.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TapaoShop"

namespace
{
	/** Smallest text of the shop (accessibility: 18 px at 1080p) */
	constexpr int32 ShopTextSize = 18;

	/** Names and prices */
	constexpr int32 ShopNameSize = 22;

	/** Side of the picture of an offer */
	constexpr float ShopPictureSize = 64.0f;

	/** Size of the buy button */
	const FVector2D ShopBuyButtonSize(150.0f, 44.0f);

	/** Title of a group of offers */
	FText CategoryTitle(EShopCategory Category)
	{
		switch (Category)
		{
		case EShopCategory::Firearm: return LOCTEXT("GroupFirearm", "Armes à feu");
		case EShopCategory::Blade: return LOCTEXT("GroupBlade", "Armes blanches");
		case EShopCategory::Explosive: return LOCTEXT("GroupExplosive", "Explosifs");
		case EShopCategory::Ammunition: return LOCTEXT("GroupAmmunition", "Munitions");
		case EShopCategory::Consumable: return LOCTEXT("GroupConsumable", "Consommables");
		default: return LOCTEXT("GroupTool", "Outils et objets");
		}
	}

	/** Name of an offer: the item, else the packet of ammunition */
	FText OfferName(const FShopOffer& Offer)
	{
		return Offer.Item ? Offer.Item->DisplayName : Offer.Ammo ? Offer.Ammo->DisplayName : LOCTEXT("Unknown", "?");
	}
}

/** Brushes of the shop on top of the inventory's: buttons, rules, the mark of the chosen line */
struct FTapaoShopBrushes
{
	FTapaoShopBrushes(const FTapaoStyle& Style, const FTapaoInventoryBrushes& Base)
		: Rule(FLinearColor(Base.Text.R, Base.Text.G, Base.Text.B, 0.22f))
		, Marker(Base.Accent, 2.0f)
		, Warning(1.0f, 0.58f, 0.45f, 1.0f)
	{
		const UTapaoUISkin* Skin = Style.GetSkin();
		const FLinearColor Gold = Base.Accent;
		const FLinearColor Cream = Base.Text;
		const FLinearColor Ink = Skin ? Skin->Ink : FLinearColor(0.02f, 0.01f, 0.005f, 1.0f);
		// sober like the cells of the inventory: dark, a gold border, gold when pressed (ink on it), faded when it cannot buy
		Buy.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.06f, 0.045f, 0.03f, 0.9f), 4.0f, Gold, 1.0f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.2f, 0.14f, 0.06f, 0.95f), 4.0f, Gold, 2.0f))
			.SetPressed(FSlateRoundedBoxBrush(Gold, 4.0f, Gold, 2.0f))
			.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.04f, 0.04f, 0.55f), 4.0f, FLinearColor(Cream.R, Cream.G, Cream.B, 0.25f), 1.0f))
			.SetNormalForeground(Cream)
			.SetHoveredForeground(Cream)
			.SetPressedForeground(Ink)
			.SetDisabledForeground(FLinearColor(Cream.R, Cream.G, Cream.B, 0.55f))
			.SetNormalPadding(FMargin(10.0f, 4.0f))
			.SetPressedPadding(FMargin(10.0f, 5.0f, 10.0f, 3.0f));
		// the click of the kit's buttons (menus), as everywhere in TAPAO
		if (USoundBase* Click = Skin ? Skin->ClickSound.Get() : nullptr)
		{
			FSlateSound Sound;
			Sound.SetResourceObject(Click);
			Buy.SetClickedSound(Sound);
		}
		Close = Buy;
	}

	FButtonStyle Buy;
	FButtonStyle Close;
	FSlateColorBrush Rule;
	FSlateRoundedBoxBrush Marker;

	/** What is missing to buy */
	FLinearColor Warning;
};

// ---------------------------------------------------------------- frame

void STapaoShopFrame::Construct(const FArguments& InArgs)
{
	Layout = InArgs._Layout;
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void STapaoShopFrame::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const TSharedRef<SWidget>& Child = ChildSlot.GetWidget();
	const EVisibility ChildVisibility = Child->GetVisibility();
	if (!ArrangedChildren.Accepts(ChildVisibility))
	{
		return;
	}
	// the screen it is given, the window's own height: fitted in the same layout pass (no frame late)
	const FVector2D Available(AllottedGeometry.GetLocalSize());
	const FVector2D Size = Layout.Fit(Available, Child->GetDesiredSize().Y);
	// whole units: the text of the window stays sharp
	const FVector2f Offset(static_cast<float>(FMath::RoundToDouble((Available.X - Size.X) * 0.5)), static_cast<float>(FMath::RoundToDouble((Available.Y - Size.Y) * 0.5)));
	const FVector2f Extent(static_cast<float>(FMath::RoundToDouble(Size.X)), static_cast<float>(FMath::RoundToDouble(Size.Y)));
	ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(Child, Offset, Extent));
}

FVector2D STapaoShopFrame::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// the overlay of the viewport gives it the whole screen; the window is fitted when arranged
	return FVector2D(ChildSlot.GetWidget()->GetDesiredSize());
}

// ---------------------------------------------------------------- screen

void STapaoShopScreen::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Shop = InArgs._Shop;
	Inventory = InArgs._Inventory;
	OnClose = InArgs._OnClose;
	Brushes = MakeShared<FTapaoInventoryBrushes>(*Style);
	ShopBrushes = MakeShared<FTapaoShopBrushes>(*Style, *Brushes);
	StatusColor = Brushes->Text;
	BuyLabel = LOCTEXT("Buy", "Acheter");
	TooDearLabel = LOCTEXT("TooDear", "Trop cher");

	UShopCatalog* Shelf = Shop.IsValid() ? Shop->GetCatalog() : nullptr;
	Catalog.Reset(Shelf);
	Layout = Shelf ? Shelf->Screen : FShopScreenLayout();

	// the order of the screen (owner 2026-10-11): groups in the order of EShopCategory, cheapest first; a purchase
	// still names the catalog index the server judges
	TArray<EShopCategory> Categories;
	TArray<int32> Prices;
	if (Shelf)
	{
		for (const FShopOffer& Offer : Shelf->Offers)
		{
			Categories.Add(UShopCatalog::ResolveCategory(Offer));
			Prices.Add(Offer.Price);
		}
	}
	const TArray<FShopSection> Sections = ShopRules::Arrange(Categories, Prices);
	for (const FShopSection& Section : Sections)
	{
		SectionStarts.Add(Order.Num());
		Order.Append(Section.Offers);
	}
	if (!Order.IsEmpty())
	{
		Selected = 0;
	}
	RefreshTreasury();

	const FSlateFontInfo TextFont = Style->Font(ETapaoFont::Text, ShopTextSize);
	const FSlateFontInfo StatusFont = Style->Font(ETapaoFont::UI, ShopTextSize, 1);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(&Brushes->Veil)
		]
		+ SOverlay::Slot()
		[
			SNew(STapaoShopFrame)
			.Layout(Layout)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SNew(SBorder).BorderImage(&Brushes->Panel).Padding(FMargin(24.0f, 18.0f))
				[
					SNew(SVerticalBox)
					// fixed on top: title, treasury
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeHeader()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 10.0f)
					[
						SNew(SBox).HeightOverride(1.0f)[SNew(SImage).Image(&ShopBrushes->Rule)]
					]
					// the offers, the only part that scrolls
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						MakeList(Sections)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 8.0f)
					[
						SNew(SBox).HeightOverride(1.0f)[SNew(SImage).Image(&ShopBrushes->Rule)]
					]
					// fixed below: the chosen offer before / after, the server's answer, the keys
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(TextFont).ColorAndOpacity(Brushes->Text).AutoWrapText(true)
						.Text_Lambda([this]() { return SelectionText; })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBox).MinDesiredHeight(26.0f)
						[
							SNew(STextBlock).Font(StatusFont).AutoWrapText(true)
							.Text_Lambda([this]() { return Status; })
							.ColorAndOpacity_Lambda([this]() { return FSlateColor(StatusColor); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						MakeFooter()
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> STapaoShopScreen::MakeHeader()
{
	const UShopCatalog* Shelf = Catalog.Get();
	const FSlateBrush* Coin = Style->Brush(TEXT("hud_coin"));
	const bool bCoin = Coin && Coin->DrawAs != ESlateBrushDrawType::NoDrawType;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(Shelf ? Shelf->Title : LOCTEXT("Title", "Le marchand du galion"))
			.Font(Style->Font(ETapaoFont::UI, 30, 1))
			.ColorAndOpacity(Brushes->Text)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(28.0f).HeightOverride(28.0f).Visibility(bCoin ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				[
					SNew(SImage).Image(Coin)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Style->Font(ETapaoFont::UI, ShopNameSize, 1))
				.ColorAndOpacity(Brushes->Accent)
				.Text_Lambda([this]() { return TreasuryText; })
			]
		];
}

TSharedRef<SWidget> STapaoShopScreen::MakeList(const TArray<FShopSection>& Sections)
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (Order.IsEmpty())
	{
		Content->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("Empty", "Rien à vendre pour l'instant.")).Font(Style->Font(ETapaoFont::UI, ShopTextSize, 1)).ColorAndOpacity(Brushes->Dim)
			];
	}
	Rows.SetNum(Order.Num());
	int32 Position = 0;
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const FShopSection& Section = Sections[SectionIndex];
		Content->AddSlot().AutoHeight().Padding(0.0f, SectionIndex == 0 ? 0.0f : 14.0f, 0.0f, 6.0f)
			[
				MakeSectionTitle(Section)
			];
		for (int32 InSection = 0; InSection < Section.Offers.Num(); ++InSection, ++Position)
		{
			TSharedRef<SWidget> Row = MakeOfferRow(Position);
			Rows[Position] = Row;
			Content->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					Row
				];
		}
	}
	return SAssignNew(List, SScrollBox)
		.Style(&Style->ScrollBox())
		.ScrollBarStyle(&Style->ScrollBar())
		.ScrollBarThickness(FVector2D(10.0f, 10.0f))
		.ScrollBarPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.AllowOverscroll(EAllowOverscroll::No)
		// the right button held drags the list, besides the wheel and the thumb of the bar
		.ScrollBarRightClickDragAllowed(true)
		.AnimateWheelScrolling(true)
		.WheelScrollMultiplier(Layout.WheelScrollMultiplier)
		.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		.NavigationDestination(EDescendantScrollDestination::IntoView)
		.NavigationScrollPadding(Layout.NavigationPadding)
		+ SScrollBox::Slot()
		[
			Content
		];
}

TSharedRef<SWidget> STapaoShopScreen::MakeSectionTitle(const FShopSection& Section)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(CategoryTitle(Section.Category)).Font(Style->Font(ETapaoFont::UI, ShopNameSize, 1)).ColorAndOpacity(Brushes->Accent)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("GroupCount", "({0})"), FText::AsNumber(Section.Offers.Num())))
			.Font(Style->Font(ETapaoFont::Text, ShopTextSize))
			.ColorAndOpacity(Brushes->Dim)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).HeightOverride(1.0f)[SNew(SImage).Image(&ShopBrushes->Rule)]
		];
}

TSharedRef<SWidget> STapaoShopScreen::MakeOfferRow(int32 Position)
{
	const FShopOffer* Found = GetOfferAt(Position);
	if (!Found)
	{
		return SNullWidget::NullWidget;
	}
	const FShopOffer& Offer = *Found;
	const int32 Index = Order[Position];
	const int32 Price = Offer.Price;
	const FSlateFontInfo NameFont = Style->Font(ETapaoFont::UI, ShopNameSize, 1);
	const FSlateFontInfo TextFont = Style->Font(ETapaoFont::Text, ShopTextSize);
	const FSlateFontInfo ButtonFont = Style->Font(ETapaoFont::UI, ShopTextSize, 1);
	// an item (weapon, tool) or a packet of ammunition (owner 2026-10-09: the models of the new weapons and of their
	// ammunition pictured in the shop, T-264)
	const FSlateBrush* Picture = Offer.Item ? GetTapaoItemBrush(*Style, Offer.Item, nullptr)
		: (Offer.Ammo && Offer.Ammo->Icon ? Style->TextureBrush(Offer.Ammo->Icon) : nullptr);
	const FText Name = OfferName(Offer);
	TSharedRef<SWidget> PictureWidget = Picture
		? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Picture))
		: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Offer.Item ? GetTapaoItemShortName(Offer.Item) : Name).Font(TextFont).ColorAndOpacity(Brushes->Text).Justification(ETextJustify::Center));

	return SNew(SBorder)
		.BorderImage_Lambda([this, Position]() { return Selected == Position ? &Brushes->CellInHand : &Brushes->Cell; })
		.Padding(FMargin(0.0f, 6.0f, 10.0f, 6.0f))
		// a left click on the line chooses it and keeps the keys on the shop (the right button still drags the list)
		.OnMouseButtonDown_Lambda([this, Position](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || MouseEvent.IsTouchEvent())
			{
				return FReply::Unhandled();
			}
			Select(Position, false);
			return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
		})
		// the mouse moving over a line chooses it (not the list scrolling under a still cursor)
		.OnMouseMove_Lambda([this, Position](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (Selected != Position && !MouseEvent.GetCursorDelta().IsNearlyZero())
			{
				Select(Position, false);
			}
			return FReply::Unhandled();
		})
		[
			SNew(SHorizontalBox)
			// the chosen line is marked by a shape too, not by its colour alone (cahier §18)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(5.0f)
				.Visibility_Lambda([this, Position]() { return Selected == Position ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
				[
					SNew(SImage).Image(&ShopBrushes->Marker)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(ShopPictureSize).HeightOverride(ShopPictureSize).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					PictureWidget
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Name).Font(NameFont).ColorAndOpacity(Brushes->Text)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Offer.Description).Font(TextFont).ColorAndOpacity(Brushes->Dim).AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SBox).MinDesiredWidth(96.0f).HAlign(HAlign_Right)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock).Text(FText::Format(LOCTEXT("Price", "{0} c"), FText::AsNumber(Price))).Font(NameFont).ColorAndOpacity(Brushes->Accent)
					]
					// why it cannot be bought, written (cahier §19: reason of an impossible purchase)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock).Font(TextFont).ColorAndOpacity(ShopBrushes->Warning)
						.Visibility_Lambda([this, Price]() { return Treasury < Price ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						.Text_Lambda([this, Position]() { return MissingTexts.IsValidIndex(Position) ? MissingTexts[Position] : FText::GetEmpty(); })
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(ShopBuyButtonSize.X).HeightOverride(ShopBuyButtonSize.Y)
				[
					SNew(SButton)
					.ButtonStyle(&ShopBrushes->Buy)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					// the keys and the pad move the chosen line, never the focus onto a button
					.IsFocusable(false)
					.IsEnabled_Lambda([this, Price]() { return Treasury >= Price; })
					.OnClicked(this, &STapaoShopScreen::HandleBuy, Index)
					[
						SNew(STextBlock).Font(ButtonFont)
						.Text_Lambda([this, Price]() { return Treasury >= Price ? BuyLabel : TooDearLabel; })
					]
				]
			]
		];
}

TSharedRef<SWidget> STapaoShopScreen::MakeFooter()
{
	const FSlateFontInfo KeyFont = Style->Font(ETapaoFont::UI, ShopTextSize, 1);
	const FSlateFontInfo HintFont = Style->Font(ETapaoFont::Text, ShopTextSize);
	TSharedRef<SWrapBox> Hints = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16.0f, 6.0f));
	// the key framed like the F of the pickup prompt, what it does beside it (as in the inventory)
	const auto AddHint = [this, &Hints, &KeyFont, &HintFont](const FText& Key, const FText& What)
	{
		Hints->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&Brushes->KeyCap).Padding(FMargin(6.0f, 1.0f, 6.0f, 2.0f))
					[
						SNew(STextBlock).Text(Key).Font(KeyFont).ColorAndOpacity(Brushes->Accent)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(What).Font(HintFont).ColorAndOpacity(Brushes->Dim)
				]
			];
	};
	AddHint(LOCTEXT("HintWheelKey", "Molette"), LOCTEXT("HintWheel", "défiler"));
	AddHint(LOCTEXT("HintArrowsKey", "↑ ↓"), LOCTEXT("HintArrows", "choisir"));
	AddHint(LOCTEXT("HintPageKey", "Pg↑ Pg↓"), LOCTEXT("HintPage", "rayon"));
	AddHint(LOCTEXT("HintBuyKey", "Entrée"), LOCTEXT("HintBuy", "acheter"));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			Hints
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).MinDesiredWidth(170.0f).HeightOverride(ShopBuyButtonSize.Y)
			[
				SNew(SButton)
				.ButtonStyle(&ShopBrushes->Close)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.IsFocusable(false)
				.OnClicked_Lambda([this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(LOCTEXT("Close", "Fermer (Échap)")).Font(KeyFont)
				]
			]
		];
}

const FShopOffer* STapaoShopScreen::GetOfferAt(int32 Position) const
{
	const UShopCatalog* Shelf = Catalog.Get();
	const int32 Index = Order.IsValidIndex(Position) ? Order[Position] : INDEX_NONE;
	return Shelf && Shelf->Offers.IsValidIndex(Index) ? &Shelf->Offers[Index] : nullptr;
}

int32 STapaoShopScreen::GetSelectedOffer() const
{
	return Order.IsValidIndex(Selected) ? Order[Selected] : INDEX_NONE;
}

int32 STapaoShopScreen::GetTreasury() const
{
	const UWorld* World = Inventory.IsValid() ? Inventory->GetWorld() : nullptr;
	if (const URunStateComponent* Run = URunStateComponent::Get(World))
	{
		return Run->GetTreasury();
	}
	// no expedition game state (L_Test_Flat): the host reads its own run, a fresh one when there is none
	const URunSubsystem* Subsystem = URunSubsystem::Get(World);
	if (!Subsystem)
	{
		return 0;
	}
	if (Subsystem->GetState().bActive)
	{
		return Subsystem->GetState().Treasury;
	}
	const URunRules* Rules = Subsystem->GetRules();
	return Rules ? Rules->StartingTreasury : 0;
}

void STapaoShopScreen::RefreshTreasury()
{
	Treasury = GetTreasury();
	TreasuryText = FText::Format(LOCTEXT("Treasury", "Trésorerie de l'équipage : {0} couronnes"), FText::AsNumber(Treasury));
	MissingTexts.SetNum(Order.Num());
	for (int32 Position = 0; Position < Order.Num(); ++Position)
	{
		const FShopOffer* Offer = GetOfferAt(Position);
		const int32 Missing = Offer ? Offer->Price - Treasury : 0;
		MissingTexts[Position] = Missing > 0 ? FText::Format(LOCTEXT("Missing", "il manque {0} c"), FText::AsNumber(Missing)) : FText::GetEmpty();
	}
	UpdateSelectionText();
}

void STapaoShopScreen::UpdateSelectionText()
{
	const FShopOffer* Offer = GetOfferAt(Selected);
	if (!Offer)
	{
		SelectionText = FText::GetEmpty();
		return;
	}
	// cahier §19: the effect before / after, or the reason it cannot be bought
	SelectionText = Treasury >= Offer->Price
		? FText::Format(LOCTEXT("SelectionAfter", "{0}, {1} c : la trésorerie passera de {2} à {3} couronnes."), OfferName(*Offer),
			FText::AsNumber(Offer->Price), FText::AsNumber(Treasury), FText::AsNumber(Treasury - Offer->Price))
		: FText::Format(LOCTEXT("SelectionMissing", "{0}, {1} c : il manque {2} couronnes à la trésorerie."), OfferName(*Offer),
			FText::AsNumber(Offer->Price), FText::AsNumber(Offer->Price - Treasury));
}

void STapaoShopScreen::Select(int32 Position, bool bScroll, bool bToTop)
{
	if (Order.IsEmpty())
	{
		Selected = INDEX_NONE;
		UpdateSelectionText();
		return;
	}
	Selected = FMath::Clamp(Position, 0, Order.Num() - 1);
	UpdateSelectionText();
	if (!bScroll || !List.IsValid())
	{
		return;
	}
	if (Selected == 0)
	{
		// the first offer brings the top of the list back, the title of its group with it
		List->ScrollToStart();
	}
	else if (Selected == Order.Num() - 1)
	{
		List->ScrollToEnd();
	}
	else if (Rows.IsValidIndex(Selected) && Rows[Selected].IsValid())
	{
		// the padding keeps the title of the group above, or a glimpse of the next line below, in view
		List->ScrollDescendantIntoView(Rows[Selected], true, bToTop ? EDescendantScrollDestination::TopOrLeft : EDescendantScrollDestination::IntoView,
			Layout.NavigationPadding);
	}
}

void STapaoShopScreen::JumpSection(int32 Delta)
{
	if (SectionStarts.IsEmpty())
	{
		return;
	}
	int32 Section = SectionStarts.Num() - 1;
	while (Section > 0 && SectionStarts[Section] > Selected)
	{
		--Section;
	}
	// backwards, the start of this group comes first
	if (Delta < 0 && Selected > SectionStarts[Section])
	{
		Select(SectionStarts[Section], true, true);
		return;
	}
	Section = FMath::Clamp(Section + Delta, 0, SectionStarts.Num() - 1);
	Select(SectionStarts[Section], true, true);
}

FReply STapaoShopScreen::HandleBuy(int32 Offer)
{
	const int32 Position = Order.Find(Offer);
	if (Position != INDEX_NONE && Position != Selected)
	{
		Select(Position, false);
	}
	if (Inventory.IsValid() && Shop.IsValid())
	{
		// cahier §19: nothing is taken here, the treasury drops when the server's answer comes back
		Status = LOCTEXT("Pending", "Achat en cours…");
		StatusColor = Brushes->Dim;
		Inventory->RequestBuy(Shop.Get(), Offer);
	}
	return FReply::Handled();
}

void STapaoShopScreen::BuySelected()
{
	const FShopOffer* Offer = GetOfferAt(Selected);
	if (!Offer)
	{
		return;
	}
	if (Treasury < Offer->Price)
	{
		// the reason, in the words of the server's refusal; nothing is asked
		ShowResult(EShopResult::NotEnoughMoney, Order[Selected]);
		return;
	}
	HandleBuy(Order[Selected]);
}

void STapaoShopScreen::ShowResult(EShopResult Result, int32 Offer)
{
	const UShopCatalog* Shelf = Catalog.Get();
	const FShopOffer* Bought = Shelf && Shelf->Offers.IsValidIndex(Offer) ? &Shelf->Offers[Offer] : nullptr;
	const bool bAmmo = Bought && !Bought->Item && Bought->Ammo;
	const FText Name = Bought ? OfferName(*Bought) : FText::GetEmpty();
	StatusColor = Result == EShopResult::Bought ? Brushes->Accent : FLinearColor(1.0f, 0.45f, 0.35f, 1.0f);
	switch (Result)
	{
	case EShopResult::Bought:
		Status = bAmmo ? FText::Format(LOCTEXT("BoughtAmmo", "{0} acheté : les munitions sont dans tes armes."), Name)
			: FText::Format(LOCTEXT("Bought", "{0} acheté : il est dans ton inventaire."), Name);
		break;
	case EShopResult::NotEnoughMoney: Status = LOCTEXT("Money", "Pas assez de couronnes dans la trésorerie."); break;
	case EShopResult::InventoryFull:
		Status = bAmmo ? LOCTEXT("NoWeapon", "Aucune de tes armes ne prend ces munitions, ou elles sont pleines.")
			: LOCTEXT("Full", "Inventaire plein : libère une case (G pour lâcher).");
		break;
	case EShopResult::TooFar: Status = LOCTEXT("Far", "Approche-toi du marchand."); break;
	case EShopResult::TooFast: Status = LOCTEXT("Fast", "Doucement, un achat à la fois."); break;
	default: Status = LOCTEXT("Invalid", "Cet article n'est plus en vente."); break;
	}
	// the host's treasury is already lowered; a client's follows with the run state (UTapaoShopUISubsystem)
	RefreshTreasury();
}

FReply STapaoShopScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::F)
	{
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	// arrows and pad cross here (held: the key repeats); the stick and Tab come as navigation (OnNavigation)
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		Select(Selected - 1, true);
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		Select(Selected + 1, true);
		return FReply::Handled();
	}
	if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightShoulder)
	{
		JumpSection(1);
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_LeftShoulder)
	{
		JumpSection(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Home)
	{
		Select(0, true);
		return FReply::Handled();
	}
	if (Key == EKeys::End)
	{
		Select(Order.Num() - 1, true);
		return FReply::Handled();
	}
	const EUINavigationAction Action = FSlateApplication::Get().GetNavigationActionFromKey(KeyEvent);
	// Enter or pad A buy, one purchase a press; the space bar does not (a jump out of habit must not spend the crew's money)
	if (Action == EUINavigationAction::Accept && Key != EKeys::SpaceBar)
	{
		if (!KeyEvent.IsRepeat())
		{
			BuySelected();
		}
		return FReply::Handled();
	}
	if (Action == EUINavigationAction::Back)
	{
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, KeyEvent);
}

FNavigationReply STapaoShopScreen::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	switch (InNavigationEvent.GetNavigationType())
	{
	case EUINavigation::Up:
	case EUINavigation::Previous:
		Select(Selected - 1, true);
		break;
	case EUINavigation::Down:
	case EUINavigation::Next:
		Select(Selected + 1, true);
		break;
	default:
		break;
	}
	// the focus stays on the screen (its buttons are not focusable): the chosen line is where the keys are
	return FNavigationReply::Stop();
}

FReply STapaoShopScreen::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// a click beside the lines keeps the keys on the shop (the game viewport would take the focus)
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
}

FReply STapaoShopScreen::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// the wheel over the title or the keys scrolls the list too (over the list, the list takes it first)
	if (!List.IsValid())
	{
		return FReply::Unhandled();
	}
	const float Step = MouseEvent.GetWheelDelta() * GetGlobalScrollAmount() * Layout.WheelScrollMultiplier;
	const float End = FMath::Max(0.0f, List->GetScrollOffsetOfEnd());
	List->SetScrollOffset(FMath::Clamp(List->GetScrollOffset() - Step, 0.0f, End));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
