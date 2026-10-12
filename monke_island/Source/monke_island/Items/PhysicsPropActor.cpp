// monke_island

#include "Items/PhysicsPropActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Items/NetRestComponent.h"
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
	// the network rest hears at once when the body wakes up (T-312)
	Mesh->BodyInstance.bGenerateWakeEvents = true;
	SetRootComponent(Mesh);

	Valuable = CreateDefaultSubobject<UValuableComponent>(TEXT("Valuable"));

	// at rest no replication at all, on the galleon's deck or in the barque glued to its base (T-312, T-294, T-295)
	NetRest = CreateDefaultSubobject<UNetRestComponent>(TEXT("NetRest"));
}

void APhysicsPropActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Mesh)
	{
		Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
}
