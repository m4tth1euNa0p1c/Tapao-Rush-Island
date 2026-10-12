// monke_island

#include "Interaction/IslanderGrabComponent.h"
#include "Engine/CollisionProfile.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IslanderInteractable.h"
#include "Props/LootCrate/LootContainer.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "TimerManager.h"
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
	GetWorld()->GetTimerManager().ClearTimer(InteractHoldTimer);
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

void UIslanderGrabComponent::GetHoldAim(FVector& OutEyes, FVector& OutDirection, float& OutYaw) const
{
	FVector Look;
	GetEyes(OutEyes, Look);
	const ACharacter* Character = GetCharacter();
	const FRotator View = Character->GetController() ? Character->GetController()->GetControlRotation() : Character->GetBaseAimRotation();
	OutYaw = View.Yaw;
	const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(View.Pitch), -MaxHoldPitchDownDeg, 75.0f);
	OutDirection = FRotator(Pitch, View.Yaw, 0.0f).Vector();
}

const FName UIslanderGrabComponent::NoGrabTag(TEXT("NoGrab"));

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
	if (!Component || !Component->IsSimulatingPhysics() || Cast<APawn>(Hit.GetActor())
		|| (Hit.GetActor() && Hit.GetActor()->ActorHasTag(NoGrabTag)))
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

bool UIslanderGrabComponent::ConsumeRequest(const TCHAR* What)
{
	// one bucket per player for every interaction request: a flood at 1 000 per second made the host trace, use, lift
	// and let go as often (security review T-269 M1)
	const UWorld* World = GetWorld();
	if (!World || RequestLimiter.Consume(World->GetTimeSeconds(), RequestBurst, RequestsPerSecond))
	{
		return true;
	}
	UE_LOG(Logmonke_island, Verbose, TEXT("%s: %s refused, more than %.0f requests at once (%d refused so far)"), *GetNameSafe(GetOwner()),
		What, RequestBurst, RequestLimiter.Refused);
	return false;
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
	if (Component && Component->IsSimulatingPhysics() && !Cast<APawn>(Actor)
		&& !(Actor && Actor->ActorHasTag(NoGrabTag)))
	{
		OutPhysics = Component;
		return true;
	}
	return FindAssistedInteractable(Eyes, Hit.ImpactPoint, OutInteractable, OutPoint);
}

bool UIslanderGrabComponent::FindAssistedInteractable(const FVector& Eyes, const FVector& AimPoint, AActor*& OutInteractable, FVector& OutPoint) const
{
	// a small object on the ground (a pistol on its flank) is hard to hit with a ray: the crosshair close to it is enough
	if (InteractionAssistRadius <= 0.0f)
	{
		return false;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderInteractAssist), false, GetOwner());
	if (!GetWorld()->OverlapMultiByChannel(Overlaps, AimPoint, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(InteractionAssistRadius), Params))
	{
		return false;
	}
	AActor* Best = nullptr;
	FVector BestPoint = FVector::ZeroVector;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Candidate || !Component || Candidate == Best || Cast<APawn>(Candidate) || !Candidate->Implements<UIslanderInteractable>())
		{
			continue;
		}
		const FVector Center = Component->Bounds.Origin;
		const double Distance = FVector::DistSquared(Center, AimPoint);
		if (Distance >= BestDistance || FVector::Dist(Eyes, Center) > GrabRange
			|| !IIslanderInteractable::Execute_CanInteract(Candidate, Pawn))
		{
			continue;
		}
		// in sight from the eyes (the server checks the same line)
		FHitResult Sight;
		if (GetWorld()->LineTraceSingleByChannel(Sight, Eyes, Center + (Center - Eyes).GetSafeNormal() * 10.0f, ECC_Visibility, Params)
			&& Sight.GetActor() != Candidate)
		{
			continue;
		}
		Best = Candidate;
		BestPoint = Center;
		BestDistance = Distance;
	}
	if (!Best)
	{
		return false;
	}
	OutInteractable = Best;
	OutPoint = BestPoint;
	return true;
}

void UIslanderGrabComponent::PressInteractKey()
{
	if (HeldComponent)
	{
		// F again lets go at once, or puts the object in the container aimed at (T-258)
		bInteractKeyPending = false;
		ReleaseOrStore();
		return;
	}
	if (HoldToGrabSeconds <= 0.0f)
	{
		bInteractKeyPending = false;
		RequestInteract();
		return;
	}
	bInteractKeyPending = true;
	GetWorld()->GetTimerManager().SetTimer(InteractHoldTimer, this, &UIslanderGrabComponent::HandleInteractHold, HoldToGrabSeconds, false);
}

void UIslanderGrabComponent::ReleaseInteractKey()
{
	GetWorld()->GetTimerManager().ClearTimer(InteractHoldTimer);
	if (bInteractKeyPending)
	{
		// a tap: the interaction of the target in view (pick up an item, sit in the boat), else lift a plain prop
		bInteractKeyPending = false;
		RequestInteract();
	}
}

void UIslanderGrabComponent::HandleInteractHold()
{
	if (!bInteractKeyPending)
	{
		return;
	}
	bInteractKeyPending = false;
	FVector Point;
	if (FindGrabTarget(Point))
	{
		// held: lift the physics object, even an item lying on the ground
		RequestGrab();
	}
	else
	{
		// nothing to lift (the boat seat, a door): a long press still interacts
		RequestInteract();
	}
}

void UIslanderGrabComponent::RequestInteract()
{
	if (HeldComponent)
	{
		ReleaseOrStore();
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
			InteractWith(Interactable, Point);
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

// a reach is measured to the target itself, never to a point a client sends: a point in the air near the eyes passed for a
// target across the map (security review T-269 C1, network review T-268 R-06)
bool UIslanderGrabComponent::IsActorInReach(const UWorld* World, const AActor* Target, const AActor* Self, const AActor* AlsoIgnored,
	const FVector& Eyes, float Reach)
{
	if (!World || !Target)
	{
		return false;
	}
	const FBox Bounds = Target->GetComponentsBoundingBox(true);
	if (!Bounds.IsValid || Bounds.ComputeSquaredDistanceToPoint(Eyes) > FMath::Square(Reach))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderReachCheck), false, Self);
	if (AlsoIgnored)
	{
		Params.AddIgnoredActor(AlsoIgnored);
	}
	return !World->LineTraceSingleByChannel(Hit, Eyes, Bounds.GetCenter(), ECC_Visibility, Params) || Hit.GetActor() == Target;
}

void UIslanderGrabComponent::ServerInteract_Implementation(AActor* Target, FVector_NetQuantize Point)
{
	if (ConsumeRequest(TEXT("ServerInteract")))
	{
		InteractWith(Target, Point);
	}
}

void UIslanderGrabComponent::InteractWith(AActor* Target, const FVector& Point)
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

	// server re-check (cahier §4): the target itself within reach of the eyes (latency tolerance) and in sight; the
	// client's point only says where it looked (T-289)
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	if (!IsActorInReach(GetWorld(), Target, GetOwner(), nullptr, Eyes, GrabRange + ServerReachTolerance))
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("%s: interaction with %s refused (out of reach or out of sight)"), *GetNameSafe(GetOwner()), *GetNameSafe(Target));
		return;
	}
	IIslanderInteractable::Execute_Interact(Target, Pawn);
}

bool UIslanderGrabComponent::FindStoreTarget(AActor*& OutContainer, FVector& OutPoint) const
{
	OutContainer = nullptr;
	AActor* Item = HeldComponent ? HeldComponent->GetOwner() : nullptr;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Item || !Pawn)
	{
		return false;
	}

	// along the active camera like the interaction (what the crosshair shows), through the object held in front
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	FVector ViewLocation = Eyes;
	FRotator ViewRotation = Direction.Rotation();
	if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const float Length = static_cast<float>(FVector::Dist(ViewLocation, Eyes)) + GrabRange;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderStore), false, GetOwner());
	Params.AddIgnoredActor(Item);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Length, ECC_Visibility, Params)
		|| FVector::Dist(Eyes, Hit.ImpactPoint) > GrabRange)
	{
		return false;
	}
	const ILootContainer* Container = Cast<ILootContainer>(Hit.GetActor());
	if (!Container || !Container->CanStoreItem(Pawn, Item))
	{
		return false;
	}
	OutContainer = Hit.GetActor();
	OutPoint = Hit.ImpactPoint;
	return true;
}

void UIslanderGrabComponent::ReleaseOrStore()
{
	AActor* Container = nullptr;
	FVector Point;
	if (!FindStoreTarget(Container, Point))
	{
		RequestRelease();
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		StoreHeld(Container, Point);
	}
	else
	{
		ServerStoreHeld(Container, Point);
	}
}

void UIslanderGrabComponent::ServerStoreHeld_Implementation(AActor* Container, FVector_NetQuantize Point)
{
	if (ConsumeRequest(TEXT("ServerStoreHeld")))
	{
		StoreHeld(Container, Point);
	}
}

void UIslanderGrabComponent::StoreHeld(AActor* Container, const FVector& Point)
{
	UPrimitiveComponent* Held = HeldComponent;
	AActor* Item = Held ? Held->GetOwner() : nullptr;
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Item || !Pawn)
	{
		return;
	}

	// server re-check (cahier §4): only what this player holds, into something else, within reach from the eyes (latency
	// tolerance), the container in sight (the held object aside), and room for the object now
	ILootContainer* Target = Cast<ILootContainer>(Container);
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	// the container itself within reach and in sight, the held object aside; never the client's point alone (T-289)
	const bool bValid = Target && Container != Item
		&& IsActorInReach(GetWorld(), Container, GetOwner(), Item, Eyes, GrabRange + ServerReachTolerance)
		&& Target->CanStoreItem(Pawn, Item);
	if (!bValid)
	{
		// refused (out of reach, full since the client looked): F lets go, as without a container
		Release(false);
		return;
	}

	// out of every pair of hands first (shared grab), then inside
	Release(false);
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		UIslanderGrabComponent* Other = It->FindComponentByClass<UIslanderGrabComponent>();
		if (Other && Other != this && Other->HeldComponent == Held)
		{
			Other->Release(false);
		}
	}
	if (!Target->StoreItem(Pawn, Item))
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: %s refused %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Container), *GetNameSafe(Item));
	}
}

bool UIslanderGrabComponent::GrabFromInteraction(UPrimitiveComponent* Target, const FVector& GrabPoint)
{
	if (!GetOwner()->HasAuthority() || HeldComponent || !Target)
	{
		return false;
	}
	Grab(Target, GrabPoint);
	return HeldComponent == Target;
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
	if (ConsumeRequest(TEXT("ServerGrab")))
	{
		Grab(Target, GrabPoint);
	}
}

void UIslanderGrabComponent::Grab(UPrimitiveComponent* Target, const FVector& GrabPoint)
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetCharacter());
	if (!Handle || HeldComponent || !Target || !Target->IsSimulatingPhysics() || Cast<APawn>(Target->GetOwner())
		|| (Islander && Islander->IsRagdoll()))
	{
		return;
	}
	// server re-check: a client cannot grab what the rules forbid
	if (Target->GetOwner() && Target->GetOwner()->ActorHasTag(NoGrabTag))
	{
		return;
	}

	// server re-check (cahier §5): range with a small tolerance for latency, line of sight, mass, single carrier; the
	// grab point sent by a client must lie on the target, and the target itself within reach (T-289, review R-06)
	FVector Eyes, Direction;
	GetEyes(Eyes, Direction);
	const FBox TargetBox = Target->Bounds.GetBox();
	if (FVector::Dist(Eyes, GrabPoint) > GrabRange + ServerReachTolerance
		|| TargetBox.ComputeSquaredDistanceToPoint(GrabPoint) > FMath::Square(ServerReachTolerance)
		|| TargetBox.ComputeSquaredDistanceToPoint(Eyes) > FMath::Square(GrabRange + ServerReachTolerance))
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
	SetHeldGhost(Target, true);
	HoldDistance = FMath::Clamp(static_cast<float>(FVector::Dist(Eyes, GrabPoint)), MinHoldDistance, MaxHoldDistance);
	FVector AimEyes, AimDirection;
	float Yaw = 0.0f;
	GetHoldAim(AimEyes, AimDirection, Yaw);
	const FQuat ViewYaw = FRotator(0.0f, Yaw, 0.0f).Quaternion();
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
	if (ConsumeRequest(TEXT("ServerRelease")))
	{
		Release(false);
	}
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
	if (ConsumeRequest(TEXT("ServerThrow")))
	{
		Release(true);
	}
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
	SetHeldGhost(Component, false);
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

void UIslanderGrabComponent::OnRep_HeldComponent(UPrimitiveComponent* PreviousHeld)
{
	if (PreviousHeld && PreviousHeld != HeldComponent)
	{
		SetHeldGhost(PreviousHeld, false);
	}
	if (HeldComponent)
	{
		SetHeldGhost(HeldComponent, true);
	}
}

void UIslanderGrabComponent::SetHeldGhost(UPrimitiveComponent* Component, bool bGhost) const
{
	if (!IsValid(Component))
	{
		return;
	}
	if (bGhost)
	{
		Component->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		// nor the holder's capsule: an object pressed against its own carrier pushed and spun the player
		Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		return;
	}
	// another player still holds it: stays a ghost
	if (CountHolders(Component) > 0)
	{
		return;
	}
	FCollisionResponseTemplate Profile;
	const bool bProfile = UCollisionProfile::Get()->GetProfileTemplate(Component->GetCollisionProfileName(), Profile);
	Component->SetCollisionResponseToChannel(ECC_PhysicsBody, bProfile ? Profile.ResponseToChannels.GetResponse(ECC_PhysicsBody) : ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_Pawn, bProfile ? Profile.ResponseToChannels.GetResponse(ECC_Pawn) : ECR_Block);
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

FVector UIslanderGrabComponent::ClampHoldTarget(const FVector& Desired) const
{
	// the handle is a spring with no force limit: a target inside a hull or a wall pulls the object into it with
	// everything it has, and a boat under the holder's feet is thrown in the air. The target stops where the
	// held object would hit something static or heavy, slides along it, and never leads by more than MaxTargetLead.
	const FVector From = HeldComponent->GetComponentLocation();
	const FVector Lead = (Desired - From).GetClampedToMaxSize(MaxTargetLead);
	if (Lead.IsNearlyZero(0.5))
	{
		return From + Lead;
	}

	// the held object ignores physics bodies (SetHeldGhost): sweep its bounds by object type to still see the hull
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderGrabHold), false, GetOwner());
	Params.AddIgnoredActor(HeldComponent->GetOwner());
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	const FCollisionShape Shape = FCollisionShape::MakeBox(HeldComponent->Bounds.BoxExtent * 0.9);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, From, From + Lead, FQuat::Identity, Objects, Shape, Params);
	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* Other = Hit.GetComponent();
		const bool bBlocks = Other
			&& (!Other->IsSimulatingPhysics() || Other->GetMass() >= HeavyBodyMassKg || Cast<APawn>(Hit.GetActor()) != nullptr);
		if (!bBlocks)
		{
			continue;
		}
		const FVector Normal = Hit.bStartPenetrating && Hit.Normal.IsNearlyZero() ? -Lead.GetSafeNormal() : FVector(Hit.Normal);
		if ((Lead | Normal) >= 0.0)
		{
			// moving away from what it rests on (lifting off the floor): free
			continue;
		}
		const float Time = Hit.bStartPenetrating ? 0.0f : FMath::Max(0.0f, Hit.Time - 0.02f);
		// up to the obstacle, then along its surface only (never into it)
		FVector Rest = Lead * (1.0f - Time);
		Rest = FVector::VectorPlaneProject(Rest, Normal);
		if ((Rest | Normal) < 0.0)
		{
			Rest -= Normal * (Rest | Normal);
		}
		return From + Lead * Time + Rest;
	}
	return From + Lead;
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

	FVector AimEyes, AimDirection;
	float Yaw = 0.0f;
	GetHoldAim(AimEyes, AimDirection, Yaw);
	FVector Desired = AimEyes + AimDirection * HoldDistance;
	// never inside the holder's own body: in front of the capsule, whatever the look
	if (const ACharacter* Character = GetCharacter())
	{
		const FVector Body = Character->GetActorLocation();
		const float Keep = Character->GetSimpleCollisionRadius() + static_cast<float>(HeldComponent->Bounds.BoxExtent.Size2D()) + 5.0f;
		const FVector Flat(Desired.X - Body.X, Desired.Y - Body.Y, 0.0);
		if (Flat.SizeSquared() < FMath::Square(Keep))
		{
			const FVector Forward = FRotator(0.0f, Yaw, 0.0f).Vector();
			Desired = FVector(Body.X, Body.Y, Desired.Z) + Forward * Keep;
		}
	}
	const FQuat ViewYaw = FRotator(0.0f, Yaw, 0.0f).Quaternion();
	Handle->SetTargetLocationAndRotation(ClampHoldTarget(Desired), (ViewYaw * RotationOffset).Rotator());

	// the holder carries its share of the weight (slows it, costs stamina)
	const float Share = Mass / FMath::Max(1, Holders);
	if (!FMath::IsNearlyEqual(Share, HeldMass, 0.5f))
	{
		HeldMass = Share;
		ApplyCarriedWeight();
	}
}
