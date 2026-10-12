// monke_island

#include "UI/Tapao/STapaoDayEnd.h"
#include "UI/Tapao/STapaoScreens.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TapaoDayEnd"

namespace
{
	/** Width the texts of the notice wrap in (px at 1080p) */
	constexpr float NoticeTextWidth = 640.0f;

	/** Font sizes (Slate points at 1080p, 1 pt = 1.33 px): every line 20 px or more */
	constexpr int32 NoticeHeaderSize = 15;
	constexpr int32 NoticeTitleSize = 46;
	constexpr int32 NoticeQuotaSize = 34;
	constexpr int32 NoticeLineSize = 20;
	constexpr int32 NoticeOutcomeSize = 22;
	constexpr int32 NoticeHintSize = 15;

	/** Size of the buttons (px at 1080p) */
	const FVector2D NoticeButtonSize(340.0f, 80.0f);
}

void STapaoDayEnd::Construct(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	const FLinearColor Cream = Skin ? Skin->Cream : FLinearColor::White;
	const FLinearColor Gold = Skin ? Skin->Gold : FLinearColor(1.0f, 0.77f, 0.31f);
	const FLinearColor Red(1.0f, 0.45f, 0.35f);
	const FLinearColor Dim(Cream.R, Cream.G, Cream.B, 0.75f);
	const FTapaoDayEndView& V = InArgs._View;
	static const FSlateRoundedBoxBrush Panel(FLinearColor(0.02f, 0.017f, 0.014f, 0.84f), 8.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.2f), 1.0f);

	// a text of the notice, centred, wrapped in its width
	auto Text = [&S](const FText& Value, ETapaoFont Role, int32 Size, int32 Outline, const FLinearColor& Color) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(NoticeTextWidth)
			[
				SNew(STextBlock)
				.Text(Value)
				.Font(S->Font(Role, Size, Outline))
				.ColorAndOpacity(Color)
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
			];
	};

	const bool bBadNews = V.Kind == ETapaoDayEndKind::ContractFailed || V.Kind == ETapaoDayEndKind::CampaignLost;
	const FLinearColor OutcomeColor = bBadNews ? Red : (V.Verdict == ERunDayVerdict::NextDay ? Cream : Gold);

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	// the Bureau's stamp over a contract notice
	if (!V.Header.IsEmpty())
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			Text(V.Header, ETapaoFont::UI, NoticeHeaderSize, 1, Dim)
		];
	}
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[
		Text(V.Title, ETapaoFont::Title, NoticeTitleSize, 2, V.bAlarm ? Red : Gold)
	];
	if (!V.Subtitle.IsEmpty())
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			Text(V.Subtitle, ETapaoFont::UI, NoticeLineSize, 1, Cream)
		];
	}
	// the quota of the contract, big (owner 2026-10-07: "affiche les quotas en fin de partie")
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		Text(V.Quota, ETapaoFont::Title, NoticeQuotaSize, 2, V.bQuotaMet ? Gold : Cream)
	];
	for (const FText& Line : V.Lines)
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			Text(Line, ETapaoFont::UI, NoticeLineSize, 1, Cream)
		];
	}
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 14.0f, 0.0f, 0.0f)
	[
		Text(V.Outcome, ETapaoFont::UI, NoticeOutcomeSize, 1, OutcomeColor)
	];

	if (V.bAsksGreed)
	{
		if (InArgs._bCanChoose)
		{
			const FOnTapaoGreedChoice OnChoice = InArgs._OnChoice;
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
				[
					MakeTapaoTextButton(S, LOCTEXT("Leave", "QUITTER LA RÉGION"), NoticeButtonSize,
						FSimpleDelegate::CreateLambda([OnChoice]() { OnChoice.ExecuteIfBound(false); }))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
				[
					MakeTapaoTextButton(S, LOCTEXT("Stay", "UN JOUR DE PLUS"), NoticeButtonSize,
						FSimpleDelegate::CreateLambda([OnChoice]() { OnChoice.ExecuteIfBound(true); }))
				]
			];
		}
		else
		{
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				Text(LOCTEXT("Captain", "Le capitaine (l'hôte) décide : partir ou rester…"), ETapaoFont::UI, NoticeLineSize, 1, Cream)
			];
		}
		// what one more day changes and the answer when nobody answers (DA_RunRules), for everybody
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			Text(V.GreedHint, ETapaoFont::UI, NoticeHintSize, 1, Dim)
		];
	}
	else if (V.bOffersNextDay)
	{
		if (InArgs._bCanChoose)
		{
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				MakeTapaoTextButton(S, LOCTEXT("NextDayButton", "JOUR SUIVANT"), NoticeButtonSize, InArgs._OnNextDay)
			];
		}
		else
		{
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				Text(LOCTEXT("CaptainNextDay", "Le capitaine (l'hôte) peut lancer le jour suivant."), ETapaoFont::UI, NoticeLineSize, 1, Cream)
			];
		}
	}

	// the time left to answer, or to the next day (live, read from the replicated server clock)
	const TAttribute<FText> Countdown = InArgs._Countdown;
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(Countdown)
		.Visibility_Lambda([Countdown]() { return Countdown.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		.Font(S->Font(ETapaoFont::UI, NoticeLineSize, 1))
		.ColorAndOpacity(Cream)
	];

	const TSharedRef<SWidget> Notice = SNew(SBorder).BorderImage(&Panel).Padding(FMargin(40.0f, 26.0f))
		[
			Box
		];
	if (InArgs._bCentered)
	{
		// a lost day: nothing to watch behind (no departure scene), the notice in the middle over a veil
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(EVisibility::HitTestInvisible)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Skin ? Skin->Veil : FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				Notice
			]
		];
		return;
	}
	// on the left: the middle of the screen shows the galleon sailing off (T-214)
	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.Padding(48.0f, 0.0f, 0.0f, 0.0f)
	[
		Notice
	];
}

#undef LOCTEXT_NAMESPACE
