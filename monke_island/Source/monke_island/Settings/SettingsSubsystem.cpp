// monke_island

#include "Settings/SettingsSubsystem.h"
#include "Settings/SettingsMath.h"
#include "Settings/SettingsRules.h"
#include "Settings/SSettingsMenu.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "monke_island.h"
#include "AudioDevice.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Slate/SGameLayerManager.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Widgets/SViewport.h"

/** Opens / closes the settings menu of one game instance */
class FSettingsInputProcessor : public IInputProcessor
{
public:

	explicit FSettingsInputProcessor(USettingsSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		USettingsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || InKeyEvent.IsRepeat())
		{
			return false;
		}
		const FKey Key = InKeyEvent.GetKey();
		// Escape opens the TAPAO pause (UTapaoUISubsystem, its OPTIONS button opens this menu): here it only closes
		const bool bToggleKey = Key == EKeys::F10 || (Key == EKeys::Escape && Subsystem->IsMenuOpen());
		if (!bToggleKey || (!Subsystem->IsMenuOpen() && !Subsystem->HasViewportFocus()))
		{
			return false;
		}
		Subsystem->ToggleMenu();
		return true;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("MonkeSettingsMenu"); }

private:

	TWeakObjectPtr<USettingsSubsystem> Owner;
};

const TCHAR* USettingsSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Settings/DA_SettingsRules.DA_SettingsRules");

bool USettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void USettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Rules = LoadObject<USettingsRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Settings: %s not found, engine defaults used"), DefaultRulesPath);
		Rules = NewObject<USettingsRules>(this);
	}
	Load();

	WorldActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &USettingsSubsystem::HandleWorldActorsInitialized);
	GetGameInstance()->GetOnPawnControllerChanged().AddDynamic(this, &USettingsSubsystem::HandlePawnControllerChanged);

	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FSettingsInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void USettingsSubsystem::Deinitialize()
{
	CloseMenu();
	OnVoiceSettingsChanged.Clear();
	OnMicTestChanged.Clear();
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsHandle);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetOnPawnControllerChanged().RemoveDynamic(this, &USettingsSubsystem::HandlePawnControllerChanged);
	}
	Super::Deinitialize();
}

// ---------------------------------------------------------------- values

FMonkeSettingsValues USettingsSubsystem::GetDefaults() const
{
	FMonkeSettingsValues Values = SettingsMath::Defaults(*Rules);
	// the display part keeps what the machine runs now
	Values.WindowMode = Applied.WindowMode;
	Values.Resolution = Applied.Resolution;
	Values.bVSync = Applied.bVSync;
	Values.Quality = Applied.Quality;
	return Values;
}

void USettingsSubsystem::Load()
{
	const UMonkeUserSettings* Saved = GetDefault<UMonkeUserSettings>();
	Applied = SettingsMath::Defaults(*Rules);
	if (Saved->bSaved)
	{
		for (int32 Index = 0; Index < Saved->Volumes.Num() && Index < static_cast<int32>(ESettingsVolume::Count); ++Index)
		{
			Applied.Volumes[Index] = Saved->Volumes[Index];
		}
		Applied.Sensitivity = Saved->Sensitivity;
		Applied.bInvertY = Saved->bInvertY;
		Applied.FieldOfView = Saved->FieldOfView;
		// an ini saved before the voice chat holds the class defaults here (push-to-talk), Sanitize keeps them in range
		Applied.MicMode = Saved->MicMode;
		Applied.MicThreshold = Saved->MicThreshold;
	}

	if (const UGameUserSettings* Display = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Applied.WindowMode = static_cast<int32>(Display->GetFullscreenMode());
		Applied.Resolution = Display->GetScreenResolution();
		Applied.bVSync = Display->IsVSyncEnabled();
		Applied.Quality = Display->GetOverallScalabilityLevel();
		// first launch: the frame rate of the cahier (60), afterwards what the player chose
		if (Saved->bSaved)
		{
			Applied.FrameRateLimit = FMath::RoundToInt(Display->GetFrameRateLimit());
		}
	}
	SettingsMath::Sanitize(*Rules, Applied);
	if (!Saved->bSaved && !GIsEditor)
	{
		ApplyDisplay(Applied);
	}
}

void USettingsSubsystem::Save() const
{
	UMonkeUserSettings* Saved = GetMutableDefault<UMonkeUserSettings>();
	Saved->bSaved = true;
	Saved->Volumes = TArray<float>(Applied.Volumes, static_cast<int32>(ESettingsVolume::Count));
	Saved->Sensitivity = Applied.Sensitivity;
	Saved->bInvertY = Applied.bInvertY;
	Saved->FieldOfView = Applied.FieldOfView;
	Saved->MicMode = Applied.MicMode;
	Saved->MicThreshold = Applied.MicThreshold;
	Saved->SaveConfig();
}

void USettingsSubsystem::PreviewAudio(const FMonkeSettingsValues& Values)
{
	ApplyAudio(Values);
}

void USettingsSubsystem::Apply(const FMonkeSettingsValues& Values)
{
	const bool bDisplay = Values.DisplayDiffers(Applied);
	Applied = Values;
	SettingsMath::Sanitize(*Rules, Applied);
	ApplyAudio(Applied);
	if (bDisplay)
	{
		ApplyDisplay(Applied);
	}
	ApplyPlayers();
	Save();
	OnVoiceSettingsChanged.Broadcast(Applied, false);
	UE_LOG(Logmonke_island, Log, TEXT("Settings applied: master %.0f %%, ambience %.0f %%, effects %.0f %%, voices %.0f %%, fov %.0f, %d fps, mic mode %d (threshold %.2f)"),
		Applied.Volume(ESettingsVolume::Master) * 100.0f, Applied.Volume(ESettingsVolume::Ambience) * 100.0f,
		Applied.Volume(ESettingsVolume::Effects) * 100.0f, Applied.Volume(ESettingsVolume::VoiceChat) * 100.0f,
		Applied.FieldOfView, Applied.FrameRateLimit, static_cast<int32>(Applied.MicMode), Applied.MicThreshold);
}

void USettingsSubsystem::Cancel()
{
	ApplyAudio(Applied);
	OnVoiceSettingsChanged.Broadcast(Applied, false);
}

void USettingsSubsystem::PreviewVoice(const FMonkeSettingsValues& Values)
{
	FMonkeSettingsValues Preview = Values;
	SettingsMath::Sanitize(*Rules, Preview);
	OnVoiceSettingsChanged.Broadcast(Preview, true);
}

void USettingsSubsystem::SetMicTest(bool bTesting)
{
	if (bMicTest == bTesting)
	{
		return;
	}
	bMicTest = bTesting;
	UE_LOG(Logmonke_island, Log, TEXT("Settings: microphone test %s"), bMicTest ? TEXT("started") : TEXT("stopped"));
	OnMicTestChanged.Broadcast(bMicTest);
}

// ---------------------------------------------------------------- application

void USettingsSubsystem::ApplyAudio(const FMonkeSettingsValues& Values)
{
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World)
	{
		return;
	}
	if (FAudioDeviceHandle Device = World->GetAudioDevice())
	{
		Device->SetTransientPrimaryVolume(Values.Volume(ESettingsVolume::Master));
	}
	if (!Rules->VolumeMix)
	{
		return;
	}
	for (const TPair<ESettingsVolume, TObjectPtr<USoundClass>>& Pair : Rules->SoundClasses)
	{
		if (Pair.Key != ESettingsVolume::Master && Pair.Value)
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Rules->VolumeMix, Pair.Value, Values.Volume(Pair.Key), 1.0f, 0.0f, true);
		}
	}
	if (!MixedWorlds.Contains(World))
	{
		MixedWorlds.Add(World);
		UGameplayStatics::PushSoundMixModifier(World, Rules->VolumeMix);
	}
}

void USettingsSubsystem::ApplyDisplay(const FMonkeSettingsValues& Values) const
{
	UGameUserSettings* Display = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Display)
	{
		return;
	}
	Display->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(Values.WindowMode));
	Display->SetScreenResolution(Values.Resolution);
	Display->SetVSyncEnabled(Values.bVSync);
	Display->SetFrameRateLimit(static_cast<float>(Values.FrameRateLimit));
	if (Values.Quality >= 0)
	{
		Display->SetOverallScalabilityLevel(Values.Quality);
	}
	// applies and saves GameUserSettings.ini
	Display->ApplySettings(false);
}

void USettingsSubsystem::ApplyPlayers() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
	{
		if (APlayerController* Controller = Player ? Player->GetPlayerController(GameInstance->GetWorld()) : nullptr)
		{
			ApplyToController(Controller);
		}
	}
}

void USettingsSubsystem::ApplyToController(AController* Controller) const
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	// mouse: the legacy look scales of the controller (bEnableLegacyInputScales), from its class defaults
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const APlayerController* Defaults = PlayerController->GetClass()->GetDefaultObject<APlayerController>();
	const FVector2D Base(Defaults->GetDeprecatedInputYawScale(), Defaults->GetDeprecatedInputPitchScale());
	const FVector2D Scales = SettingsMath::LookScales(Base, Applied.Sensitivity, Applied.bInvertY);
	PlayerController->SetDeprecatedInputYawScale(Scales.X);
	PlayerController->SetDeprecatedInputPitchScale(Scales.Y);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// field of view of the first person camera (the musket narrows it from there when shouldering)
	if (const APawn* Pawn = PlayerController->GetPawn())
	{
		TArray<UCameraComponent*> Cameras;
		Pawn->GetComponents(Cameras);
		for (UCameraComponent* Camera : Cameras)
		{
			if (Camera->ComponentHasTag(TEXT("FirstPerson")))
			{
				Camera->SetFieldOfView(Applied.FieldOfView);
			}
		}
	}
}

void USettingsSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	if (Params.World && Params.World->GetGameInstance() == GetGameInstance())
	{
		MixedWorlds.Remove(nullptr);
		ApplyAudio(Applied);
	}
}

void USettingsSubsystem::HandlePawnControllerChanged(APawn* Pawn, AController* Controller)
{
	ApplyToController(Controller);
}

// ---------------------------------------------------------------- menu

bool USettingsSubsystem::HasViewportFocus() const
{
	const UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	return Widget.IsValid() && Widget->HasAnyUserFocusOrFocusedDescendants();
}

void USettingsSubsystem::ToggleMenu()
{
	if (IsMenuOpen())
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void USettingsSubsystem::OpenMenu()
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController();
	if (!Viewport || Menu.IsValid())
	{
		return;
	}
	Menu = SNew(SSettingsMenu).Owner(this);
	Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 1000);
	if (Controller)
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Menu);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
	}
	FSlateApplication::Get().SetKeyboardFocus(Menu);
}

void USettingsSubsystem::CloseMenu()
{
	if (!Menu.IsValid())
	{
		return;
	}
	// the microphone test belongs to the menu (cahier §18): it ends with it
	SetMicTest(false);
	Cancel();
	if (UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	if (APlayerController* Controller = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
	// opened from the TAPAO main menu or pause: that screen takes the input back
	if (UTapaoUISubsystem* Tapao = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTapaoUISubsystem>() : nullptr)
	{
		Tapao->RefocusScreen();
	}
}
