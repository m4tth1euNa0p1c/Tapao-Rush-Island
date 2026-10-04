// monke_island

#include "Hazards/FallingCoconutActor.h"
#include "Hazards/CoconutFallRules.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

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
	// owner request 2026-10-04: a nut falling on the boat shoved it backwards. A falling nut passes through the
	// physics bodies (boat hull, loose props); it still lands on the ground and knocks the players out (capsule).
	// Once somebody picked it up and let go, the grab restores the profile: a nut put in the boat rests in it.
	Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
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
		SetLifeSpan(Rules->bFallenNutsStay ? 0.0f : Rules->CoconutLifeSpan);
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
	if (!Cast<APawn>(OtherActor))
	{
		TryLandSound(NormalImpulse, Hit.ImpactPoint);
	}
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

void AFallingCoconutActor::TryLandSound(const FVector& NormalImpulse, const FVector& Location)
{
	if (!Rules || Rules->LandSounds.IsEmpty() || LandSoundCount >= Rules->MaxLandSounds)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (LastLandSoundTime >= 0.0 && Now - LastLandSoundTime < Rules->LandSoundCooldown)
	{
		return;
	}
	// speed lost in the impact (cm/s): the impulse over the mass of the nut
	const float ImpactSpeed = static_cast<float>(NormalImpulse.Size()) / FMath::Max(Mesh->GetMass(), 0.1f);
	const float Volume = Rules->LandVolumeFor(ImpactSpeed);
	if (Volume <= 0.0f)
	{
		return;
	}
	USoundBase* Sound = Rules->LandSounds[FMath::RandRange(0, Rules->LandSounds.Num() - 1)];
	if (!Sound)
	{
		return;
	}
	++LandSoundCount;
	LastLandSoundTime = Now;
	MulticastLandSound(Sound, Rules->LandAttenuation, Location, Volume);
}

void AFallingCoconutActor::MulticastLandSound_Implementation(USoundBase* Sound, USoundAttenuation* Attenuation, FVector_NetQuantize Location, float Volume)
{
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume, FMath::FRandRange(0.92f, 1.08f), 0.0f, Attenuation);
	}
}

void AFallingCoconutActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFallingCoconutActor, NutMesh);
}
