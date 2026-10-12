// monke_island

#include "Misc/AutomationTest.h"
#include "Stealth/StealthMath.h"
#include "Stealth/StealthRules.h"
#include <limits>

#if WITH_AUTOMATION_TESTS

namespace
{
	/** A look at a player DistanceCm away (bodies of 30 and 34 cm, the tiki and the bean), sight range 2500 cm */
	FStealthSight LookAtPlayer(float DistanceCm, bool bCrouched, bool bNight, bool bInBush = false)
	{
		FStealthSight Look;
		Look.DistanceCm = DistanceCm;
		Look.RangeCm = 2500.0f;
		Look.GapCm = StealthMath::BodyGap(DistanceCm, 30.0f, 34.0f);
		Look.bTargetIsPlayer = true;
		Look.bCrouched = bCrouched;
		Look.bNight = bNight;
		Look.bInBush = bInBush;
		return Look;
	}

	/** A noise of a player (or not) of RadiusCm at Location, made at Time */
	FStealthNoiseRecord Noise(const FVector& Location, float RadiusCm, double Time, bool bFromPlayer = true, EStealthNoise Kind = EStealthNoise::Footstep)
	{
		FStealthNoiseRecord Record;
		Record.Noise.Location = Location;
		Record.Noise.RadiusCm = RadiusCm;
		Record.Noise.Time = Time;
		Record.Noise.Kind = Kind;
		Record.bFromPlayer = bFromPlayer;
		return Record;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Stealth
BEGIN_DEFINE_SPEC(FStealthSpec, "monke_island.Stealth.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FStealthSightRules Sight;
END_DEFINE_SPEC(FStealthSpec)

void FStealthSpec::Define()
{
	BeforeEach([this]()
	{
		// the class defaults of DA_StealthRules: contact at a gap of 100 cm (about 1.5 m between a tiki and the bean),
		// 50 cm crouched in a bush
		Sight = FStealthSightRules();
	});

	Describe("Sight", [this]()
	{
		It("sees a standing player by day within its range, not beyond", [this]()
		{
			TestTrue(TEXT("20 m"), StealthMath::CanSee(LookAtPlayer(2000.0f, false, false), Sight));
			TestFalse(TEXT("30 m, beyond 25 m"), StealthMath::CanSee(LookAtPlayer(3000.0f, false, false), Sight));
			FStealthSight Unlimited = LookAtPlayer(30000.0f, false, false);
			Unlimited.RangeCm = 0.0f;
			TestTrue(TEXT("no range: no limit"), StealthMath::CanSee(Unlimited, Sight));
		});

		It("hides a crouched player day and night, unless at contact", [this]()
		{
			TestFalse(TEXT("crouched at 5 m by day"), StealthMath::CanSee(LookAtPlayer(500.0f, true, false), Sight));
			TestFalse(TEXT("crouched at 2 m by day"), StealthMath::CanSee(LookAtPlayer(200.0f, true, false), Sight));
			TestTrue(TEXT("crouched at 1.5 m: contact"), StealthMath::CanSee(LookAtPlayer(150.0f, true, false), Sight));
			TestFalse(TEXT("crouched at 5 m by night"), StealthMath::CanSee(LookAtPlayer(500.0f, true, true), Sight));
			TestTrue(TEXT("crouched at 1.5 m by night"), StealthMath::CanSee(LookAtPlayer(150.0f, true, true), Sight));
		});

		It("sees nobody at night but at contact: only the hearing works", [this]()
		{
			TestFalse(TEXT("standing at 5 m by night"), StealthMath::CanSee(LookAtPlayer(500.0f, false, true), Sight));
			TestTrue(TEXT("standing at 1.6 m by night"), StealthMath::CanSee(LookAtPlayer(160.0f, false, true), Sight));
			TestTrue(TEXT("standing at 5 m by day"), StealthMath::CanSee(LookAtPlayer(500.0f, false, false), Sight));
		});

		It("keeps the bush of T-207: crouched in a bush, the contact is closer still", [this]()
		{
			TestFalse(TEXT("in a bush at 1.5 m"), StealthMath::CanSee(LookAtPlayer(150.0f, true, false, true), Sight));
			TestTrue(TEXT("in a bush at 1.1 m"), StealthMath::CanSee(LookAtPlayer(110.0f, true, false, true), Sight));
			TestTrue(TEXT("standing in a bush by day: seen"), StealthMath::CanSee(LookAtPlayer(500.0f, false, false, true), Sight));
			// the crouch rule off, only the bush hides (the rule of T-207 alone)
			Sight.bCrouchHides = false;
			TestTrue(TEXT("crouched in the open, rule off"), StealthMath::CanSee(LookAtPlayer(500.0f, true, false), Sight));
			TestFalse(TEXT("crouched in a bush, rule off"), StealthMath::CanSee(LookAtPlayer(500.0f, true, false, true), Sight));
		});

		It("looks the bush up only when the answer depends on it", [this]()
		{
			TestFalse(TEXT("standing"), StealthMath::NeedsBushCheck(LookAtPlayer(150.0f, false, false), Sight));
			TestFalse(TEXT("crouched far: hidden anyway"), StealthMath::NeedsBushCheck(LookAtPlayer(500.0f, true, false), Sight));
			TestTrue(TEXT("crouched at 1.5 m"), StealthMath::NeedsBushCheck(LookAtPlayer(150.0f, true, false), Sight));
			TestFalse(TEXT("crouched at 1 m: seen anyway"), StealthMath::NeedsBushCheck(LookAtPlayer(100.0f, true, false), Sight));
		});

		It("leaves the mobs as they were: only the players hide", [this]()
		{
			FStealthSight Mob = LookAtPlayer(500.0f, true, true);
			Mob.bTargetIsPlayer = false;
			TestTrue(TEXT("a soldier at 5 m by night"), StealthMath::CanSee(Mob, Sight));
			Mob.DistanceCm = 3000.0f;
			TestFalse(TEXT("beyond the range"), StealthMath::CanSee(Mob, Sight));
		});

		It("measures the contact between the bodies, never below zero", [this]()
		{
			TestEqual(TEXT("gap"), StealthMath::BodyGap(164.0f, 30.0f, 34.0f), 100.0f);
			TestEqual(TEXT("overlap"), StealthMath::BodyGap(40.0f, 30.0f, 34.0f), 0.0f);
			// the contact grows with the body of the observer: a giant crab touches at 2.2 m, a tiki does not
			TestTrue(TEXT("crab at 2.2 m"), StealthMath::IsContact(StealthMath::BodyGap(220.0f, 95.0f, 34.0f), Sight.ContactGapCm));
			TestFalse(TEXT("tiki at 2.2 m"), StealthMath::IsContact(StealthMath::BodyGap(220.0f, 30.0f, 34.0f), Sight.ContactGapCm));
		});
	});

	Describe("Hearing", [this]()
	{
		It("hears a noise within its radius times the hearing scale", [this]()
		{
			TestTrue(TEXT("14 m of a 15 m step"), StealthMath::CanHear(1400.0f, 1500.0f, 1.0f));
			TestFalse(TEXT("14 m, hearing 0.8"), StealthMath::CanHear(1400.0f, 1500.0f, 0.8f));
			TestFalse(TEXT("deaf"), StealthMath::CanHear(10.0f, 1500.0f, 0.0f));
		});

		It("carries a run less far than a voice, a voice less than a shot, a shot less than a blast", [this]()
		{
			const FStealthHearingRules& Hearing = GetDefault<UStealthRules>()->Hearing;
			TestTrue(TEXT("run < voice"), Hearing.RadiusOf(EStealthNoise::Footstep) < Hearing.RadiusOf(EStealthNoise::Voice));
			TestTrue(TEXT("voice < shot"), Hearing.RadiusOf(EStealthNoise::Voice) < Hearing.RadiusOf(EStealthNoise::Gunshot));
			TestTrue(TEXT("shot < blast"), Hearing.RadiusOf(EStealthNoise::Gunshot) < Hearing.RadiusOf(EStealthNoise::Impact));
			TestTrue(TEXT("a listener thinking now and then hears it"), GetDefault<UStealthRules>()->ListenIntervalSeconds < Hearing.NoiseWindowSeconds);
		});

		It("bounds the radius a caller asks for", [this]()
		{
			TestEqual(TEXT("kept"), StealthMath::ClampNoiseRadius(1800.0f, 20000.0f), 1800.0f);
			TestEqual(TEXT("capped"), StealthMath::ClampNoiseRadius(1.0e9f, 20000.0f), 20000.0f);
			TestEqual(TEXT("negative"), StealthMath::ClampNoiseRadius(-5.0f, 20000.0f), 0.0f);
			TestEqual(TEXT("not a number"), StealthMath::ClampNoiseRadius(std::numeric_limits<float>::quiet_NaN(), 20000.0f), 0.0f);
		});

		It("picks the newest noise of a player in reach, the nearest of the same age", [this]()
		{
			const TArray<FStealthNoiseRecord> Noises = {
				Noise(FVector(1000, 0, 0), 1500.0f, 10.0),
				Noise(FVector(1200, 0, 0), 1500.0f, 10.5),
				Noise(FVector(400, 0, 0), 1500.0f, 10.5),
				Noise(FVector(5000, 0, 0), 1500.0f, 10.8) };
			TestEqual(TEXT("newest in reach, nearest of its age"), StealthMath::PickHeardNoise(Noises, FVector::ZeroVector, nullptr, 1.0f, 11.0, 1.0f), 2);
		});

		It("never hears the noises of the mobs", [this]()
		{
			const TArray<FStealthNoiseRecord> Noises = { Noise(FVector(100, 0, 0), 5000.0f, 10.0, false, EStealthNoise::Gunshot) };
			TestEqual(TEXT("a shot of a tiki"), StealthMath::PickHeardNoise(Noises, FVector::ZeroVector, nullptr, 1.0f, 10.1, 1.0f), INDEX_NONE);
		});

		It("lets a noise fade after its window", [this]()
		{
			const TArray<FStealthNoiseRecord> Noises = { Noise(FVector(100, 0, 0), 1500.0f, 10.0) };
			TestEqual(TEXT("fresh"), StealthMath::PickHeardNoise(Noises, FVector::ZeroVector, nullptr, 1.0f, 10.9, 1.0f), 0);
			TestEqual(TEXT("faded"), StealthMath::PickHeardNoise(Noises, FVector::ZeroVector, nullptr, 1.0f, 11.2, 1.0f), INDEX_NONE);
		});
	});

	Describe("Memory", [this]()
	{
		It("goes to a noise once, and forgets it after the memory", [this]()
		{
			FStealthLead Lead;
			FStealthNoise Step;
			Step.Location = FVector(300, 0, 0);
			Step.Time = 20.0;
			TestTrue(TEXT("taken"), Lead.TakeNoise(Step));
			TestFalse(TEXT("the same noise again"), Lead.TakeNoise(Step));
			TestFalse(TEXT("it walks to a noise"), Lead.bHunting);
			TestTrue(TEXT("active 9 s later"), Lead.IsActive(29.0, 10.0f));
			TestFalse(TEXT("forgotten 11 s later"), Lead.IsActive(31.0, 10.0f));
			TestTrue(TEXT("forgotten"), StealthMath::IsForgotten(20.0, 31.0, 10.0f));
		});

		It("keeps hunting a lost foe it hears again, and does not take a cleared noise back", [this]()
		{
			FStealthLead Lead;
			Lead.Hunt(nullptr, FVector(500, 0, 0), 40.0);
			TestTrue(TEXT("hunting"), Lead.bHunting);
			FStealthNoise Step;
			Step.Location = FVector(800, 0, 0);
			Step.Time = 41.0;
			TestTrue(TEXT("heard again"), Lead.TakeNoise(Step));
			TestTrue(TEXT("still a hunt"), Lead.bHunting);
			TestEqual(TEXT("to the new place"), Lead.Location, FVector(800, 0, 0));
			Lead.Clear();
			TestFalse(TEXT("cleared"), Lead.IsActive(41.0, 10.0f));
			TestFalse(TEXT("the cleared noise stays taken"), Lead.TakeNoise(Step));
		});
	});

	Describe("Exposure", [this]()
	{
		It("tells the HUD: running is heard, crouched or at night unseen, otherwise seen", [this]()
		{
			TestTrue(TEXT("running"), StealthMath::Exposure(false, false, true, Sight) == EStealthExposure::Noisy);
			TestTrue(TEXT("crouched"), StealthMath::Exposure(true, false, false, Sight) == EStealthExposure::Hidden);
			TestTrue(TEXT("night"), StealthMath::Exposure(false, true, false, Sight) == EStealthExposure::Hidden);
			TestTrue(TEXT("day"), StealthMath::Exposure(false, false, false, Sight) == EStealthExposure::Visible);
		});
	});
}

#endif
