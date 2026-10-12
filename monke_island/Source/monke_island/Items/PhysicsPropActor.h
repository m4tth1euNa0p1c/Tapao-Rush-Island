// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicsPropActor.generated.h"

class UNetRestComponent;
class UStaticMeshComponent;
class UValuableComponent;

/**
 *  Free physics object the players can grab, carry together, throw and break (R.E.P.O. style).
 *  Simulates on the server and replicates its movement; mass comes from MassKg. At rest it stops replicating, and on
 *  the rolling deck of the galleon or in the barque it rides glued to its base on every machine (UNetRestComponent,
 *  T-312 / T-294 / T-295).
 */
UCLASS()
class MONKE_ISLAND_API APhysicsPropActor : public AActor
{
	GENERATED_BODY()

public:

	APhysicsPropActor();

	/** Mass of the object (kg), sets the physics body mass */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(ClampMin="0.1"))
	float MassKg = 10.0f;

	/** Returns the mesh */
	UFUNCTION(BlueprintPure, Category="Prop")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** Returns the valuable component */
	UFUNCTION(BlueprintPure, Category="Prop")
	UValuableComponent* GetValuable() const { return Valuable; }

	/** Returns the network rest of the object (dormancy at rest, glue to a moving base) */
	UNetRestComponent* GetNetRest() const { return NetRest; }

protected:

	virtual void OnConstruction(const FTransform& Transform) override;

private:

	/** Physics mesh (root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Value lost on impacts */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UValuableComponent> Valuable;

	/** Network rest: dormant at rest, glued to the deck of the galleon or the hull of the barque (T-312, T-294, T-295) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetRestComponent> NetRest;
};
