// monke_island

#include "Settings/SSettingsMenu.h"
#include "Settings/SettingsMath.h"
#include "Settings/SettingsRules.h"
#include "Settings/SettingsSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SettingsMenu"

namespace
{
	const FLinearColor Backdrop(0.0f, 0.0f, 0.0f, 0.55f);
	const FLinearColor Panel(0.06f, 0.07f, 0.09f, 0.96f);
	const FLinearColor Accent(0.95f, 0.72f, 0.3f, 1.0f);
	const FLinearColor TextColor(0.92f, 0.92f, 0.9f, 1.0f);
	const FLinearColor Dim(0.6f, 0.62f, 0.66f, 1.0f);

	FSlateFontInfo Font(int32 Size, const TCHAR* Style = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	FText Percent(float Value)
	{
		return FText::AsNumber(FMath::RoundToInt(Value * 100.0f));
	}

	/** Combo box over a fixed list of labelled values */
	struct FChoiceList
	{
		TArray<TSharedPtr<int32>> Items;
		TArray<FText> Labels;
	};
}

void SSettingsMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	if (const USettingsSubsystem* Subsystem = Owner.Get())
	{
		Pending = Subsystem->GetApplied();
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Backdrop)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Panel)
				.Padding(28.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
					[
						SNew(STextBlock).Text(LOCTEXT("Title", "Réglages")).Font(Font(26, TEXT("Bold"))).ColorAndOpacity(Accent)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[MakeTabButton(ETab::Audio, LOCTEXT("TabAudio", "Audio"))]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[MakeTabButton(ETab::Display, LOCTEXT("TabDisplay", "Affichage"))]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[MakeTabButton(ETab::Mouse, LOCTEXT("TabMouse", "Souris"))]
						+ SHorizontalBox::Slot().AutoWidth()[MakeTabButton(ETab::Comfort, LOCTEXT("TabComfort", "Confort"))]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).MinDesiredHeight(330.0f)
						[
							SAssignNew(Pages, SWidgetSwitcher)
							+ SWidgetSwitcher::Slot()[MakeAudioTab()]
							+ SWidgetSwitcher::Slot()[MakeDisplayTab()]
							+ SWidgetSwitcher::Slot()[MakeMouseTab()]
							+ SWidgetSwitcher::Slot()[MakeComfortTab()]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).OnClicked(this, &SSettingsMenu::OnRestoreTab)
							[
								SNew(STextBlock).Text(LOCTEXT("Restore", "Restaurer cet onglet")).Font(Font(13))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SNew(SButton).OnClicked(this, &SSettingsMenu::OnCancel).IsEnabled_Lambda([this]() { return HasChanges(); })
							[
								SNew(STextBlock).Text(LOCTEXT("Cancel", "Annuler")).Font(Font(13))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SNew(SButton).OnClicked(this, &SSettingsMenu::OnApply).IsEnabled_Lambda([this]() { return HasChanges(); })
							[
								SNew(STextBlock).Text(LOCTEXT("Apply", "Appliquer")).Font(Font(13, TEXT("Bold")))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SNew(SButton).OnClicked(this, &SSettingsMenu::OnClose)
							[
								SNew(STextBlock).Text(LOCTEXT("Close", "Fermer")).Font(Font(13))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
					[
						SNew(STextBlock)
						.Font(Font(11))
						.ColorAndOpacity(Dim)
						.Text_Lambda([this]()
						{
							return HasChanges()
								? LOCTEXT("Unapplied", "Modifications non appliquées : Fermer les abandonne (le son est déjà prévisualisé).")
								: (GIsEditor ? LOCTEXT("HintEditor", "F10 ouvre et ferme ce menu (Échap arrête la partie dans l'éditeur).")
									: LOCTEXT("Hint", "Échap ou F10 ouvre et ferme ce menu."));
						})
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SSettingsMenu::MakeTabButton(ETab Tab, const FText& Label)
{
	return SNew(SButton)
		.OnClicked_Lambda([this, Tab]()
		{
			CurrentTab = Tab;
			Pages->SetActiveWidgetIndex(static_cast<int32>(Tab));
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Font(15, TEXT("Bold")))
			.ColorAndOpacity_Lambda([this, Tab]() { return FSlateColor(CurrentTab == Tab ? Accent : Dim); })
		];
}

TSharedRef<SWidget> SSettingsMenu::MakeRow(const FText& Label, TSharedRef<SWidget> Control)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(240.0f)
			[
				SNew(STextBlock).Text(Label).Font(Font(14)).ColorAndOpacity(TextColor)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			Control
		];
}

TSharedRef<SWidget> SSettingsMenu::MakeSlider(TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, TFunction<FText(float)> Format)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SSlider)
			.MinValue(Min)
			.MaxValue(Max)
			.Value_Lambda([Get]() { return Get(); })
			.OnValueChanged_Lambda([Set](float Value) { Set(Value); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0)
		[
			SNew(SBox).WidthOverride(64.0f)
			[
				SNew(STextBlock).Font(Font(14)).ColorAndOpacity(TextColor).Text_Lambda([Get, Format]() { return Format(Get()); })
			]
		];
}

TSharedRef<SWidget> SSettingsMenu::MakeChoice(TArray<TPair<FText, int32>> Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	TSharedRef<FChoiceList> List = MakeShared<FChoiceList>();
	for (const TPair<FText, int32>& Option : Options)
	{
		List->Items.Add(MakeShared<int32>(Option.Value));
		List->Labels.Add(Option.Key);
	}
	const auto LabelOf = [List](int32 Value)
	{
		for (int32 Index = 0; Index < List->Items.Num(); ++Index)
		{
			if (*List->Items[Index] == Value)
			{
				return List->Labels[Index];
			}
		}
		return LOCTEXT("Custom", "Personnalisé");
	};
	return SNew(SComboBox<TSharedPtr<int32>>)
		.OptionsSource(&List->Items)
		.OnGenerateWidget_Lambda([List, LabelOf](TSharedPtr<int32> Item)
		{
			return SNew(STextBlock).Text(LabelOf(Item.IsValid() ? *Item : -999)).Font(Font(13));
		})
		.OnSelectionChanged_Lambda([List, Set](TSharedPtr<int32> Item, ESelectInfo::Type)
		{
			if (Item.IsValid())
			{
				Set(*Item);
			}
		})
		[
			SNew(STextBlock).Font(Font(14)).ColorAndOpacity(TextColor).Text_Lambda([Get, LabelOf]() { return LabelOf(Get()); })
		];
}

TSharedRef<SWidget> SSettingsMenu::MakeCheck(TFunction<bool()> Get, TFunction<void(bool)> Set)
{
	return SNew(SCheckBox)
		.IsChecked_Lambda([Get]() { return Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([Set](ECheckBoxState State) { Set(State == ECheckBoxState::Checked); });
}

TSharedRef<SWidget> SSettingsMenu::MakeAudioTab()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const TPair<ESettingsVolume, FText> Channels[] =
	{
		{ ESettingsVolume::Master, LOCTEXT("Master", "Volume général") },
		{ ESettingsVolume::Music, LOCTEXT("Music", "Musique") },
		{ ESettingsVolume::Effects, LOCTEXT("Effects", "Effets (pas, armes, impacts)") },
		{ ESettingsVolume::Ambience, LOCTEXT("Ambience", "Ambiance (vagues, vent, village)") },
		{ ESettingsVolume::Dialogue, LOCTEXT("Dialogue", "Dialogues") },
		{ ESettingsVolume::Interface, LOCTEXT("Interface", "Interface") },
	};
	for (const TPair<ESettingsVolume, FText>& Channel : Channels)
	{
		const ESettingsVolume Key = Channel.Key;
		Box->AddSlot().AutoHeight().Padding(0, 6)
		[
			MakeRow(Channel.Value, MakeSlider(
				[this, Key]() { return Pending.Volume(Key); },
				[this, Key](float Value)
				{
					Pending.Volume(Key) = Value;
					if (USettingsSubsystem* Subsystem = Owner.Get())
					{
						Subsystem->PreviewAudio(Pending);
					}
				},
				0.0f, 1.0f, [](float Value) { return FText::Format(LOCTEXT("PercentFmt", "{0} %"), Percent(Value)); }))
		];
	}
	return Box;
}

TSharedRef<SWidget> SSettingsMenu::MakeDisplayTab()
{
	TArray<TPair<FText, int32>> Modes =
	{
		{ LOCTEXT("Fullscreen", "Plein écran"), 0 },
		{ LOCTEXT("Borderless", "Fenêtré sans bordure"), 1 },
		{ LOCTEXT("Windowed", "Fenêtré"), 2 },
	};

	// resolutions are encoded as width * 100000 + height
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	Supported.AddUnique(Pending.Resolution);
	Supported.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X > B.X : A.Y > B.Y; });
	TArray<TPair<FText, int32>> Resolutions;
	for (const FIntPoint& Size : Supported)
	{
		Resolutions.Add({ FText::FromString(FString::Printf(TEXT("%d x %d"), Size.X, Size.Y)), Size.X * 100000 + Size.Y });
	}

	TArray<TPair<FText, int32>> Rates;
	const USettingsSubsystem* Subsystem = Owner.Get();
	const USettingsRules* Rules = Subsystem ? Subsystem->GetRules() : nullptr;
	for (const int32 Limit : Rules ? Rules->FrameRateChoices : TArray<int32>{ 30, 60, 120, 0 })
	{
		Rates.Add({ FText::FromString(SettingsMath::FrameRateLabel(Limit)), Limit });
	}

	TArray<TPair<FText, int32>> Qualities =
	{
		{ LOCTEXT("Low", "Faible"), 0 },
		{ LOCTEXT("Medium", "Moyen"), 1 },
		{ LOCTEXT("High", "Élevé"), 2 },
		{ LOCTEXT("Ultra", "Ultra"), 3 },
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("Mode", "Mode d'affichage"), MakeChoice(Modes, [this]() { return Pending.WindowMode; }, [this](int32 Value) { Pending.WindowMode = Value; }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("Resolution", "Résolution"), MakeChoice(Resolutions,
				[this]() { return Pending.Resolution.X * 100000 + Pending.Resolution.Y; },
				[this](int32 Value) { Pending.Resolution = FIntPoint(Value / 100000, Value % 100000); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("VSync", "Synchronisation verticale"), MakeCheck([this]() { return Pending.bVSync; }, [this](bool bValue) { Pending.bVSync = bValue; }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("Fps", "Limite d'images par seconde"), MakeChoice(Rates, [this]() { return Pending.FrameRateLimit; }, [this](int32 Value) { Pending.FrameRateLimit = Value; }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("Quality", "Qualité graphique"), MakeChoice(Qualities, [this]() { return Pending.Quality; }, [this](int32 Value) { Pending.Quality = Value; }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10)
		[
			SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Dim).AutoWrapText(true)
			.Text(GIsEditor ? LOCTEXT("EditorDisplay", "Dans l'éditeur, la fenêtre et la résolution ne changent pas : elles s'appliquent au jeu packagé.") : FText::GetEmpty())
		];
}

TSharedRef<SWidget> SSettingsMenu::MakeMouseTab()
{
	const USettingsSubsystem* Subsystem = Owner.Get();
	const USettingsRules* Rules = Subsystem ? Subsystem->GetRules() : nullptr;
	const FVector2D Range = Rules ? Rules->SensitivityRange : FVector2D(0.2f, 3.0f);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("Sensitivity", "Sensibilité"), MakeSlider(
				[this]() { return Pending.Sensitivity; },
				[this](float Value) { Pending.Sensitivity = Value; },
				static_cast<float>(Range.X), static_cast<float>(Range.Y),
				[](float Value) { return FText::AsNumber(FMath::RoundToFloat(Value * 100.0f) / 100.0f); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("InvertY", "Inverser l'axe vertical"), MakeCheck([this]() { return Pending.bInvertY; }, [this](bool bValue) { Pending.bInvertY = bValue; }))
		];
}

TSharedRef<SWidget> SSettingsMenu::MakeComfortTab()
{
	const USettingsSubsystem* Subsystem = Owner.Get();
	const USettingsRules* Rules = Subsystem ? Subsystem->GetRules() : nullptr;
	const FVector2D Range = Rules ? Rules->FieldOfViewRange : FVector2D(80.0f, 110.0f);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
		[
			MakeRow(LOCTEXT("Fov", "Champ de vision"), MakeSlider(
				[this]() { return Pending.FieldOfView; },
				[this](float Value) { Pending.FieldOfView = FMath::RoundToFloat(Value); },
				static_cast<float>(Range.X), static_cast<float>(Range.Y),
				[](float Value) { return FText::Format(LOCTEXT("FovFmt", "{0}°"), FText::AsNumber(FMath::RoundToInt(Value))); }))
		];
}

bool SSettingsMenu::HasChanges() const
{
	const USettingsSubsystem* Subsystem = Owner.Get();
	if (!Subsystem)
	{
		return false;
	}
	const FMonkeSettingsValues& Applied = Subsystem->GetApplied();
	for (int32 Index = 0; Index < static_cast<int32>(ESettingsVolume::Count); ++Index)
	{
		if (!FMath::IsNearlyEqual(Pending.Volumes[Index], Applied.Volumes[Index]))
		{
			return true;
		}
	}
	return !FMath::IsNearlyEqual(Pending.Sensitivity, Applied.Sensitivity) || Pending.bInvertY != Applied.bInvertY
		|| !FMath::IsNearlyEqual(Pending.FieldOfView, Applied.FieldOfView) || Pending.DisplayDiffers(Applied);
}

FReply SSettingsMenu::OnRestoreTab()
{
	USettingsSubsystem* Subsystem = Owner.Get();
	if (!Subsystem)
	{
		return FReply::Handled();
	}
	const FMonkeSettingsValues Defaults = Subsystem->GetDefaults();
	switch (CurrentTab)
	{
	case ETab::Audio:
		FMemory::Memcpy(Pending.Volumes, Defaults.Volumes, sizeof(Pending.Volumes));
		Subsystem->PreviewAudio(Pending);
		break;
	case ETab::Display:
		Pending.FrameRateLimit = Defaults.FrameRateLimit;
		Pending.bVSync = false;
		Pending.WindowMode = 1;
		break;
	case ETab::Mouse:
		Pending.Sensitivity = Defaults.Sensitivity;
		Pending.bInvertY = Defaults.bInvertY;
		break;
	case ETab::Comfort:
		Pending.FieldOfView = Defaults.FieldOfView;
		break;
	default:
		break;
	}
	return FReply::Handled();
}

FReply SSettingsMenu::OnCancel()
{
	if (USettingsSubsystem* Subsystem = Owner.Get())
	{
		Pending = Subsystem->GetApplied();
		Subsystem->Cancel();
	}
	return FReply::Handled();
}

FReply SSettingsMenu::OnApply()
{
	if (USettingsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->Apply(Pending);
		Pending = Subsystem->GetApplied();
	}
	return FReply::Handled();
}

FReply SSettingsMenu::OnClose()
{
	if (USettingsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->CloseMenu();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
