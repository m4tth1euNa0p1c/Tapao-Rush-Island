// monke_island

#pragma once

#include "CoreMinimal.h"
#include "UI/Tapao/TapaoDayEndText.h"
#include "Widgets/SCompoundWidget.h"

class FTapaoStyle;

DECLARE_DELEGATE_OneParam(FOnTapaoGreedChoice, bool /*bStay*/);

/**
 *  End of a day (ADR-007 §7, §9, §14, §16; T-173): "JOUR N TERMINÉ" or "JOUR N PERDU", and the Bureau's notices of a
 *  contract, "CONTRAT TERMINÉ", "CAMPAGNE HONORÉE", "CONTRAT ÉCHOUÉ": the value brought back, the dead and their fees,
 *  the quota, the excess paid or the crowns missing, the treasury, then what follows. When the quota is met with days
 *  left, the host chooses QUITTER LA RÉGION or UN JOUR DE PLUS, the others read that the captain decides; after a lost
 *  day the host may send the galleon on (JOUR SUIVANT). The texts come from TapaoDayEndText (pure, tested); a day
 *  survived shows on the left (the departure scene in the middle), a lost day in the middle over a veil.
 */
class MONKE_ISLAND_API STapaoDayEnd : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoDayEnd) : _bCanChoose(false), _bCentered(false) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		/** The texts of the notice and what it offers (TapaoDayEndText::Build) */
		SLATE_ARGUMENT(FTapaoDayEndView, View)
		/** This machine answers the greed question and sends the galleon on (host or solo) */
		SLATE_ARGUMENT(bool, bCanChoose)
		/** In the middle of the screen over a veil (a lost day: no departure scene to watch), else on the left */
		SLATE_ARGUMENT(bool, bCentered)
		/** Live line under the notice: the time left to answer, or to the next day; hidden when empty */
		SLATE_ATTRIBUTE(FText, Countdown)
		SLATE_EVENT(FOnTapaoGreedChoice, OnChoice)
		/** The host sends the galleon to the next island now (a lost day) */
		SLATE_EVENT(FSimpleDelegate, OnNextDay)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
};
