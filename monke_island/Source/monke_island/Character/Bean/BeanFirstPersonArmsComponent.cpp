// monke_island

#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderMovementComponent.h"

UBeanFirstPersonArmsComponent::UBeanFirstPersonArmsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UBeanFirstPersonArmsComponent::BeginPlay()
{
	Super::BeginPlay();

	AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	if (!Islander || !ArmMesh || !Islander->GetFirstPersonCamera())
	{
		SetComponentTickEnabled(false);
		return;
	}

	// follow the first person camera (and its view rotation)
	AttachToComponent(Islander->GetFirstPersonCamera(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		UStaticMeshComponent* Arm = NewObject<UStaticMeshComponent>(GetOwner(), Side == 0 ? TEXT("FPArmRight") : TEXT("FPArmLeft"));
		Arm->SetStaticMesh(ArmMesh);
		Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arm->SetOnlyOwnerSee(true);
		Arm->SetCastShadow(false);
		Arm->SetupAttachment(this);
		Arm->RegisterComponent();
		// the left arm is the right one mirrored across the view
		Arm->SetRelativeScale3D(FVector(1.0, Side == 0 ? 1.0 : -1.0, 1.0));
		Arm->SetVisibility(false);
		(Side == 0 ? RightArm : LeftArm) = Arm;
	}
}

void UBeanFirstPersonArmsComponent::SetPoseOverride(const FBeanArmPose& Right, const FBeanArmPose& Left)
{
	bPoseOverride = true;
	OverrideRight = Right;
	OverrideLeft = Left;
}

void UBeanFirstPersonArmsComponent::ClearPoseOverride()
{
	bPoseOverride = false;
}

void UBeanFirstPersonArmsComponent::UpdateArm(UStaticMeshComponent* Arm, const FBeanArmPose& Target, float Mirror, float Alpha, float DeltaTime)
{
	const FVector Location(Target.Location.X, Target.Location.Y * Mirror, Target.Location.Z);
	const FRotator Rotation(Target.Rotation.Pitch, Target.Rotation.Yaw * Mirror, Target.Rotation.Roll * Mirror);
	Arm->SetRelativeLocation(FMath::VInterpTo(Arm->GetRelativeLocation(), Location, DeltaTime, BlendSpeed * Alpha));
	Arm->SetRelativeRotation(FMath::RInterpTo(Arm->GetRelativeRotation(), Rotation, DeltaTime, BlendSpeed * Alpha));
}

void UBeanFirstPersonArmsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	if (!Islander || !RightArm || !LeftArm)
	{
		return;
	}

	// only for the local player in first person, standing (knocked out = third person)
	const bool bShow = Islander->IsLocallyControlled() && Islander->IsFirstPerson() && !Islander->IsRagdoll() && !bHiddenByItem;
	if (RightArm->IsVisible() != bShow)
	{
		RightArm->SetVisibility(bShow);
		LeftArm->SetVisibility(bShow);
		if (bShow)
		{
			// appear from below the view
			RightArm->SetRelativeLocationAndRotation(RestPose.Location, RestPose.Rotation);
			LeftArm->SetRelativeLocationAndRotation(FVector(RestPose.Location.X, -RestPose.Location.Y, RestPose.Location.Z),
				FRotator(RestPose.Rotation.Pitch, -RestPose.Rotation.Yaw, -RestPose.Rotation.Roll));
		}
	}
	if (!bShow)
	{
		return;
	}

	// an equipped item (musket) blends its own poses: apply them as they are
	if (bPoseOverride)
	{
		RightArm->SetRelativeLocationAndRotation(OverrideRight.Location, OverrideRight.Rotation);
		LeftArm->SetRelativeLocationAndRotation(FVector(OverrideLeft.Location.X, -OverrideLeft.Location.Y, OverrideLeft.Location.Z),
			FRotator(OverrideLeft.Rotation.Pitch, -OverrideLeft.Rotation.Yaw, -OverrideLeft.Rotation.Roll));
		RightArm->SetRelativeScale3D(FVector(OverrideRight.Stretch, 1.0, 1.0));
		LeftArm->SetRelativeScale3D(FVector(OverrideLeft.Stretch, -1.0, 1.0));
		return;
	}
	if (RightArm->GetRelativeScale3D().X != 1.0 || LeftArm->GetRelativeScale3D().X != 1.0)
	{
		// back from an item that stretched them
		RightArm->SetRelativeScale3D(FVector(1.0, 1.0, 1.0));
		LeftArm->SetRelativeScale3D(FVector(1.0, -1.0, 1.0));
	}

	const UIslanderCombatComponent* Combat = Islander->GetCombatComponent();
	const UIslanderGrabComponent* Grab = Islander->FindComponentByClass<UIslanderGrabComponent>();
	const UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(Islander->GetCharacterMovement());
	const EIslanderAttackPhase Phase = Combat ? Combat->GetAttackPhase() : EIslanderAttackPhase::Idle;
	const bool bGuard = Combat && Combat->IsGuarding();
	const bool bHolding = Grab && Grab->IsHolding();
	const bool bSprinting = Move && Move->IsSprinting();

	// sprint pumping, arms in opposite phase
	FBeanArmPose RightTarget = RestPose;
	FBeanArmPose LeftTarget = RestPose;
	if (bSprinting)
	{
		PumpTime += DeltaTime;
		const float Pump = FMath::Sin(PumpTime * SprintPumpFrequency * UE_TWO_PI) * SprintPumpAmplitude;
		RightTarget = SprintPose;
		LeftTarget = SprintPose;
		RightTarget.Location.X += Pump;
		LeftTarget.Location.X -= Pump;
	}
	if (bHolding)
	{
		RightTarget = HoldPose;
		LeftTarget = HoldPose;
	}
	if (bGuard)
	{
		RightTarget = GuardPose;
		LeftTarget = GuardPose;
	}

	// the punching arm overrides everything; the strike snaps forward
	float RightSpeed = 1.0f;
	switch (Phase)
	{
	case EIslanderAttackPhase::Windup:
		RightTarget = WindupPose;
		break;
	case EIslanderAttackPhase::Strike:
		RightTarget = StrikePose;
		RightSpeed = 3.0f;
		break;
	default:
		break;
	}

	UpdateArm(RightArm, RightTarget, 1.0f, RightSpeed, DeltaTime);
	UpdateArm(LeftArm, LeftTarget, -1.0f, 1.0f, DeltaTime);
}
