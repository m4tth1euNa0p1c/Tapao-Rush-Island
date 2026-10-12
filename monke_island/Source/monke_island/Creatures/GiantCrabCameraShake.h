// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GiantCrabCameraShake.generated.h"

/**
 *  Tremor of the ground under the giant crab (T-261): a rumble of a few mixed frequencies that swells in BuildUp and
 *  fades to the end of Duration, pitch, roll and a little up and down push of the view. Engine module only (the
 *  EngineCameras plugin patterns are not a dependency of monke_island).
 */
UCLASS()
class MONKE_ISLAND_API UGiantCrabTremorPattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:

	UGiantCrabTremorPattern(const FObjectInitializer& ObjectInitializer);

	/** Length of the tremor (s) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0.01", Units="s"))
	float Duration = 0.5f;

	/** Time it takes to swell to its peak (s) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0", Units="s"))
	float BuildUp = 0.0f;

	/** Peak rotation of the view (deg) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float RotationAmplitude = 1.0f;

	/** Peak displacement of the view (cm) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float LocationAmplitude = 3.0f;

	/** Main oscillations per second */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float Frequency = 14.0f;

private:

	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	/** Offsets of the view Time seconds into the tremor */
	void Evaluate(float Time, FCameraShakePatternUpdateResult& OutResult) const;

	/** Timing and blend of the shake */
	FCameraShakeState State;

	/** Random phases so two tremors do not shake alike */
	FVector Phases = FVector::ZeroVector;
};

/** A heavy leg on the sand: a short thud */
UCLASS()
class MONKE_ISLAND_API UGiantCrabStepShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UGiantCrabStepShake(const FObjectInitializer& ObjectInitializer);
};

/** A slam, a stomp, the body dropped on the sand: a hard jolt */
UCLASS()
class MONKE_ISLAND_API UGiantCrabImpactShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UGiantCrabImpactShake(const FObjectInitializer& ObjectInitializer);
};

/** The roar: a long rumble that swells */
UCLASS()
class MONKE_ISLAND_API UGiantCrabRoarShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UGiantCrabRoarShake(const FObjectInitializer& ObjectInitializer);
};
