// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundAttenuation.h"
#include "VoiceRules.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 *  Values of the proximity voice chat (DA_VoiceRules, T-263, docs/features/chat-vocal.md): the push-to-talk input, the
 *  microphone threshold of push-to-talk, how a voice fades with distance, where it comes from on the speaker, the noise
 *  a voice makes for the mobs, and the network limits of the "I am talking" report. The player's own choices
 *  (microphone mode, voice activation threshold, the voices' volume) are settings (DA_SettingsRules, USettingsSubsystem).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UVoiceRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Voice chat on in this game; off: no voice session, the microphone never opens (the engine also needs
	 *  [Voice] bEnabled=true in DefaultEngine.ini) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	bool bVoiceChatEnabled = true;

	/** Held to talk (IA_PushToTalk, boolean) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> PushToTalkAction;

	/** Context of the push-to-talk key (IMC_Voice: T), added to the local player by the voice chat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Priority of IMC_Voice: above the contexts of the pawn, so the key (consumed by IA_PushToTalk) only talks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	int32 MappingPriority = 10;

	/** Microphone level (0..1, the engine's silence detection) a held push-to-talk key needs before anything goes out:
	 *  low, only silence and breathing stay home. Voice activation uses the player's own threshold (settings) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Microphone", meta=(ClampMin="0", ClampMax="1"))
	float PushToTalkThreshold = 0.04f;

	/** Up to this distance from the speaker's head a voice is heard at full volume (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity", meta=(ClampMin="0", Units="cm"))
	float InnerRadiusCm = 300.0f;

	/** From this distance on a voice is no longer heard at all (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity", meta=(ClampMin="1", Units="cm"))
	float MaxDistanceCm = 2500.0f;

	/** How the volume falls between the two */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity")
	EAttenuationDistanceModel DistanceModel = EAttenuationDistanceModel::NaturalSound;

	/** Attenuation at MaxDistanceCm for the natural sound model (dB, negative) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity", meta=(ClampMin="-60", ClampMax="0"))
	float AttenuationAtMaxDb = -60.0f;

	/** Optional attenuation asset (ATT_) used instead of the values above, for a sound designer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity")
	TObjectPtr<USoundAttenuation> AttenuationOverride;

	/** Socket or bone of the speaker's mesh the voice comes from */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity")
	FName HeadSocketName = TEXT("head");

	/** Where the voice comes from on a pawn without that socket, from its root (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Proximity")
	FVector HeadFallbackOffset = FVector(0.0f, 0.0f, 60.0f);

	/** Radius of the noise a talking player makes for the mobs (cm); 0: the voice radius of DA_StealthRules */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Noise", meta=(ClampMin="0", Units="cm"))
	float VoiceNoiseRadiusCm = 0.0f;

	/** While a player talks, the server reports the noise of the voice this often (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Noise", meta=(ClampMin="0.1", Units="s"))
	float NoiseIntervalSeconds = 0.5f;

	/** A client tells the server it talks or stopped at most this often (s); the last state always goes out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.05", Units="s"))
	float ClientMinSendIntervalSeconds = 0.25f;

	/** Server: "I talk" reports a player may send in a burst before the limit (stopping is always accepted) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="1"))
	float ServerTalkBurst = 6.0f;

	/** Server: "I talk" reports given back per second to each player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1"))
	float ServerTalkRefillPerSecond = 3.0f;

	/** Period of the voice chat's updates: push-to-talk check, talkers, noise cadence, HUD names (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.02", ClampMax="1", Units="s"))
	float UpdateIntervalSeconds = 0.1f;

	/** HUD: names of the players heard near the local player, at most */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0", ClampMax="8"))
	int32 MaxNamesShown = 3;

	/** HUD: a name longer than this is cut */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD", meta=(ClampMin="4", ClampMax="64"))
	int32 MaxNameChars = 18;
};
