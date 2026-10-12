// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrenadeProjectile.generated.h"

class AFirearmWeapon;
class UGrenadeRules;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  A grenade in flight (BP_GrenadeProjectile, thrown by AGrenadeWeapon; owner 2026-10-09): a replicated sphere that
 *  bounces and rolls (projectile movement, the server's motion replicated, the clients simulating between updates)
 *  and goes off once its fuse is burnt. The blast is the server's: damage falling off with the distance to every
 *  target in sight (FFirearmDamageEvent, so the soldiers and the crab react as to a shot), players knocked out,
 *  physics bodies pushed. The blast effects (sound, flash, light, Niagara, scorch decal) play on every machine when
 *  bExploded replicates. Rules: the weapon's DA_GrenadeRules (set at the throw), else the class default.
 */
UCLASS()
class MONKE_ISLAND_API AGrenadeProjectile : public AActor
{
	GENERATED_BODY()

public:

	AGrenadeProjectile();

	/** Rules of the grenade (DA_GrenadeRules): the thrower's weapon sets them, the class default serves the clients */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenade")
	TObjectPtr<UGrenadeRules> Rules;

	/** Server, before FinishSpawning: the thrower's weapon (damage causer), the throw and the fuse left (s) */
	void Init(UGrenadeRules* InRules, AFirearmWeapon* InWeapon, const FVector& Velocity, float FuseLeft);

	/** The grenade went off */
	UFUNCTION(BlueprintPure, Category="Grenade")
	bool HasExploded() const { return bExploded; }

	/** Returns the sphere */
	UFUNCTION(BlueprintPure, Category="Grenade")
	USphereComponent* GetCollision() const { return Collision; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Every machine: the blast effects */
	UFUNCTION()
	void OnRep_Exploded();

	/** Every machine: a bounce (sound) */
	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

private:

	/** Server: the fuse is burnt */
	void Explode();

	/** Server: damage, knockouts and impulses around the blast */
	void ApplyBlast();

	/** Every machine: sound, Niagara, flash, light, scorch */
	void PlayExplosionEffects();

	/** Hides the flash and the light */
	void HideFlash();

	/** Sphere that bounces (root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> Collision;

	/** Grenade mesh on the sphere (no collision) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Flight, bounces and rolling */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** Flash of the blast (local, created at the blast) */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlashComponent;

	/** Light of the blast (local, created at the blast) */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;

	/** Server: the weapon that threw it, the damage causer while it lives */
	TWeakObjectPtr<AFirearmWeapon> Weapon;

	/** Set once by the server when the fuse is burnt */
	UPROPERTY(ReplicatedUsing=OnRep_Exploded)
	bool bExploded = false;

	/** Server: fuse left at the throw (s), armed at BeginPlay */
	float PendingFuse = 0.0f;

	/** Last bounce sound (local world time) */
	double LastBounceSoundTime = -1.0;

	/** Server: the fuse */
	FTimerHandle FuseTimer;

	/** Flash and light out */
	FTimerHandle FlashTimer;
};
