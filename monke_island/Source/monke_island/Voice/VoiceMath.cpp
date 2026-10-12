// monke_island

#include "Voice/VoiceMath.h"

namespace
{
	/** The engine's default silence threshold (voice.SilenceDetectionThreshold) */
	constexpr float EngineDefaultThreshold = 0.08f;

	float FiniteOr(float Value, float Fallback)
	{
		return FMath::IsFinite(Value) ? Value : Fallback;
	}
}

namespace VoiceMath
{
	bool ShouldTransmit(const FVoiceLocalState& State)
	{
		if (State.bMicTest)
		{
			return true;
		}
		if (State.bDead)
		{
			return false;
		}
		switch (State.Mode)
		{
		case EVoiceMicMode::VoiceActivation:
			return true;
		case EVoiceMicMode::PushToTalk:
			return State.bKeyHeld;
		default:
			return false;
		}
	}

	bool IsTalking(const FVoiceLocalState& State)
	{
		if (State.bMicTest || State.bDead)
		{
			return false;
		}
		if (State.bFakeTalk && State.bKeyHeld)
		{
			return true;
		}
		return State.bTransmitting && State.bEngineTalking;
	}

	EVoiceIndicator Indicator(const FVoiceLocalState& State)
	{
		if (State.bMicTest)
		{
			return EVoiceIndicator::Test;
		}
		if (State.bDead)
		{
			// tells why nothing goes out when the player tries
			return State.bKeyHeld || State.Mode == EVoiceMicMode::VoiceActivation ? EVoiceIndicator::Dead : EVoiceIndicator::None;
		}
		if (IsTalking(State))
		{
			return EVoiceIndicator::Talking;
		}
		return State.bTransmitting ? EVoiceIndicator::Open : EVoiceIndicator::None;
	}

	float EngineThreshold(EVoiceMicMode Mode, float PlayerThreshold, float PushToTalkThreshold)
	{
		const float Value = Mode == EVoiceMicMode::VoiceActivation ? PlayerThreshold : PushToTalkThreshold;
		return FMath::Clamp(FiniteOr(Value, EngineDefaultThreshold), 0.0f, 1.0f);
	}

	float NoiseRadius(float VoiceRulesRadiusCm, float StealthVoiceRadiusCm)
	{
		const float Own = FiniteOr(VoiceRulesRadiusCm, 0.0f);
		if (Own > 0.0f)
		{
			return Own;
		}
		return FMath::Max(0.0f, FiniteOr(StealthVoiceRadiusCm, 0.0f));
	}

	FSoundAttenuationSettings MakeAttenuation(float InnerRadiusCm, float MaxDistanceCm, EAttenuationDistanceModel Model, float AttenuationAtMaxDb)
	{
		const float Inner = FMath::Max(0.0f, FiniteOr(InnerRadiusCm, 0.0f));
		const float Max = FMath::Max(Inner + 1.0f, FiniteOr(MaxDistanceCm, Inner + 1.0f));

		FSoundAttenuationSettings Settings;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(Inner, 0.0f, 0.0f);
		Settings.FalloffDistance = Max - Inner;
		Settings.DistanceAlgorithm = Model;
		// the natural model goes silent past its shape instead of fading on: nobody is heard beyond MaxDistance
		Settings.FalloffMode = ENaturalSoundFalloffMode::Silent;
		Settings.dBAttenuationAtMax = FMath::Clamp(FiniteOr(AttenuationAtMaxDb, -60.0f), -60.0f, 0.0f);
		return Settings;
	}

	FString DisplayName(const FString& Name, int32 MaxChars)
	{
		FString Trimmed = Name.TrimStartAndEnd();
		const int32 Limit = FMath::Max(2, MaxChars);
		if (Trimmed.Len() > Limit)
		{
			Trimmed.LeftInline(Limit - 1);
			Trimmed.AppendChar(TCHAR(0x2026));
		}
		return Trimmed;
	}
}

bool FVoiceTalkLimiter::Accept(double Now, bool bTalking, float Burst, float RefillPerSecond)
{
	const double Capacity = FMath::Max(1.0, static_cast<double>(FiniteOr(Burst, 1.0f)));
	const double Refill = FMath::Max(0.0, static_cast<double>(FiniteOr(RefillPerSecond, 0.0f)));
	if (Tokens < 0.0)
	{
		Tokens = Capacity;
		LastRefill = Now;
	}
	// a clock that went back (a new world) gives nothing
	Tokens = FMath::Min(Capacity, Tokens + FMath::Max(0.0, Now - LastRefill) * Refill);
	LastRefill = Now;
	if (!bTalking)
	{
		return true;
	}
	if (Tokens >= 1.0)
	{
		Tokens -= 1.0;
		return true;
	}
	++Refused;
	return false;
}

bool FVoiceSendThrottle::Update(double Now, bool bDesired, float MinInterval, bool& OutState)
{
	if (bDesired == bSent || Now - LastSend < FMath::Max(0.0f, FiniteOr(MinInterval, 0.0f)))
	{
		// nothing new, or too soon: a later update sends the state then in force
		return false;
	}
	bSent = bDesired;
	LastSend = Now;
	OutState = bDesired;
	return true;
}

bool FVoiceNoiseCadence::ShouldReport(double Now, bool bTalking, float Interval)
{
	if (!bTalking || Now < NextReport)
	{
		return false;
	}
	NextReport = Now + FMath::Max(0.05f, FiniteOr(Interval, 0.5f));
	return true;
}
