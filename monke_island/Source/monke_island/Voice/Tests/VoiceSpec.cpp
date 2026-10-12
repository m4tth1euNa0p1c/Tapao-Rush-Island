// monke_island

#include "Misc/AutomationTest.h"
#include "Settings/SettingsMath.h"
#include "Settings/SettingsRules.h"
#include "Voice/VoiceMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Voice
BEGIN_DEFINE_SPEC(FVoiceSpec, "monke_island.Voice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** State of a living player in a mode */
	static FVoiceLocalState Living(EVoiceMicMode Mode, bool bKeyHeld)
	{
		FVoiceLocalState State;
		State.Mode = Mode;
		State.bKeyHeld = bKeyHeld;
		return State;
	}

END_DEFINE_SPEC(FVoiceSpec)

void FVoiceSpec::Define()
{
	Describe("Modes", [this]()
	{
		It("sends the microphone in push-to-talk only while the key is held", [this]()
		{
			TestFalse(TEXT("key up"), VoiceMath::ShouldTransmit(Living(EVoiceMicMode::PushToTalk, false)));
			TestTrue(TEXT("key held"), VoiceMath::ShouldTransmit(Living(EVoiceMicMode::PushToTalk, true)));
		});

		It("keeps the microphone open in voice activation, closed when off, whatever the key", [this]()
		{
			TestTrue(TEXT("voice activation"), VoiceMath::ShouldTransmit(Living(EVoiceMicMode::VoiceActivation, false)));
			TestFalse(TEXT("off, key up"), VoiceMath::ShouldTransmit(Living(EVoiceMicMode::Off, false)));
			TestFalse(TEXT("off, key held"), VoiceMath::ShouldTransmit(Living(EVoiceMicMode::Off, true)));
		});

		It("never sends the voice of the dead, but runs the microphone test even when off", [this]()
		{
			FVoiceLocalState Dead = Living(EVoiceMicMode::VoiceActivation, true);
			Dead.bDead = true;
			TestFalse(TEXT("dead"), VoiceMath::ShouldTransmit(Dead));
			FVoiceLocalState Test = Living(EVoiceMicMode::Off, false);
			Test.bMicTest = true;
			TestTrue(TEXT("test when off"), VoiceMath::ShouldTransmit(Test));
		});

		It("talks only when the engine hears a voice that is sent", [this]()
		{
			FVoiceLocalState State = Living(EVoiceMicMode::PushToTalk, true);
			State.bTransmitting = true;
			TestFalse(TEXT("key held, silence"), VoiceMath::IsTalking(State));
			State.bEngineTalking = true;
			TestTrue(TEXT("key held, voice"), VoiceMath::IsTalking(State));
			State.bTransmitting = false;
			TestFalse(TEXT("voice heard but not sent"), VoiceMath::IsTalking(State));
		});

		It("makes the held key talk in the noise test, never during the microphone test nor dead", [this]()
		{
			FVoiceLocalState Fake = Living(EVoiceMicMode::PushToTalk, true);
			Fake.bFakeTalk = true;
			TestTrue(TEXT("noise test, key held"), VoiceMath::IsTalking(Fake));
			Fake.bKeyHeld = false;
			TestFalse(TEXT("noise test, key up"), VoiceMath::IsTalking(Fake));

			FVoiceLocalState Test = Living(EVoiceMicMode::VoiceActivation, false);
			Test.bMicTest = true;
			Test.bTransmitting = true;
			Test.bEngineTalking = true;
			TestFalse(TEXT("microphone test: the mobs hear nothing"), VoiceMath::IsTalking(Test));

			FVoiceLocalState Dead = Living(EVoiceMicMode::PushToTalk, true);
			Dead.bDead = true;
			Dead.bFakeTalk = true;
			TestFalse(TEXT("dead"), VoiceMath::IsTalking(Dead));
		});

		It("shows the microphone whenever it is open (security.md §6)", [this]()
		{
			FVoiceLocalState State = Living(EVoiceMicMode::PushToTalk, false);
			TestEqual(TEXT("closed"), VoiceMath::Indicator(State), EVoiceIndicator::None);
			State.bKeyHeld = true;
			State.bTransmitting = true;
			TestEqual(TEXT("open"), VoiceMath::Indicator(State), EVoiceIndicator::Open);
			State.bEngineTalking = true;
			TestEqual(TEXT("talking"), VoiceMath::Indicator(State), EVoiceIndicator::Talking);
			State.bMicTest = true;
			TestEqual(TEXT("test"), VoiceMath::Indicator(State), EVoiceIndicator::Test);
			FVoiceLocalState Dead = Living(EVoiceMicMode::PushToTalk, true);
			Dead.bDead = true;
			TestEqual(TEXT("dead, trying"), VoiceMath::Indicator(Dead), EVoiceIndicator::Dead);
			Dead.bKeyHeld = false;
			TestEqual(TEXT("dead, silent"), VoiceMath::Indicator(Dead), EVoiceIndicator::None);
		});
	});

	Describe("Threshold", [this]()
	{
		It("uses the player's threshold in voice activation and the rules' one in push-to-talk", [this]()
		{
			TestEqual(TEXT("voice activation"), VoiceMath::EngineThreshold(EVoiceMicMode::VoiceActivation, 0.2f, 0.04f), 0.2f);
			TestEqual(TEXT("push-to-talk"), VoiceMath::EngineThreshold(EVoiceMicMode::PushToTalk, 0.2f, 0.04f), 0.04f);
		});

		It("keeps the engine threshold a level between 0 and 1", [this]()
		{
			TestEqual(TEXT("above 1"), VoiceMath::EngineThreshold(EVoiceMicMode::VoiceActivation, 5.0f, 0.04f), 1.0f);
			TestEqual(TEXT("negative"), VoiceMath::EngineThreshold(EVoiceMicMode::PushToTalk, 0.2f, -1.0f), 0.0f);
			TestEqual(TEXT("not a number"), VoiceMath::EngineThreshold(EVoiceMicMode::VoiceActivation, NAN, 0.04f), 0.08f);
		});

		It("brings saved microphone settings back inside the rules", [this]()
		{
			USettingsRules* Rules = NewObject<USettingsRules>();
			Rules->AddToRoot();
			FMonkeSettingsValues Values = SettingsMath::Defaults(*Rules);
			TestEqual(TEXT("push-to-talk by default (cahier §18)"), Values.MicMode, EVoiceMicMode::PushToTalk);
			TestEqual(TEXT("default threshold"), Values.MicThreshold, Rules->DefaultMicThreshold);
			TestEqual(TEXT("the voices at 80 %"), Values.Volume(ESettingsVolume::VoiceChat), 0.8f);

			Values.MicMode = static_cast<EVoiceMicMode>(42);
			Values.MicThreshold = 9.0f;
			SettingsMath::Sanitize(*Rules, Values);
			TestEqual(TEXT("unknown mode"), Values.MicMode, Rules->DefaultMicMode);
			TestEqual(TEXT("threshold capped"), Values.MicThreshold, static_cast<float>(Rules->MicThresholdRange.Y));
			Values.MicThreshold = NAN;
			SettingsMath::Sanitize(*Rules, Values);
			TestEqual(TEXT("threshold that is not a number"), Values.MicThreshold, Rules->DefaultMicThreshold);
			Rules->RemoveFromRoot();
		});

		It("tells a microphone change from a volume change", [this]()
		{
			FMonkeSettingsValues A;
			FMonkeSettingsValues B = A;
			B.Volume(ESettingsVolume::VoiceChat) = 0.1f;
			TestFalse(TEXT("volume only"), A.VoiceDiffers(B));
			B.MicMode = EVoiceMicMode::Off;
			TestTrue(TEXT("mode"), A.VoiceDiffers(B));
		});
	});

	Describe("RateLimit", [this]()
	{
		It("accepts a burst of talk reports, then refuses until refilled", [this]()
		{
			FVoiceTalkLimiter Limiter;
			for (int32 Index = 0; Index < 6; ++Index)
			{
				TestTrue(*FString::Printf(TEXT("report %d of the burst"), Index), Limiter.Accept(10.0, true, 6.0f, 3.0f));
			}
			TestFalse(TEXT("seventh at once"), Limiter.Accept(10.0, true, 6.0f, 3.0f));
			TestEqual(TEXT("refusals counted"), Limiter.Refused, 1);
			TestTrue(TEXT("a third of a second later"), Limiter.Accept(10.34, true, 6.0f, 3.0f));
		});

		It("always accepts silence, so a refused report never leaves a player talking", [this]()
		{
			FVoiceTalkLimiter Limiter;
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Limiter.Accept(0.0, true, 2.0f, 1.0f);
			}
			TestTrue(TEXT("stop when empty"), Limiter.Accept(0.0, false, 2.0f, 1.0f));
		});

		It("lets an honest client through (one start every half second, for a minute)", [this]()
		{
			FVoiceTalkLimiter Limiter;
			bool bAll = true;
			for (int32 Index = 0; Index < 120; ++Index)
			{
				const double Now = Index * 0.25;
				bAll &= Limiter.Accept(Now, Index % 2 == 0, 6.0f, 3.0f);
			}
			TestTrue(TEXT("every report accepted"), bAll);
		});

		It("gives nothing back when the clock goes back, and survives broken rules", [this]()
		{
			FVoiceTalkLimiter Limiter;
			TestTrue(TEXT("first"), Limiter.Accept(100.0, true, 1.0f, 1.0f));
			TestFalse(TEXT("clock back"), Limiter.Accept(50.0, true, 1.0f, 1.0f));
			FVoiceTalkLimiter Broken;
			TestTrue(TEXT("burst not a number: one report"), Broken.Accept(0.0, true, NAN, NAN));
			TestFalse(TEXT("refill not a number: none back"), Broken.Accept(5.0, true, NAN, NAN));
		});
	});

	Describe("Throttle", [this]()
	{
		It("sends a change at once, then waits the interval and sends the last state", [this]()
		{
			FVoiceSendThrottle Throttle;
			bool bState = false;
			TestFalse(TEXT("silence is the start"), Throttle.Update(0.0, false, 0.25f, bState));
			TestTrue(TEXT("talks"), Throttle.Update(1.0, true, 0.25f, bState));
			TestTrue(TEXT("sent talking"), bState);
			TestFalse(TEXT("stops too soon: waits"), Throttle.Update(1.1, false, 0.25f, bState));
			TestTrue(TEXT("interval over: the stop goes"), Throttle.Update(1.3, false, 0.25f, bState));
			TestFalse(TEXT("sent silence"), bState);
		});

		It("sends nothing when the state comes back before the interval", [this]()
		{
			FVoiceSendThrottle Throttle;
			bool bState = false;
			Throttle.Update(0.0, true, 0.25f, bState);
			TestFalse(TEXT("a blink of silence"), Throttle.Update(0.1, false, 0.25f, bState));
			TestFalse(TEXT("talking again: nothing to send"), Throttle.Update(0.3, true, 0.25f, bState));
		});

		It("reports a voice again after a reset (a new world)", [this]()
		{
			FVoiceSendThrottle Throttle;
			bool bState = false;
			Throttle.Update(0.0, true, 0.25f, bState);
			Throttle.Reset();
			TestTrue(TEXT("talking again"), Throttle.Update(0.0, true, 0.25f, bState));
		});
	});

	Describe("NoiseCadence", [this]()
	{
		It("reports at once when the voice starts, then every interval while it lasts", [this]()
		{
			FVoiceNoiseCadence Cadence;
			TestFalse(TEXT("silent"), Cadence.ShouldReport(0.0, false, 0.5f));
			TestTrue(TEXT("first word"), Cadence.ShouldReport(1.0, true, 0.5f));
			TestFalse(TEXT("0.2 s later"), Cadence.ShouldReport(1.2, true, 0.5f));
			TestTrue(TEXT("0.5 s later"), Cadence.ShouldReport(1.5, true, 0.5f));
		});

		It("never reports faster than the interval, however the voice blinks", [this]()
		{
			FVoiceNoiseCadence Cadence;
			int32 Reports = 0;
			for (int32 Index = 0; Index < 100; ++Index)
			{
				Reports += Cadence.ShouldReport(Index * 0.01, Index % 2 == 0, 0.5f) ? 1 : 0;
			}
			TestEqual(TEXT("one second of blinking voice: 2 reports"), Reports, 2);
		});
	});

	Describe("Noise", [this]()
	{
		It("takes the voice rules' radius when set, else the stealth rules' one", [this]()
		{
			TestEqual(TEXT("own"), VoiceMath::NoiseRadius(1200.0f, 1800.0f), 1200.0f);
			TestEqual(TEXT("stealth"), VoiceMath::NoiseRadius(0.0f, 1800.0f), 1800.0f);
			TestEqual(TEXT("broken own"), VoiceMath::NoiseRadius(NAN, 1800.0f), 1800.0f);
			TestEqual(TEXT("broken stealth"), VoiceMath::NoiseRadius(0.0f, -5.0f), 0.0f);
		});
	});

	Describe("Proximity", [this]()
	{
		It("hears a voice at full volume within the inner radius and nothing from the maximum distance", [this]()
		{
			const FSoundAttenuationSettings Settings = VoiceMath::MakeAttenuation(300.0f, 2500.0f, EAttenuationDistanceModel::NaturalSound, -60.0f);
			TestTrue(TEXT("attenuates"), static_cast<bool>(Settings.bAttenuate));
			TestTrue(TEXT("spatialised"), static_cast<bool>(Settings.bSpatialize));
			TestEqual(TEXT("sphere"), Settings.AttenuationShape.GetValue(), EAttenuationShape::Sphere);
			TestEqual(TEXT("inner radius"), static_cast<float>(Settings.AttenuationShapeExtents.X), 300.0f);
			TestEqual(TEXT("falloff up to 25 m"), Settings.FalloffDistance, 2200.0f);
			TestEqual(TEXT("silent past the falloff"), Settings.FalloffMode, ENaturalSoundFalloffMode::Silent);
		});

		It("keeps a falloff when the rules are broken", [this]()
		{
			const FSoundAttenuationSettings Inverted = VoiceMath::MakeAttenuation(800.0f, 100.0f, EAttenuationDistanceModel::Linear, -60.0f);
			TestEqual(TEXT("max before inner"), Inverted.FalloffDistance, 1.0f);
			const FSoundAttenuationSettings Broken = VoiceMath::MakeAttenuation(NAN, NAN, EAttenuationDistanceModel::Linear, 12.0f);
			TestEqual(TEXT("no inner"), static_cast<float>(Broken.AttenuationShapeExtents.X), 0.0f);
			TestEqual(TEXT("1 cm falloff"), Broken.FalloffDistance, 1.0f);
			TestEqual(TEXT("dB capped at 0"), Broken.dBAttenuationAtMax, 0.0f);
		});
	});

	Describe("Names", [this]()
	{
		It("trims a name and cuts a long one with an ellipsis", [this]()
		{
			TestEqual(TEXT("trimmed"), VoiceMath::DisplayName(TEXT("  Hina  "), 18), FString(TEXT("Hina")));
			const FString Cut = VoiceMath::DisplayName(TEXT("DESKTOP-ABCDEFGH-1234567890"), 10);
			TestEqual(TEXT("ten characters"), Cut.Len(), 10);
			TestTrue(TEXT("ellipsis"), Cut.EndsWith(FString(1, TEXT("…"))));
		});
	});
}

#endif
