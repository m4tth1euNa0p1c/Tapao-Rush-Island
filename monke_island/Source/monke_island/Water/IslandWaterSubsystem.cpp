// monke_island

#include "Water/IslandWaterSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "monke_island.h"

const FName UIslandWaterSubsystem::WaterSurfaceTag(TEXT("IslandWaterSurface"));

UIslandWaterSubsystem* UIslandWaterSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UIslandWaterSubsystem>() : nullptr;
}

bool UIslandWaterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// only game worlds (PIE and standalone) have a playable sea
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UIslandWaterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// gather the water surfaces once, they define the initial sea level
	WaterSurfaces.Reset();
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(WaterSurfaceTag))
		{
			WaterSurfaces.Add(*It);
		}
	}

	if (WaterSurfaces.Num() > 0)
	{
		SeaLevel = static_cast<float>(WaterSurfaces[0]->GetActorLocation().Z);
	}
	else
	{
		UE_LOG(Logmonke_island, Log, TEXT("No actor tagged '%s' in %s: sea level stays at %.1f cm"),
			*WaterSurfaceTag.ToString(), *InWorld.GetName(), SeaLevel);
	}
}

void UIslandWaterSubsystem::SetSeaLevel(float NewSeaLevel)
{
	if (FMath::IsNearlyEqual(NewSeaLevel, SeaLevel))
	{
		return;
	}

	SeaLevel = NewSeaLevel;

	// water surfaces must be Movable to follow the sea level at runtime
	for (const TWeakObjectPtr<AActor>& Surface : WaterSurfaces)
	{
		if (AActor* SurfaceActor = Surface.Get())
		{
			FVector Location = SurfaceActor->GetActorLocation();
			Location.Z = SeaLevel;
			SurfaceActor->SetActorLocation(Location);
		}
	}

	OnSeaLevelChanged.Broadcast(SeaLevel);
}

float UIslandWaterSubsystem::GetWaterDepthAt(const FVector& Location) const
{
	return FMath::Max(0.0f, GetWaterSurfaceAt(Location) - static_cast<float>(Location.Z));
}

float UIslandWaterSubsystem::GetWaterSurfaceAt(const FVector& Location) const
{
	float SurfaceZ = SeaLevel;
	float LocalZ = 0.0f;
	if (SurfaceProvider && SurfaceProviderOwner.IsValid() && SurfaceProvider(Location, LocalZ))
	{
		SurfaceZ = FMath::Max(SurfaceZ, LocalZ);
	}
	return SurfaceZ;
}

void UIslandWaterSubsystem::SetSurfaceProvider(const UObject* Owner, FSurfaceProvider Provider)
{
	if (!Provider)
	{
		if (SurfaceProviderOwner.Get() == Owner)
		{
			SurfaceProviderOwner.Reset();
			SurfaceProvider.Reset();
		}
		return;
	}
	SurfaceProviderOwner = Owner;
	SurfaceProvider = MoveTemp(Provider);
}
