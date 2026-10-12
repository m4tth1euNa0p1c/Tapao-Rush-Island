// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Creatures/TikiBody.h"
#include "TikiCorpse.generated.h"

class UTikiCorpseCollisionComponent;
class UMaterialInterface;
class UNetRestComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTikiDismemberSet;
class UTikiRules;

/** A body at one instant, what a corpse is made from: a dead or maimed creature, or a corpse cut in two (server) */
struct FTikiBodySnapshot
{
	/** Body mesh */
	USkeletalMesh* Mesh = nullptr;

	/** Materials of the body mesh, by slot */
	TArray<UMaterialInterface*> Materials;

	/** Item held in the hand (weapon, basket), on the same skeleton; nullptr when none */
	USkeletalMesh* HeldMesh = nullptr;

	/** Materials of the held item, by slot */
	TArray<UMaterialInterface*> HeldMaterials;

	/** Bone space transforms of the body */
	TArray<FTransform> Pose;

	/** World transform of the body mesh */
	FTransform MeshWorld;

	/** Bones cut off before (each with what hangs from it) */
	TArray<FName> CutBones;

	/** Closed modules of the body (T-155), nullptr when the creature has none */
	UTikiDismemberSet* DismemberSet = nullptr;
};

/**
 *  Corpse of a tiki, or a piece of one: the body frozen in the pose it died in, one rigid body whose collision is the
 *  physics asset bodies in that pose (a box around the joints without physics asset), laid on the ground with the
 *  collision touching it (T-151). The players can grab (one drags a whole corpse, two lift it) and stow in the boat, which counts its mass. A cut blow of the
 *  cutlass cuts a whole corpse in two halves. It stays until the rising water covers it.
 *  The server simulates and replicates the movement; every machine builds the look from the replicated snapshot.
 */
UCLASS(NotBlueprintable)
class MONKE_ISLAND_API ATikiCorpse : public AActor
{
	GENERATED_BODY()

public:

	ATikiCorpse();

	/** Server: spawns a corpse, or a part of one, from a body, moving at Velocity (cm/s); nullptr when it could not appear */
	static ATikiCorpse* SpawnPart(UWorld* World, UTikiRules* Rules, const FTikiBodySnapshot& Body, ETikiBodyPart Part, FName PartBone, const FVector& Velocity);

	/** Snapshot of a body mesh and of the item it holds (Held may be null) */
	static FTikiBodySnapshot Snapshot(USkeletalMeshComponent* BodyMesh, USkeletalMeshComponent* Held, const TArray<FName>& CutBones,
		UTikiDismemberSet* DismemberSet = nullptr);

	/** Part of the body this corpse is */
	UFUNCTION(BlueprintPure, Category="Tiki")
	ETikiBodyPart GetPart() const { return Part; }

	/** Mass of the corpse (kg) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	float GetMassKg() const { return MassKg; }

	/** Server: replaces this whole corpse by its two halves, the upper one flung along Direction */
	void CutInTwo(const FVector& Direction);

	/** Server: washes the oldest corpses away (rigid and ragdoll) beyond the count kept in the level */
	static void KeepCount(UWorld* World, int32 MaxCount);

	/** Ground under Point, looked for from Above over Point down to Below under it (static and dynamic world, water
	 *  surfaces and Ignore left out); false when none */
	static bool FindGround(const UWorld* World, const FVector& Point, float Above, float Below, const AActor* Ignore, float& OutGroundZ);

	/** Collision of a part of a posed body: the physics asset shapes of the bones it keeps, in the space of
	 *  ComponentToTarget (empty without physics asset) */
	static FKAggregateGeom PartGeometry(const USkeletalMesh* Mesh, const TArray<FTransform>& Pose, ETikiBodyPart Part, FName PartBone,
		const TArray<FName>& CutBones, const FTransform& ComponentToTarget);

	/** Water above this corpse (cm), never counted from below the ground it lies on (server) */
	float GetWaterDepth() const;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Rigid collision (root), the physics asset bodies in the pose: what the players grab and the boat weighs */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTikiCorpseCollisionComponent> Body;

	/** Body frozen in its pose */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPoseableMeshComponent> Mesh;

	/** Held item, following the body pose */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Held;

	/** Face closing the cut of a part */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Cap;

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

	/** Bone space transforms of the whole body (replicated once; the part is masked on every machine) */
	UPROPERTY(Replicated)
	TArray<FTransform> Pose;

	/** Body mesh relative to the box (replicated once) */
	UPROPERTY(Replicated)
	FTransform MeshRelative;

	/** Part of the body (replicated once) */
	UPROPERTY(Replicated)
	ETikiBodyPart Part = ETikiBodyPart::Whole;

	/** Bone the part is cut at (replicated once) */
	UPROPERTY(Replicated)
	FName PartBone;

	/** Bones cut off before (replicated once) */
	UPROPERTY(Replicated)
	TArray<FName> CutBones;

	/** Half size of the box around the joints, the collision of a mesh without physics asset (cm, replicated once) */
	UPROPERTY(Replicated)
	FVector BoxExtent = FVector(30.0f);

	/** Centre of that box from the root (cm, replicated once) */
	UPROPERTY(Replicated)
	FVector BoxOffset = FVector::ZeroVector;

	/** Mass (kg, replicated once) */
	UPROPERTY(Replicated)
	float MassKg = 40.0f;

	/** Mesh and material of the cut face, radius of the cut (replicated once) */
	UPROPERTY(Replicated)
	TObjectPtr<UStaticMesh> CapMesh;

	UPROPERTY(Replicated)
	TObjectPtr<UMaterialInterface> CapMaterial;

	UPROPERTY(Replicated)
	float CapRadius = 20.0f;

	/** Closed modules of the body (replicated once, T-155): the part is drawn with them when they can close its cuts */
	UPROPERTY(Replicated)
	TObjectPtr<UTikiDismemberSet> DismemberSet;

	/** Modules drawn instead of Mesh, in the same frozen pose (every machine, T-155) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPoseableMeshComponent>> Modules;

	/** Draws the part with the closed modules in the full pose Local, Mesh hidden; false when the set cannot (T-155) */
	bool ApplyModules(const TArray<FTransform>& Local);

	/** Rules of the creatures (replicated once: every machine builds the same collision and damping) */
	UPROPERTY(Replicated)
	TObjectPtr<UTikiRules> Rules;

	/** Server world time the corpse appeared (replicated once): a fresh cut bleeds, a player joining late sees no
	 *  jet and a pool already grown (T-153) */
	UPROPERTY(Replicated)
	float SpawnServerTime = 0.0f;

	/** Timer of the flood check (server) */
	FTimerHandle FloodTimer;

	/** Blood of a fresh corpse on this machine (T-153): the jet out of the cut of a piece, chunks and a splat when a
	 *  body was cut in two, the pool growing under a body. JointLocal and OutOfCut are the cut joint and the way out
	 *  of the cut in the space of the body mesh (unused for a whole body) */
	void SpawnBlood(const FVector& JointLocal, const FVector& OutOfCut, FName AttachBone);

	/** Frozen pose of a dead body: starts asleep, at rest, where the creature's mesh was (replicated once, T-151) */
	UPROPERTY(Replicated)
	bool bFrozen = false;

	/** Height a frozen body still has to go down to touch the ground (cm, negative; server) */
	float SettleLeft = 0.0f;

	/** Timer of the slow descent of a floating frozen body (server) */
	FTimerHandle SettleTimer;

	/** Period of that descent (s) */
	static constexpr float SettleInterval = 1.0f / 30.0f;

	/** One step of the descent; asleep on the ground at the end (server) */
	void SettleStep();

	/** Simulating but asleep, without speed (every machine) */
	void Sleep();

	/** Builds the look from the replicated snapshot (every machine) */
	void ApplyLook();

	/** Washes the corpse away once the water covers it (server) */
	void CheckFlood();

	/** A fresh corpse goes through the other physics bodies (the other pieces, the body it was cut from, a ragdoll)
	 *  until it no longer overlaps any: they never push each other away (every machine, T-155) */
	bool bGhost = false;

	/** A fresh piece was put to sleep on the ground (server, T-155) */
	bool bRested = false;

	/** Timer of the checks of a fresh corpse (every machine) */
	FTimerHandle FreshTimer;

	/** Collides again with the physics bodies once apart from them; puts a piece at rest to sleep (T-155) */
	void CheckFresh();

	/** True when the collision overlaps another simulated physics body (a piece, a corpse, a ragdoll) */
	bool OverlapsOtherBody() const;

};
