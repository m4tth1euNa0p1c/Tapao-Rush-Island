// monke_island

#include "Creatures/GiantCrabCameraShake.h"

UGiantCrabTremorPattern::UGiantCrabTremorPattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGiantCrabTremorPattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(FMath::Max(Duration, 0.01f));
	OutInfo.BlendIn = 0.0f;
	OutInfo.BlendOut = 0.0f;
}

void UGiantCrabTremorPattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	State.Start(this, Params);
	Phases = FVector(FMath::FRandRange(0.0f, UE_TWO_PI), FMath::FRandRange(0.0f, UE_TWO_PI), FMath::FRandRange(0.0f, UE_TWO_PI));
}

void UGiantCrabTremorPattern::Evaluate(float Time, FCameraShakePatternUpdateResult& OutResult) const
{
	// swells over BuildUp, then fades out to the end; three frequencies that do not line up: a rumble, not a buzz
	const float Length = FMath::Max(Duration, 0.01f);
	const float Swell = BuildUp > 0.0f ? FMath::Clamp(Time / BuildUp, 0.0f, 1.0f) : 1.0f;
	const float Fade = FMath::Square(1.0f - FMath::Clamp((Time - BuildUp) / FMath::Max(Length - BuildUp, 0.01f), 0.0f, 1.0f));
	const float Weight = Swell * Fade;
	const float Angle = UE_TWO_PI * Frequency * Time;
	const float A = FMath::Sin(Angle + static_cast<float>(Phases.X));
	const float B = FMath::Sin(Angle * 1.618f + static_cast<float>(Phases.Y));
	const float C = FMath::Sin(Angle * 0.53f + static_cast<float>(Phases.Z));
	OutResult.Rotation.Pitch = RotationAmplitude * Weight * (0.6f * A + 0.4f * C);
	OutResult.Rotation.Roll = RotationAmplitude * 0.5f * Weight * B;
	OutResult.Rotation.Yaw = RotationAmplitude * 0.25f * Weight * C;
	OutResult.Location.Z = LocationAmplitude * Weight * (0.7f * B + 0.3f * A);
	OutResult.Location.Y = LocationAmplitude * 0.4f * Weight * C;
}

void UGiantCrabTremorPattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Weight = State.Update(Params.DeltaTime);
	Evaluate(State.GetElapsedTime(), OutResult);
	OutResult.ApplyScale(Weight);
}

void UGiantCrabTremorPattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Weight = State.Scrub(Params.AbsoluteTime);
	Evaluate(Params.AbsoluteTime, OutResult);
	OutResult.ApplyScale(Weight);
}

bool UGiantCrabTremorPattern::IsFinishedImpl() const
{
	return !State.IsPlaying();
}

void UGiantCrabTremorPattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	State.Stop(Params.bImmediately);
}

void UGiantCrabTremorPattern::TeardownShakePatternImpl()
{
	State = FCameraShakeState();
}

namespace
{
	UGiantCrabTremorPattern* CrabMakeTremor(UCameraShakeBase* Shake, float Duration, float BuildUp, float Rotation, float Location, float Frequency)
	{
		UGiantCrabTremorPattern* Pattern = Shake->CreateDefaultSubobject<UGiantCrabTremorPattern>(TEXT("Tremor"));
		Pattern->Duration = Duration;
		Pattern->BuildUp = BuildUp;
		Pattern->RotationAmplitude = Rotation;
		Pattern->LocationAmplitude = Location;
		Pattern->Frequency = Frequency;
		return Pattern;
	}
}

UGiantCrabStepShake::UGiantCrabStepShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CrabMakeTremor(this, 0.18f, 0.0f, 0.35f, 1.2f, 21.0f));
}

UGiantCrabImpactShake::UGiantCrabImpactShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRootShakePattern(CrabMakeTremor(this, 0.7f, 0.0f, 1.8f, 6.0f, 13.0f));
}

UGiantCrabRoarShake::UGiantCrabRoarShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CrabMakeTremor(this, 2.4f, 0.35f, 0.9f, 2.5f, 9.0f));
}
