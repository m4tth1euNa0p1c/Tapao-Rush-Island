// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TikiRagdollCorpse.generated.h"

class UMaterialInterface;
class UNetRestComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTikiDismemberSet;
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
	 *  nullptr when the mesh has no physics asset (the caller falls back on a rigid corpse). bFrozen keeps the pose
	 *  of the snapshot, laid on the ground and asleep (T-151) */
	static ATikiRagdollCorpse* Spawn(UWorld* World, UTikiRules* Rules, const FTikiBodySnapshot& Body, const USkeletalMeshComponent* MovingFrom, bool bFrozen = false);

	/** Calms a simulating body (every machine): damping, slow push out of overlaps, bounded spin, joints locked
	 *  in place with projection (no stretch); warns once about bodies without a joint to their parent */
	static void CalmRagdoll(USkeletalMeshComponent* Mesh, const UTikiRules* Rules);

	/** Bounds the speed of every body of a simulating mesh (cm/s) */
	static void BoundSpeeds(USkeletalMeshComponent* Mesh, float MaxSpeed);

	/** Moves every body so the lowest one lies on the ground below it (vertical trace); true when moved */
	static bool SnapToGround(USkeletalMeshComponent* Mesh, const UTikiRules* Rules);

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

	/** Network rest: no replication while it lies asleep; in the barque or on the galleon's deck it rides glued, its
	 *  movement not sent (T-312) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UNetRestComponent> NetRest;

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

	/** Keeps the pose it was spawned with, asleep on the ground (replicated once) */
	UPROPERTY(Replicated)
	bool bFrozen = false;

	/** World transforms of the bodies once the server laid the corpse down, in skeleton order of the bones with a
	 *  body: every machine ends on the same pose */
	UPROPERTY(ReplicatedUsing=OnRep_SettledBodies)
	TArray<FTransform> SettledBodies;

	/** Mass of the whole body (kg, replicated once) */
	UPROPERTY(Replicated)
	float MassKg = 40.0f;

	/** Rules of the creatures (replicated once: every machine calms the ragdoll the same way) */
	UPROPERTY(Replicated)
	TObjectPtr<UTikiRules> Rules;

	/** Closed modules of the body, handed to the halves when it is cut in two (server, T-155) */
	UPROPERTY()
	TObjectPtr<UTikiDismemberSet> DismemberSet;

	/** Velocities of the bodies at the handover, by bone (server, applied once at BeginPlay) */
	TMap<FName, FVector> StartVelocities;

	/** Timer of the flood check (server) */
	FTimerHandle FloodTimer;

	/** Timer that lays a moving corpse down (server) */
	FTimerHandle SettleTimer;

	/** Server: lays the corpse on the ground, puts it to sleep and replicates the pose */
	void Settle();

	/** Puts the bodies at the pose the server settled on, asleep (clients) */
	UFUNCTION()
	void OnRep_SettledBodies();

	/** Bounds the speeds after a hit (the push of the weapon comes after the damage) */
	void BoundSpeedsAfterHit();

	/** Builds the ragdoll from the replicated snapshot (every machine) */
	void ApplyLook();

	/** Washes the corpse away once the water covers it; lays it down again if it went to sleep above the ground
	 *  (server) */
	void CheckFlood();
};
