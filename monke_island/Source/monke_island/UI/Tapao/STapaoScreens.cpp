// monke_island

#include "UI/Tapao/STapaoScreens.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SoundBase.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TapaoUI"

namespace
{
	/** Wide buttons of the kit at 1920 x 1080 (sprite 1200 x 300) */
	const FVector2D WideButton(360.0f, 90.0f);

	/** A kit sound, through the main audio device (menus have no world to play in) */
	void PlayUISound(USoundBase* Sound)
	{
		if (Sound && FSlateApplication::IsInitialized())
		{
			FSlateSound Slate;
			Slate.SetResourceObject(Sound);
			FSlateApplication::Get().PlaySound(Slate);
		}
	}

	/** Full screen picture, cropped to fill */
	TSharedRef<SWidget> MakeBackground(const FTapaoStyle& Style, UTexture2D* Texture)
	{
		return SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage).Image(Style.TextureBrush(Texture))
			];
	}

	/** A sprite at a width, keeping its proportions */
	TSharedRef<SWidget> MakeSprite(const FTapaoStyle& Style, FName Sprite, float Width)
	{
		const FVector2D Size = Style.Size(Sprite);
		const float Height = Size.X > 0.0f ? Width * Size.Y / Size.X : Width;
		return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
			[
				SNew(SImage).Image(Style.Brush(Sprite))
			];
	}
}

TSharedRef<SWidget> MakeTapaoButton(const TSharedRef<FTapaoStyle>& Style, FName Sprite, const FVector2D& Size, FSimpleDelegate OnClicked, TAttribute<bool> Enabled)
{
	UTapaoUISkin* Skin = Style->GetSkin();
	USoundBase* Hover = Skin ? Skin->HoverSound.Get() : nullptr;
	USoundBase* Click = Skin ? Skin->ClickSound.Get() : nullptr;
	return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
		[
			SNew(SButton)
			.ButtonStyle(&Style->Button(Sprite))
			.IsEnabled(Enabled)
			.OnHovered_Lambda([Hover]() { PlayUISound(Hover); })
			.OnClicked_Lambda([OnClicked, Click]()
			{
				PlayUISound(Click);
				OnClicked.ExecuteIfBound();
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> MakeTapaoTextButton(const TSharedRef<FTapaoStyle>& Style, const FText& Label, const FVector2D& Size, FSimpleDelegate OnClicked, TAttribute<bool> Enabled)
{
	UTapaoUISkin* Skin = Style->GetSkin();
	USoundBase* Hover = Skin ? Skin->HoverSound.Get() : nullptr;
	USoundBase* Click = Skin ? Skin->ClickSound.Get() : nullptr;
	return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
		[
			SNew(SButton)
			.ButtonStyle(&Style->Button(TEXT("btn_wide_blank")))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(0.0f))
			.IsEnabled(Enabled)
			.OnHovered_Lambda([Hover]() { PlayUISound(Hover); })
			.OnClicked_Lambda([OnClicked, Click]()
			{
				PlayUISound(Click);
				OnClicked.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(Style->Font(ETapaoFont::UI, 24, 2)).ColorAndOpacity(Skin ? Skin->Cream : FLinearColor::White)
			]
		];
}

// ---------------------------------------------------------------- main menu

void STapaoMainMenu::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	const UTapaoUISkin* Skin = Style->GetSkin();
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			MakeBackground(*Style, Skin ? Skin->MenuBackground.Get() : nullptr)
		]
		+ SOverlay::Slot()
		[
			SAssignNew(Pages, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()[MakeHome(InArgs)]
			+ SWidgetSwitcher::Slot()[MakeCredits()]
		]
	];
}

TSharedRef<SWidget> STapaoMainMenu::MakeHome(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	const FLinearColor Cream = Skin ? Skin->Cream : FLinearColor::White;
	TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
	auto Add = [&Buttons, &S](FName Sprite, FSimpleDelegate Event, TAttribute<bool> Enabled = true)
	{
		Buttons->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f)
			[
				MakeTapaoButton(S, Sprite, WideButton, Event, Enabled)
			];
	};
	Add(TEXT("btn_jouer"), InArgs._OnPlay);
	Add(TEXT("btn_continuer"), InArgs._OnContinue, InArgs._CanContinue);
	Add(TEXT("btn_nouvelle_partie"), InArgs._OnNewGame);
	Add(TEXT("btn_options"), InArgs._OnOptions);
	Add(TEXT("btn_credits"), FSimpleDelegate::CreateSP(this, &STapaoMainMenu::ShowCredits, true));
	Add(TEXT("btn_quitter"), InArgs._OnQuit);

	auto Icon = [&S](FName Sprite, FSimpleDelegate Event)
	{
		return MakeTapaoButton(S, Sprite, FVector2D(96.0f, 96.0f), Event);
	};

	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.0f, 40.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				MakeSprite(*S, TEXT("logo"), 560.0f)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 30.0f, 0.0f, 0.0f)
			[
				Buttons
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._ContinueHint)
				.Font(S->Font(ETapaoFont::Text, 16, 1))
				.ColorAndOpacity(Cream)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(30.0f, 0.0f, 0.0f, 22.0f)
		[
			SNew(STextBlock)
			.Text(Skin ? Skin->VersionText : FText::GetEmpty())
			.Font(S->Font(ETapaoFont::Text, 20, 2))
			.ColorAndOpacity(Cream)
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 24.0f, 24.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[Icon(TEXT("icon_info"), FSimpleDelegate::CreateSP(this, &STapaoMainMenu::ShowCredits, true))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[Icon(TEXT("icon_settings"), InArgs._OnOptions)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)
				[
					SAssignNew(SoundButton, SButton)
					.ButtonStyle(&Style->Button(TEXT("icon_sound_on")))
					.OnClicked_Lambda([this]() { ToggleSound(); return FReply::Handled(); })
				]
			]
		];
}

TSharedRef<SWidget> STapaoMainMenu::MakeCredits()
{
	const TSharedRef<FTapaoStyle> S = Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	if (Skin)
	{
		for (const FText& Line : Skin->Credits)
		{
			Lines->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 3.0f)
				[
					SNew(STextBlock).Text(Line).Font(S->Font(ETapaoFont::Text, 20)).ColorAndOpacity(Skin->Ink).Justification(ETextJustify::Center).AutoWrapText(true)
				];
		}
	}
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Skin ? Skin->Veil : FLinearColor(0, 0, 0, 0.5f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(900.0f).HeightOverride(760.0f)
			[
				SNew(SBorder).BorderImage(S->Brush(TEXT("panel_parchment"))).Padding(FMargin(80.0f, 70.0f, 80.0f, 50.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("Credits", "CRÉDITS")).Font(S->Font(ETapaoFont::Title, 54)).ColorAndOpacity(Skin ? Skin->Ink : FLinearColor::Black)
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f)
					[
						SNew(SScrollBox) + SScrollBox::Slot()[Lines]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						MakeTapaoButton(S, TEXT("btn_retour"), WideButton, FSimpleDelegate::CreateSP(this, &STapaoMainMenu::ShowCredits, false))
					]
				]
			]
		];
}

void STapaoMainMenu::ShowCredits(bool bShow)
{
	if (Pages.IsValid())
	{
		Pages->SetActiveWidgetIndex(bShow ? 1 : 0);
	}
}

void STapaoMainMenu::ToggleSound()
{
	bMuted = !bMuted;
	if (SoundButton.IsValid())
	{
		SoundButton->SetButtonStyle(&Style->Button(bMuted ? TEXT("icon_sound_off") : TEXT("icon_sound_on")));
	}
	if (FAudioDeviceHandle Device = GEngine ? GEngine->GetMainAudioDevice() : FAudioDeviceHandle())
	{
		Device->SetTransientPrimaryVolume(bMuted ? 0.0f : 1.0f);
	}
}

FReply STapaoMainMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && Pages.IsValid() && Pages->GetActiveWidgetIndex() != 0)
	{
		ShowCredits(false);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

// ---------------------------------------------------------------- pause

void STapaoPause::Construct(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Skin ? Skin->Veil : FLinearColor(0, 0, 0, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(560.0f).HeightOverride(620.0f)
				[
					SNew(SBorder).BorderImage(S->Brush(TEXT("panel_parchment"))).Padding(FMargin(60.0f, 110.0f, 60.0f, 50.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f)[MakeTapaoButton(S, TEXT("btn_reprendre"), WideButton, InArgs._OnResume)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f)[MakeTapaoButton(S, TEXT("btn_options"), WideButton, InArgs._OnOptions)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f)[MakeTapaoButton(S, TEXT("btn_menu_principal"), WideButton, InArgs._OnMainMenu)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f)[MakeTapaoButton(S, TEXT("btn_quitter"), WideButton, InArgs._OnQuit)]
					]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 0.0f, 560.0f)
		[
			MakeSprite(*S, TEXT("ribbon_pause"), 520.0f)
		]
	];
}

// ---------------------------------------------------------------- loading

namespace
{
	/** Start of the loading in progress, and when a loading screen was last drawn: a screen opened right after
	 *  another (menu, boot map, island) goes on with the same gauge instead of starting again */
	double GLoadingStart = 0.0;
	double GLoadingSeen = -1000.0;
}

void STapaoLoading::Construct(const FArguments& InArgs)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - GLoadingSeen > 1.0)
	{
		GLoadingStart = Now;
	}
	GLoadingSeen = Now;
	StartTime = GLoadingStart;
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	UTapaoUISkin* Skin = S->GetSkin();

	static FProgressBarStyle BarStyle;
	BarStyle = FProgressBarStyle()
		.SetBackgroundImage(*S->TextureBrush(Skin ? Skin->Fills.FindRef(TEXT("fill_background")) : nullptr))
		.SetFillImage(*S->TextureBrush(Skin ? Skin->Fills.FindRef(TEXT("fill_gold")) : nullptr))
		.SetMarqueeImage(FSlateNoResource());

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			MakeBackground(*S, Skin ? Skin->LoadingBackground.Get() : nullptr)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				MakeSprite(*S, TEXT("logo"), 620.0f)
			]
			// "Jour N / Période P" (ADR-006), passed as the line of the screen; nothing when empty
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Visibility(InArgs._Tip.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				.Text(InArgs._Tip)
				.Font(S->Font(ETapaoFont::Title, 44, 2))
				.ColorAndOpacity(Skin ? Skin->Cream : FLinearColor::White)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f)
			[
				// 25 % finer than the kit's gauge (owner request 2026-10-04)
				SNew(SBox).WidthOverride(640.0f).HeightOverride(52.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().Padding(FMargin(22.0f, 13.0f))
					[
						SNew(SProgressBar).Style(&BarStyle).Percent(this, &STapaoLoading::GetProgress)
					]
					+ SOverlay::Slot()
					[
						SNew(SImage).Image(S->Brush(TEXT("bar_blank")))
					]
				]
			]
		]
	];
}

TOptional<float> STapaoLoading::GetProgress() const
{
	// a slow fill that never quite ends: the map gives no progress
	const double Now = FPlatformTime::Seconds();
	GLoadingSeen = Now;
	return static_cast<float>(1.0 - FMath::Exp(-(Now - StartTime) / 4.0));
}

// ---------------------------------------------------------------- title screen

void STapaoSplash::Construct(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	OnDone = InArgs._OnDone;
	MinSeconds = InArgs._MinSeconds;
	StartTime = FPlatformTime::Seconds();
	RegisterActiveTimer(FMath::Max(InArgs._MaxSeconds, 0.1f), FWidgetActiveTimerDelegate::CreateSP(this, &STapaoSplash::HandleTimeout));
	const FLinearColor Cream = Skin ? Skin->Cream : FLinearColor::White;
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			MakeBackground(*S, Skin ? Skin->MenuBackground.Get() : nullptr)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				MakeSprite(*S, TEXT("logo"), 760.0f)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 40.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PressAKey", "Appuyez sur une touche"))
				.Font(S->Font(ETapaoFont::UI, 30, 2))
				.ColorAndOpacity_Lambda([this, Cream]()
				{
					// a slow blink
					FLinearColor Colour = Cream;
					Colour.A = 0.55f + 0.45f * FMath::Sin(static_cast<float>(FPlatformTime::Seconds() - StartTime) * 3.0f);
					return FSlateColor(Colour);
				})
			]
		]
	];
}

FReply STapaoSplash::TryLeave()
{
	if (!bDone && FPlatformTime::Seconds() - StartTime >= MinSeconds)
	{
		bDone = true;
		OnDone.ExecuteIfBound();
	}
	return FReply::Handled();
}

FReply STapaoSplash::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return TryLeave();
}

FReply STapaoSplash::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TryLeave();
}

EActiveTimerReturnType STapaoSplash::HandleTimeout(double CurrentTime, float DeltaTime)
{
	if (!bDone)
	{
		bDone = true;
		OnDone.ExecuteIfBound();
	}
	return EActiveTimerReturnType::Stop;
}

// ---------------------------------------------------------------- confirm

void STapaoConfirm::Construct(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	OnNo = InArgs._OnNo;
	const FVector2D Button(300.0f, 75.0f);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Skin ? Skin->Veil : FLinearColor(0, 0, 0, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(760.0f)
			[
				SNew(SBorder).BorderImage(S->Brush(TEXT("panel_parchment"))).Padding(FMargin(70.0f, 60.0f, 70.0f, 50.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Text(InArgs._Question).Font(S->Font(ETapaoFont::Text, 26)).ColorAndOpacity(Skin ? Skin->Ink : FLinearColor::Black)
						.Justification(ETextJustify::Center).AutoWrapText(true).WrapTextAt(600.0f)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 34.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[MakeTapaoTextButton(S, InArgs._Yes, Button, InArgs._OnYes)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[MakeTapaoTextButton(S, InArgs._No, Button, InArgs._OnNo)]
					]
				]
			]
		]
	];
}

FReply STapaoConfirm::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnNo.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

// ---------------------------------------------------------------- end of a game

void STapaoEndScreen::Construct(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	for (const FText& Line : InArgs._Lines)
	{
		Lines->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
			[
				SNew(STextBlock).Text(Line).Font(S->Font(ETapaoFont::Text, 24)).ColorAndOpacity(Skin ? Skin->Ink : FLinearColor::Black)
			];
	}
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Skin ? Skin->Veil : FLinearColor(0, 0, 0, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				MakeSprite(*S, InArgs._Ribbon, 620.0f)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(620.0f)
				[
					SNew(SBorder).BorderImage(S->Brush(TEXT("panel_parchment"))).Padding(FMargin(60.0f, 50.0f, 60.0f, 44.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Lines]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 24.0f, 0.0f, 6.0f)
						[
							MakeTapaoButton(S, InArgs._FirstButton, WideButton, InArgs._OnFirst, InArgs._FirstEnabled)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							MakeTapaoButton(S, TEXT("btn_menu_principal"), WideButton, InArgs._OnMainMenu)
						]
					]
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
