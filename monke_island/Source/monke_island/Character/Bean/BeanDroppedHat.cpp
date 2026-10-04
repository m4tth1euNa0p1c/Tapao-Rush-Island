// monke_island

#include "Character/Bean/BeanDroppedHat.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"

ABeanDroppedHat::ABeanDroppedHat()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	Tags.Add(UIslanderGrabComponent::NoGrabTag);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->BodyInstance.bUseCCD = true;
}

void ABeanDroppedHat::Initialize(UStaticMesh* InMesh, const FVector& Velocity, float UpwardKick)
{
	if (!HasAuthority())
	{
		return;
	}
	HatMesh = InMesh;
	OnRep_HatMesh();
	Mesh->SetPhysicsLinearVelocity(Velocity + FVector(FMath::FRandRange(-SidewaysKick, SidewaysKick),
		FMath::FRandRange(-SidewaysKick, SidewaysKick), UpwardKick));
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-SpinSpeed.X, SpinSpeed.X),
		FMath::FRandRange(-SpinSpeed.Y, SpinSpeed.Y), FMath::FRandRange(-SpinSpeed.Z, SpinSpeed.Z)));
}

void ABeanDroppedHat::OnRep_HatMesh()
{
	Mesh->SetStaticMesh(HatMesh);
	if (HatMesh)
	{
		Mesh->SetSimulatePhysics(true);
		Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
}

FText ABeanDroppedHat::GetInteractionPrompt_Implementation(APawn* User) const
{
	return NSLOCTEXT("BeanHat", "PutOnHat", "Remettre le chapeau");
}

bool ABeanDroppedHat::CanInteract_Implementation(APawn* User) const
{
	const AIslanderCharacter* Wearer = Cast<AIslanderCharacter>(User);
	if (!IsValid(Wearer) || Wearer != GetOwner() || Wearer->IsRagdoll() || IsActorBeingDestroyed() || !HatMesh)
	{
		return false;
	}
	const UIslanderHealthComponent* Health = Wearer->GetHealthComponent();
	const UBeanPuppetComponent* Puppet = Wearer->FindComponentByClass<UBeanPuppetComponent>();
	if ((Health && Health->IsDead()) || !Puppet || !Puppet->IsHatDropped()
		|| GetGameTimeSinceCreation() < Puppet->HatPickupDelay)
	{
		return false;
	}
	// Check the real hat position, never a point supplied by a client.
	const FVector Feet = Wearer->GetActorLocation() - FVector(0.0, 0.0, Wearer->GetSimpleCollisionHalfHeight());
	const FVector HatLocation = GetActorLocation();
	if (FVector::Dist2D(HatLocation, Feet) > Puppet->HatPickupRadius
		|| FMath::Abs(HatLocation.Z - Feet.Z) > 2.0 * Puppet->HatPickupRadius)
	{
		return false;
	}
	const FVector Eyes = Wearer->GetFirstPersonCamera() ? Wearer->GetFirstPersonCamera()->GetComponentLocation()
		: Wearer->GetPawnViewLocation();
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(BeanHatPickup), false, Wearer);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, HatLocation, ECC_Visibility, Params) || Hit.GetActor() == this;
}

void ABeanDroppedHat::Interact_Implementation(APawn* User)
{
	if (HasAuthority() && CanInteract_Implementation(User))
	{
		if (UBeanPuppetComponent* Puppet = User->FindComponentByClass<UBeanPuppetComponent>())
		{
			Puppet->TryPickUpHat(this, User);
		}
	}
}

void ABeanDroppedHat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABeanDroppedHat, HatMesh);
}
