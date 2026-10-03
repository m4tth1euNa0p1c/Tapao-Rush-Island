// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallingCoconutActor.generated.h"

class UCoconutFallRules;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  A coconut dropped by a palm: a replicated physics body that falls, bounces and rolls (the storm wind pushes it
 *  like any free object). The first time it hits a player fast enough, the server knocks the player out (ragdoll
 *  on the ground, health loss) — once per nut.
 */
UCLASS()
class MONKE_ISLAND_API AFallingCoconutActor : public AActor
{
	GENERATED_BODY()

public:

	AFallingCoconutActor();

	/** Sets the mesh, mass and rules (server, before the first physics step) */
	void Setup(UStaticMesh* InMesh, const UCoconutFallRules* InRules);

	/** Gives the nut its starting velocity (server, after spawning) */
	void Launch(const FVector& Velocity);

	/** Whether this nut already knocked someone out */
	UFUNCTION(BlueprintPure, Category="Coconut")
	bool HasHit() const { return bHasHit; }

protected:

	virtual void BeginPlay() override;

private:

	/** Body of the nut */
	UPROPERTY(VisibleAnywhere, Category="Coconut")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Rules of the falls, set by the director */
	UPROPERTY(Transient)
	TObjectPtr<const UCoconutFallRules> Rules;

	/** Mesh picked by the server, replicated so clients show the same nut */
	UPROPERTY(ReplicatedUsing=OnRep_NutMesh)
	TObjectPtr<UStaticMesh> NutMesh;

	/** True once the nut knocked someone out */
	bool bHasHit = false;

	UFUNCTION()
	void OnRep_NutMesh();

	/** Knocks out a player hit fast enough (server) */
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
