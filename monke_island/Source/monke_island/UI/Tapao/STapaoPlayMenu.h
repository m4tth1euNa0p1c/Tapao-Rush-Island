// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FTapaoStyle;
class SEditableTextBox;

DECLARE_DELEGATE_OneParam(FOnTapaoJoin, const FString& /*Address*/);

/**
 *  How to play (ADR-007 §1, owner request 2026-10-06), shown by JOUER / CONTINUER / NOUVELLE PARTIE:
 *  SOLO (no server), CRÉER UNE PARTIE (listen server, up to 4 players who join by its address), REJOINDRE (address of
 *  the host, e.g. 192.168.1.20, port 7777 by default), RETOUR. A line tells the run that goes on.
 */
class MONKE_ISLAND_API STapaoPlayMenu : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoPlayMenu) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		/** The run that goes on, or "Nouvelle partie" */
		SLATE_ARGUMENT(FText, RunLine)
		/** Address proposed in the join box */
		SLATE_ARGUMENT(FString, DefaultAddress)
		SLATE_EVENT(FSimpleDelegate, OnSolo)
		SLATE_EVENT(FSimpleDelegate, OnHost)
		SLATE_EVENT(FOnTapaoJoin, OnJoin)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	void Join();

	TSharedPtr<SEditableTextBox> AddressBox;
	FOnTapaoJoin OnJoin;
	FSimpleDelegate OnBack;
};
