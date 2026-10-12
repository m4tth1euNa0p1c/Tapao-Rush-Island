// monke_island

#include "Weapons/MachineGunCameraShake.h"

UMachineGunShakePattern::UMachineGunShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMachineGunShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(FMath::Max(Duration, 0.01f));
	OutInfo.BlendIn = 0.0f;
	OutInfo.BlendOut = 0.0f;
}

void UMachineGunShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	State.Start(this, Params);
	Phase = FMath::FRandRange(0.0f, UE_TWO_PI);
	Side = FMath::RandBool() ? 1.0f : -1.0f;
}

void UMachineGunShakePattern::Evaluate(float Time, FCameraShakePatternUpdateResult& OutResult) const
{
	// the round kicks at once, then the shudder dies before the next one
	const float Alpha = FMath::Clamp(Time / FMath::Max(Duration, 0.01f), 0.0f, 1.0f);
	const float Decay = FMath::Square(1.0f - Alpha);
	const float Angle = UE_TWO_PI * Frequency * Time + Phase;
	// mostly up: the jump of the muzzle, a smaller wobble around it
	OutResult.Rotation.Pitch = PitchAmplitude * Decay * (0.65f + 0.35f * FMath::Sin(Angle));
	OutResult.Rotation.Yaw = Side * YawAmplitude * Decay * FMath::Sin(Angle * 1.31f + 0.7f);
	OutResult.Rotation.Roll = Side * RollAmplitude * Decay * FMath::Sin(Angle * 0.87f + 1.3f);
	OutResult.Location.X = -LocationAmplitude * Decay;
	OutResult.Location.Z = LocationAmplitude * 0.3f * Decay * FMath::Sin(Angle * 0.6f);
}

void UMachineGunShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Weight = State.Update(Params.DeltaTime);
	Evaluate(State.GetElapsedTime(), OutResult);
	OutResult.ApplyScale(Weight);
}

void UMachineGunShakePattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Weight = State.Scrub(Params.AbsoluteTime);
	Evaluate(Params.AbsoluteTime, OutResult);
	OutResult.ApplyScale(Weight);
}

bool UMachineGunShakePattern::IsFinishedImpl() const
{
	return !State.IsPlaying();
}

void UMachineGunShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	State.Stop(Params.bImmediately);
}

void UMachineGunShakePattern::TeardownShakePatternImpl()
{
	State = FCameraShakeState();
}

UMachineGunFireCameraShake::UMachineGunFireCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UMachineGunShakePattern>(TEXT("RoundPattern")));
}
