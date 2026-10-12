// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "TikiAnimInstance.generated.h"

class UAnimSequenceBase;

/** One clip of the crossfade: where it is, how fast it runs */
struct FTikiClipState
{
	/** The clip, none when nothing plays */
	UAnimSequenceBase* Clip = nullptr;

	/** Position in the clip (s) */
	float Time = 0.0f;

	/** Speed of the clip (1 = as authored) */
	float PlayRate = 1.0f;

	/** Loops, else holds its last frame */
	bool bLoop = true;
};

/** Evaluates the crossfade of the clip going out and the clip coming in (worker thread) */
struct FTikiAnimInstanceProxy : public FAnimInstanceProxy
{
	FTikiAnimInstanceProxy() = default;

	explicit FTikiAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;

	virtual bool Evaluate(FPoseContext& Output) override;

	/** Copies of the instance state for this frame */
	FTikiClipState In;
	FTikiClipState Out;

	/** Weight of the clip coming in */
	float Alpha = 1.0f;
};

/**
 *  Animation of the tiki creatures without an AnimBP: one clip at a time, as before, but every change of clip is a
 *  crossfade (walk to run, idle to a blow...) and the locomotion clips follow the speed of the body. Replaces the
 *  single node mode, whose clip changes popped.
 */
UCLASS(Transient, NotBlueprintable)
class MONKE_ISLAND_API UTikiAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	friend struct FTikiAnimInstanceProxy;

public:

	/** Crossfades to Clip over BlendTime (s), from StartTime in the clip (s); nothing happens when it is already the
	 *  clip coming in, unless bRestart */
	void PlayClip(UAnimSequenceBase* Clip, bool bLoop, float BlendTime, float PlayRate = 1.0f, bool bRestart = false, float StartTime = 0.0f);

	/** Speed of the clip coming in (the locomotion follows the body) */
	void SetPlayRate(float PlayRate) { In.PlayRate = PlayRate; }

	/** Clip coming in, none before the first one */
	UAnimSequenceBase* GetClip() const { return In.Clip; }

protected:

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:

	/** Clips referenced while they play (garbage collection) */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> InClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> OutClip;

	/** Clip coming in, clip going out */
	FTikiClipState In;
	FTikiClipState Out;

	/** Weight of the clip coming in, and how fast it grows (1/s) */
	float Alpha = 1.0f;
	float BlendRate = 0.0f;
};
