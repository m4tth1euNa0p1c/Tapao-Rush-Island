// monke_island

#include "Weapons/CutlassCameraShake.h"

UCutlassHitShakePattern::UCutlassHitShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCutlassHitShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(FMath::Max(Duration, 0.01f));
	OutInfo.BlendIn = 0.0f;
	OutInfo.BlendOut = 0.0f;
}

void UCutlassHitShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	State.Start(this, Params);
	Phase = FMath::FRandRange(0.0f, UE_TWO_PI);
}

void UCutlassHitShakePattern::Evaluate(float Time, FCameraShakePatternUpdateResult& OutResult) const
{
	// a decaying sine: strongest at the bite, gone at the end
	const float Decay = FMath::Square(1.0f - FMath::Clamp(Time / FMath::Max(Duration, 0.01f), 0.0f, 1.0f));
	const float Angle = UE_TWO_PI * Frequency * Time + Phase;
	OutResult.Rotation.Pitch = RotationAmplitude * Decay * FMath::Sin(Angle);
	OutResult.Rotation.Yaw = RotationAmplitude * 0.6f * Decay * FMath::Sin(Angle * 1.37f + 1.0f);
	OutResult.Location.X = -LocationAmplitude * Decay * FMath::Abs(FMath::Sin(Angle * 0.5f));
	OutResult.Location.Z = LocationAmplitude * 0.5f * Decay * FMath::Sin(Angle * 0.8f);
}

void UCutlassHitShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Weight = State.Update(Params.DeltaTime);
	Evaluate(State.GetElapsedTime(), OutResult);
	OutResult.ApplyScale(Weight);
}

void UCutlassHitShakePattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Weight = State.Scrub(Params.AbsoluteTime);
	Evaluate(Params.AbsoluteTime, OutResult);
	OutResult.ApplyScale(Weight);
}

bool UCutlassHitShakePattern::IsFinishedImpl() const
{
	return !State.IsPlaying();
}

void UCutlassHitShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	State.Stop(Params.bImmediately);
}

void UCutlassHitShakePattern::TeardownShakePatternImpl()
{
	State = FCameraShakeState();
}

UCutlassHitCameraShake::UCutlassHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UCutlassHitShakePattern>(TEXT("HitPattern")));
}
