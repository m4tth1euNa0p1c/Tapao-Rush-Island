// monke_island

#include "UI/Tapao/STapaoPlayMenu.h"
#include "UI/Tapao/STapaoScreens.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TapaoPlayMenu"

void STapaoPlayMenu::Construct(const FArguments& InArgs)
{
	const TSharedRef<FTapaoStyle> S = InArgs._Style.ToSharedRef();
	const UTapaoUISkin* Skin = S->GetSkin();
	const FLinearColor Cream = Skin ? Skin->Cream : FLinearColor::White;
	const FLinearColor Gold = Skin ? Skin->Gold : FLinearColor(1.0f, 0.77f, 0.31f);
	const FVector2D Wide(400.0f, 84.0f);
	OnJoin = InArgs._OnJoin;
	OnBack = InArgs._OnBack;

	static const FSlateRoundedBoxBrush Panel(FLinearColor(0.02f, 0.017f, 0.014f, 0.88f), 8.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.2f), 1.0f);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Skin ? Skin->Veil : FLinearColor(0, 0, 0, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(&Panel).Padding(FMargin(36.0f, 26.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Title", "JOUER")).Font(S->Font(ETapaoFont::Title, 46, 2)).ColorAndOpacity(Gold)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 16.0f)
				[
					SNew(STextBlock).Text(InArgs._RunLine).Font(S->Font(ETapaoFont::UI, 18, 1)).ColorAndOpacity(Cream)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
				[
					MakeTapaoTextButton(S, LOCTEXT("Solo", "SOLO"), Wide, InArgs._OnSolo)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
				[
					MakeTapaoTextButton(S, LOCTEXT("Host", "CRÉER UNE PARTIE"), Wide, InArgs._OnHost)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("HostHint", "Multijoueur : jusqu'à 4 joueurs, qui rejoignent ton adresse IP"))
					.Font(S->Font(ETapaoFont::UI, 15, 1)).ColorAndOpacity(FLinearColor(Cream.R, Cream.G, Cream.B, 0.7f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 18.0f, 0.0f, 4.0f)
				[
					SNew(SBox).WidthOverride(400.0f)
					[
						SAssignNew(AddressBox, SEditableTextBox)
						.Text(FText::FromString(InArgs._DefaultAddress))
						.HintText(LOCTEXT("AddressHint", "Adresse de l'hôte (ex. 192.168.1.20)"))
						.Font(S->Font(ETapaoFont::UI, 20, 0))
						.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit)
						{
							if (Commit == ETextCommit::OnEnter)
							{
								Join();
							}
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
				[
					MakeTapaoTextButton(S, LOCTEXT("Join", "REJOINDRE"), Wide, FSimpleDelegate::CreateSP(this, &STapaoPlayMenu::Join))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 18.0f, 0.0f, 0.0f)
				[
					MakeTapaoTextButton(S, LOCTEXT("Back", "RETOUR"), FVector2D(260.0f, 70.0f), InArgs._OnBack)
				]
			]
		]
	];
}

void STapaoPlayMenu::Join()
{
	const FString Address = AddressBox.IsValid() ? AddressBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!Address.IsEmpty())
	{
		OnJoin.ExecuteIfBound(Address);
	}
}

FReply STapaoPlayMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnBack.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
