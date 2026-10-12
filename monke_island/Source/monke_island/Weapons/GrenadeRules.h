// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"
#include "GrenadeRules.generated.h"

class AGrenadeProjectile;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

/**
 *  Tunable rules of the bean's grenade (DA_GrenadeRules; owner 2026-10-09, pack bean-grenade: 13 gestures at 30 fps,
 *  events Light, Fire and HammerCocked at the next grenade taken). A bag of grenades carried as a firearm: Fire lobs
 *  the one in hand (short throw), AimFire throws it far; the fuse is lit at the Light event of the clip and the
 *  grenade leaves at its Fire event; Reload takes the next grenade out of the bag at HammerCocked. The projectile is
 *  a bouncing replicated sphere that explodes FuseSeconds after it was lit: damage falling off with the distance,
 *  players knocked out, physics bodies pushed, the blast effects on every machine.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UGrenadeRules : public UFirearmRules
{
	GENERATED_BODY()

public:

	UGrenadeRules();

	/** Grenades a player carries at most, the one in hand included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1"))
	int32 MaxGrenades = 6;

	/** Grenades at spawn (a bag of three) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 StartGrenades = 3;

	/** One of them is in hand from the start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	bool bStartInHand = true;

	/** Pressing fire with an empty hand takes the next grenade (when the bag has one) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoTakeNextOnFire = true;

	/** The next grenade comes to hand by itself after a throw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoTakeNextAfterThrow = true;

	/** Event of the Reload clip where the next grenade is in hand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName ReloadLoadedEvent = TEXT("HammerCocked");

	/** Time from the fuse lit to the blast (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fuse", meta=(ClampMin="0.2", Units="s"))
	float FuseSeconds = 3.0f;

	/** Event of the Fire / AimFire clips where the fuse is lit (the throw lights it when the clip has none) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fuse")
	FName LightEvent = TEXT("Light");

	/** Bones of the weapon mesh hidden while no grenade is in the hand (the body and its fuse): the slow match and the bag
	 *  of the pack stay; empty = the whole weapon mesh is hidden */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenade")
	TArray<FName> HandHiddenBones = { TEXT("body"), TEXT("fuse") };

	/** A grenade thrown with little fuse left still flies this long before it goes off (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fuse", meta=(ClampMin="0", Units="s"))
	float MinFuseAfterThrow = 0.3f;

	/** Speed of the short throw, from the hip (Fire; cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0", Units="cm/s"))
	float ShortThrowSpeed = 900.0f;

	/** Speed of the long throw, aimed (AimFire; cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0", Units="cm/s"))
	float LongThrowSpeed = 1600.0f;

	/** The throw leaves above the aim by this angle (deg): a lob */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throw", meta=(ClampMin="-30", ClampMax="60", Units="deg"))
	float ThrowPitchDegrees = 10.0f;

	/** Speed a grenade is let go of at when the throw is cut after the fuse was lit (cm/s): it rolls at the feet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0", Units="cm/s"))
	float DropSpeed = 150.0f;

	/** Where the grenade leaves from, relative to the carrier's eyes and view (X forward, Y right, Z up; cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throw")
	FVector ThrowOffset = FVector(40.0f, 15.0f, -10.0f);

	/** Share of the carrier's own velocity the grenade keeps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0", ClampMax="1"))
	float InheritedVelocityScale = 0.5f;

	/** Reach of the blast (cm): no damage beyond */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="50", Units="cm"))
	float BlastRadiusCm = 400.0f;

	/** Full damage up to this distance from the blast (cm), then down to nothing at BlastRadiusCm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0", Units="cm"))
	float FullDamageRadiusCm = 150.0f;

	/** Damage at the centre of the blast */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0"))
	float CenterDamage = 90.0f;

	/** Only targets in sight of the blast take it (a wall shelters) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast")
	bool bBlastNeedsLineOfSight = true;

	/** Players caught in the blast are knocked out (thrower included) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast")
	bool bKnockoutPlayers = true;

	/** Speed a knocked out player is thrown away from the blast at (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0", Units="cm/s"))
	float KnockoutSpeed = 450.0f;

	/** Impulse given to the physics bodies in the blast (kg cm/s, falling off linearly to the radius) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0"))
	float BlastImpulse = 60000.0f;

	/** A tiki or a soldier killed within this distance of the blast bursts (cm, carried by the damage event); 0 = none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0", Units="cm"))
	float BurstDistanceCm = 200.0f;

	/** Limbs a lethal blast within BurstDistanceCm shreds off a tiki, drawn between these */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0", ClampMax="5"))
	int32 BurstShredLimbsMin = 1;

	/** See BurstShredLimbsMin */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="0", ClampMax="5"))
	int32 BurstShredLimbsMax = 3;

	/** Bone of a character struck by the blast: the one nearest the line from the blast to its centre (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blast", meta=(ClampMin="1", Units="cm"))
	float BlastBoneRadiusCm = 40.0f;

	/** Projectile thrown (BP_GrenadeProjectile, AGrenadeProjectile); none = no throw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	TSubclassOf<AGrenadeProjectile> ProjectileClass;

	/** Mesh of the grenade in flight (SM_Bean_Grenade), on the projectile's sphere */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UStaticMesh> ProjectileMesh;

	/** Radius of the projectile's sphere (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="1", Units="cm"))
	float ProjectileRadiusCm = 6.0f;

	/** Speed kept at each bounce (0 none, 1 all) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0", ClampMax="1"))
	float Bounciness = 0.35f;

	/** Sliding friction on the ground after a bounce */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0", ClampMax="1"))
	float Friction = 0.6f;

	/** Gravity of the projectile (1 = the world's) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0"))
	float ProjectileGravityScale = 1.0f;

	/** Sound of a bounce (not more than one every BounceSoundInterval) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USoundBase> BounceSound;

	/** See BounceSound (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0", Units="s"))
	float BounceSoundInterval = 0.15f;

	/** Sound of the blast (the musket's shot when nothing better) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	/** Niagara systems spawned together at the blast (the shotgun's muzzle flame and smoke fit), pointing up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	TArray<TObjectPtr<UNiagaraSystem>> ExplosionSystems;

	/** Scale of the explosion systems */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0.1"))
	float ExplosionSystemsScale = 2.5f;

	/** Flash of the blast: the musket's flame mesh and material, scaled */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	TObjectPtr<UStaticMesh> ExplosionFlashMesh;

	/** See ExplosionFlashMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	TObjectPtr<UMaterialInterface> ExplosionFlashMaterial;

	/** See ExplosionFlashMesh (scale of a 1 m sphere) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	FVector ExplosionFlashScale = FVector(2.5f, 2.5f, 2.5f);

	/** Light of the blast (unitless intensity, 0 = none) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0"))
	float ExplosionLightIntensity = 80000.0f;

	/** See ExplosionLightIntensity (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0", Units="cm"))
	float ExplosionLightRadius = 900.0f;

	/** Time the flash and the light of the blast last (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0.01", Units="s"))
	float ExplosionFlashSeconds = 0.12f;

	/** Scorch left on the ground under the blast (decal material; none = nothing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	TObjectPtr<UMaterialInterface> ExplosionDecal;

	/** Size of the scorch decal (depth, width, height; cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion")
	FVector ExplosionDecalSize = FVector(64.0f, 180.0f, 180.0f);

	/** Time the scorch stays (s; 0 = for ever) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0", Units="s"))
	float ExplosionDecalSeconds = 45.0f;

	/** Time the exploded projectile stays before it is destroyed, so the blast reaches every machine (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0.1", Units="s"))
	float ProjectileDestroyDelay = 2.0f;
};
