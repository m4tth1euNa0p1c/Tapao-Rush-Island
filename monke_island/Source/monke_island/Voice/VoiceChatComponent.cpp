// monke_island

#include "Voice/VoiceChatComponent.h"
#include "Voice/VoiceChatSubsystem.h"
#include "Voice/VoiceRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Settings/SettingsSubsystem.h"
#include "Stealth/StealthSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/VoiceConfig.h"

UVoiceChatComponent::UVoiceChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// the "I talk" report is a server RPC of this component: it must replicate (nothing else does)
	SetIsReplicatedByDefault(true);
}

APlayerController* UVoiceChatComponent::GetController() const
{
	return Cast<APlayerController>(GetOwner());
}

const UVoiceRules& UVoiceChatComponent::GetRules() const
{
	const UVoiceChatSubsystem* Subsystem = UVoiceChatSubsystem::Get(this);
	return Subsystem ? Subsystem->GetRules() : *GetDefault<UVoiceRules>();
}

bool UVoiceChatComponent::IsPawnDead(const APawn* Pawn)
{
	const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	return Health && Health->IsDead();
}

// ---------------------------------------------------------------- owning client

void UVoiceChatComponent::LocalUpdate(double Now)
{
	APlayerController* Controller = GetController();
	UWorld* World = GetWorld();
	if (!Controller || !World || !Controller->IsLocalController())
	{
		return;
	}
	const UVoiceRules& Rules = GetRules();
	UVoiceChatSubsystem* Subsystem = UVoiceChatSubsystem::Get(this);
	EnsureInput(Controller, Rules);
	EnsureSettings(Controller);

	// a key released while the game did not have the keyboard (a menu took it) is released here
	if (bKeyHeld && VoiceInput && BoundAction.IsValid() && !VoiceInput->GetBoundActionValue(BoundAction.Get()).Get<bool>())
	{
		bKeyHeld = false;
	}

	FVoiceLocalState State;
	State.Mode = Mode;
	State.bKeyHeld = bKeyHeld;
	State.bDead = IsPawnDead(Controller->GetPawn());
	State.bMicTest = bMicTest;
	State.bFakeTalk = UVoiceChatSubsystem::IsFakeTalkOn();

	// the session (re)registers the local talker, which opens the microphone: state it again once it is there
	const bool bSessionReady = Subsystem && Subsystem->IsVoiceSessionReady();
	const bool bForce = bSessionReady && !bSawVoiceSession;
	bSawVoiceSession = bSessionReady;
	ApplyTransmission(Controller, Rules.bVoiceChatEnabled && VoiceMath::ShouldTransmit(State), bForce);
	ApplyThreshold(Rules);

	State.bTransmitting = bTransmitting;
	State.bEngineTalking = Subsystem && Subsystem->IsLocalTalking();
	Indicator = VoiceMath::Indicator(State);

	bool bReport = false;
	if (Throttle.Update(Now, VoiceMath::IsTalking(State), Rules.ClientMinSendIntervalSeconds, bReport))
	{
		ServerSetTalking(bReport);
	}

	// the voice of the test goes nowhere, and without network nothing takes the voice away: drop it every frame
	if (Subsystem)
	{
		Subsystem->SetSuppressOutgoingVoice(bTransmitting && (bMicTest || World->GetNetDriver() == nullptr));
	}
}

void UVoiceChatComponent::UpdateNow()
{
	if (const UWorld* World = GetWorld())
	{
		LocalUpdate(World->GetTimeSeconds());
	}
}

void UVoiceChatComponent::EnsureInput(APlayerController* Controller, const UVoiceRules& Rules)
{
	if (bInputBound || !Rules.PushToTalkAction)
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		// the local player comes a little after the controller on a client: next update
		return;
	}
	if (!VoiceInput)
	{
		VoiceInput = NewObject<UEnhancedInputComponent>(Controller, NAME_None, RF_Transient);
		VoiceInput->bBlockInput = false;
		VoiceInput->RegisterComponent();
		VoiceInput->BindAction(Rules.PushToTalkAction, ETriggerEvent::Started, this, &UVoiceChatComponent::HandlePushToTalkPressed);
		VoiceInput->BindAction(Rules.PushToTalkAction, ETriggerEvent::Completed, this, &UVoiceChatComponent::HandlePushToTalkReleased);
		VoiceInput->BindAction(Rules.PushToTalkAction, ETriggerEvent::Canceled, this, &UVoiceChatComponent::HandlePushToTalkReleased);
		VoiceInput->BindActionValue(Rules.PushToTalkAction);
		BoundAction = Rules.PushToTalkAction.Get();
	}
	// on the controller's own stack: the key works with any pawn, or none
	Controller->PushInputComponent(VoiceInput);
	if (Rules.MappingContext && !Input->HasMappingContext(Rules.MappingContext))
	{
		Input->AddMappingContext(Rules.MappingContext, Rules.MappingPriority);
	}
	BoundContext = Rules.MappingContext.Get();
	bInputBound = true;
}

void UVoiceChatComponent::EnsureSettings(APlayerController* Controller)
{
	if (bSettingsBound)
	{
		return;
	}
	const UGameInstance* GameInstance = Controller->GetGameInstance();
	USettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<USettingsSubsystem>() : nullptr;
	if (!Settings)
	{
		return;
	}
	const FMonkeSettingsValues& Applied = Settings->GetApplied();
	Mode = Applied.MicMode;
	ThresholdMode = Applied.MicMode;
	ThresholdValue = Applied.MicThreshold;
	VoiceSettingsHandle = Settings->OnVoiceSettingsChanged.AddUObject(this, &UVoiceChatComponent::HandleVoiceSettings);
	MicTestHandle = Settings->OnMicTestChanged.AddUObject(this, &UVoiceChatComponent::HandleMicTest);
	BoundSettings = Settings;
	bSettingsBound = true;
	// a test started before this controller (a travel): it goes on
	bMicTest = Settings->IsMicTestOn();
	SetLoopback(bMicTest);
}

void UVoiceChatComponent::ApplyTransmission(APlayerController* Controller, bool bTransmit, bool bForce)
{
	if (!bForce && bTransmissionApplied && bTransmit == bTransmitting)
	{
		return;
	}
	bTransmitting = bTransmit;
	bTransmissionApplied = true;
	// the engine's own toggle, not the controller's override that routes back here
	Controller->APlayerController::ToggleSpeaking(bTransmit);
	UE_LOG(LogMonkeVoice, Verbose, TEXT("Voice: microphone %s (%s)"), bTransmit ? TEXT("sent") : TEXT("closed"), *GetNameSafe(Controller));
}

void UVoiceChatComponent::ApplyThreshold(const UVoiceRules& Rules)
{
	const float Threshold = VoiceMath::EngineThreshold(ThresholdMode, ThresholdValue, Rules.PushToTalkThreshold);
	if (AppliedThreshold >= 0.0f && FMath::IsNearlyEqual(Threshold, AppliedThreshold))
	{
		return;
	}
	AppliedThreshold = Threshold;
	// silence detection decides what goes out; the noise gate turns what is quieter into silence: the same level
	UVOIPStatics::SetMicThreshold(Threshold);
	if (IConsoleVariable* NoiseGate = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicNoiseGateThreshold")))
	{
		NoiseGate->Set(Threshold, ECVF_SetByGameSetting);
	}
}

void UVoiceChatComponent::SetLoopback(bool bLoopback)
{
#if !UE_BUILD_SHIPPING
	if (bLoopbackSet == bLoopback)
	{
		return;
	}
	bLoopbackSet = bLoopback;
	// the engine's local loopback (OnlineSubsystem, compiled out of Shipping): the voice read is played back here
	if (IConsoleVariable* Loopback = IConsoleManager::Get().FindConsoleVariable(TEXT("OSS.VoiceLoopback")))
	{
		Loopback->Set(bLoopback ? 1 : 0, ECVF_SetByCode);
	}
#endif
}

bool UVoiceChatComponent::HandleEngineToggleSpeaking(bool bSpeaking)
{
	APlayerController* Controller = GetController();
	if (!Controller || !Controller->IsLocalController())
	{
		return false;
	}
	// the engine asks at login (ClientEnableNetworkVoice) and the console can ask too: the player's mode stands
	UE_LOG(LogMonkeVoice, Verbose, TEXT("Voice: engine asked the microphone %s, the voice chat keeps it %s"),
		bSpeaking ? TEXT("on") : TEXT("off"), bTransmitting ? TEXT("on") : TEXT("off"));
	ApplyTransmission(Controller, bTransmitting, true);
	return true;
}

void UVoiceChatComponent::HandlePushToTalkPressed()
{
	bKeyHeld = true;
	UpdateNow();
}

void UVoiceChatComponent::HandlePushToTalkReleased()
{
	bKeyHeld = false;
	UpdateNow();
}

void UVoiceChatComponent::HandleVoiceSettings(const FMonkeSettingsValues& Values, bool bPreview)
{
	// a preview only moves the threshold (heard during the test); the mode changes once applied
	ThresholdMode = Values.MicMode;
	ThresholdValue = Values.MicThreshold;
	if (!bPreview && Mode != Values.MicMode)
	{
		Mode = Values.MicMode;
		UE_LOG(LogMonkeVoice, Log, TEXT("Voice: microphone mode %d"), static_cast<int32>(Mode));
	}
	UpdateNow();
}

void UVoiceChatComponent::HandleMicTest(bool bTesting)
{
	bMicTest = bTesting;
	SetLoopback(bTesting);
	UpdateNow();
}

// ---------------------------------------------------------------- server

void UVoiceChatComponent::ServerSetTalking_Implementation(bool bTalking)
{
	const UWorld* World = GetWorld();
	APlayerController* Controller = GetController();
	if (!World || !Controller)
	{
		return;
	}
	// a bool cannot be out of range: the checks are the rate, the pawn and its life, all here (security.md §2)
	const UVoiceRules& Rules = GetRules();
	const double Now = World->GetTimeSeconds();
	if (!Limiter.Accept(Now, bTalking, Rules.ServerTalkBurst, Rules.ServerTalkRefillPerSecond))
	{
		if (Now - LastRefusalLog >= 1.0)
		{
			LastRefusalLog = Now;
			UE_LOG(LogMonkeVoice, Warning, TEXT("Voice: talk report of %s refused by the rate limit (%d refused so far)"),
				*GetNameSafe(Controller), Limiter.Refused);
		}
		return;
	}
	if (bReportedTalking == bTalking)
	{
		return;
	}
	bReportedTalking = bTalking;
	UE_LOG(LogMonkeVoice, Verbose, TEXT("Voice: %s %s"), *GetNameSafe(Controller), bTalking ? TEXT("talks") : TEXT("is silent"));

	// the first noise at once, the next ones from the update
	APawn* Pawn = Controller->GetPawn();
	if (bTalking && Pawn && !IsPawnDead(Pawn) && Cadence.ShouldReport(Now, true, Rules.NoiseIntervalSeconds))
	{
		ReportVoiceNoise(Pawn, Rules);
	}
}

void UVoiceChatComponent::ServerUpdate(double Now)
{
	APlayerController* Controller = GetController();
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}
	APawn* Pawn = Controller->GetPawn();
	const bool bDead = IsPawnDead(Pawn);
	SyncDeadMute(Controller, bDead);
	if (!Pawn || bDead)
	{
		return;
	}
	// what the player says it does, or the voice the host hears from it (a remote player only: the local one reports
	// itself, its microphone test included)
	const UVoiceChatSubsystem* Subsystem = UVoiceChatSubsystem::Get(this);
	const bool bHeard = !Controller->IsLocalController() && Subsystem && Subsystem->IsHeardTalking(Controller->PlayerState);
	const UVoiceRules& Rules = GetRules();
	if (Cadence.ShouldReport(Now, bReportedTalking || bHeard, Rules.NoiseIntervalSeconds))
	{
		ReportVoiceNoise(Pawn, Rules);
	}
}

void UVoiceChatComponent::ReportVoiceNoise(APawn* Pawn, const UVoiceRules& Rules) const
{
	UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
	if (!Stealth || !Pawn)
	{
		return;
	}
	const float Radius = VoiceMath::NoiseRadius(Rules.VoiceNoiseRadiusCm, Stealth->GetNoiseRadius(EStealthNoise::Voice));
	Stealth->ReportNoise(Pawn, Pawn->GetActorLocation(), Radius, EStealthNoise::Voice);
}

void UVoiceChatComponent::SyncDeadMute(APlayerController* Controller, bool bDead)
{
	const APlayerState* PlayerState = Controller->PlayerState;
	if (!PlayerState || !PlayerState->GetUniqueId().IsValid())
	{
		return;
	}
	const FUniqueNetIdRepl& Id = PlayerState->GetUniqueId();
	if (bDead)
	{
		// the server stops relaying the voice to each of them, and each mutes it locally (the host too)
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Other = It->Get();
			if (Other && Other != Controller && !MutedFor.Contains(Other))
			{
				Other->GameplayMutePlayer(Id);
				MutedFor.Add(Other);
			}
		}
		return;
	}
	if (MutedFor.Num() > 0)
	{
		for (const TWeakObjectPtr<APlayerController>& Weak : MutedFor)
		{
			if (APlayerController* Other = Weak.Get(); Other && !Other->IsActorBeingDestroyed())
			{
				Other->GameplayUnmutePlayer(Id);
			}
		}
		MutedFor.Reset();
		UE_LOG(LogMonkeVoice, Log, TEXT("Voice: %s alive again, heard by the others"), *GetNameSafe(Controller));
	}
}

// ---------------------------------------------------------------- end

void UVoiceChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	APlayerController* Controller = GetController();
	// a player leaving the game: nobody keeps it muted (it may come back with the same id)
	if (EndPlayReason == EEndPlayReason::Destroyed && Controller && MutedFor.Num() > 0)
	{
		SyncDeadMute(Controller, false);
	}
	MutedFor.Reset();

	if (Controller && VoiceInput)
	{
		Controller->PopInputComponent(VoiceInput);
	}
	if (VoiceInput)
	{
		VoiceInput->DestroyComponent();
		VoiceInput = nullptr;
	}
	if (const UInputMappingContext* Context = BoundContext.Get())
	{
		const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
		if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
		{
			Input->RemoveMappingContext(Context);
		}
	}
	BoundContext.Reset();
	bInputBound = false;

	if (USettingsSubsystem* Settings = BoundSettings.Get())
	{
		Settings->OnVoiceSettingsChanged.Remove(VoiceSettingsHandle);
		Settings->OnMicTestChanged.Remove(MicTestHandle);
	}
	BoundSettings.Reset();
	bSettingsBound = false;
	SetLoopback(false);

	Super::EndPlay(EndPlayReason);
}
