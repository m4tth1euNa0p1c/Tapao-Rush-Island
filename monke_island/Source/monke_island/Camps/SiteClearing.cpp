// monke_island

#include "Camps/SiteClearing.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace
{
	/** Vegetation taken away is moved this far down: its instance index stays */
	constexpr double HiddenDepth = 100000.0;
}

void FSiteClearing::Clear(const AActor* Ignore, const FVector& Centre, double Radius)
{
	UWorld* World = Ignore ? Ignore->GetWorld() : nullptr;
	if (!World || Radius <= 0.0)
	{
		return;
	}
	TSet<UInstancedStaticMeshComponent*> Touched;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (*It == Ignore)
		{
			continue;
		}
		TArray<UInstancedStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UInstancedStaticMeshComponent* Component : Components)
		{
			if (!Component || Component->GetInstanceCount() == 0 || !Component->Bounds.GetBox().ExpandBy(Radius).IsInsideXY(Centre))
			{
				continue;
			}
			for (const int32 Index : Component->GetInstancesOverlappingSphere(Centre, Radius, true))
			{
				FTransform Instance;
				if (!Component->GetInstanceTransform(Index, Instance, true) || FVector::DistSquared2D(Instance.GetLocation(), Centre) > Radius * Radius)
				{
					continue;
				}
				FMovedInstance& Entry = Moved.AddDefaulted_GetRef();
				Entry.Component = Component;
				Entry.Index = Index;
				Entry.Original = Instance;
				FTransform Away = Instance;
				Away.AddToTranslation(FVector(0.0, 0.0, -HiddenDepth));
				Component->UpdateInstanceTransform(Index, Away, true, false, true);
				Touched.Add(Component);
			}
		}
	}
	for (UInstancedStaticMeshComponent* Component : Touched)
	{
		Component->MarkRenderStateDirty();
	}
}

void FSiteClearing::Restore()
{
	for (const FMovedInstance& Entry : Moved)
	{
		if (UInstancedStaticMeshComponent* Component = Entry.Component.Get())
		{
			Component->UpdateInstanceTransform(Entry.Index, Entry.Original, true, false, true);
		}
	}
	for (const FMovedInstance& Entry : Moved)
	{
		if (UInstancedStaticMeshComponent* Component = Entry.Component.Get())
		{
			Component->MarkRenderStateDirty();
		}
	}
	Moved.Reset();
}

FVector FSiteClearing::GroundAt(const AActor* Owner, const FVector& World, TConstArrayView<const UPrimitiveComponent*> Ignored)
{
	if (!Owner || !Owner->GetWorld())
	{
		return World;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SiteGround), false, Owner);
	for (const UPrimitiveComponent* Component : Ignored)
	{
		Params.AddIgnoredComponent(Component);
	}
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const FVector Start = World + FVector(0.0, 0.0, 500.0);
	const FVector End = World - FVector(0.0, 0.0, 1500.0);
	// instanced vegetation (moved away, or out of the zone) is skipped: only the ground counts
	for (int32 Try = 0; Try < 6; ++Try)
	{
		if (!Owner->GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
		{
			return World;
		}
		if (Hit.GetComponent() && Hit.GetComponent()->IsA<UInstancedStaticMeshComponent>())
		{
			Params.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}
		return Hit.ImpactPoint;
	}
	return World;
}
