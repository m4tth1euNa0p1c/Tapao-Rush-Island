// monke_island

#include "Water/IslandWaterSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "monke_island.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Water/WaveRules.h"
#include "Water/WaveSurfaceActor.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWaves(
		TEXT("monke.Waves"), 1,
		TEXT("Gerstner waves on the sea: 1 on, 0 flat sea (read when a world starts playing)"));
}

const TCHAR* UIslandWaterSubsystem::DefaultWaveRulesPath = TEXT("/Game/MonkeIsland/Environment/Water/DA_WaveRules.DA_WaveRules");

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

	// waves: every machine computes them (buoyancy on the server, drawing on the clients)
	WaveRules = CVarWaves.GetValueOnGameThread() != 0 ? LoadObject<UWaveRules>(nullptr, DefaultWaveRulesPath, nullptr, LOAD_NoWarn) : nullptr;
	WaveTrains.Reset();
	if (!WaveRules)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Waves: %s missing or monke.Waves 0, flat sea"), DefaultWaveRulesPath);
		return;
	}
	WaveTrains = WaveMath::MakeTrains(WaveRules->Waves, WaveRules->Intensity, WaveRules->Gravity, UWaveRules::MaxWaves);
	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWaveSurfaceActor* Surface = InWorld.SpawnActor<AWaveSurfaceActor>(Params))
		{
			Surface->Init(WaveRules, this);
			WaveSurface = Surface;
		}
	}
}

float UIslandWaterSubsystem::GetWaveTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const double Time = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	return static_cast<float>(Time);
}

float UIslandWaterSubsystem::GetWaveHeightAt(const FVector& Location) const
{
	return WaveTrains.Num() > 0 ? WaveMath::HeightAt(WaveTrains, FVector2D(Location.X, Location.Y), GetWaveTime()) : 0.0f;
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
	float SurfaceZ = SeaLevel + GetWaveHeightAt(Location);
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
