// monke_island

#include "UI/Tapao/STapaoHUD.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Galleon/GalleonActor.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/MusketWeapon.h"
#include "Weapons/TorchItem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Kit sprites are @2x of a 4K layout: the HUD draws them at this fraction of their source size (1080p) */
	constexpr float HudScale = 0.3f;

	/** The gauges: a fine line (px at 1080p) after a small icon */
	const FVector2D FineBar(200.0f, 7.0f);
	constexpr float FineIcon = 20.0f;

	/** Fill window of the gauge frames (ui_manifest.json fill_window_px 190, 44, 1060, 112 in 1280 x 200) */
	const FMargin FillWindow(190.0f / 1280.0f, 44.0f / 200.0f, 30.0f / 1280.0f, 44.0f / 200.0f);
	const FVector2D BarSource(1280.0f, 200.0f);
	const FVector2D CounterSource(680.0f, 200.0f);
	constexpr float SlotSize = 64.0f;

	/** Kit thumbnail of an inventory item, from its class name */
	FName ItemIcon(const AInventoryItemActor* Item)
	{
		if (!Item)
		{
			return NAME_None;
		}
		const FString Name = Item->GetClass()->GetName();
		if (Name.Contains(TEXT("Musket"))) { return TEXT("item_musket"); }
		if (Name.Contains(TEXT("Cutlass"))) { return TEXT("item_cutlass"); }
		if (Name.Contains(TEXT("Torch"))) { return TEXT("item_torch"); }
		if (Name.Contains(TEXT("Trumpet"))) { return TEXT("item_trumpet"); }
		return NAME_None;
	}

	const UIslanderInventoryComponent* GetInventory(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	}

	template <typename T>
	const T* FindItem(const APawn* Pawn, bool bEquippedOnly)
	{
		const UIslanderInventoryComponent* Inventory = GetInventory(Pawn);
		if (!Inventory)
		{
			return nullptr;
		}
		if (bEquippedOnly)
		{
			return Cast<T>(Inventory->GetEquippedItem());
		}
		for (int32 Slot = 0; Slot < 9; ++Slot)
		{
			if (const T* Item = Cast<T>(Inventory->GetItem(Slot)))
			{
				return Item;
			}
		}
		return nullptr;
	}
}

APawn* STapaoHUD::GetPawn() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const APlayerController* Controller = Instance ? Instance->GetFirstLocalPlayerController() : nullptr;
	return Controller ? Controller->GetPawn() : nullptr;
}

void STapaoHUD::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	GameInstance = InArgs._GameInstance;
	SetVisibility(EVisibility::HitTestInvisible);
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &STapaoHUD::Refresh));

	auto Health = [this]() -> const UIslanderHealthComponent*
	{
		const APawn* Pawn = GetPawn();
		return Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	};
	auto Movement = [this]() -> const UIslanderMovementComponent*
	{
		const ACharacter* Character = Cast<ACharacter>(GetPawn());
		return Character ? Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	};

	ChildSlot
	[
		SNew(SOverlay)
		// bottom left: torch fuel (in hand only), health, stamina, above the stance line of AIslanderHUD
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.0f, 0.0f, 0.0f, 62.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeBar(TEXT("hud_flame"), TEXT("fill_fuel"), FuelStyle,
					[this]()
					{
						const ATorchItem* Torch = FindItem<ATorchItem>(GetPawn(), true);
						return Torch && Torch->Rules && Torch->Rules->BurnTime > 0.0f ? Torch->GetFuel() / Torch->Rules->BurnTime : 0.0f;
					},
					[this]()
					{
						const ATorchItem* Torch = FindItem<ATorchItem>(GetPawn(), true);
						return Torch ? FText::AsNumber(FMath::CeilToInt32(Torch->GetFuel())) : FText::GetEmpty();
					},
					TAttribute<EVisibility>::CreateLambda([this]() { return FindItem<ATorchItem>(GetPawn(), true) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeBar(TEXT("hud_heart"), TEXT("fill_health"), HealthStyle,
					[Health]() { const UIslanderHealthComponent* H = Health(); return H ? H->GetHealthFraction() : 0.0f; },
					[Health]() { const UIslanderHealthComponent* H = Health(); return H ? FText::AsNumber(FMath::CeilToInt32(H->GetHealth())) : FText::GetEmpty(); },
					EVisibility::HitTestInvisible)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeBar(TEXT("hud_bolt"), TEXT("fill_stamina"), StaminaStyle,
					[Movement]() { const UIslanderMovementComponent* M = Movement(); return M ? M->GetStaminaFraction() : 0.0f; },
					[Movement]() { const UIslanderMovementComponent* M = Movement(); return M ? FText::AsNumber(FMath::CeilToInt32(M->GetStamina())) : FText::GetEmpty(); },
					EVisibility::HitTestInvisible)
			]
		]
		// bottom right: the money alone (value of the cargo aboard the galleon), no plate, no icon; the flood clock is top right
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 28.0f, 22.0f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return bHasGalleon ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text_Lambda([this]() { return FText::Format(NSLOCTEXT("TapaoUI", "Cargo", "{0} $"), CargoValue); })
			.Font(Style->Font(ETapaoFont::UI, 26, 2))
			.ColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor::White)
		]
		// bottom centre: item bar
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 18.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return GetInventory(GetPawn()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[MakeSlot(0)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[MakeSlot(1)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[MakeSlot(2)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[MakeSlot(3)]
		]
	];
}

TSharedRef<SWidget> STapaoHUD::MakeBar(FName Frame, FName Fill, FProgressBarStyle& BarStyle, TFunction<float()> Fraction, TFunction<FText()> Label, TAttribute<EVisibility> Visibility)
{
	// a fine line and its small icon (owner request 2026-10-04: much finer, no frame, no number); Frame names the icon
	UTapaoUISkin* Skin = Style->GetSkin();
	BarStyle = FProgressBarStyle()
		.SetBackgroundImage(*Style->TextureBrush(Skin ? Skin->Fills.FindRef(TEXT("fill_background")) : nullptr))
		.SetFillImage(*Style->TextureBrush(Skin ? Skin->Fills.FindRef(Fill) : nullptr))
		.SetMarqueeImage(FSlateNoResource());
	return SNew(SHorizontalBox)
		.Visibility(Visibility)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 2.0f, 6.0f, 2.0f)
		[
			SNew(SBox).WidthOverride(FineIcon).HeightOverride(FineIcon)
			[
				SNew(SImage).Image(Style->Brush(Frame))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(FineBar.X).HeightOverride(FineBar.Y)
			[
				SNew(SProgressBar).Style(&BarStyle).Percent_Lambda([Fraction]() { return TOptional<float>(FMath::Clamp(Fraction(), 0.0f, 1.0f)); })
			]
		];
}

TSharedRef<SWidget> STapaoHUD::MakeCounter(FName Icon, TFunction<FText()> Value, TAttribute<EVisibility> Visibility)
{
	const UTapaoUISkin* Skin = Style->GetSkin();
	const FVector2D Size = CounterSource * HudScale;
	return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y).Visibility(Visibility)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(Style->Brush(TEXT("counter_plate")))
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(46.0f).HeightOverride(46.0f)
				[
					SNew(SImage).Image(Style->Brush(Icon))
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 26.0f, 0.0f)
			[
				SNew(STextBlock).Text_Lambda([Value]() { return Value(); }).Font(Style->Font(ETapaoFont::UI, 24, 2)).ColorAndOpacity(Skin ? Skin->Cream : FLinearColor::White)
			]
		];
}

TSharedRef<SWidget> STapaoHUD::MakeSlot(int32 Slot)
{
	const FName Badge(*FString::Printf(TEXT("badge_%d"), Slot + 1));
	const FButtonStyle* SlotStyle = &Style->Button(TEXT("slot_0"));
	return SNew(SBox).WidthOverride(SlotSize).HeightOverride(SlotSize)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				// the slot in hand is lit (the hover picture of the kit)
				SNew(SImage).Image_Lambda([this, Slot, SlotStyle]()
				{
					const UIslanderInventoryComponent* Inventory = GetInventory(GetPawn());
					return Inventory && Inventory->GetActiveSlot() == Slot ? &SlotStyle->Hovered : &SlotStyle->Normal;
				})
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(SlotSize * 0.66f).HeightOverride(SlotSize * 0.66f)
				[
					SNew(SImage).Image_Lambda([this, Slot]()
					{
						const UIslanderInventoryComponent* Inventory = GetInventory(GetPawn());
						const FName Icon = ItemIcon(Inventory ? Inventory->GetItem(Slot) : nullptr);
						UTapaoUISkin* Skin = Style->GetSkin();
						return Style->TextureBrush(Skin && !Icon.IsNone() ? Skin->Items.FindRef(Icon).Get() : nullptr);
					})
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
			[
				SNew(SBox).WidthOverride(22.0f).HeightOverride(22.0f)
				[
					SNew(SImage).Image(Style->Brush(Badge))
				]
			]
		];
}

EActiveTimerReturnType STapaoHUD::Refresh(double CurrentTime, float DeltaTime)
{
	const APawn* Pawn = GetPawn();
	const AGalleonActor* Galleon = Pawn ? AGalleonActor::Get(Pawn) : nullptr;
	bHasGalleon = Galleon != nullptr;
	CargoValue = Galleon ? Galleon->GetCargoValue() : 0;
	return EActiveTimerReturnType::Continue;
}
