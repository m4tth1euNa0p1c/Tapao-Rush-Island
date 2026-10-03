// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicsPropActor.generated.h"

class UStaticMeshComponent;
class UValuableComponent;

/**
 *  Free physics object the players can grab, carry together, throw and break (R.E.P.O. style).
 *  Simulates on the server and replicates its movement; mass comes from MassKg.
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

protected:

	virtual void OnConstruction(const FTransform& Transform) override;

private:

	/** Physics mesh (root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Value lost on impacts */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UValuableComponent> Valuable;
};
