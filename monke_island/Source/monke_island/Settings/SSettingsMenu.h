// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Settings/MonkeUserSettings.h"
#include "Widgets/SCompoundWidget.h"

class USettingsSubsystem;
class SWidgetSwitcher;

/**
 *  Settings menu (cahier §18), drawn over the game by USettingsSubsystem: tabs Audio, Display, Mouse, Comfort and
 *  Voice (T-263: microphone mode, voice activation threshold, the voices' volume, microphone test), footer Restore this
 *  tab / Cancel / Apply / Close. Edits a pending copy of the values; audio is heard live.
 */
class MONKE_ISLAND_API SSettingsMenu : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSettingsMenu) {}
		/** Subsystem that owns the values */
		SLATE_ARGUMENT(TWeakObjectPtr<USettingsSubsystem>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }

private:

	/** Tabs of the menu */
	enum class ETab : int32 { Audio, Display, Mouse, Comfort, Voice, Count };

	TSharedRef<SWidget> MakeTabButton(ETab Tab, const FText& Label);
	TSharedRef<SWidget> MakeAudioTab();
	TSharedRef<SWidget> MakeDisplayTab();
	TSharedRef<SWidget> MakeMouseTab();
	TSharedRef<SWidget> MakeComfortTab();
	TSharedRef<SWidget> MakeVoiceTab();

	/** Starts or stops the microphone test */
	FReply OnToggleMicTest();

	/** A labelled row */
	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Control);

	/** A slider with its value shown on the right */
	TSharedRef<SWidget> MakeSlider(TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, TFunction<FText(float)> Format);

	/** A choice between labelled values (combo box) */
	TSharedRef<SWidget> MakeChoice(TArray<TPair<FText, int32>> Options, TFunction<int32()> Get, TFunction<void(int32)> Set);

	/** A check box */
	TSharedRef<SWidget> MakeCheck(TFunction<bool()> Get, TFunction<void(bool)> Set);

	/** Puts the current tab back to its defaults (pending only) */
	FReply OnRestoreTab();
	FReply OnCancel();
	FReply OnApply();
	FReply OnClose();

	/** Whether pending differs from applied */
	bool HasChanges() const;

	/** Subsystem */
	TWeakObjectPtr<USettingsSubsystem> Owner;

	/** Values being edited */
	FMonkeSettingsValues Pending;

	/** Tab shown */
	ETab CurrentTab = ETab::Audio;

	/** Tab pages */
	TSharedPtr<SWidgetSwitcher> Pages;
};
