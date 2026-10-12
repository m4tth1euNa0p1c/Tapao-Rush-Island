// monke_island

#include "UI/Tapao/STapaoDeath.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TapaoUI"

namespace
{
	/** Font sizes (Slate points at 1080p, 1 pt = 1.33 px): every line 20 px or more */
	constexpr int32 TitleSize = 72;
	constexpr int32 CauseSize = 30;
	constexpr int32 HintSize = 18;
	constexpr int32 BarNameSize = 22;
	constexpr int32 BarLineSize = 16;
	constexpr int32 BarNoteSize = 15;

	/** Over the quick bar, the ammo line and the boss bar of the HUD (they end about 195 px over the bottom at 1080p) */
	constexpr float BarBottom = 205.0f;
}

void STapaoDeath::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	const UTapaoUISkin* Skin = Style->GetSkin();
	const FLinearColor Cream = Skin ? Skin->Cream : FLinearColor::White;
	const FLinearColor Gold = Skin ? Skin->Gold : FLinearColor(1.0f, 0.77f, 0.31f);
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.85f);
	const FSlateBrush* Plain = FCoreStyle::Get().GetBrush("WhiteBrush");
	SetVisibility(EVisibility::HitTestInvisible);

	auto When = [this](EDeathScreen Screen)
	{
		return TAttribute<EVisibility>::CreateLambda([this, Screen]()
		{
			return View.Screen == Screen ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		});
	};
	const TAttribute<EVisibility> WithTeammates = TAttribute<EVisibility>::CreateLambda([this]()
	{
		return View.Observables.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
	});
	auto Line = [this, Cream, Shadow](TAttribute<FText> Text, ETapaoFont Role, int32 Size, float Opacity = 1.0f) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Style->Font(Role, Size, 1))
			.ColorAndOpacity(FLinearColor(Cream.R, Cream.G, Cream.B, Opacity))
			.ShadowOffset(FVector2D(2.0f, 2.0f))
			.ShadowColorAndOpacity(Shadow)
			.Justification(ETextJustify::Center);
	};

	ChildSlot
	[
		SNew(SOverlay)
		// the death: a light veil (the island stays in view), the title, the cause, what the clicks do
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(When(EDeathScreen::Dead))
			.BorderImage(Plain)
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.4f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DeathTitle", "TU ES MORT"))
					.Font(Style->Font(ETapaoFont::Title, TitleSize, 2))
					.ColorAndOpacity(Cream)
					.ShadowOffset(FVector2D(3.0f, 3.0f))
					.ShadowColorAndOpacity(Shadow)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					Line(TAttribute<FText>::CreateLambda([this]() { return View.Cause; }), ETapaoFont::Text, CauseSize)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 36.0f, 0.0f, 0.0f)
				[
					SNew(SBox).Visibility(WithTeammates)
					[
						Line(LOCTEXT("DeathFollowHint", "Clic gauche ou clic droit : suivre un équipier"), ETapaoFont::UI, HintSize)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SBox).Visibility(WithTeammates)
					[
						Line(TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("DeathAlive", "En vie : {0}"), ObservablesText); }),
							ETapaoFont::Text, HintSize, 0.9f)
					]
				]
			]
		]
		// the spectator: a bar at the bottom, over the quick bar; the name is written, not only coloured
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, BarBottom)
		[
			SNew(SBorder)
			.Visibility(When(EDeathScreen::Following))
			.BorderImage(Plain)
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f))
			.Padding(FMargin(30.0f, 14.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Line(LOCTEXT("SpectatorFollows", "Tu suis "), ETapaoFont::UI, BarNameSize)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return View.Followed; })
						.Font(Style->Font(ETapaoFont::UI, BarNameSize, 1))
						.ColorAndOpacity(Gold)
						.ShadowOffset(FVector2D(2.0f, 2.0f))
						.ShadowColorAndOpacity(Shadow)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					Line(LOCTEXT("SpectatorKeys", "Clic gauche : équipier précédent   ·   Clic droit : suivant"), ETapaoFont::UI, BarLineSize, 0.95f)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					Line(TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("SpectatorCause", "Tu es mort : {0}"), View.Cause); }),
						ETapaoFont::Text, BarLineSize, 0.9f)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					Line(TAttribute<FText>::CreateLambda([this]()
					{
						return FText::Format(LOCTEXT("SpectatorAliveReturn", "En vie : {0}   ·   retour à bord au prochain jour"), ObservablesText);
					}), ETapaoFont::Text, BarNoteSize, 0.85f)
				]
			]
		]
	];
}

void STapaoDeath::SetView(const FTapaoDeathView& InView)
{
	View = InView;
	ObservablesText = FText::Join(FText::FromString(TEXT("  ·  ")), View.Observables);
}

#undef LOCTEXT_NAMESPACE
