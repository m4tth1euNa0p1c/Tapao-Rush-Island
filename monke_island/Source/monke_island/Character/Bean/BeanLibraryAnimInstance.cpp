// monke_island

#include "Character/Bean/BeanLibraryAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"

FAnimInstanceProxy* UBeanLibraryAnimInstance::CreateAnimInstanceProxy()
{
	return new FBeanLibraryAnimInstanceProxy(this);
}

void UBeanLibraryAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FBeanLibraryAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UBeanLibraryAnimInstance* Instance = Cast<UBeanLibraryAnimInstance>(InAnimInstance))
	{
		Frame = Instance->GetFrame();
	}
}

void FBeanLibraryAnimInstanceProxy::Sample(const FBeanAnimSample& Clip, FPoseContext& Out) const
{
	FAnimationPoseData PoseData(Out);
	const double Length = Clip.Sequence->GetPlayLength();
	double Time = Clip.Time;
	if (Clip.bLoop && Length > 0.0)
	{
		Time = FMath::Fmod(Time, Length);
		Time = Time < 0.0 ? Time + Length : Time;
	}
	else
	{
		Time = FMath::Clamp(Time, 0.0, Length);
	}
	Clip.Sequence->GetAnimationPose(PoseData, FAnimExtractContext(Time, false, {}, Clip.bLoop));
}

void FBeanLibraryAnimInstanceProxy::EvaluateGroup(const TArray<FBeanAnimSample>& Samples, FPoseContext& Output) const
{
	Output.ResetToRefPose();
	float Accumulated = 0.0f;
	for (const FBeanAnimSample& Clip : Samples)
	{
		if (!Clip.Sequence || Clip.Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (Accumulated <= 0.0f)
		{
			Sample(Clip, Output);
			Accumulated = Clip.Weight;
			continue;
		}
		FPoseContext Other(Output);
		Sample(Clip, Other);
		// running weighted average: the new clip takes its share of the weights seen so far
		Accumulated += Clip.Weight;
		const float Alpha = Clip.Weight / Accumulated;
		for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
		{
			Output.Pose[Bone].BlendWith(Other.Pose[Bone], Alpha);
		}
	}
}

bool FBeanLibraryAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	EvaluateGroup(Frame.Base, Output);

	// one shot of the whole body (pickup, hit, emote) over the base
	if (Frame.FullBody.Sequence && Frame.FullBody.Weight > KINDA_SMALL_NUMBER)
	{
		FPoseContext Shot(Output);
		Sample(Frame.FullBody, Shot);
		for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
		{
			Output.Pose[Bone].BlendWith(Shot.Pose[Bone], FMath::Clamp(Frame.FullBody.Weight, 0.0f, 1.0f));
		}
	}

	// upper body layer: the root bone and all its children
	if (Frame.UpperWeight > KINDA_SMALL_NUMBER && !Frame.Upper.IsEmpty())
	{
		FPoseContext Upper(Output);
		EvaluateGroup(Frame.Upper, Upper);
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
		const int32 RootIndex = RefSkeleton.FindBoneIndex(Frame.UpperBodyRoot);
		if (RootIndex != INDEX_NONE)
		{
			const float Alpha = FMath::Clamp(Frame.UpperWeight, 0.0f, 1.0f);
			// the pelvis itself (not the legs under it) also takes the upper clip: the run played up to three times
			// faster bounces the pelvis, which would shake the whole trunk, the arms and the weapon
			const int32 PelvisIndex = RefSkeleton.GetParentIndex(RootIndex);
			for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				const int32 MeshIndex = Bones.MakeMeshPoseIndex(Bone).GetInt();
				if (MeshIndex == RootIndex || MeshIndex == PelvisIndex || RefSkeleton.BoneIsChildOf(MeshIndex, RootIndex))
				{
					Output.Pose[Bone].BlendWith(Upper.Pose[Bone], Alpha);
				}
			}
		}
	}
	Output.Pose.NormalizeRotations();
	return true;
}
