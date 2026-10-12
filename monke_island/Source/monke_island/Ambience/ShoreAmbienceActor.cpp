// monke_island

#include "Ambience/ShoreAmbienceActor.h"
#include "Ambience/ShoreAmbienceMath.h"
#include "Ambience/ShoreAmbienceRules.h"
#include "monke_island.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "Water/IslandWaterSubsystem.h"

AShoreAmbienceActor::AShoreAmbienceActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicates(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Surf = CreateDefaultSubobject<UAudioComponent>(TEXT("Surf"));
	Surf->SetupAttachment(RootComponent);
	Surf->bAutoActivate = false;
	// world placed on the shore line, it does not follow the actor
	Surf->SetUsingAbsoluteLocation(true);

	Sea = CreateDefaultSubobject<UAudioComponent>(TEXT("Sea"));
	Sea->SetupAttachment(RootComponent);
	Sea->bAutoActivate = false;
	Sea->bAllowSpatialization = false;
}

void AShoreAmbienceActor::Init(UShoreAmbienceRules* InRules)
{
	Rules = InRules;
	if (!Rules)
	{
		return;
	}

	USoundBase* SurfSound = Rules->SurfSound.LoadSynchronous();
	USoundBase* SeaSound = Rules->SeaSound.LoadSynchronous();
	if (!SurfSound && !SeaSound)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("ShoreAmbience: no surf nor sea sound in the rules, nothing to play"));
		return;
	}

	if (SurfSound)
	{
		// heard from the shore line: full within SurfInnerRadius, gone SurfFalloff further
		Surf->SetSound(SurfSound);
		Surf->bOverrideAttenuation = true;
		Surf->AttenuationOverrides.bAttenuate = true;
		Surf->AttenuationOverrides.bSpatialize = true;
		Surf->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		Surf->AttenuationOverrides.AttenuationShapeExtents = FVector(Rules->SurfInnerRadius, 0.0, 0.0);
		Surf->AttenuationOverrides.FalloffDistance = Rules->SurfFalloff;
		Surf->SetVolumeMultiplier(0.0f);
		Surf->Play(FMath::FRandRange(0.0f, 30.0f));
	}
	if (SeaSound)
	{
		Sea->SetSound(SeaSound);
		Sea->SetVolumeMultiplier(0.0f);
		Sea->Play(FMath::FRandRange(0.0f, 20.0f));
	}
	SetActorTickEnabled(true);
}

void AShoreAmbienceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Surf->Stop();
	Sea->Stop();
	Super::EndPlay(EndPlayReason);
}

bool AShoreAmbienceActor::IsWater(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	const UIslandWaterSubsystem* Water = World ? World->GetSubsystem<UIslandWaterSubsystem>() : nullptr;
	const float Surface = Water ? Water->GetWaterSurfaceAt(Location) : 0.0f;

	// the ground only: landscape, rocks, buildings (static); nothing hit = open sea
	FHitResult Hit;
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShoreProbe), false);
	const FVector Start(Location.X, Location.Y, Surface + Rules->ProbeHeight);
	const FVector End(Location.X, Location.Y, Surface - Rules->ProbeHeight);
	if (!World->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
	{
		return true;
	}
	return Hit.ImpactPoint.Z < Surface;
}

void AShoreAmbienceActor::Probe()
{
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	if (!Viewer || !Viewer->IsLocalController())
	{
		SurfTargetVolume = 0.0f;
		SeaTargetVolume = 0.0f;
		return;
	}
	FRotator ViewRotation;
	Viewer->GetPlayerViewPoint(ListenerLocation, ViewRotation);

	const int32 Directions = Rules->ProbeDirections;
	const TArray<float>& Radii = Rules->ProbeRadii;
	TArray<bool> Wet;
	Wet.SetNumUninitialized(Directions * Radii.Num());
	for (int32 Direction = 0; Direction < Directions; ++Direction)
	{
		const FVector Axis = ShoreAmbienceMath::ProbeDirection(Direction, Directions);
		for (int32 Ring = 0; Ring < Radii.Num(); ++Ring)
		{
			Wet[Direction * Radii.Num() + Ring] = IsWater(ListenerLocation + Axis * Radii[Ring]);
		}
	}
	const ShoreAmbienceMath::FShore Shore = ShoreAmbienceMath::FindShore(IsWater(ListenerLocation), Wet, Radii, Directions);

	SeaTargetVolume = Rules->SeaVolume * Shore.WaterShare;
	SurfTargetVolume = Shore.bFound ? Rules->SurfVolume : 0.0f;
	if (Shore.bFound)
	{
		FVector Point = ListenerLocation + ShoreAmbienceMath::ProbeDirection(Shore.Direction, Directions) * Shore.Distance;
		const UIslandWaterSubsystem* Water = GetWorld()->GetSubsystem<UIslandWaterSubsystem>();
		Point.Z = Water ? Water->GetWaterSurfaceAt(Point) : Point.Z;
		SurfTarget = Point;
		if (!bSurfPlaced)
		{
			Surf->SetWorldLocation(SurfTarget);
			bSurfPlaced = true;
		}
	}
}

void AShoreAmbienceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules)
	{
		return;
	}

	ProbeCountdown -= DeltaSeconds;
	if (ProbeCountdown <= 0.0f)
	{
		ProbeCountdown = Rules->ProbeInterval;
		Probe();
	}

	// the surf slides along the shore instead of jumping; volumes fade
	if (bSurfPlaced)
	{
		const FVector Location = Surf->GetComponentLocation();
		const FVector Step = (SurfTarget - Location).GetClampedToMaxSize(Rules->SurfMoveSpeed * DeltaSeconds);
		Surf->SetWorldLocation(Location + Step);
	}
	SurfCurrentVolume = FMath::FInterpTo(SurfCurrentVolume, SurfTargetVolume, DeltaSeconds, Rules->FadeSpeed);
	SeaCurrentVolume = FMath::FInterpTo(SeaCurrentVolume, SeaTargetVolume, DeltaSeconds, Rules->FadeSpeed);
	Surf->SetVolumeMultiplier(SurfCurrentVolume);
	Sea->SetVolumeMultiplier(SeaCurrentVolume);
}
