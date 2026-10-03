// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Creatures/TikiBody.h"
#include "TikiCorpse.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UPoseableMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
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
};

/**
 *  Corpse of a tiki, or a piece of one: the body frozen in the pose it died in, on a physics box the players can
 *  grab (one drags a whole corpse, two lift it) and stow in the boat, which counts its mass. A cut blow of the
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
	static FTikiBodySnapshot Snapshot(USkeletalMeshComponent* BodyMesh, USkeletalMeshComponent* Held, const TArray<FName>& CutBones);

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

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Physics box (root): what the players grab and the boat weighs */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> Body;

	/** Body frozen in its pose */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPoseableMeshComponent> Mesh;

	/** Held item, following the body pose */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Held;

	/** Face closing the cut of a part */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Cap;

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

	/** Half size of the physics box (cm, replicated once) */
	UPROPERTY(Replicated)
	FVector BoxExtent = FVector(30.0f);

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

	/** Rules of the creatures (server) */
	UPROPERTY(Transient)
	TObjectPtr<UTikiRules> Rules;

	/** Timer of the flood check (server) */
	FTimerHandle FloodTimer;

	/** Builds the look from the replicated snapshot (every machine) */
	void ApplyLook();

	/** Washes the corpse away once the water covers it (server) */
	void CheckFlood();

};
