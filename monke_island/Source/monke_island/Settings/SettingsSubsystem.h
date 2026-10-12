// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Settings/MonkeUserSettings.h"
#include "SettingsSubsystem.generated.h"

class APawn;
class AController;
class SSettingsMenu;
class USettingsRules;
class FSettingsInputProcessor;

/** Microphone values changed (T-263): bPreview while the menu edits them, false once applied or cancelled */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMonkeVoiceSettingsChanged, const FMonkeSettingsValues& /*Values*/, bool /*bPreview*/);

/** The microphone test started or stopped (T-263) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMonkeMicTestChanged, bool /*bTesting*/);

/**
 *  Player settings (cahier §18), one per game instance, never on a dedicated server: loads the saved values,
 *  applies them (volumes through DA_SettingsRules' sound mix, display through UGameUserSettings, mouse and field of
 *  view on the local pawn) and opens the settings menu (Escape in a packaged game, F10 everywhere, F10 only in
 *  the editor where Escape stops the play session). The menu edits a pending copy: audio is previewed live, Apply
 *  saves, Cancel restores what was applied. The microphone values (T-263) are only kept and broadcast here: the voice
 *  chat (Voice/) applies them.
 */
UCLASS()
class MONKE_ISLAND_API USettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	/** Opens the menu if closed, closes it (cancelling unapplied changes) if open */
	void ToggleMenu();

	/** Whether the menu is open */
	bool IsMenuOpen() const { return Menu.IsValid(); }

	/** Closes the menu, cancelling unapplied changes */
	void CloseMenu();

	/** Values in force */
	const FMonkeSettingsValues& GetApplied() const { return Applied; }

	/** Rules (bounds, choices), may be null when DA_SettingsRules is missing */
	const USettingsRules* GetRules() const { return Rules; }

	/** Plays the volumes of a pending copy without saving them */
	void PreviewAudio(const FMonkeSettingsValues& Values);

	/** Applies and saves a copy */
	void Apply(const FMonkeSettingsValues& Values);

	/** Drops the preview: back to the applied values */
	void Cancel();

	/** Values a first launch would use */
	FMonkeSettingsValues GetDefaults() const;

	/** Whether this game instance's viewport has the keyboard (several PIE windows each own a subsystem) */
	bool HasViewportFocus() const;

	/** Previews the microphone threshold of a pending copy (heard during the microphone test) without saving it */
	void PreviewVoice(const FMonkeSettingsValues& Values);

	/** Starts or stops the microphone test (cahier §18: the player hears their own voice, nobody else does); closing
	 *  the menu stops it */
	void SetMicTest(bool bTesting);

	/** Whether the microphone test runs */
	bool IsMicTestOn() const { return bMicTest; }

	/** Microphone values applied, cancelled or previewed: the voice chat (Voice/) listens, Settings knows nothing of it */
	FOnMonkeVoiceSettingsChanged OnVoiceSettingsChanged;

	/** The microphone test started or stopped: the voice chat loops the voice back without sending it */
	FOnMonkeMicTestChanged OnMicTestChanged;

private:

	/** Reads the saved values and the engine display settings */
	void Load();

	/** Writes the values the engine does not keep */
	void Save() const;

	/** Volumes on the audio device of this game instance */
	void ApplyAudio(const FMonkeSettingsValues& Values);

	/** Window, resolution, vsync, frame rate, quality */
	void ApplyDisplay(const FMonkeSettingsValues& Values) const;

	/** Look scales and field of view of the local players */
	void ApplyPlayers() const;

	/** Look scales and field of view of one controller's pawn */
	void ApplyToController(AController* Controller) const;

	/** A world of this game instance finished spawning its actors: the mix goes on its audio device */
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	/** A pawn got a controller: mouse and field of view follow the local player */
	UFUNCTION()
	void HandlePawnControllerChanged(APawn* Pawn, AController* Controller);

	/** Opens the menu over the game */
	void OpenMenu();

	/** Rules (DA_SettingsRules) */
	UPROPERTY(Transient)
	TObjectPtr<USettingsRules> Rules;

	/** Values in force */
	FMonkeSettingsValues Applied;

	/** Open menu */
	TSharedPtr<SSettingsMenu> Menu;

	/** Key listener */
	TSharedPtr<FSettingsInputProcessor> InputProcessor;

	/** Worlds the mix was pushed on */
	TSet<TWeakObjectPtr<UWorld>> MixedWorlds;

	/** Handle of the world delegate */
	FDelegateHandle WorldActorsHandle;

	/** The microphone test runs */
	bool bMicTest = false;
};
