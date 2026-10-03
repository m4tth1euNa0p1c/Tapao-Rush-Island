// monke_island

#include "Items/PhysicsPropActor.h"
#include "Components/StaticMeshComponent.h"
#include "Items/ValuableComponent.h"

APhysicsPropActor::APhysicsPropActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// the server simulates, clients follow the replicated movement (cahier §5: free objects simulate physics)
	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetLinearDamping(0.1f);
	Mesh->SetAngularDamping(0.3f);
	SetRootComponent(Mesh);

	Valuable = CreateDefaultSubobject<UValuableComponent>(TEXT("Valuable"));
}

void APhysicsPropActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Mesh)
	{
		Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
}
