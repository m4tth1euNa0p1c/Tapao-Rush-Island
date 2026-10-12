// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "TikiCorpseCollisionComponent.generated.h"

class UBodySetup;

/**
 *  Collision of a frozen corpse (T-151): one rigid body made of the shapes of the physics asset bodies, placed in the
 *  pose the creature died in, so the collision hugs the visible mesh (no box wider or lower than the body). Built at
 *  run time on every machine from the replicated pose; draws nothing.
 */
UCLASS(NotBlueprintable, ClassGroup=(Monke))
class MONKE_ISLAND_API UTikiCorpseCollisionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:

	UTikiCorpseCollisionComponent();

	/** Replaces the shapes (relative to this component) and rebuilds the physics body */
	void SetGeometry(const FKAggregateGeom& Geom);

	/** Shapes of the body, relative to this component */
	const FKAggregateGeom& GetGeometry() const;

	virtual UBodySetup* GetBodySetup() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	/** Highest speed a blow gives this body (cm/s, T-155); 0 or less: not bounded */
	void SetBlowSpeedLimit(float MaxSpeed) { BlowSpeedLimit = MaxSpeed; }

	/** A blow (a weapon, a punch) moves the body at BlowSpeedLimit at most and does not spin it; a velocity change
	 *  (a throw by a player) is not bounded (T-155) */
	virtual void AddImpulse(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false) override;

	/** A blow at a point moves the body at BlowSpeedLimit at most and does not spin it (T-155) */
	virtual void AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName = NAME_None) override;

private:

	/** Body setup made at run time from the shapes */
	UPROPERTY(Transient)
	TObjectPtr<UBodySetup> Setup;

	/** Highest speed a blow gives this body (cm/s); 0 or less: not bounded */
	float BlowSpeedLimit = 0.0f;

	/** Applies a blow giving the body Change (cm/s), bounded */
	void ApplyBoundedBlow(const FVector& Change);
};
