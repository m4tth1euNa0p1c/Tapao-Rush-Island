// monke_island

#include "Capture/CapturePrisonerAnimInstance.h"
#include "Weapons/CutlassMath.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"

namespace
{
	/** One arm of the bound hands: shoulder, elbow and wrist turned so the wrist reaches Target, the elbow toward Pole;
	 *  the scale of the bones (the FBX root at x100) and the hand's turn on its forearm are kept */
	void SolveArm(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex Hand,
		const FVector& Target, const FVector& Pole, TArray<FBoneTransform, TInlineAllocator<6>>& OutSolved)
	{
		const FTransform UpperCS = Pose.GetComponentSpaceTransform(Upper);
		const FTransform LowerCS = Pose.GetComponentSpaceTransform(Lower);
		const FTransform HandCS = Pose.GetComponentSpaceTransform(Hand);
		const FVector Shoulder = UpperCS.GetLocation();
		const FVector Elbow = LowerCS.GetLocation();
		const FVector Wrist = HandCS.GetLocation();
		const float UpperLength = static_cast<float>(FVector::Dist(Shoulder, Elbow));
		const float LowerLength = static_cast<float>(FVector::Dist(Elbow, Wrist));
		if (UpperLength < 0.1f || LowerLength < 0.1f)
		{
			return;
		}
		const FVector NewElbow = CutlassMath::SolveElbow(Shoulder, Target, UpperLength, LowerLength, Pole);
		const FQuat UpperDelta = FQuat::FindBetweenNormals((Elbow - Shoulder).GetSafeNormal(), (NewElbow - Shoulder).GetSafeNormal());
		const FVector NewWrist = NewElbow + (Target - NewElbow).GetSafeNormal() * LowerLength;
		const FQuat LowerDelta = FQuat::FindBetweenNormals(UpperDelta.RotateVector(Wrist - Elbow).GetSafeNormal(), (NewWrist - NewElbow).GetSafeNormal());

		FTransform NewUpper = UpperCS;
		NewUpper.SetRotation(UpperDelta * UpperCS.GetRotation());
		FTransform NewLower = LowerCS;
		NewLower.SetLocation(NewElbow);
		NewLower.SetRotation(LowerDelta * UpperDelta * LowerCS.GetRotation());
		FTransform NewHand = HandCS;
		NewHand.SetLocation(NewWrist);
		NewHand.SetRotation(LowerDelta * UpperDelta * HandCS.GetRotation());
		OutSolved.Add(FBoneTransform(Upper, NewUpper));
		OutSolved.Add(FBoneTransform(Lower, NewLower));
		OutSolved.Add(FBoneTransform(Hand, NewHand));
	}
}

FAnimInstanceProxy* UCapturePrisonerAnimInstance::CreateAnimInstanceProxy()
{
	return new FCapturePrisonerAnimProxy(this);
}

void UCapturePrisonerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FCapturePrisonerAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FBeanLibraryAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UCapturePrisonerAnimInstance* Instance = Cast<UCapturePrisonerAnimInstance>(InAnimInstance))
	{
		Hands = Instance->GetBoundHands();
	}
}

bool FCapturePrisonerAnimProxy::Evaluate(FPoseContext& Output)
{
	FBeanLibraryAnimInstanceProxy::Evaluate(Output);
	BindHands(Output);
	return true;
}

void FCapturePrisonerAnimProxy::BindHands(FPoseContext& Output) const
{
	if (Hands.Alpha <= ZERO_ANIMWEIGHT_THRESH)
	{
		return;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	const auto Find = [&Bones, &RefSkeleton](FName Name)
	{
		const int32 MeshIndex = Name.IsNone() ? INDEX_NONE : RefSkeleton.FindBoneIndex(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	const FCompactPoseBoneIndex Pelvis = Find(Hands.Pelvis);
	const FCompactPoseBoneIndex Arms[2][3] = {
		{ Find(Hands.UpperArmL), Find(Hands.LowerArmL), Find(Hands.HandL) },
		{ Find(Hands.UpperArmR), Find(Hands.LowerArmR), Find(Hands.HandR) }
	};
	// a skeleton without these bones (or a level of detail without them) keeps the arms of its clips
	if (!Pelvis.IsValid())
	{
		return;
	}
	for (const auto& Arm : Arms)
	{
		if (!Arm[0].IsValid() || !Arm[1].IsValid() || !Arm[2].IsValid())
		{
			return;
		}
	}

	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);
	const FVector Middle = Pose.GetComponentSpaceTransform(Pelvis).GetLocation() + Hands.Forward * Hands.ForwardCm + Hands.Up * Hands.UpCm;
	TArray<FBoneTransform, TInlineAllocator<6>> Solved;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// left wrist on the left of the middle, right wrist on its right; the elbows bend out, down and a little back
		const float Sign = Side == 0 ? -1.0f : 1.0f;
		const FVector Target = Middle + Hands.Right * (Sign * Hands.GapCm * 0.5f);
		const FVector Pole = Hands.Right * Sign - Hands.Up * 0.6f - Hands.Forward * 0.4f;
		SolveArm(Pose, Arms[Side][0], Arms[Side][1], Arms[Side][2], Target, Pole, Solved);
	}
	if (Solved.IsEmpty())
	{
		return;
	}
	// parents before children, as the blend wants them
	Solved.Sort(FCompareBoneTransformIndex());
	Pose.LocalBlendCSBoneTransforms(Solved, FMath::Clamp(Hands.Alpha, 0.0f, 1.0f));
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
}
