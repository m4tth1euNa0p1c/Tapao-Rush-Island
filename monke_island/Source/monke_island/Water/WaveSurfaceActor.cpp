// monke_island

#include "Water/WaveSurfaceActor.h"
#include "Water/IslandWaterSubsystem.h"
#include "Water/WaveMath.h"
#include "Water/WaveRules.h"
#include "monke_island.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "ProceduralMeshComponent.h"

AWaveSurfaceActor::AWaveSurfaceActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// after the camera moved this frame
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetReplicates(false);

	Surface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surface"));
	RootComponent = Surface;
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->bUseAsyncCooking = false;
	Surface->SetCanEverAffectNavigation(false);
}

void AWaveSurfaceActor::Init(UWaveRules* InRules, UIslandWaterSubsystem* InWater)
{
	Rules = InRules;
	Water = InWater;
	UMaterialInterface* Material = Rules ? Rules->SurfaceMaterial.LoadSynchronous() : nullptr;
	Parameters = Rules ? Rules->WaveParameters.LoadSynchronous() : nullptr;
	if (!Material || !Parameters || !InWater)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Waves: no surface material or MPC_Waves in DA_WaveRules, the sea stays flat on screen"));
		return;
	}

	// grid: dense at the camera, the spacing grows geometrically to the horizon
	const int32 Resolution = FMath::Clamp(Rules->SurfaceResolution, 16, 1024);
	GridRatio = WaveMath::SurfaceRatio(Resolution, Rules->SurfaceInnerSpacing, Rules->SurfaceHalfSize);
	TArray<float> Coordinates;
	Coordinates.SetNumUninitialized(Resolution + 1);
	for (int32 Index = 0; Index <= Resolution; ++Index)
	{
		Coordinates[Index] = WaveMath::SurfaceCoordinate(Index, Resolution, Rules->SurfaceInnerSpacing, Rules->SurfaceHalfSize);
	}
	const int32 Side = Resolution + 1;
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;
	Vertices.Reserve(Side * Side);
	Normals.Init(FVector::UpVector, Side * Side);
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), Side * Side);
	UVs.Reserve(Side * Side);
	for (int32 Y = 0; Y < Side; ++Y)
	{
		for (int32 X = 0; X < Side; ++X)
		{
			Vertices.Add(FVector(Coordinates[X], Coordinates[Y], 0.0f));
			UVs.Add(FVector2D(Coordinates[X], Coordinates[Y]) / 100.0f);
		}
	}
	Triangles.Reserve(Resolution * Resolution * 6);
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 A = Y * Side + X;
			const int32 B = A + 1;
			const int32 C = A + Side;
			const int32 D = C + 1;
			// clockwise seen from above: Unreal's front face points up
			Triangles.Append({ A, C, B, B, C, D });
		}
	}
	Surface->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
	Surface->SetMaterial(0, Material);
	// the grid stays at the world origin and the material moves it under the camera (WaveFollow): bounds wide
	// enough for any camera on the map plus the grid around it
	Surface->SetBoundsScale(2.5f);

	// the flat sea planes would cut through the troughs
	for (const TWeakObjectPtr<AActor>& Plane : InWater->GetWaterSurfaces())
	{
		if (AActor* Actor = Plane.Get())
		{
			Actor->SetActorHiddenInGame(true);
			HiddenPlanes.Add(Actor);
		}
	}

	UpdateParameters();
	SetActorTickEnabled(true);
	UE_LOG(Logmonke_island, Log, TEXT("Waves: surface of %d x %d vertices, %d trains, highest crest %.0f cm"),
		Side, Side, InWater->GetWaveTrains().Num(), WaveMath::MaxHeight(InWater->GetWaveTrains()));
}

void AWaveSurfaceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<AActor>& Plane : HiddenPlanes)
	{
		if (AActor* Actor = Plane.Get())
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AWaveSurfaceActor::UpdateParameters()
{
	const UIslandWaterSubsystem* WaterSystem = Water.Get();
	if (!Parameters || !WaterSystem || !Rules)
	{
		return;
	}
	UWorld* World = GetWorld();
	UKismetMaterialLibrary::SetScalarParameterValue(World, Parameters, TEXT("WaveTime"), WaterSystem->GetWaveTime());
	// cells of the grid: the material fades each wave where they get too wide to draw it
	UKismetMaterialLibrary::SetScalarParameterValue(World, Parameters, TEXT("WaveGridInner"), Rules->SurfaceInnerSpacing);
	UKismetMaterialLibrary::SetScalarParameterValue(World, Parameters, TEXT("WaveGridGrowth"), static_cast<float>(GridRatio - 1.0));
	const TArray<WaveMath::FTrain>& Trains = WaterSystem->GetWaveTrains();
	// foam on the crests is measured against the highest crest
	UKismetMaterialLibrary::SetScalarParameterValue(World, Parameters, TEXT("WaveMaxHeight"), FMath::Max(WaveMath::MaxHeight(Trains), 1.0f));
	for (int32 Index = 0; Index < UWaveRules::MaxWaves; ++Index)
	{
		// A = (direction X, direction Y, wave number, amplitude), B = (horizontal, angular speed, phase, wavelength)
		FLinearColor A(1.0f, 0.0f, 0.0f, 0.0f);
		FLinearColor B(0.0f, 0.0f, 0.0f, 100.0f);
		if (Trains.IsValidIndex(Index))
		{
			const WaveMath::FTrain& Train = Trains[Index];
			A = FLinearColor(Train.Direction.X, Train.Direction.Y, Train.K, Train.Amplitude);
			B = FLinearColor(Train.Horizontal, Train.Omega, Train.Phase, UE_TWO_PI / FMath::Max(Train.K, UE_KINDA_SMALL_NUMBER));
		}
		UKismetMaterialLibrary::SetVectorParameterValue(World, Parameters, FName(*FString::Printf(TEXT("Wave%dA"), Index)), A);
		UKismetMaterialLibrary::SetVectorParameterValue(World, Parameters, FName(*FString::Printf(TEXT("Wave%dB"), Index)), B);
	}
}

void AWaveSurfaceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UIslandWaterSubsystem* WaterSystem = Water.Get();
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	if (!WaterSystem || !Viewer || !Rules)
	{
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Viewer->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// the actor never moves sideways: moving it made the renderer see the whole sea slide each frame (smearing with
	// TSR / motion blur). The material shifts the grid under the camera by whole inner cells instead; the collection
	// has no previous frame value, so the sea gets no false motion vectors
	const double Snap = FMath::Max(Rules->SurfaceInnerSpacing, 1.0f);
	const FVector Origin(0.0, 0.0, WaterSystem->GetSeaLevel());
	if (!GetActorLocation().Equals(Origin, 0.1))
	{
		SetActorLocation(Origin);
	}
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), Parameters, TEXT("WaveFollow"),
		FLinearColor(static_cast<float>(FMath::GridSnap(ViewLocation.X, Snap)), static_cast<float>(FMath::GridSnap(ViewLocation.Y, Snap)), 0.0f, 0.0f));
	UpdateParameters();
}
