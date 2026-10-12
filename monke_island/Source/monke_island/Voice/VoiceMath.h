// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Settings/SettingsRules.h"
#include "Sound/SoundAttenuation.h"
#include "Voice/VoiceTypes.h"

/** What the local player's microphone does now: the inputs of the pure rules of the voice chat */
struct FVoiceLocalState
{
	/** Microphone mode chosen in the settings */
	EVoiceMicMode Mode = EVoiceMicMode::PushToTalk;

	/** The push-to-talk key is held */
	bool bKeyHeld = false;

	/** The player's pawn is dead */
	bool bDead = false;

	/** The microphone test runs (the voice comes back to the player alone) */
	bool bMicTest = false;

	/** The engine sends the microphone (what the voice chat asked it) */
	bool bTransmitting = false;

	/** The engine hears a voice above the threshold (its talking state of the local player) */
	bool bEngineTalking = false;

	/** monke.Voice.FakeTalk: the held key counts as talking, a noise test without a microphone */
	bool bFakeTalk = false;
};

/** Pure rules of the proximity voice chat (T-263), tested by VoiceSpec */
namespace VoiceMath
{
	/** Whether the engine should send the microphone: always during the test (nothing leaves the machine then), never
	 *  for the dead, always in voice activation, while the key is held in push-to-talk, never when off */
	MONKE_ISLAND_API bool ShouldTransmit(const FVoiceLocalState& State);

	/** Whether the local player talks, for the mobs and the HUD: heard by the engine while transmitting, or the held key
	 *  in the noise test; never during the microphone test nor when dead */
	MONKE_ISLAND_API bool IsTalking(const FVoiceLocalState& State);

	/** Indicator of the local microphone on the HUD */
	MONKE_ISLAND_API EVoiceIndicator Indicator(const FVoiceLocalState& State);

	/** Silence threshold of the engine for a mode: the player's own in voice activation, the rules' low one otherwise;
	 *  within 0..1, 0.08 (the engine's default) for a value that is not a number */
	MONKE_ISLAND_API float EngineThreshold(EVoiceMicMode Mode, float PlayerThreshold, float PushToTalkThreshold);

	/** Radius of the noise of a voice (cm): the voice rules' own when above 0, else the stealth rules' voice radius;
	 *  never negative nor NaN */
	MONKE_ISLAND_API float NoiseRadius(float VoiceRulesRadiusCm, float StealthVoiceRadiusCm);

	/** Attenuation of a voice: full volume within InnerRadiusCm of the speaker's head, silent from MaxDistanceCm on (a
	 *  sphere of radius Inner, the falloff the rest; MaxDistance is kept at least 1 cm beyond Inner) */
	MONKE_ISLAND_API FSoundAttenuationSettings MakeAttenuation(float InnerRadiusCm, float MaxDistanceCm, EAttenuationDistanceModel Model, float AttenuationAtMaxDb);

	/** A player's name for the HUD: trimmed, cut to MaxChars with an ellipsis */
	MONKE_ISLAND_API FString DisplayName(const FString& Name, int32 MaxChars);
}

/** Server: token bucket of the "I talk" reports of one player; a report of silence always passes (it only takes noise
 *  away), so a refused report never leaves a player talking forever */
struct MONKE_ISLAND_API FVoiceTalkLimiter
{
	/** Reports left (-1: full at the first report) */
	double Tokens = -1.0;

	/** Time of the last refill (s) */
	double LastRefill = 0.0;

	/** "I talk" reports refused so far */
	int32 Refused = 0;

	/** Whether a report of bTalking at Now passes; Burst reports at most, RefillPerSecond given back per second */
	bool Accept(double Now, bool bTalking, float Burst, float RefillPerSecond);
};

/** Client: sends the talking state when it changes, at most once every MinInterval; the last state always goes out */
struct MONKE_ISLAND_API FVoiceSendThrottle
{
	/** State the server was told */
	bool bSent = false;

	/** Time of the last report (s) */
	double LastSend = -1.0e9;

	/** True, with the state in OutState, when a report must go now */
	bool Update(double Now, bool bDesired, float MinInterval, bool& OutState);

	/** Forgets what was sent (a new world): the next Update reports a voice again */
	void Reset()
	{
		bSent = false;
		LastSend = -1.0e9;
	}
};

/** Server: when the noise of a talking player is reported: at once when the voice starts, then every Interval; never
 *  more often than that, however the reports come */
struct MONKE_ISLAND_API FVoiceNoiseCadence
{
	/** Earliest time of the next report (s) */
	double NextReport = -1.0e9;

	/** True when a noise must be reported now; nothing while silent */
	bool ShouldReport(double Now, bool bTalking, float Interval);
};
