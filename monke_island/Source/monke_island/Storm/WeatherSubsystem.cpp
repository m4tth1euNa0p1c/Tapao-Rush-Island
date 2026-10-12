// monke_island

#include "Storm/WeatherSubsystem.h"
#include "Storm/WeatherDirector.h"
#include "Storm/WeatherRules.h"
#include "monke_island.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** Rules loaded when DefaultGame.ini does not say otherwise */
	const TCHAR* DefaultWeatherRulesPath = TEXT("/Game/MonkeIsland/Environment/Weather/DA_WeatherRules.DA_WeatherRules");

#if !UE_BUILD_SHIPPING
	/** Director of the world a command was typed in, on the server; nullptr with a warning otherwise */
	AWeatherDirector* DebugDirector(UWorld* World)
	{
		AWeatherDirector* Director = AWeatherDirector::Get(World);
		if (!Director)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Weather: no weather director in this world (DA_WeatherRules missing?)"));
			return nullptr;
		}
		if (!Director->HasAuthority())
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Weather: the weather is the server's, type the command in the server's window"));
			return nullptr;
		}
		return Director;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdWeatherTurn(
		TEXT("monke.Weather.Turn"),
		TEXT("Turns the weather now, as a seized temple artifact would (server, not in Shipping)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AWeatherDirector* Director = DebugDirector(World))
			{
				Director->TurnWeather(TEXT("console"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWeatherClear(
		TEXT("monke.Weather.Clear"),
		TEXT("Clears the turned weather (server, not in Shipping)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AWeatherDirector* Director = DebugDirector(World))
			{
				Director->ClearWeather();
			}
		}));
#endif
}

UWeatherSubsystem::UWeatherSubsystem()
{
	RulesPath = FSoftObjectPath(DefaultWeatherRulesPath);
}

UWeatherSubsystem* UWeatherSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWeatherSubsystem>() : nullptr;
}

bool UWeatherSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWeatherSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// the server owns the weather; a client receives its director by replication
	if (InWorld.GetNetMode() == NM_Client || Director.IsValid())
	{
		return;
	}
	// a small asset of soft references: its rain and sounds load in the background on the machines that show them
	UWeatherRules* Rules = Cast<UWeatherRules>(RulesPath.TryLoad());
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Weather: %s not found, the weather never turns in this world"), *RulesPath.ToString());
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	if (AWeatherDirector* Spawned = InWorld.SpawnActor<AWeatherDirector>(AWeatherDirector::StaticClass(), FTransform::Identity, Params))
	{
		Spawned->Rules = Rules;
		Spawned->FinishSpawning(FTransform::Identity);
	}
}

void UWeatherSubsystem::RegisterDirector(AWeatherDirector* InDirector)
{
	if (Director.IsValid() && Director.Get() != InDirector)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Weather: a second director (%s) in this world, %s kept"), *GetNameSafe(InDirector), *GetNameSafe(Director.Get()));
		return;
	}
	Director = InDirector;
}

void UWeatherSubsystem::UnregisterDirector(AWeatherDirector* InDirector)
{
	if (Director.Get() == InDirector)
	{
		Director.Reset();
	}
}

float UWeatherSubsystem::GetTurnWeight() const
{
	const AWeatherDirector* Current = Director.Get();
	return Current ? Current->GetTurnWeight() : 0.0f;
}

float UWeatherSubsystem::GetWindFloor() const
{
	const AWeatherDirector* Current = Director.Get();
	return Current ? Current->GetWindFloor() : 0.0f;
}

float UWeatherSubsystem::GetSkyFactor() const
{
	const AWeatherDirector* Current = Director.Get();
	return Current ? Current->GetSkyFactor() : 0.0f;
}
