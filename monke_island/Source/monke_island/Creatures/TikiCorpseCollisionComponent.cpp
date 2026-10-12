// monke_island

#include "Creatures/TikiCorpseCollisionComponent.h"
#include "Creatures/TikiMath.h"
#include "PhysicsEngine/BodySetup.h"

UTikiCorpseCollisionComponent::UTikiCorpseCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetGenerateOverlapEvents(false);
	bHiddenInGame = true;
	SetCastShadow(false);
}

void UTikiCorpseCollisionComponent::SetGeometry(const FKAggregateGeom& Geom)
{
	// a new body setup each time: nothing stale from the previous shapes
	Setup = NewObject<UBodySetup>(this, NAME_None, RF_Transient);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	Setup->AggGeom = Geom;
	BodyInstance.BodySetup = Setup;
	if (IsRegistered())
	{
		RecreatePhysicsState();
	}
	UpdateBounds();
}

const FKAggregateGeom& UTikiCorpseCollisionComponent::GetGeometry() const
{
	static const FKAggregateGeom Empty;
	return Setup ? Setup->AggGeom : Empty;
}

UBodySetup* UTikiCorpseCollisionComponent::GetBodySetup()
{
	return Setup;
}

FBoxSphereBounds UTikiCorpseCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const FBox Box = Setup ? Setup->AggGeom.CalcAABB(LocalToWorld) : FBox(ForceInit);
	return Box.IsValid ? FBoxSphereBounds(Box) : FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
}

void UTikiCorpseCollisionComponent::AddImpulse(FVector Impulse, FName BoneName, bool bVelChange)
{
	if (bVelChange || BlowSpeedLimit <= 0.0f || !IsSimulatingPhysics())
	{
		Super::AddImpulse(Impulse, BoneName, bVelChange);
		return;
	}
	const float Mass = GetMass();
	ApplyBoundedBlow(Mass > UE_KINDA_SMALL_NUMBER ? Impulse / Mass : FVector::ZeroVector);
}

void UTikiCorpseCollisionComponent::AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName)
{
	if (BlowSpeedLimit <= 0.0f || !IsSimulatingPhysics())
	{
		Super::AddImpulseAtLocation(Impulse, Location, BoneName);
		return;
	}
	const float Mass = GetMass();
	ApplyBoundedBlow(Mass > UE_KINDA_SMALL_NUMBER ? Impulse / Mass : FVector::ZeroVector);
}

void UTikiCorpseCollisionComponent::ApplyBoundedBlow(const FVector& Change)
{
	// a sabre impulse of 25000 kg cm/s gave a 4 kg arm 60 m/s: the blow only nudges it now, without spin
	WakeRigidBody();
	SetPhysicsLinearVelocity(TikiMath::BoundedBlowVelocity(GetPhysicsLinearVelocity(), Change, BlowSpeedLimit));
}
