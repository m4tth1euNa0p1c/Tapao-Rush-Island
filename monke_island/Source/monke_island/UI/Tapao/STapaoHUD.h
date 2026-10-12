// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class FTapaoStyle;
class FTapaoInventoryModel;
struct FTapaoInventoryBrushes;
class SHorizontalBox;
class APawn;
class UGameInstance;
class UIslanderHealthComponent;
class UTrumpetEmoteComponent;

/**
 *  In game HUD of TAPAO (maquette 02_hud_jour, owner request 2026-10-04): top left the health, the stamina and the
 *  fuel of the torch (only while it is in hand); top right the value of the cargo aboard the galleon and the musket
 *  balls; bottom centre the quick bar 1-4 (T-166: the sober cells of the inventory screen, item in hand outlined,
 *  refreshed by the inventory's change event); top centre the countdowns and, under them, the line of the departure
 *  (vote, notice, wheel; T-286) and of the wait for the crew (T-292); bottom left, right of the gauges, the spell of the
 *  player as in League of Legends (the trumpet call on A, T-333, STapaoSpellSlot). Reads the local pawn and the
 *  replicated state of the expedition, changes nothing. The flood timer keeps its own widget (UFloodTimerSubsystem),
 *  top centre.
 */
class MONKE_ISLAND_API STapaoHUD : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STapaoHUD) {}
		SLATE_ARGUMENT(TSharedPtr<FTapaoStyle>, Style)
		SLATE_ARGUMENT(TWeakObjectPtr<UGameInstance>, GameInstance)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** A gauge of the kit: its fill under its frame, the value on the right */
	TSharedRef<SWidget> MakeBar(FName Frame, FName Fill, FProgressBarStyle& BarStyle, TFunction<float()> Fraction, TFunction<FText()> Label, TAttribute<EVisibility> Visibility);

	/** A counter plate with its icon */
	TSharedRef<SWidget> MakeCounter(FName Icon, TFunction<FText()> Value, TAttribute<EVisibility> Visibility);

	/** Builds the quick bar cells again (the number of bar cells of the followed inventory changed) */
	void RebuildBar();

	/** Pawn of the local player, or nullptr */
	APawn* GetPawn() const;

	/** Balls loaded / in the pouch of the firearm in hand, and its reload time left; empty without one */
	FText GetAmmoText() const;

	/** "Jour 3 / 10": the day of the campaign (days played + 1) over the days of its stages; empty without a run */
	FText GetDayText() const;

	/** Value brought back against the quota of the stage; empty without a run */
	FText GetQuotaText() const;

	/** The deadline of the contract in days of the campaign, and the greed day ("Contrat : jour 3 · jour de cupidité",
	 *  T-283); empty without a run */
	FText GetContractText() const;

	/** Middle of the screen: "Départ dans N" during the notice of a departure and once the day is survived, "Jour N" at
	 *  the arrival (T-214); empty else */
	FText GetCenterText() const;

	/** Line under the centre (T-286, T-292): the wait for the crew, the vote of the departure and its key, the notice,
	 *  the wheel and its key, why the last vote failed; empty when there is nothing to tell */
	FText GetDepartureText() const;

	/** A scene of the day plays (UDayTransitionSubsystem): the HUD of the player steps aside */
	bool IsSceneUnderWay() const;

	/** Opacity of the centre text */
	float GetCenterAlpha() const;

	/** The trumpet call of the local pawn while it lives and has it (T-333): the spell slot shows it; nullptr otherwise */
	const UTrumpetEmoteComponent* GetTrumpetCall() const;

	/** Slow values (cargo aboard), refreshed twice a second */
	EActiveTimerReturnType Refresh(double CurrentTime, float DeltaTime);

	TSharedPtr<FTapaoStyle> Style;
	TWeakObjectPtr<UGameInstance> GameInstance;
	FProgressBarStyle HealthStyle;
	FProgressBarStyle StaminaStyle;
	FProgressBarStyle FuelStyle;
	TSharedPtr<FTapaoInventoryModel> InventoryModel;
	TSharedPtr<FTapaoInventoryBrushes> InventoryBrushes;
	TSharedPtr<SHorizontalBox> BarBox;
	int32 BuiltBarNum = -1;
	int32 CargoValue = 0;
	bool bHasGalleon = false;

	/** Trumpet and health of the local pawn, found again twice a second (respawn, possession) */
	TWeakObjectPtr<const UTrumpetEmoteComponent> TrumpetCall;
	TWeakObjectPtr<const UIslanderHealthComponent> PawnHealth;

	/** Days of the campaign (sum of the days of the stages of DA_RunRules), read once */
	mutable int32 CampaignDays = -1;
};
