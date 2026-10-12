// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "CutlassCameraShake.generated.h"

/**
 *  Short, decaying jolt of the view when the cutlass bites (T-152): a few fast oscillations in pitch and yaw, a little
 *  push of the camera. Engine module only (the EngineCameras plugin patterns are not a dependency of monke_island).
 */
UCLASS()
class MONKE_ISLAND_API UCutlassHitShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:

	UCutlassHitShakePattern(const FObjectInitializer& ObjectInitializer);

	/** Length of the shake (s) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0.01", Units="s"))
	float Duration = 0.14f;

	/** Peak rotation of the view (deg) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float RotationAmplitude = 0.8f;

	/** Peak displacement of the view (cm) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float LocationAmplitude = 1.2f;

	/** Oscillations per second */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float Frequency = 26.0f;

private:

	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	/** Offsets of the view Time seconds into the shake */
	void Evaluate(float Time, FCameraShakePatternUpdateResult& OutResult) const;

	/** Timing and blend of the shake */
	FCameraShakeState State;

	/** Random phase so two hits do not shake alike */
	float Phase = 0.0f;
};

/** Camera shake of a cutlass hit (default of UCutlassRules::HitCameraShake) */
UCLASS()
class MONKE_ISLAND_API UCutlassHitCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UCutlassHitCameraShake(const FObjectInitializer& ObjectInitializer);
};
