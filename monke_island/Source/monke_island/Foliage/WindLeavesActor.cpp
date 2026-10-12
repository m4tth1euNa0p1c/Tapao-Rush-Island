// monke_island

#include "Foliage/WindLeavesActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Foliage/WindLeavesRules.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"

namespace
{
	TAutoConsoleVariable<float> CVarWindLeavesStrength(
		TEXT("monke.WindLeaves.Strength"), -1.0f,
		TEXT("Forces the wind strength seen by the flying leaves (0 calm .. 1 storm); -1 = read MPC_Wind"));

	const FName WindStrengthName(TEXT("WindStrength"));
	const FName WindDirectionName(TEXT("WindDirection"));
}

AWindLeavesActor::AWindLeavesActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
	Random.Initialize(0x1EAF);
}

void AWindLeavesActor::Init(UWindLeavesRules* InRules)
{
	if (!InRules || Rules)
	{
		return;
	}
	Rules = InRules;
	WindParameters = Rules->WindParameters.LoadSynchronous();
	if (!WindParameters)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("WindLeaves: no wind parameters (MPC_Wind): the leaves drift with the breeze only"));
	}

	TArray<UStaticMesh*> Meshes;
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Rules->LeafMeshes)
	{
		if (UStaticMesh* Loaded = Mesh.LoadSynchronous())
		{
			Meshes.Add(Loaded);
		}
	}
	if (Meshes.IsEmpty())
	{
		// no stand-in: a placeholder mesh would fill the sky with big untextured squares
		UE_LOG(Logmonke_island, Warning, TEXT("WindLeaves: no leaf mesh in the rules (DA_WindLeavesRules.LeafMeshes): no flying leaves"));
		return;
	}
	UMaterialInterface* Material = Rules->LeafMaterial.LoadSynchronous();

	const int32 Kinds = FMath::Max(1, Meshes.Num());
	const int32 Budget = Meshes.IsEmpty() ? 0 : Rules->MaxLeaves;
	Leaves.SetNum(Budget);
	InstanceTransforms.SetNum(Meshes.Num());
	for (int32 Kind = 0; Kind < Meshes.Num(); ++Kind)
	{
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Leaves_%d"), Kind));
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetupAttachment(RootComponent);
		Component->SetAbsolute(true, true, true);
		Component->SetStaticMesh(Meshes[Kind]);
		if (Material)
		{
			for (int32 Slot = 0; Slot < Meshes[Kind]->GetStaticMaterials().Num(); ++Slot)
			{
				Component->SetMaterial(Slot, Material);
			}
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(Rules->bCastShadow);
		Component->bAffectDistanceFieldLighting = false;
		Component->bAffectDynamicIndirectLighting = false;
		Component->SetCullDistances(0, FMath::RoundToInt(Rules->CullDistance));
		Component->RegisterComponent();
		LeafComponents.Add(Component);

		// fixed instance budget: slots Kind, Kind + Kinds, ...
		const int32 Count = Budget / Kinds + (Kind < Budget % Kinds ? 1 : 0);
		TArray<FTransform>& Transforms = InstanceTransforms[Kind];
		Transforms.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.001)), Count);
		Component->AddInstances(Transforms, false, true);
	}
	for (int32 Slot = 0; Slot < Leaves.Num(); ++Slot)
	{
		Leaves[Slot].Kind = Slot % Kinds;
	}
}

int32 AWindLeavesActor::GetLiveLeafCount() const
{
	return Leaves.FilterByPredicate([](const WindLeavesSim::FLeaf& Leaf) { return Leaf.State != WindLeavesSim::EState::Dead; }).Num();
}

void AWindLeavesActor::ReadWind(float& OutStrength, FVector2D& OutDirection) const
{
	const float Yaw = FMath::DegreesToRadians(Rules->BreezeTowardYawDeg);
	OutDirection = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
	OutStrength = Rules->BreezeStrength;

	if (const UMaterialParameterCollectionInstance* Parameters = WindParameters ? GetWorld()->GetParameterCollectionInstance(WindParameters) : nullptr)
	{
		float Storm = 0.0f;
		if (Parameters->GetScalarParameterValue(WindStrengthName, Storm))
		{
			OutStrength = FMath::Max(OutStrength, Storm);
		}
		FLinearColor Direction;
		if (Parameters->GetVectorParameterValue(WindDirectionName, Direction))
		{
			const FVector2D Planar(Direction.R, Direction.G);
			if (Planar.SizeSquared() > 0.01f)
			{
				OutDirection = Planar.GetSafeNormal();
			}
		}
	}

	const float Forced = CVarWindLeavesStrength.GetValueOnGameThread();
	if (Forced >= 0.0f)
	{
		OutStrength = Forced;
	}
}

bool AWindLeavesActor::SpawnLeaf(WindLeavesSim::FLeaf& Leaf, const FVector& ViewLocation, const FVector2D& WindDirection, float WaterZ)
{
	const FVector2D Point = WindLeavesSim::SpawnPoint(FVector2D(ViewLocation), WindDirection, Rules->SpawnRadius,
		Random.GetFraction(), Random.GetFraction());

	// ground under the birth point: terrain, rocks, hard bushes, trunks (one trace per birth)
	FHitResult Hit;
	const FVector Top(Point.X, Point.Y, ViewLocation.Z + 3000.0);
	const FVector Bottom(Point.X, Point.Y, ViewLocation.Z - 6000.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WindLeafGround), false, this);
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	const float GroundZ = bGround ? static_cast<float>(Hit.ImpactPoint.Z) : WaterZ;
	if (!bGround && WaterZ < -1.0e5f)
	{
		return false;
	}

	const int32 Kind = Leaf.Kind;
	Leaf = WindLeavesSim::FLeaf();
	Leaf.Kind = Kind;
	Leaf.State = WindLeavesSim::EState::Flying;
	Leaf.GroundZ = GroundZ;
	Leaf.Seed = Random.GetFraction();
	Leaf.Location = FVector(Point.X, Point.Y, FMath::Max(GroundZ, WaterZ) + Random.FRandRange(Rules->SpawnHeight.X, Rules->SpawnHeight.Y));
	Leaf.Rotation = FRotator(Random.FRandRange(-180.0f, 180.0f), Random.FRandRange(-180.0f, 180.0f), Random.FRandRange(-180.0f, 180.0f)).Quaternion();
	Leaf.SpinAxis = Random.GetUnitVector();
	Leaf.Spin = FMath::DegreesToRadians(Random.FRandRange(Rules->SpinDegPerSecond.X, Rules->SpinDegPerSecond.Y));
	Leaf.Scale = Random.FRandRange(Rules->LeafScale.X, Rules->LeafScale.Y);
	Leaf.Life = Random.FRandRange(Rules->LifeSeconds.X, Rules->LifeSeconds.Y);
	return true;
}

void AWindLeavesActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Rules || Leaves.IsEmpty() || DeltaSeconds <= 0.0f)
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector View = PC->PlayerCameraManager->GetCameraLocation();
	// a long hitch (loading, alt-tab) must not throw the leaves across the map
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	AirTime += Dt;

	float Strength = 0.0f;
	FVector2D Direction;
	ReadWind(Strength, Direction);
	const float Storm = FMath::Clamp((Strength - Rules->BreezeStrength) / FMath::Max(0.01f, 1.0f - Rules->BreezeStrength), 0.0f, 1.0f);

	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	WindLeavesSim::FAir Air;
	Air.Wind = FVector(Direction, 0.0) * (Rules->WindSpeedAtFullStrength * Strength);
	Air.Turbulence = Rules->Turbulence * (0.5f + 0.5f * FMath::Min(Strength, 1.5f));
	Air.TurbulenceSize = Rules->TurbulenceSize;
	Air.FallSpeed = Rules->FallSpeed;
	Air.Response = Rules->AirResponse;
	Air.WaterZ = Water ? Water->GetWaterSurfaceAt(View) : -1.0e6f;
	Air.WaterDrift = Rules->WaterDrift;
	Air.Time = AirTime;

	SpawnBudget = FMath::Min(SpawnBudget + Dt * FMath::Lerp(Rules->SpawnRateCalm, Rules->SpawnRateStorm, Storm), 8.0f);
	const double RecycleSq = FMath::Square(static_cast<double>(Rules->RecycleRadius));
	int32 Births = 0;
	for (WindLeavesSim::FLeaf& Leaf : Leaves)
	{
		if (Leaf.State != WindLeavesSim::EState::Dead
			&& (!WindLeavesSim::Step(Leaf, Air, Dt) || FVector::DistSquared2D(Leaf.Location, View) > RecycleSq))
		{
			Leaf.State = WindLeavesSim::EState::Dead;
		}
		// at most 4 traces a frame
		if (Leaf.State == WindLeavesSim::EState::Dead && SpawnBudget >= 1.0f && Births < 4)
		{
			SpawnBudget -= 1.0f;
			++Births;
			if (!SpawnLeaf(Leaf, View, Direction, Air.WaterZ))
			{
				Leaf.State = WindLeavesSim::EState::Dead;
			}
		}
	}

	PushInstances(View);
}

void AWindLeavesActor::PushInstances(const FVector& ViewLocation)
{
	const int32 Kinds = LeafComponents.Num();
	if (Kinds == 0)
	{
		return;
	}
	const FTransform Hidden(FQuat::Identity, ViewLocation, FVector(0.001));
	for (int32 Slot = 0; Slot < Leaves.Num(); ++Slot)
	{
		const WindLeavesSim::FLeaf& Leaf = Leaves[Slot];
		TArray<FTransform>& Transforms = InstanceTransforms[Slot % Kinds];
		const int32 Index = Slot / Kinds;
		if (!Transforms.IsValidIndex(Index))
		{
			continue;
		}
		const float Scale = Leaf.State == WindLeavesSim::EState::Dead ? 0.0f : WindLeavesSim::FadeScale(Leaf);
		Transforms[Index] = Scale > 0.01f ? FTransform(Leaf.Rotation, Leaf.Location, FVector(Scale)) : Hidden;
	}
	for (int32 Kind = 0; Kind < Kinds; ++Kind)
	{
		if (InstanceTransforms[Kind].Num() > 0)
		{
			LeafComponents[Kind]->BatchUpdateInstancesTransforms(0, InstanceTransforms[Kind], true, true, true);
		}
	}
}
