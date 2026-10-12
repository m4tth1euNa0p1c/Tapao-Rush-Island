// monke_island

#include "Capture/CaptiveComponent.h"
#include "Capture/CaptureMath.h"
#include "Capture/CapturePrisoner.h"
#include "Capture/CaptureRules.h"
#include "Capture/HandcuffsItem.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Water/IslandWaterSubsystem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** The ground ahead of a step is looked for this far in front of the capsule (cm) */
	constexpr float WaterProbeAheadCm = 40.0f;

	/** ... and this far above and below the feet (cm) */
	constexpr float WaterProbeUpCm = 150.0f;
	constexpr float WaterProbeDownCm = 400.0f;
}

UCaptiveComponent::UCaptiveComponent()
{
	// the prisoner's own tick drives it (one tick for the pose, the steps and the chain)
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCaptiveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCaptiveComponent, State);
	DOREPLIFETIME(UCaptiveComponent, Jailer);
}

void UCaptiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnStateChanged.Clear();
	OnCageReached.Clear();
	LinkTransforms.Reset();
	Super::EndPlay(EndPlayReason);
}

ACapturePrisoner* UCaptiveComponent::GetPrisoner() const
{
	return Cast<ACapturePrisoner>(GetOwner());
}

const UCaptureRules* UCaptiveComponent::GetRules() const
{
	const ACapturePrisoner* Prisoner = GetPrisoner();
	return Prisoner ? Prisoner->Rules.Get() : nullptr;
}

void UCaptiveComponent::OnRep_State()
{
	OnStateChanged.Broadcast(State);
}

void UCaptiveComponent::ApplyState(ECaptiveState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	// the server hears its own change (clients through OnRep_State)
	OnStateChanged.Broadcast(State);
}

void UCaptiveComponent::SetHandcuffs(AHandcuffsItem* InHandcuffs)
{
	Handcuffs = InHandcuffs;
}

AHandcuffsItem* UCaptiveComponent::GetHandcuffs() const
{
	return Handcuffs.Get();
}

void UCaptiveComponent::SetLed(APawn* InJailer)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(InJailer))
	{
		return;
	}
	Jailer = InJailer;
	NextRepathTime = 0.0;
	bPathing = false;
	ApplyState(ECaptiveState::Led);
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: led by %s"), *GetNameSafe(GetOwner()), *GetNameSafe(InJailer));
}

void UCaptiveComponent::Abandon(ECaptiveRelease Reason)
{
	ACapturePrisoner* Prisoner = GetPrisoner();
	if (!Prisoner || !Prisoner->HasAuthority() || State != ECaptiveState::Led)
	{
		return;
	}
	StopMoving();
	APawn* Former = Jailer;
	Jailer = nullptr;
	ApplyState(ECaptiveState::Abandoned);
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: let go by %s (%s)"), *GetNameSafe(Prisoner), *GetNameSafe(Former), *UEnum::GetValueAsString(Reason));
	Prisoner->OnAbandoned(Former, Reason);
}

void UCaptiveComponent::WalkIntoCage(const FVector& Entry, const FVector& Inside, float InsideYaw)
{
	ACapturePrisoner* Prisoner = GetPrisoner();
	if (!Prisoner || !Prisoner->HasAuthority())
	{
		return;
	}
	StopMoving();
	Jailer = nullptr;
	CagePoints[0] = Entry;
	CagePoints[1] = Inside;
	// already past the entry (it stands in front of the door): straight in
	CagePoint = FVector::Dist2D(Prisoner->GetActorLocation(), Entry) < 30.0f ? 1 : 0;
	CageWalkStart = GetWorld()->GetTimeSeconds();
	CageYaw = InsideYaw;
	// moved along the points by the server: no gravity, no collision against the bars it walks through
	if (UCharacterMovementComponent* Move = Prisoner->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_None);
	}
	ApplyState(ECaptiveState::EnteringCage);
}

void UCaptiveComponent::SetCaged()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ApplyState(ECaptiveState::Caged);
	}
}

void UCaptiveComponent::StopMoving()
{
	ACapturePrisoner* Prisoner = GetPrisoner();
	if (!Prisoner)
	{
		return;
	}
	if (bPathing)
	{
		if (AAIController* AI = Cast<AAIController>(Prisoner->GetController()))
		{
			AI->StopMovement();
		}
	}
	bPathing = false;
	if (UCharacterMovementComponent* Move = Prisoner->GetCharacterMovement(); Move && Move->IsMovingOnGround())
	{
		Move->StopActiveMovement();
	}
}

bool UCaptiveComponent::IsDryAhead(const FVector& Direction) const
{
	const ACapturePrisoner* Prisoner = GetPrisoner();
	const UCaptureRules* Rules = GetRules();
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Prisoner || !Rules || !Water)
	{
		return true;
	}
	const UCapsuleComponent* Capsule = Prisoner->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 30.0f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 55.0f;
	const FVector Feet = Prisoner->GetActorLocation() - FVector(0.0, 0.0, HalfHeight) + Direction * (Radius + WaterProbeAheadCm);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CaptiveWaterProbe), false, Prisoner);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Feet + FVector(0.0, 0.0, WaterProbeUpCm), Feet - FVector(0.0, 0.0, WaterProbeDownCm),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		// no ground found: never stopped by a missing collision
		return true;
	}
	return Water->GetSeaLevel() - static_cast<float>(Hit.ImpactPoint.Z) <= Rules->MaxWadeDepthCm;
}

void UCaptiveComponent::ServerUpdate(float DeltaSeconds)
{
	const UCaptureRules* Rules = GetRules();
	if (!Rules || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (State == ECaptiveState::EnteringCage)
	{
		StepIntoCage(DeltaSeconds);
		return;
	}
	if (State != ECaptiveState::Led)
	{
		return;
	}
	// the chain holds while its jailer lives, walks the land and stays near
	APawn* Current = Jailer;
	CaptureMath::FLeadFacts Facts;
	Facts.bJailerValid = IsValid(Current);
	if (Facts.bJailerValid)
	{
		const UIslanderHealthComponent* Health = Current->FindComponentByClass<UIslanderHealthComponent>();
		const ACharacter* Character = Cast<ACharacter>(Current);
		const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
		Facts.bJailerDead = Health && Health->IsDead();
		Facts.bJailerSwimming = Move && Move->IsSwimming();
		Facts.Distance = static_cast<float>(FVector::Dist(Current->GetActorLocation(), GetOwner()->GetActorLocation()));
	}
	const ECaptiveRelease Release = CaptureMath::JudgeLead(Facts, Rules->ReleaseDistanceCm);
	if (Release != ECaptiveRelease::None)
	{
		Abandon(Release);
		return;
	}
	Follow(Current, DeltaSeconds);
}

void UCaptiveComponent::Follow(APawn* InJailer, float DeltaSeconds)
{
	ACapturePrisoner* Prisoner = GetPrisoner();
	const UCaptureRules* Rules = GetRules();
	UCharacterMovementComponent* Move = Prisoner ? Prisoner->GetCharacterMovement() : nullptr;
	if (!Prisoner || !Rules || !Move || !InJailer)
	{
		return;
	}
	const FVector Here = Prisoner->GetActorLocation();
	const FVector There = InJailer->GetActorLocation();
	const float Distance = static_cast<float>(FVector::Dist2D(Here, There));
	float Speed = CaptureMath::FollowSpeed(Distance, Rules->FollowDistanceCm, Rules->ChainLengthCm, static_cast<float>(InJailer->GetVelocity().Size2D()),
		Rules->FollowWalkSpeed, Rules->FollowMaxSpeed);
	if (Prisoner->IsCrawling())
	{
		Speed = FMath::Min(Speed, Rules->CrawlFollowSpeed);
	}
	const FVector Direction = (There - Here).GetSafeNormal2D();
	// close enough, or the sea ahead: it waits (the jailer who swims away lets it go)
	if (Speed <= 0.0f || Direction.IsNearlyZero() || !IsDryAhead(Direction))
	{
		StopMoving();
		return;
	}
	Move->MaxWalkSpeed = Speed;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextRepathTime)
	{
		// a path on the navigation mesh when there is one (the fort), straight steps otherwise (the beach, the flat)
		NextRepathTime = Now + Rules->FollowRepathSeconds;
		AAIController* AI = Cast<AAIController>(Prisoner->GetController());
		const EPathFollowingRequestResult::Type Result = AI ? AI->MoveToLocation(There, Rules->FollowDistanceCm * 0.8f, true, true, true, false, nullptr, true)
			: EPathFollowingRequestResult::Failed;
		bPathing = Result == EPathFollowingRequestResult::RequestSuccessful;
	}
	if (!bPathing)
	{
		Prisoner->AddMovementInput(Direction, 1.0f, true);
	}
}

void UCaptiveComponent::StepIntoCage(float DeltaSeconds)
{
	ACapturePrisoner* Prisoner = GetPrisoner();
	const UCaptureRules* Rules = GetRules();
	if (!Prisoner || !Rules)
	{
		return;
	}
	const UCapsuleComponent* Capsule = Prisoner->GetCapsuleComponent();
	const FVector Lift(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 55.0f);
	UCharacterMovementComponent* Move = Prisoner->GetCharacterMovement();
	const bool bLate = GetWorld()->GetTimeSeconds() - CageWalkStart > Rules->CageEntryMaxSeconds;
	const FVector Here = Prisoner->GetActorLocation();
	const FVector Goal = CagePoints[FMath::Clamp(CagePoint, 0, 1)] + Lift;
	const FVector Delta = Goal - Here;
	const float Step = Rules->CageEntrySpeed * FMath::Max(DeltaSeconds, 0.0f);
	if (bLate || Delta.Size() <= Step)
	{
		// at the point (or out of time: put inside at once)
		if (bLate || CagePoint >= 1)
		{
			Prisoner->SetActorLocationAndRotation(CagePoints[1] + Lift, FRotator(0.0f, CageYaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			if (Move)
			{
				Move->Velocity = FVector::ZeroVector;
			}
			OnCageReached.Broadcast();
			return;
		}
		Prisoner->SetActorLocation(Goal, false, nullptr, ETeleportType::None);
		++CagePoint;
		return;
	}
	const FVector Direction = Delta.GetSafeNormal();
	Prisoner->SetActorLocationAndRotation(Here + Direction * Step, FRotator(0.0f, Direction.Rotation().Yaw, 0.0f), false, nullptr, ETeleportType::None);
	if (Move)
	{
		// the pose and the other machines read the pace from the velocity
		Move->Velocity = Direction * Rules->CageEntrySpeed;
	}
}

FVector UCaptiveComponent::GetJailerChainEnd(const APawn* InJailer) const
{
	const UCaptureRules* Rules = GetRules();
	if (!InJailer || !Rules)
	{
		return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(InJailer))
	{
		// its own player in first person sees the chain come into the view (the body's hand is below it)
		const UCameraComponent* Camera = Islander->GetFirstPersonCamera();
		if (Rules->bFirstPersonChainToView && Camera && Islander->IsLocallyControlled() && Islander->IsFirstPerson())
		{
			return Camera->GetComponentLocation() + Camera->GetComponentQuat().RotateVector(Rules->FirstPersonChainOffset);
		}
		// the body of the jailer, looked for once per jailer (not every frame)
		if (JailerPuppetOwner.Get() != Islander)
		{
			JailerPuppetOwner = Islander;
			JailerPuppet = Islander->FindComponentByClass<UBeanPuppetComponent>();
		}
		const UBeanPuppetComponent* Puppet = JailerPuppet.Get();
		if (Puppet && Puppet->GetBoneIndex(Rules->JailerChainBone) != INDEX_NONE)
		{
			// the middle of the mitten: the offset turned with the bone (its scale, the FBX's x100, left out)
			const FTransform Bone = Puppet->GetSocketTransform(Rules->JailerChainBone, RTS_World);
			return Bone.GetLocation() + Bone.GetRotation().RotateVector(Rules->JailerChainOffset);
		}
	}
	const ACharacter* Character = Cast<ACharacter>(InJailer);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh && Mesh->GetBoneIndex(Rules->JailerChainBone) != INDEX_NONE)
	{
		const FTransform Bone = Mesh->GetSocketTransform(Rules->JailerChainBone, RTS_World);
		return Bone.GetLocation() + Bone.GetRotation().RotateVector(Rules->JailerChainOffset);
	}
	return InJailer->GetActorLocation();
}

void UCaptiveComponent::UpdateChain(UInstancedStaticMeshComponent* Links, const FVector& Anchor)
{
	if (!Links)
	{
		return;
	}
	const UCaptureRules* Rules = GetRules();
	const APawn* Current = GetJailer();
	if (!Rules || !Current || !Rules->ChainLinkMesh)
	{
		if (Links->GetInstanceCount() > 0)
		{
			Links->ClearInstances();
		}
		LinkTransforms.Reset();
		return;
	}
	const FVector End = GetJailerChainEnd(Current);
	const float Distance = static_cast<float>(FVector::Dist(Anchor, End));
	const float Sag = CaptureMath::ChainSag(Distance, Rules->ChainLengthCm, Rules->ChainMinSagCm);
	// as many links as the chain has (its length, at rest): pulled taut they spread over the distance
	const int32 Count = CaptureMath::ChainLinkCount(Rules->ChainLengthCm, Rules->ChainMinSagCm, Rules->ChainLinkPitchCm, Rules->MaxChainLinks);
	LinkTransforms.SetNum(Count, EAllowShrinking::No);
	const FVector Straight = (End - Anchor).GetSafeNormal();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = (static_cast<float>(Index) + 0.5f) / static_cast<float>(Count);
		const float HalfStep = 0.5f / static_cast<float>(Count);
		const FVector Point = CaptureMath::ChainPoint(Anchor, End, Alpha, Sag);
		FVector Tangent = CaptureMath::ChainPoint(Anchor, End, FMath::Min(Alpha + HalfStep, 1.0f), Sag)
			- CaptureMath::ChainPoint(Anchor, End, FMath::Max(Alpha - HalfStep, 0.0f), Sag);
		if (!Tangent.Normalize())
		{
			Tangent = Straight.IsNearlyZero() ? FVector::ForwardVector : Straight;
		}
		// long axis along the chain, every other link turned a quarter about it
		FQuat Rotation = FRotationMatrix::MakeFromXZ(Tangent, FVector::UpVector).ToQuat();
		if (Index % 2 == 1)
		{
			Rotation = Rotation * FQuat(FVector::XAxisVector, UE_HALF_PI);
		}
		LinkTransforms[Index] = FTransform(Rotation, Point, Rules->ChainLinkScale);
	}
	if (Links->GetInstanceCount() != Count)
	{
		Links->ClearInstances();
		Links->AddInstances(LinkTransforms, false, true, false);
		return;
	}
	Links->BatchUpdateInstancesTransforms(0, LinkTransforms, true, true, false);
}
