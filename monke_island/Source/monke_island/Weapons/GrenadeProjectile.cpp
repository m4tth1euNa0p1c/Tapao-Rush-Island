// monke_island

#include "Weapons/GrenadeProjectile.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/FirearmWeapon.h"
#include "Weapons/GrenadeMath.h"
#include "Weapons/GrenadeRules.h"
#include "monke_island.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/IslanderCharacter.h"
#include "Stealth/StealthSubsystem.h"
#include "TimerManager.h"

AGrenadeProjectile::AGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	// the sphere bounces off everything that blocks, pawns included; the crosshair looks past it
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(6.0f);
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 0.0f;
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.35f;
	Movement->Friction = 0.6f;
	Movement->ProjectileGravityScale = 1.0f;
	Movement->bRotationFollowsVelocity = false;
}

void AGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGrenadeProjectile, bExploded);
}

void AGrenadeProjectile::Init(UGrenadeRules* InRules, AFirearmWeapon* InWeapon, const FVector& Velocity, float FuseLeft)
{
	if (InRules)
	{
		Rules = InRules;
	}
	Weapon = InWeapon;
	PendingFuse = FMath::Max(FuseLeft, 0.05f);
	// InitialSpeed is 0: the movement keeps this velocity as it is
	Movement->Velocity = Velocity;
}

void AGrenadeProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	// the clients fly on between two updates of the server
	if (Movement)
	{
		Movement->Velocity = NewVelocity;
	}
}

void AGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (Rules)
	{
		Collision->SetSphereRadius(Rules->ProjectileRadiusCm);
		Movement->Bounciness = Rules->Bounciness;
		Movement->Friction = Rules->Friction;
		Movement->ProjectileGravityScale = Rules->ProjectileGravityScale;
		if (Rules->ProjectileMesh)
		{
			Mesh->SetStaticMesh(Rules->ProjectileMesh);
		}
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Grenade %s: no rules (DA_GrenadeRules), it will not explode"), *GetName());
	}
	// never caught by the one who threw it on its way out
	if (AActor* Thrower = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);
	}
	Movement->OnProjectileBounce.AddDynamic(this, &AGrenadeProjectile::HandleBounce);
	if (HasAuthority() && Rules)
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &AGrenadeProjectile::Explode, PendingFuse, false);
	}
}

void AGrenadeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void AGrenadeProjectile::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (!Rules || !Rules->BounceSound || bExploded || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastBounceSoundTime < Rules->BounceSoundInterval)
	{
		return;
	}
	LastBounceSoundTime = Now;
	const float Volume = FMath::Clamp(static_cast<float>(ImpactVelocity.Size()) / 600.0f, 0.2f, 1.0f);
	UGameplayStatics::PlaySoundAtLocation(this, Rules->BounceSound, ImpactResult.ImpactPoint, Volume);
}

// ---------------------------------------------------------------- blast (server)

void AGrenadeProjectile::Explode()
{
	if (!HasAuthority() || bExploded || !Rules)
	{
		return;
	}
	bExploded = true;
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ApplyBlast();
	// the blast is heard by the mobs and the villagers around, as the thrower's noise (server, T-262)
	UStealthSubsystem::Report(this, GetInstigator() ? static_cast<AActor*>(GetInstigator()) : static_cast<AActor*>(this), GetActorLocation(), EStealthNoise::Impact);
	// the host plays the effects itself (no OnRep on the server); the actor stays until the clients heard of it
	OnRep_Exploded();
	SetLifeSpan(FMath::Max(Rules->ProjectileDestroyDelay, 0.1f));
}

void AGrenadeProjectile::ApplyBlast()
{
	const FVector Center = GetActorLocation();
	const float Radius = Rules->BlastRadiusCm;
	AActor* Causer = Weapon.IsValid() ? static_cast<AActor*>(Weapon.Get()) : static_cast<AActor*>(this);
	AController* InstigatorController = GetInstigatorController();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrenadeBlast), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
		FCollisionShape::MakeSphere(Radius), Params);

	TSet<AActor*> Seen;
	int32 Struck = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		UPrimitiveComponent* Body = Overlap.GetComponent();
		if (!Target || Target == this)
		{
			continue;
		}
		ACharacter* Character = Cast<ACharacter>(Target);
		// loose bodies are pushed, every body of them
		if (Body && Body->IsSimulatingPhysics() && !Character && Rules->BlastImpulse > 0.0f)
		{
			Body->AddRadialImpulse(Center, Radius, Rules->BlastImpulse, RIF_Linear, false);
		}
		if (Seen.Contains(Target))
		{
			continue;
		}
		Seen.Add(Target);

		// the nearest point of the target, in sight of the blast
		FVector Point = Target->GetActorLocation();
		if (Body)
		{
			FVector Closest;
			if (Body->GetClosestPointOnCollision(Center, Closest) >= 0.0f)
			{
				Point = Closest;
			}
		}
		const float Distance = static_cast<float>(FVector::Dist(Center, Point));
		if (Distance > Radius)
		{
			continue;
		}
		if (Rules->bBlastNeedsLineOfSight)
		{
			FHitResult Sight;
			FCollisionQueryParams SightParams(SCENE_QUERY_STAT(GrenadeBlastSight), false, this);
			SightParams.AddIgnoredActor(Target);
			if (GetWorld()->LineTraceSingleByChannel(Sight, Center, Point, ECC_Visibility, SightParams))
			{
				continue;
			}
		}

		const FVector Direction = (Point - Center).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const float Damage = GrenadeMath::DamageAtDistance(Rules->CenterDamage, Distance, Rules->FullDamageRadiusCm, Radius);
		const APawn* TargetPawn = Cast<APawn>(Target);
		const bool bFriendly = TargetPawn && TargetPawn->IsPlayerControlled();
		if (Damage > 0.0f && Target->CanBeDamaged() && (!bFriendly || Rules->bFriendlyFire))
		{
			// one blow, as a shot: the soldiers and the crab read its distance and bone (an explosion up close)
			FFirearmDamageEvent Shot;
			Shot.DamageTypeClass = UDamageType::StaticClass();
			Shot.Damage = Damage;
			Shot.ShotDirection = Direction;
			Shot.HitInfo = FHitResult(Target, Body, Point, -Direction);
			Shot.HitInfo.TraceStart = Center;
			Shot.HitInfo.TraceEnd = Point + Direction * 50.0f;
			Shot.HitInfo.Distance = Distance;
			Shot.HitInfo.BoneName = Character ? FirearmDamage::FindStruckBone(Character->GetMesh(), Center, Point + Direction * 50.0f, Rules->BlastBoneRadiusCm) : NAME_None;
			FirearmDamage::AddPellet(Shot.BoneHits, Shot.HitInfo.BoneName, Damage, Point);
			Shot.Impacts.Add(Point);
			Shot.PelletsFired = 1;
			Shot.LimbDamageScale = Rules->LimbDamageScale;
			Shot.CloseBurstDistance = Rules->BurstDistanceCm;
			if (Rules->BurstDistanceCm > 0.0f && Distance <= Rules->BurstDistanceCm)
			{
				Shot.ShredLimbs = FMath::RandRange(Rules->BurstShredLimbsMin, FMath::Max(Rules->BurstShredLimbsMin, Rules->BurstShredLimbsMax));
			}
			Target->TakeDamage(Damage, Shot, InstigatorController, Causer);
			++Struck;
		}
		if (AIslanderCharacter* Player = Cast<AIslanderCharacter>(Target))
		{
			// the blast throws the players over, the thrower included (server only call)
			if (Rules->bKnockoutPlayers)
			{
				Player->Knockout(GrenadeMath::KnockoutVelocity(Point - Center, Rules->KnockoutSpeed));
			}
		}
		else if (Character && Rules->KnockbackSpeed > 0.0f)
		{
			const float Share = Rules->CenterDamage > 0.0f ? FMath::Clamp(Damage / Rules->CenterDamage, 0.0f, 1.0f) : 1.0f;
			Character->LaunchCharacter(Direction.GetSafeNormal2D() * Rules->KnockbackSpeed * Share + FVector(0.0f, 0.0f, 80.0f * Share), true, false);
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("Grenade %s: blast at %s, %d target(s) hurt of %d around"), *GetName(), *Center.ToCompactString(), Struck, Seen.Num());
}

// ---------------------------------------------------------------- effects (every machine)

void AGrenadeProjectile::OnRep_Exploded()
{
	if (!bExploded)
	{
		return;
	}
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false, true);
	PlayExplosionEffects();
}

void AGrenadeProjectile::PlayExplosionEffects()
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Center = GetActorLocation();
	if (Rules->ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rules->ExplosionSound, Center);
	}
	// the muzzle systems spray along their local +X: pointed up
	for (UNiagaraSystem* System : Rules->ExplosionSystems)
	{
		if (System)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Center, FRotator(90.0f, 0.0f, 0.0f), FVector(Rules->ExplosionSystemsScale), true, true,
				ENCPoolMethod::AutoRelease);
		}
	}
	if (Rules->ExplosionFlashMesh)
	{
		if (!FlashComponent)
		{
			FlashComponent = NewObject<UStaticMeshComponent>(this, TEXT("ExplosionFlash"));
			FlashComponent->SetStaticMesh(Rules->ExplosionFlashMesh);
			if (Rules->ExplosionFlashMaterial)
			{
				FlashComponent->SetMaterial(0, Rules->ExplosionFlashMaterial);
			}
			FlashComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			FlashComponent->SetCastShadow(false);
			FlashComponent->SetUsingAbsoluteScale(true);
			FlashComponent->RegisterComponent();
			FlashComponent->AttachToComponent(Collision, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		FlashComponent->SetWorldScale3D(Rules->ExplosionFlashScale * FMath::FRandRange(0.9f, 1.1f));
		FlashComponent->SetVisibility(true);
	}
	if (Rules->ExplosionLightIntensity > 0.0f)
	{
		if (!FlashLight)
		{
			FlashLight = NewObject<UPointLightComponent>(this, TEXT("ExplosionLight"));
			FlashLight->SetCastShadows(false);
			FlashLight->RegisterComponent();
			FlashLight->AttachToComponent(Collision, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		FlashLight->SetLightColor(FLinearColor(1.0f, 0.7f, 0.35f));
		FlashLight->SetIntensity(Rules->ExplosionLightIntensity);
		FlashLight->SetAttenuationRadius(Rules->ExplosionLightRadius);
		FlashLight->SetVisibility(true);
	}
	GetWorldTimerManager().SetTimer(FlashTimer, this, &AGrenadeProjectile::HideFlash, FMath::Max(Rules->ExplosionFlashSeconds, 0.01f), false);

	// a scorch on the ground under the blast, laid on the surface
	if (Rules->ExplosionDecal)
	{
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrenadeScorch), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Ground, Center + FVector(0.0, 0.0, 30.0), Center - FVector(0.0, 0.0, 200.0), ECC_Visibility, Params))
		{
			// the decal projects along its local X: into the surface
			UGameplayStatics::SpawnDecalAtLocation(this, Rules->ExplosionDecal, Rules->ExplosionDecalSize, Ground.ImpactPoint,
				(-FVector(Ground.ImpactNormal)).Rotation(), Rules->ExplosionDecalSeconds);
		}
	}
}

void AGrenadeProjectile::HideFlash()
{
	if (FlashComponent)
	{
		FlashComponent->SetVisibility(false);
	}
	if (FlashLight)
	{
		FlashLight->SetVisibility(false);
	}
}
