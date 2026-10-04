// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BeanLibraryAnimInstance.generated.h"

class UAnimSequence;

/** One clip sampled at a time, with its weight in its group */
struct FBeanAnimSample
{
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.0f;
	float Weight = 0.0f;
	bool bLoop = false;
};

/** Pose of one frame, decided on the game thread by UBeanAnimDriverComponent: weighted clips for the whole body,
 *  an optional one shot over it, and an upper body layer from UpperBodyRoot */
struct FBeanAnimFrame
{
	TArray<FBeanAnimSample> Base;
	FBeanAnimSample FullBody;
	TArray<FBeanAnimSample> Upper;
	float UpperWeight = 0.0f;
	FName UpperBodyRoot = NAME_None;
};

/** Proxy: samples and blends the clips of the frame (worker thread), no anim graph */
struct FBeanLibraryAnimInstanceProxy : public FAnimInstanceProxy
{
	FBeanLibraryAnimInstanceProxy() = default;
	explicit FBeanLibraryAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	/** Weighted average of Samples into Output (reference pose when the weights are all zero) */
	void EvaluateGroup(const TArray<FBeanAnimSample>& Samples, FPoseContext& Output) const;

	/** Samples one clip into Out */
	void Sample(const FBeanAnimSample& Clip, FPoseContext& Out) const;

	FBeanAnimFrame Frame;
};

/**
 *  Native anim instance of the hidden SK_Bean_Body that plays the Bean animation library (T-078): it has no graph,
 *  the proxy builds the pose from the frame the driver component sets every tick.
 */
UCLASS(Transient, NotBlueprintable)
class MONKE_ISLAND_API UBeanLibraryAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** Game thread: the pose to play this frame */
	void SetFrame(const FBeanAnimFrame& InFrame) { Frame = InFrame; }

	/** Frame read by the proxy */
	const FBeanAnimFrame& GetFrame() const { return Frame; }

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:

	FBeanAnimFrame Frame;
};
