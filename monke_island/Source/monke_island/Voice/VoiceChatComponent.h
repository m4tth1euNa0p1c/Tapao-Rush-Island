// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Voice/VoiceMath.h"
#include "VoiceChatComponent.generated.h"

class APawn;
class APlayerController;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class USettingsSubsystem;
class UVoiceRules;
struct FMonkeSettingsValues;

/**
 *  Proximity voice chat of one player (T-263, docs/features/chat-vocal.md): a component of the player controller,
 *  on the server and on the owning client. Driven by UVoiceChatSubsystem (its update timer), never ticks.
 *  - Owning client: the push-to-talk key (IA_PushToTalk in IMC_Voice), the microphone mode of the settings, what the
 *    engine sends (APlayerController::ToggleSpeaking), the microphone test, and the "I talk" report to the server: a
 *    bool, reliable, only when it changes, at most every ClientMinSendIntervalSeconds.
 *  - Server: takes the report through a rate limit; while the player talks (its report, or the voice the host hears)
 *    and lives, reports the noise of the voice to the stealth (UStealthSubsystem::ReportNoise) every
 *    NoiseIntervalSeconds; mutes a dead player for the other players (gameplay mute: the server no longer relays the
 *    voice) and lifts it when the player lives again.
 *  No audio is handled here: the engine's VoIP captures, encodes, relays and plays the voices.
 */
UCLASS(ClassGroup=(Voice), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UVoiceChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVoiceChatComponent();

	/** Owning client: whether the player talks, sent when it changes (a bool: the server decides what it is worth) */
	UFUNCTION(Server, Reliable)
	void ServerSetTalking(bool bTalking);

	/** Owning client, every update of the subsystem: key, mode, transmission, report */
	void LocalUpdate(double Now);

	/** Server, every update of the subsystem: noise of the voice, mute of the dead */
	void ServerUpdate(double Now);

	/** The engine asked the controller to start or stop sending the microphone (at login, or the ToggleSpeaking
	 *  command): the voice chat keeps the player's mode instead. True when handled */
	bool HandleEngineToggleSpeaking(bool bSpeaking);

	/** Indicator of the local microphone for the HUD */
	EVoiceIndicator GetIndicator() const { return Indicator; }

	/** Server: the player's last accepted report says it talks */
	bool IsReportedTalking() const { return bReportedTalking; }

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Controller owning this component */
	APlayerController* GetController() const;

	/** Rules of the world's voice chat (DA_VoiceRules, or the class defaults) */
	const UVoiceRules& GetRules() const;

	/** Owning client: IA_PushToTalk bound and IMC_Voice added, once the local player exists */
	void EnsureInput(APlayerController* Controller, const UVoiceRules& Rules);

	/** Owning client: listens to the settings of this game instance, once */
	void EnsureSettings(APlayerController* Controller);

	/** Owning client: asks the engine to send the microphone or not (bForce: even if already asked) */
	void ApplyTransmission(APlayerController* Controller, bool bTransmit, bool bForce);

	/** Owning client: the engine's silence threshold for the mode (the threshold being edited during a preview) */
	void ApplyThreshold(const UVoiceRules& Rules);

	/** Owning client: the engine sends this player's voice back to them (the microphone test) */
	void SetLoopback(bool bLoopback);

	/** Owning client: runs LocalUpdate now (a key, a setting) */
	void UpdateNow();

	/** Server: the noise of the voice at the pawn, for the mobs that listen */
	void ReportVoiceNoise(APawn* Pawn, const UVoiceRules& Rules) const;

	/** Server: the other players no longer hear this one while dead, and hear it again once alive */
	void SyncDeadMute(APlayerController* Controller, bool bDead);

	/** True when the pawn carries a health that says dead */
	static bool IsPawnDead(const APawn* Pawn);

	void HandlePushToTalkPressed();
	void HandlePushToTalkReleased();
	void HandleVoiceSettings(const FMonkeSettingsValues& Values, bool bPreview);
	void HandleMicTest(bool bTesting);

	/** Owning client: input component pushed on the controller for the push-to-talk key */
	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> VoiceInput;

	/** Action and context bound (to unbind them) */
	TWeakObjectPtr<const UInputAction> BoundAction;
	TWeakObjectPtr<const UInputMappingContext> BoundContext;

	/** Settings listened to, and the handles */
	TWeakObjectPtr<USettingsSubsystem> BoundSettings;
	FDelegateHandle VoiceSettingsHandle;
	FDelegateHandle MicTestHandle;

	/** Microphone mode applied (settings) */
	EVoiceMicMode Mode = EVoiceMicMode::PushToTalk;

	/** Mode and threshold of the engine's silence detection (the applied ones, or those being previewed) */
	EVoiceMicMode ThresholdMode = EVoiceMicMode::PushToTalk;
	float ThresholdValue = 0.08f;

	/** Threshold last given to the engine (-1: none yet) */
	float AppliedThreshold = -1.0f;

	/** Owning client state */
	bool bInputBound = false;
	bool bSettingsBound = false;
	bool bKeyHeld = false;
	bool bMicTest = false;
	bool bLoopbackSet = false;
	bool bTransmitting = false;
	bool bTransmissionApplied = false;
	bool bSawVoiceSession = false;
	EVoiceIndicator Indicator = EVoiceIndicator::None;
	FVoiceSendThrottle Throttle;

	/** Server state */
	bool bReportedTalking = false;
	FVoiceTalkLimiter Limiter;
	FVoiceNoiseCadence Cadence;
	double LastRefusalLog = -1.0e9;

	/** Server: controllers that were told to mute this player while dead */
	TArray<TWeakObjectPtr<APlayerController>> MutedFor;
};
