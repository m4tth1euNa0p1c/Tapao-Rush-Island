// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "MachineGunCameraShake.generated.h"

/**
 *  Kick of the view at every round of the crank machine gun (T-273): a short jolt up and back, a little roll, gone
 *  before the next round (at 10 rounds a second the jolts make the shudder of the burst). Engine module only (the
 *  EngineCameras plugin patterns are not a dependency of monke_island).
 */
UCLASS()
class MONKE_ISLAND_API UMachineGunShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:

	UMachineGunShakePattern(const FObjectInitializer& ObjectInitializer);

	/** Length of one jolt (s) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0.01", Units="s"))
	float Duration = 0.09f;

	/** Peak pitch of the jolt (deg, the view jumps up then settles) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0", Units="deg"))
	float PitchAmplitude = 0.35f;

	/** Peak sideways shake (deg) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0", Units="deg"))
	float YawAmplitude = 0.12f;

	/** Peak roll (deg) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0", Units="deg"))
	float RollAmplitude = 0.45f;

	/** Push of the view backwards (cm) */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0", Units="cm"))
	float LocationAmplitude = 0.7f;

	/** Oscillations per second of the shudder */
	UPROPERTY(EditAnywhere, Category="Shake", meta=(ClampMin="0"))
	float Frequency = 32.0f;

private:

	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	/** Offsets of the view Time seconds into the jolt */
	void Evaluate(float Time, FCameraShakePatternUpdateResult& OutResult) const;

	/** Timing and blend of the jolt */
	FCameraShakeState State;

	/** Random side and phase so two rounds never shake alike */
	float Phase = 0.0f;
	float Side = 1.0f;
};

/** Camera shake of one round of the machine gun (default of UMachineGunRules::FireCameraShake); one instance at a time:
 *  every round restarts it */
UCLASS()
class MONKE_ISLAND_API UMachineGunFireCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UMachineGunFireCameraShake(const FObjectInitializer& ObjectInitializer);
};
