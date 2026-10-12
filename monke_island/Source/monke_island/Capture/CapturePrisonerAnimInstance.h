// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Character/Bean/BeanLibraryAnimInstance.h"
#include "CapturePrisonerAnimInstance.generated.h"

/**
 *  Bound hands of a prisoner without captive clips for its skeleton (the Veilleurs, T-336): after its own clips both
 *  hands are pulled together in front of its belly by a two bone IK of each arm, in the mesh's component space.
 *  Decided on the game thread every frame (the directions of the body in component space), read by the proxy.
 */
struct FCaptureBoundHands
{
	/** Weight of the pose (0: the arms of the clips) */
	float Alpha = 0.0f;

	/** Forward, up and right of the body in the mesh's component space */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	FVector Up = FVector(0.0, 0.0, 1.0);
	FVector Right = FVector(1.0, 0.0, 0.0);

	/** Hands from the pelvis: forward, up, and the gap between the wrists (component space units) */
	float ForwardCm = 22.0f;
	float UpCm = 10.0f;
	float GapCm = 7.0f;

	/** Bones (the bean and the mob skeletons share these names) */
	FName Pelvis;
	FName UpperArmL;
	FName LowerArmL;
	FName HandL;
	FName UpperArmR;
	FName LowerArmR;
	FName HandR;
};

/** Proxy: the bean library pose (base clips, one shot, upper layer), then the bound hands (worker thread) */
struct FCapturePrisonerAnimProxy : public FBeanLibraryAnimInstanceProxy
{
	FCapturePrisonerAnimProxy() = default;
	explicit FCapturePrisonerAnimProxy(UAnimInstance* Instance) : FBeanLibraryAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	/** Pulls both hands together after the clips */
	void BindHands(FPoseContext& Output) const;

	FCaptureBoundHands Hands;
};

/**
 *  Anim instance of a prisoner (ACapturePrisoner, T-336): the native bean library instance (no graph: the prisoner sets
 *  the clips of each frame, its captive or own idle and walk) with the bound hands on top when its clips lack them.
 */
UCLASS(Transient, NotBlueprintable)
class MONKE_ISLAND_API UCapturePrisonerAnimInstance : public UBeanLibraryAnimInstance
{
	GENERATED_BODY()

public:

	/** Game thread: the bound hands of this frame */
	void SetBoundHands(const FCaptureBoundHands& InHands) { Hands = InHands; }

	/** Read by the proxy */
	const FCaptureBoundHands& GetBoundHands() const { return Hands; }

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:

	FCaptureBoundHands Hands;
};
