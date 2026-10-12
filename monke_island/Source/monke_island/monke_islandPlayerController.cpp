// Copyright Epic Games, Inc. All Rights Reserved.


#include "monke_islandPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InputMappingContext.h"
#include "monke_islandCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Death/DeathSpectatorComponent.h"
#include "GameFramework/Pawn.h"
#include "monke_island.h"
#include "Voice/VoiceChatComponent.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	/** APlayerController::ServerExec cuts a command there before sending it */
	constexpr int32 EngineServerExecMaxLength = 128;

	/** A flood of refused commands logs one line per this many seconds */
	constexpr double ConsoleRefusalLogInterval = 1.0;

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarMonkeAllowClientCheats(
		TEXT("monke.Net.AllowClientCheats"),
		0,
		TEXT("1: the host lets its clients use serverexec, EnableCheats and pause (a development test between trusted machines, T-296). ")
		TEXT("0 (default): refused. Read on the host; development builds only."),
		ECVF_Cheat);
#endif
}

Amonke_islandPlayerController::Amonke_islandPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = Amonke_islandCameraManager::StaticClass();

	// proximity voice chat (T-263): replicated, so its "I talk" report reaches the server
	VoiceChat = CreateDefaultSubobject<UVoiceChatComponent>(TEXT("VoiceChat"));

	// death and spectator (T-135): replicated, so the dead player's choice of teammate reaches the server
	DeathSpectator = CreateDefaultSubobject<UDeathSpectatorComponent>(TEXT("DeathSpectator"));
}

void Amonke_islandPlayerController::ToggleSpeaking(bool bInSpeaking)
{
	if (VoiceChat && VoiceChat->HandleEngineToggleSpeaking(bInSpeaking))
	{
		return;
	}
	Super::ToggleSpeaking(bInSpeaking);
}

void Amonke_islandPlayerController::GetPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation) const
{
	// only the server holds the controllers of the other players: their relevancy follows what a dead one watches
	if (DeathSpectator && !IsLocalController())
	{
		if (const APawn* Followed = DeathSpectator->GetFollowedPawn())
		{
			Followed->GetActorEyesViewPoint(out_Location, out_Rotation);
			return;
		}
	}
	Super::GetPlayerViewPoint(out_Location, out_Rotation);
}

bool Amonke_islandPlayerController::IsHostConsoleAllowed(bool bLocalController, bool bClientCheatsAllowed)
{
	return bLocalController || bClientCheatsAllowed;
}

bool Amonke_islandPlayerController::MayUseHostConsole() const
{
	// on the host, a remote player's controller is never local (APlayerController::IsLocalController, set at its spawn)
#if UE_BUILD_SHIPPING
	return IsLocalController();
#else
	return IsHostConsoleAllowed(IsLocalController(), CVarMonkeAllowClientCheats.GetValueOnGameThread() != 0);
#endif
}

void Amonke_islandPlayerController::LogHostConsoleRefusal(const TCHAR* What)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	if (Now - LastConsoleRefusalLogTime < ConsoleRefusalLogInterval)
	{
		return;
	}
	LastConsoleRefusalLogTime = Now;
	// the controller's object name only, never the player's name (security norm SEC-09)
	UE_LOG(Logmonke_island, Warning, TEXT("%s: %s from a client refused, the host's console stays the host's (monke.Net.AllowClientCheats, T-296)"),
		*GetName(), What);
}

void Amonke_islandPlayerController::EnableCheats()
{
	if (!MayUseHostConsole())
	{
		LogHostConsoleRefusal(TEXT("EnableCheats"));
		return;
	}
	Super::EnableCheats();
}

void Amonke_islandPlayerController::AddCheats(bool bForce)
{
	// a cheat manager on a remote player's controller would run that client's exec commands on the host
	if (!MayUseHostConsole())
	{
		return;
	}
	Super::AddCheats(bForce);
}

bool Amonke_islandPlayerController::ServerExecRPC_Validate(const FString& Msg)
{
	return Msg.Len() <= EngineServerExecMaxLength;
}

void Amonke_islandPlayerController::ServerExecRPC_Implementation(const FString& Msg)
{
	if (!MayUseHostConsole())
	{
		LogHostConsoleRefusal(TEXT("serverexec"));
		return;
	}
	Super::ServerExecRPC_Implementation(Msg);
}

void Amonke_islandPlayerController::ServerPause_Implementation()
{
	if (!MayUseHostConsole())
	{
		LogHostConsoleRefusal(TEXT("pause"));
		return;
	}
	Super::ServerPause_Implementation();
}

void Amonke_islandPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logmonke_island, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void Amonke_islandPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool Amonke_islandPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
