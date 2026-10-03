// monke_island

#include "Interaction/IslanderGrabComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IslanderInteractable.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderMovementComponent.h"
#include "monke_island.h"

UIslanderGrabComponent::UIslanderGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UIslanderGrabComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UIslanderGrabComponent, HeldComponent);
	DOREPLIFETIME_CONDITION(UIslanderGrabComponent, HeldMass, COND_OwnerOnly);
}

void UIslanderGrabComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		Handle = NewObject<UPhysicsHandleComponent>(GetOwner(), TEXT("GrabHandle"));
		Handle->bSoftAngularConstraint = true;
		Handle->bSoftLinearConstraint = true;
		Handle->RegisterComponent();
	}
}

void UIslanderGrabComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// cahier §5: the object is released once when its holder leaves
	if (GetOwner()->HasAuthority() && HeldComponent)
	{
		Release(false);
	}
	Super::EndPlay(EndPlayReason);
}

ACharacter* UIslanderGrabComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

float UIslanderGrabComponent::ComputeStrength(float MassKg, int32 Holders, float InLiftMassKg, float MinFraction)
{
	const float Capacity = InLiftMassKg * FMath::Max(1, Holders);
	return MassKg <= Capacity ? 1.0f : FMath::Max(MinFraction, Capacity / MassKg);
}

void UIslanderGrabComponent::GetEyes(FVector& OutLocation, FVector& OutDirection) const
{
	const ACharacter* Character = GetCharacter();
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character);
	OutLocation = Islander && Islander->GetFirstPersonCamera() ? Islander->GetFirstPersonCamera()->GetComponentLocation()
		: Character->GetPawnViewLocation();
	const FRotator View = Character->GetController() ? Character->GetController()->GetControlRotation() : Character->GetBaseAimRotation();
	OutDirection = View.Vector();
}

UPrimitiveComponent* UIslanderGrabComponent::FindGrabTarget(FVector& OutGrabPoint) const
{
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderGrab), false, GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Eyes + Direction * GrabRange, ECC_Visibility, Params))
	{
		return nullptr;
	}

	UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component || !Component->IsSimulatingPhysics() || Cast<APawn>(Hit.GetActor()))
	{
		return nullptr;
	}
	OutGrabPoint = Hit.ImpactPoint;
	return Component;
}

int32 UIslanderGrabComponent::CountHolders(const UPrimitiveComponent* Component) const
{
	int32 Count = 0;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (const UIslanderGrabComponent* Grab = It->FindComponentByClass<UIslanderGrabComponent>())
		{
			Count += Grab->HeldComponent == Component ? 1 : 0;
		}
	}
	return Count;
}

bool UIslanderGrabComponent::FindInteractionTarget(AActor*& OutInteractable, UPrimitiveComponent*& OutPhysics, FVector& OutPoint) const
{
	OutInteractable = nullptr;
	OutPhysics = nullptr;

	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);

	// seated / attached (boat): the actor the pawn rides comes first, e.g. "Se lever"
	APawn* Rider = Cast<APawn>(GetOwner());
	AActor* Ride = Rider ? Rider->GetAttachParentActor() : nullptr;
	if (Ride && Ride->Implements<UIslanderInteractable>() && IIslanderInteractable::Execute_CanInteract(Ride, Rider))
	{
		OutInteractable = Ride;
		OutPoint = Eyes;
		return true;
	}

	// aim along the active camera so the target is what the crosshair shows, in first and third person
	FVector ViewLocation = Eyes;
	FRotator ViewRotation = Direction.Rotation();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const float Length = static_cast<float>(FVector::Dist(ViewLocation, Eyes)) + GrabRange;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderInteract), false, GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Length, ECC_Visibility, Params))
	{
		return false;
	}
	// the reach is measured from the eyes (cahier §4: 200 cm)
	if (FVector::Dist(Eyes, Hit.ImpactPoint) > GrabRange)
	{
		return false;
	}

	OutPoint = Hit.ImpactPoint;
	AActor* Actor = Hit.GetActor();
	if (Actor && Actor->Implements<UIslanderInteractable>() && IIslanderInteractable::Execute_CanInteract(Actor, const_cast<APawn*>(Pawn)))
	{
		OutInteractable = Actor;
		return true;
	}
	UPrimitiveComponent* Component = Hit.GetComponent();
	if (Component && Component->IsSimulatingPhysics() && !Cast<APawn>(Actor))
	{
		OutPhysics = Component;
		return true;
	}
	return false;
}

void UIslanderGrabComponent::RequestInteract()
{
	if (HeldComponent)
	{
		RequestRelease();
		return;
	}

	AActor* Interactable = nullptr;
	UPrimitiveComponent* Physics = nullptr;
	FVector Point;
	if (!FindInteractionTarget(Interactable, Physics, Point))
	{
		return;
	}
	if (Interactable)
	{
		if (GetOwner()->HasAuthority())
		{
			ServerInteract_Implementation(Interactable, Point);
		}
		else
		{
			ServerInteract(Interactable, Point);
		}
	}
	else if (Physics)
	{
		if (GetOwner()->HasAuthority())
		{
			Grab(Physics, Point);
		}
		else
		{
			ServerGrab(Physics, Point);
		}
	}
}

void UIslanderGrabComponent::ServerInteract_Implementation(AActor* Target, FVector_NetQuantize Point)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Target || !Pawn || !Target->Implements<UIslanderInteractable>() || !IIslanderInteractable::Execute_CanInteract(Target, Pawn))
	{
		return;
	}

	// the actor the pawn rides is always in reach
	if (Target == Pawn->GetAttachParentActor())
	{
		IIslanderInteractable::Execute_Interact(Target, Pawn);
		return;
	}

	// server re-check (cahier §4): range from the eyes with a latency tolerance, and line of sight to the target
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	if (FVector::Dist(Eyes, Point) > GrabRange + 50.0f)
	{
		return;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderInteractCheck), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, FVector(Point) + (FVector(Point) - Eyes).GetSafeNormal() * 10.0f, ECC_Visibility, Params)
		&& Hit.GetActor() != Target)
	{
		return;
	}
	IIslanderInteractable::Execute_Interact(Target, Pawn);
}

void UIslanderGrabComponent::RequestGrab()
{
	if (HeldComponent)
	{
		return;
	}
	FVector GrabPoint;
	if (UPrimitiveComponent* Target = FindGrabTarget(GrabPoint))
	{
		if (GetOwner()->HasAuthority())
		{
			Grab(Target, GrabPoint);
		}
		else
		{
			ServerGrab(Target, GrabPoint);
		}
	}
}

void UIslanderGrabComponent::ServerGrab_Implementation(UPrimitiveComponent* Target, FVector_NetQuantize GrabPoint)
{
	Grab(Target, GrabPoint);
}

void UIslanderGrabComponent::Grab(UPrimitiveComponent* Target, const FVector& GrabPoint)
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetCharacter());
	if (!Handle || HeldComponent || !Target || !Target->IsSimulatingPhysics() || Cast<APawn>(Target->GetOwner())
		|| (Islander && Islander->IsRagdoll()))
	{
		return;
	}

	// server re-check (cahier §5): range with a small tolerance for latency, line of sight, mass, single carrier
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	if (FVector::Dist(Eyes, GrabPoint) > GrabRange + 50.0f)
	{
		return;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderGrabCheck), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, GrabPoint + (GrabPoint - Eyes).GetSafeNormal() * 10.0f, ECC_Visibility, Params)
		&& Hit.GetComponent() != Target)
	{
		return;
	}
	const float Mass = Target->GetMass();
	if (Mass > MaxGrabMassKg || (!bAllowSharedGrab && CountHolders(Target) > 0))
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s cannot grab %s (%.0f kg)"), *GetNameSafe(GetOwner()), *GetNameSafe(Target->GetOwner()), Mass);
		return;
	}

	HeldComponent = Target;
	HoldDistance = FMath::Clamp(static_cast<float>(FVector::Dist(Eyes, GrabPoint)), MinHoldDistance, MaxHoldDistance);
	const FQuat ViewYaw = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f).Quaternion();
	RotationOffset = ViewYaw.Inverse() * Target->GetComponentQuat();
	Handle->GrabComponentAtLocationWithRotation(Target, NAME_None, GrabPoint, Target->GetComponentRotation());
	SetComponentTickEnabled(true);
	UE_LOG(Logmonke_island, Log, TEXT("%s grabs %s (%.0f kg)"), *GetNameSafe(GetOwner()), *GetNameSafe(Target->GetOwner()), Mass);
}

void UIslanderGrabComponent::RequestRelease()
{
	if (!HeldComponent)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		Release(false);
	}
	else
	{
		ServerRelease();
	}
}

void UIslanderGrabComponent::ServerRelease_Implementation()
{
	Release(false);
}

void UIslanderGrabComponent::RequestThrow()
{
	if (!HeldComponent)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		Release(true);
	}
	else
	{
		ServerThrow();
	}
}

void UIslanderGrabComponent::ServerThrow_Implementation()
{
	Release(true);
}

void UIslanderGrabComponent::Release(bool bThrow)
{
	UPrimitiveComponent* Component = HeldComponent;
	if (!Component)
	{
		return;
	}

	if (Handle)
	{
		Handle->ReleaseComponent();
	}
	HeldComponent = nullptr;
	HeldMass = 0.0f;
	ApplyCarriedWeight();
	SetComponentTickEnabled(false);

	if (bThrow && Component->IsSimulatingPhysics())
	{
		FVector Eyes, Direction;
		GetEyes(Eyes, Direction);
		const float Strength = ComputeStrength(Component->GetMass(), CountHolders(Component) + 1, LiftMassKg, MinStrengthFraction);
		Component->AddImpulse(Direction * ThrowSpeed * Strength, NAME_None, true);
	}
}

void UIslanderGrabComponent::AdjustHoldDistance(float Notches)
{
	if (!HeldComponent || FMath::IsNearlyZero(Notches))
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		HoldDistance = FMath::Clamp(HoldDistance + Notches * WheelStep, MinHoldDistance, MaxHoldDistance);
	}
	else
	{
		ServerAdjustHoldDistance(Notches);
	}
}

void UIslanderGrabComponent::ServerAdjustHoldDistance_Implementation(float Notches)
{
	HoldDistance = FMath::Clamp(HoldDistance + FMath::Clamp(Notches, -3.0f, 3.0f) * WheelStep, MinHoldDistance, MaxHoldDistance);
}

void UIslanderGrabComponent::OnRep_HeldMass()
{
	ApplyCarriedWeight();
}

void UIslanderGrabComponent::ApplyCarriedWeight()
{
	if (const ACharacter* Character = GetCharacter())
	{
		if (UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()))
		{
			Move->SetCarriedWeight(HeldMass);
		}
	}
}

void UIslanderGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority() || !Handle)
	{
		return;
	}

	// destroyed, broken, asleep for good or knocked out of reach: let go once
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetCharacter());
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	if (!IsValid(HeldComponent) || !HeldComponent->IsSimulatingPhysics() || (Islander && Islander->IsRagdoll())
		|| FVector::Dist(Eyes, HeldComponent->GetComponentLocation()) > GrabRange * 2.0f)
	{
		Release(false);
		return;
	}

	// strength shared with the other holders; heavy objects sag and drag
	const int32 Holders = CountHolders(HeldComponent);
	const float Mass = HeldComponent->GetMass();
	const float Strength = ComputeStrength(Mass, Holders, LiftMassKg, MinStrengthFraction);
	Handle->SetLinearStiffness(LinearStiffness * Strength);
	Handle->SetLinearDamping(LinearDamping * Strength);
	Handle->SetAngularStiffness(AngularStiffness * Strength);
	Handle->SetAngularDamping(AngularDamping * Strength);

	const FQuat ViewYaw = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f).Quaternion();
	Handle->SetTargetLocationAndRotation(Eyes + Direction * HoldDistance, (ViewYaw * RotationOffset).Rotator());

	// the holder carries its share of the weight (slows it, costs stamina)
	const float Share = Mass / FMath::Max(1, Holders);
	if (!FMath::IsNearlyEqual(Share, HeldMass, 0.5f))
	{
		HeldMass = Share;
		ApplyCarriedWeight();
	}
}
