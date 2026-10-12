// monke_island

#include "UI/BossHealthBar.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** The bar at 1080p: long and thin, above the ammo line and the quick bar of the HUD */
	constexpr float BossBarWidth = 880.0f;
	constexpr float BossBarHeight = 9.0f;
	constexpr float BossBarBottom = 150.0f;
	constexpr int32 BossNameFontSize = 22;
	constexpr int32 BossBannerFontSize = 54;

	/** Marks of the later phases at most */
	constexpr int32 BossMaxMarks = 3;

	/** The pale chunk holds still this long after a blow (s), then slides down at this rate (share of the bar per s) */
	constexpr float BossTrailHoldSeconds = 0.8f;
	constexpr float BossTrailSlide = 0.35f;

	/** Fade of the bar (share per s); the banner: in, held, out (s) */
	constexpr float BossFadeRate = 2.5f;
	constexpr float BossBannerIn = 0.6f;
	constexpr float BossBannerHold = 2.8f;
	constexpr float BossBannerOut = 1.2f;

	const FLinearColor BossBloodRed(0.42f, 0.03f, 0.02f);
	const FLinearColor BossFrenzyRed(0.95f, 0.12f, 0.02f);
	const FLinearColor BossTrailGold(0.86f, 0.68f, 0.32f, 0.9f);
}

void SBossHealthBar::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	GameInstance = InArgs._GameInstance;
	// the bar ticks while hidden: it must notice a boss coming
	SetVisibility(EVisibility::HitTestInvisible);
	BarStyle = FProgressBarStyle().SetBackgroundImage(FSlateNoResource()).SetFillImage(FillBrush).SetMarqueeImage(FSlateNoResource());
	const UTapaoUISkin* Skin = Style.IsValid() ? Style->GetSkin() : nullptr;
	const FLinearColor Cream = Skin ? Skin->Cream : FLinearColor::White;
	const FLinearColor Gold = Skin ? Skin->Gold : FLinearColor(1.0f, 0.77f, 0.31f);
	const FSlateFontInfo NameFont = Style.IsValid() ? Style->Font(ETapaoFont::Title, BossNameFontSize, 1) : FCoreStyle::GetDefaultFontStyle("Regular", BossNameFontSize);
	const FSlateFontInfo BannerFont = Style.IsValid() ? Style->Font(ETapaoFont::Title, BossBannerFontSize, 2) : FCoreStyle::GetDefaultFontStyle("Bold", BossBannerFontSize);

	TSharedRef<SOverlay> Track = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(&BackgroundBrush)
		]
		+ SOverlay::Slot()
		[
			SNew(SProgressBar).Style(&BarStyle).Percent_Lambda([this]() { return TOptional<float>(Trail); }).FillColorAndOpacity(BossTrailGold)
		]
		+ SOverlay::Slot()
		[
			SNew(SProgressBar).Style(&BarStyle).Percent_Lambda([this]() { return TOptional<float>(Fill); })
			.FillColorAndOpacity_Lambda([this]() { return GetFillColor(); })
		];
	for (int32 Index = 0; Index < BossMaxMarks; ++Index)
	{
		Track->AddSlot().HAlign(HAlign_Left).Padding(TAttribute<FMargin>::CreateLambda([this, Index]() { return GetMarkPadding(Index); }))
		[
			SNew(SBox).WidthOverride(2.0f).Visibility_Lambda([this, Index]() { return GetMarkVisibility(Index); })
			[
				SNew(SImage).Image(&MarkBrush)
			]
		];
	}

	ChildSlot
	[
		SNew(SOverlay)
		// bottom centre: the name over the bar, Dark Souls style
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, BossBarBottom)
		[
			SNew(SBox)
			.WidthOverride(BossBarWidth)
			.Visibility_Lambda([this]() { return Alpha > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(&NoBrush)
				.Padding(0.0f)
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, Alpha); })
				[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return View.Name; })
					.Font(NameFont)
					.ColorAndOpacity(Cream)
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(BossBarHeight)
					[
						Track
					]
				]
				]
			]
		]
		// middle: the boss fell
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 0.0f, 140.0f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return BannerTime >= 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text(NSLOCTEXT("TapaoUI", "BossFelled", "GRAND ENNEMI ABATTU"))
			.Font(BannerFont)
			.ColorAndOpacity_Lambda([this, Gold]()
			{
				const float In = FMath::Clamp(BannerTime / BossBannerIn, 0.0f, 1.0f);
				const float Out = 1.0f - FMath::Clamp((BannerTime - BossBannerIn - BossBannerHold) / BossBannerOut, 0.0f, 1.0f);
				return FSlateColor(FLinearColor(Gold.R, Gold.G, Gold.B, In * Out));
			})
			.ShadowOffset(FVector2D(2.0f, 2.0f))
			.ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * FMath::Clamp(BannerTime / BossBannerIn, 0.0f, 1.0f)); })
		]
	];
}

const APawn* SBossHealthBar::GetPawn() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const APlayerController* Controller = Instance ? Instance->GetFirstLocalPlayerController() : nullptr;
	return Controller ? Controller->GetPawn() : nullptr;
}

void SBossHealthBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const APawn* Pawn = GetPawn();
	const UBossBarSubsystem* Bars = Pawn ? UBossBarSubsystem::Get(Pawn) : nullptr;
	FBossBarView Next;
	const bool bNowEngaged = Bars && Bars->FindBarFor(Pawn, Next);
	if (bNowEngaged)
	{
		// a new fight starts full; the boss that falls under the player's eyes gets its banner
		if (!bEngaged && Alpha <= 0.01f)
		{
			Fill = Next.HealthFraction;
			Trail = Next.HealthFraction;
		}
		if (Next.bDead && !View.bDead && bEngaged)
		{
			BannerTime = 0.0f;
		}
		View = MoveTemp(Next);
	}
	bEngaged = bNowEngaged;

	// the fill follows the health at once, the pale chunk holds a moment then slides down to it
	const float Health = FMath::Clamp(View.HealthFraction, 0.0f, 1.0f);
	if (Health < Fill - UE_KINDA_SMALL_NUMBER)
	{
		TrailHold = BossTrailHoldSeconds;
	}
	Fill = Health;
	if (Trail < Fill)
	{
		Trail = Fill;
	}
	else if (TrailHold > 0.0f)
	{
		TrailHold -= InDeltaTime;
	}
	else
	{
		Trail = FMath::Max(Fill, Trail - BossTrailSlide * InDeltaTime);
	}
	Alpha = FMath::FInterpConstantTo(Alpha, bEngaged ? 1.0f : 0.0f, InDeltaTime, BossFadeRate);
	Pulse += InDeltaTime;
	if (BannerTime >= 0.0f)
	{
		BannerTime += InDeltaTime;
		if (BannerTime > BossBannerIn + BossBannerHold + BossBannerOut)
		{
			BannerTime = -1.0f;
		}
	}
}

FSlateColor SBossHealthBar::GetFillColor() const
{
	if (!View.bFrenzy)
	{
		return FSlateColor(BossBloodRed);
	}
	// the frenzy burns: a slow pulse between blood and fire
	const float Beat = 0.5f + 0.5f * FMath::Sin(Pulse * 6.0f);
	return FSlateColor(FMath::Lerp(BossBloodRed, BossFrenzyRed, 0.4f + 0.6f * Beat));
}

FMargin SBossHealthBar::GetMarkPadding(int32 Index) const
{
	const float Mark = View.PhaseMarks.IsValidIndex(Index) ? FMath::Clamp(View.PhaseMarks[Index], 0.0f, 1.0f) : 0.0f;
	return FMargin(BossBarWidth * Mark - 1.0f, 0.0f, 0.0f, 0.0f);
}

EVisibility SBossHealthBar::GetMarkVisibility(int32 Index) const
{
	return View.PhaseMarks.IsValidIndex(Index) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}
