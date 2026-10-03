// monke_island

#include "Hazards/FallingCoconutActor.h"
#include "Hazards/CoconutFallRules.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Same tag as AWindStormActor: a dense nut in free fall barely drifts, the wind takes it once it has bounced */
	const FName CoconutWindShelteredTag(TEXT("WindSheltered"));
}

AFallingCoconutActor::AFallingCoconutActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->BodyInstance.bUseCCD = true;
	Mesh->SetCastShadow(true);
}

void AFallingCoconutActor::Setup(UStaticMesh* InMesh, const UCoconutFallRules* InRules)
{
	Rules = InRules;
	NutMesh = InMesh;
	Tags.AddUnique(CoconutWindShelteredTag);
	OnRep_NutMesh();
	if (Rules)
	{
		Mesh->SetMassOverrideInKg(NAME_None, Rules->CoconutMassKg, true);
		SetLifeSpan(Rules->CoconutLifeSpan);
	}
}

void AFallingCoconutActor::Launch(const FVector& Velocity)
{
	Mesh->SetPhysicsLinearVelocity(Velocity);
}

void AFallingCoconutActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Mesh->OnComponentHit.AddDynamic(this, &AFallingCoconutActor::HandleHit);
	}
}

void AFallingCoconutActor::OnRep_NutMesh()
{
	if (NutMesh)
	{
		Mesh->SetStaticMesh(NutMesh);
	}
}

void AFallingCoconutActor::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// first contact with anything: the fall is over, the wind can now roll the nut
	Tags.Remove(CoconutWindShelteredTag);

	AIslanderCharacter* Islander = Cast<AIslanderCharacter>(OtherActor);
	if (bHasHit || !Islander || !Rules || Islander->IsRagdoll())
	{
		return;
	}
	const FVector Velocity = Mesh->GetPhysicsLinearVelocity();
	const float ImpactSpeed = static_cast<float>((Velocity - Islander->GetVelocity()).Size());
	UE_LOG(Logmonke_island, Log, TEXT("%s hit %s at %.0f cm/s"), *GetName(), *Islander->GetName(), ImpactSpeed);
	if (!Rules->KnocksOut(ImpactSpeed))
	{
		return;
	}

	bHasHit = true;
	UE_LOG(Logmonke_island, Log, TEXT("%s knocked out %s at %.0f cm/s"), *GetName(), *Islander->GetName(), ImpactSpeed);
	if (UIslanderHealthComponent* Health = Islander->FindComponentByClass<UIslanderHealthComponent>())
	{
		Health->ApplyHealthDamage(Rules->KnockoutDamage, this);
	}
	Islander->Knockout(Velocity * Rules->KnockoutVelocityShare);
}

void AFallingCoconutActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFallingCoconutActor, NutMesh);
}
