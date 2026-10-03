// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TikiRagdollCorpse.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTikiRules;
struct FTikiBodySnapshot;

/**
 *  Whole corpse of a tiki: the body goes on as a ragdoll from where it fell (its physics asset), heavy (CorpseMassKg
 *  shared over its bodies), so it slumps, rolls and drags like a body. The players grab it by any limb (one drags
 *  it, two lift it) and the boat counts its mass. A cut blow of the cutlass cuts it in two halves (ATikiCorpse,
 *  rigid pieces). It stays until the rising water covers it.
 *  The server simulates and replicates the root body; the other bodies follow it on every machine.
 */
UCLASS(NotBlueprintable)
class MONKE_ISLAND_API ATikiRagdollCorpse : public AActor
{
	GENERATED_BODY()

public:

	ATikiRagdollCorpse();

	/** Server: the ragdoll of a body (a dead creature), its bodies placed and moving as in the snapshot;
	 *  nullptr when the mesh has no physics asset (the caller falls back on a rigid corpse) */
	static ATikiRagdollCorpse* Spawn(UWorld* World, UTikiRules* Rules, const FTikiBodySnapshot& Body, const USkeletalMeshComponent* MovingFrom);

	/** Server: replaces it by its two halves, the upper one flung along Direction */
	void CutInTwo(const FVector& Direction);

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** The body (root), simulating its physics asset */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Held item, following the body */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Held;

	/** Body mesh (replicated once) */
	UPROPERTY(Replicated)
	TObjectPtr<USkeletalMesh> BodyMesh;

	/** Materials of the body mesh (replicated once) */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<UMaterialInterface>> BodyMaterials;

	/** Held item mesh, none when the body held nothing (replicated once) */
	UPROPERTY(Replicated)
	TObjectPtr<USkeletalMesh> HeldMesh;

	/** Materials of the held item (replicated once) */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<UMaterialInterface>> HeldMaterials;

	/** Bone space transforms the ragdoll starts from (replicated once) */
	UPROPERTY(Replicated)
	TArray<FTransform> Pose;

	/** Bones cut off before death, hidden with their bodies (replicated once) */
	UPROPERTY(Replicated)
	TArray<FName> CutBones;

	/** Mass of the whole body (kg, replicated once) */
	UPROPERTY(Replicated)
	float MassKg = 40.0f;

	/** Rules of the creatures (server) */
	UPROPERTY(Transient)
	TObjectPtr<UTikiRules> Rules;

	/** Velocities of the bodies at the handover, by bone (server, applied once at BeginPlay) */
	TMap<FName, FVector> StartVelocities;

	/** Timer of the flood check (server) */
	FTimerHandle FloodTimer;

	/** Builds the ragdoll from the replicated snapshot (every machine) */
	void ApplyLook();

	/** Washes the corpse away once the water covers it (server) */
	void CheckFlood();
};
