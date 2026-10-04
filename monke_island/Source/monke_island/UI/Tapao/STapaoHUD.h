// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class FTapaoStyle;
class APawn;
class UGameInstance;

/**
 *  In game HUD of TAPAO (maquette 02_hud_jour, owner request 2026-10-04): top left the health, the stamina and the
 *  fuel of the torch (only while it is in hand); top right the value of the cargo aboard the galleon and the musket
 *  balls; bottom centre the item bar 1-4 with the selected slot. Reads the local pawn every frame, changes nothing.
 *  The flood timer keeps its own widget (UFloodTimerSubsystem), top centre.
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

	/** One slot of the item bar */
	TSharedRef<SWidget> MakeSlot(int32 Slot);

	/** Pawn of the local player, or nullptr */
	APawn* GetPawn() const;

	/** Slow values (cargo aboard), refreshed twice a second */
	EActiveTimerReturnType Refresh(double CurrentTime, float DeltaTime);

	TSharedPtr<FTapaoStyle> Style;
	TWeakObjectPtr<UGameInstance> GameInstance;
	FProgressBarStyle HealthStyle;
	FProgressBarStyle StaminaStyle;
	FProgressBarStyle FuelStyle;
	int32 CargoValue = 0;
	bool bHasGalleon = false;
};
