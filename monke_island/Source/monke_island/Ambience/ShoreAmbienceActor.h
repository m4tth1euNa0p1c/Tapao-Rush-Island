// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShoreAmbienceActor.generated.h"

class UAudioComponent;
class UShoreAmbienceRules;

/**
 *  Shore ambience of the local player (spawned by UShoreAmbienceSubsystem, never on a dedicated server).
 *  Every ProbeInterval it probes the ground on rings around the view (land above the water surface, water
 *  below it or nothing hit) and finds the nearest shore line: the surf loop plays in 3D there, so the waves are
 *  heard from the right side and fade away inland; the open sea loop plays around the listener, louder the more
 *  water there is around (on the beach, swimming, in the boat).
 */
UCLASS(NotPlaceable, Transient)
class MONKE_ISLAND_API AShoreAmbienceActor : public AActor
{
	GENERATED_BODY()

public:

	AShoreAmbienceActor();

	/** Starts the loops with these rules */
	void Init(UShoreAmbienceRules* InRules);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Probes the ground around the listener and sets the targets */
	void Probe();

	/** True when the ground at Location (X, Y) is under water or missing */
	bool IsWater(const FVector& Location) const;

	/** Rules */
	UPROPERTY(Transient)
	TObjectPtr<UShoreAmbienceRules> Rules;

	/** Surf on the shore line (3D) */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Surf;

	/** Open sea around the listener (2D) */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sea;

	/** Where the surf should be, its volume and the sea volume, from the last probe */
	FVector SurfTarget = FVector::ZeroVector;
	float SurfTargetVolume = 0.0f;
	float SeaTargetVolume = 0.0f;

	/** Current volumes (faded) */
	float SurfCurrentVolume = 0.0f;
	float SeaCurrentVolume = 0.0f;

	/** The surf has been placed once */
	bool bSurfPlaced = false;

	/** Time until the next probe (s) */
	float ProbeCountdown = 0.0f;

	/** Listener of the last probe */
	FVector ListenerLocation = FVector::ZeroVector;
};
