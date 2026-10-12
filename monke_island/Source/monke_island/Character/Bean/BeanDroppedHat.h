// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "BeanDroppedHat.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** One persistent, replicated hat. Only its wearer can explicitly put it back on with F. */
UCLASS()
class MONKE_ISLAND_API ABeanDroppedHat : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:
	ABeanDroppedHat();

	/** Sets the skin and launches the physical hat on the server */
	void Initialize(UStaticMesh* InMesh, const FVector& Velocity, float UpwardKick);

	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Physical root: its movement is replicated independently of the wearer */
	UPROPERTY(VisibleAnywhere, Category="Bean|Hat")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Skin selected by the server, also sent to late joining clients */
	UPROPERTY(ReplicatedUsing=OnRep_HatMesh)
	TObjectPtr<UStaticMesh> HatMesh;

	/** Mass of the loose hat in kg */
	UPROPERTY(EditDefaultsOnly, Category="Bean|Hat", meta=(ClampMin="0.01", Units="kg"))
	float MassKg = 0.3f;

	/** Maximum sideways launch velocity in cm/s */
	UPROPERTY(EditDefaultsOnly, Category="Bean|Hat", meta=(ClampMin="0"))
	float SidewaysKick = 60.0f;

	/** Maximum launch spin in degrees/s on each axis */
	UPROPERTY(EditDefaultsOnly, Category="Bean|Hat")
	FVector SpinSpeed = FVector(360.0, 360.0, 180.0);

	/** Applies the replicated mesh before enabling its physical body */
	UFUNCTION()
	void OnRep_HatMesh();
};
