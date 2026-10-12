// monke_island

#include "Creatures/TikiAnimInstance.h"
#include "Creatures/PoseRoot.h"
#include "Creatures/TikiMath.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"

void UTikiAnimInstance::PlayClip(UAnimSequenceBase* Clip, bool bLoop, float BlendTime, float PlayRate, bool bRestart, float StartTime)
{
	if (!Clip || (Clip == In.Clip && !bRestart))
	{
		In.bLoop = bLoop;
		In.PlayRate = PlayRate;
		return;
	}
	// what was coming in goes out from where it is; a crossfade cut short keeps its weight
	if (In.Clip && BlendTime > 0.0f)
	{
		Out = In;
		OutClip = InClip;
		Alpha = 0.0f;
		BlendRate = 1.0f / BlendTime;
	}
	else
	{
		Out = FTikiClipState();
		OutClip = nullptr;
		Alpha = 1.0f;
		BlendRate = 0.0f;
	}
	In.Clip = Clip;
	In.Time = FMath::Clamp(StartTime, 0.0f, Clip->GetPlayLength());
	In.PlayRate = PlayRate;
	In.bLoop = bLoop;
	InClip = Clip;
}

void UTikiAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	In.Time = TikiMath::AdvanceClip(In.Time, DeltaSeconds * In.PlayRate, In.Clip ? In.Clip->GetPlayLength() : 0.0f, In.bLoop);
	if (Out.Clip)
	{
		Out.Time = TikiMath::AdvanceClip(Out.Time, DeltaSeconds * Out.PlayRate, Out.Clip->GetPlayLength(), Out.bLoop);
		Alpha = FMath::Min(1.0f, Alpha + DeltaSeconds * BlendRate);
		if (Alpha >= 1.0f)
		{
			Out = FTikiClipState();
			OutClip = nullptr;
		}
	}
}

FAnimInstanceProxy* UTikiAnimInstance::CreateAnimInstanceProxy()
{
	return new FTikiAnimInstanceProxy(this);
}

void UTikiAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FTikiAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UTikiAnimInstance* Instance = Cast<UTikiAnimInstance>(InAnimInstance))
	{
		In = Instance->In;
		Out = Instance->Out;
		Alpha = Instance->Alpha;
	}
}

bool FTikiAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!In.Clip)
	{
		Output.ResetToRefPose();
		return true;
	}
	FAnimationPoseData InData(Output);
	In.Clip->GetAnimationPose(InData, FAnimExtractContext(static_cast<double>(In.Time), false, {}, In.bLoop));
	if (Out.Clip && Alpha < 1.0f)
	{
		FPoseContext OutPose(Output);
		FAnimationPoseData OutData(OutPose);
		Out.Clip->GetAnimationPose(OutData, FAnimExtractContext(static_cast<double>(Out.Time), false, {}, Out.bLoop));
		FAnimationRuntime::BlendTwoPosesTogetherInPlace(InData, OutData, Alpha);
	}
	// the root keeps the scale of the mesh: the GoreBox clips carry a root at 1 (owner 2026-10-08)
	PoseRoot::KeepReferenceScale(Output.Pose);
	return true;
}
