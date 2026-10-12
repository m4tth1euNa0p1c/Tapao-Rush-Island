// monke_island

#include "Storm/WeatherDirector.h"
#include "Storm/WeatherMath.h"
#include "Storm/WeatherPresentationComponent.h"
#include "Storm/WeatherRules.h"
#include "Storm/WeatherSubsystem.h"
#include "Storm/WindRules.h"
#include "Storm/WindStormActor.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Marae/MaraeRules.h"
#include "Marae/MaraeSite.h"
#include "monke_island.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AWeatherDirector::AWeatherDirector()
{
	// the presentation ticks itself while there is weather to show; the director never ticks
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	// the state changes once a day: the server forces an update when it does
	SetNetUpdateFrequency(1.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Presentation = CreateDefaultSubobject<UWeatherPresentationComponent>(TEXT("Presentation"));
}

AWeatherDirector* AWeatherDirector::Get(const UObject* WorldContextObject)
{
	const UWeatherSubsystem* Subsystem = UWeatherSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetDirector() : nullptr;
}

void AWeatherDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AWeatherDirector, Rules, COND_InitialOnly);
	DOREPLIFETIME(AWeatherDirector, State);
}

void AWeatherDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UWeatherSubsystem* Subsystem = UWeatherSubsystem::Get(this))
	{
		Subsystem->RegisterDirector(this);
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_WeatherRules: the weather never turns"), *GetName());
		return;
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		Presentation->Prepare(Rules);
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(SeizureTimer, this, &AWeatherDirector::CheckSeizure, FMath::Max(0.05f, Rules->TriggerCheckInterval), true);
	}
	// a player arriving in an already turned weather: no sting, the rain is already there
	if (HasTurned())
	{
		ApplyState(false);
	}
}

void AWeatherDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SeizureTimer);
	if (UWeatherSubsystem* Subsystem = UWeatherSubsystem::Get(this))
	{
		Subsystem->UnregisterDirector(this);
	}
	Super::EndPlay(EndPlayReason);
}

double AWeatherDirector::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float AWeatherDirector::GetTurnWeight() const
{
	return Rules ? WeatherMath::TurnWeight(State.StartServerTime, GetServerTime(), Rules->TransitionSeconds) : 0.0f;
}

float AWeatherDirector::GetWindFloor() const
{
	return Rules ? WeatherMath::WindFloor(*Rules, GetTurnWeight()) : 0.0f;
}

float AWeatherDirector::GetSkyFactor() const
{
	return Rules ? WeatherMath::SkyFactor(*Rules, GetTurnWeight()) : 0.0f;
}

void AWeatherDirector::TurnWeather(FName Cause)
{
	if (!HasAuthority() || !Rules || HasTurned())
	{
		return;
	}
	State.StartServerTime = GetServerTime();
	State.Cause = Cause;
	ForceNetUpdate();
	GetWorldTimerManager().ClearTimer(SeizureTimer);
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s seized, the weather turns in %.0f s (wind over %.2f, sky %.2f) until the end of the day"),
		*GetName(), *Cause.ToString(), Rules->TransitionSeconds, Rules->WindStrength, Rules->SkyStormFactor);
	SpawnWindIfMissing();
	ApplyState(true);
}

void AWeatherDirector::ClearWeather()
{
	if (!HasAuthority() || !Rules || !HasTurned())
	{
		return;
	}
	State = FWeatherTurnState();
	ForceNetUpdate();
	UE_LOG(Logmonke_island, Log, TEXT("%s: weather cleared (debug)"), *GetName());
	GetWorldTimerManager().SetTimer(SeizureTimer, this, &AWeatherDirector::CheckSeizure, FMath::Max(0.05f, Rules->TriggerCheckInterval), true);
	ApplyState(false);
}

void AWeatherDirector::OnRep_State(const FWeatherTurnState& Previous)
{
	if (HasActorBegunPlay())
	{
		ApplyState(Previous.StartServerTime < 0.0 && HasTurned());
	}
}

void AWeatherDirector::ApplyState(bool bTurnedNow)
{
	if (GetNetMode() == NM_DedicatedServer || !Presentation)
	{
		return;
	}
	if (HasTurned())
	{
		Presentation->OnTurned(bTurnedNow);
	}
	else
	{
		Presentation->OnCleared();
	}
}

void AWeatherDirector::RefreshTriggerClasses()
{
	TriggerClasses.Reset();
	for (const TSoftClassPtr<AActor>& Artifact : Rules->ArtifactClasses)
	{
		// a class that is not loaded has no actor in the world to seize: nothing to load
		if (UClass* Loaded = Artifact.Get())
		{
			TriggerClasses.AddUnique(Loaded);
		}
	}
	if (Rules->bArtifactPoolsOfSites)
	{
		for (TActorIterator<AMaraeSite> It(GetWorld()); It; ++It)
		{
			if (const UMaraeRules* SiteRules = It->Rules)
			{
				for (const TSubclassOf<AActor>& Artifact : SiteRules->ArtifactPool)
				{
					if (Artifact)
					{
						TriggerClasses.AddUnique(Artifact.Get());
					}
				}
			}
		}
	}
	TriggerClassesTime = GetWorld()->GetTimeSeconds();
}

void AWeatherDirector::CheckSeizure()
{
	UWorld* World = GetWorld();
	if (!World || !Rules || HasTurned())
	{
		GetWorldTimerManager().ClearTimer(SeizureTimer);
		return;
	}
	if (World->GetTimeSeconds() - TriggerClassesTime >= Rules->SiteRefreshInterval)
	{
		RefreshTriggerClasses();
	}
	if (TriggerClasses.IsEmpty())
	{
		return;
	}
	TArray<const UClass*> Triggers;
	Triggers.Reserve(TriggerClasses.Num());
	for (const TObjectPtr<UClass>& Class : TriggerClasses)
	{
		Triggers.Add(Class.Get());
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		const UIslanderGrabComponent* Grab = Pawn ? Pawn->FindComponentByClass<UIslanderGrabComponent>() : nullptr;
		const UPrimitiveComponent* Held = Grab ? Grab->GetHeldComponent() : nullptr;
		const AActor* Seized = Held ? Held->GetOwner() : nullptr;
		// a temple places its artifacts as their owner (AMaraeSite::SpawnActors); a shrine's are not a temple's
		const bool bFromTemple = Seized && Seized->GetOwner() && Seized->GetOwner()->IsA<AMaraeSite>();
		if (Seized && (bFromTemple || !Rules->bOnlyTempleArtifacts) && WeatherMath::IsTriggerClass(Seized->GetClass(), Triggers))
		{
			UE_LOG(Logmonke_island, Log, TEXT("%s: %s holds the artifact %s"), *GetName(), *GetNameSafe(Pawn), *Seized->GetName());
			TurnWeather(Seized->GetClass()->GetFName());
			return;
		}
	}
}

void AWeatherDirector::SpawnWindIfMissing()
{
	if (AWindStormActor::Get(this))
	{
		return;
	}
	UWindRules* WindRules = Rules->WindRulesForMapsWithoutWind.LoadSynchronous();
	if (!WindRules)
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: this map has no wind and DA_WeatherRules gives none: rain and sky only"), *GetName());
		return;
	}
	// replicated and always relevant; its rules travel with it (initial replication)
	AWindStormActor* Wind = GetWorld()->SpawnActorDeferred<AWindStormActor>(AWindStormActor::StaticClass(), FTransform::Identity,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Wind)
	{
		Wind->Rules = WindRules;
		Wind->FinishSpawning(FTransform::Identity);
		UE_LOG(Logmonke_island, Log, TEXT("%s: this map had no wind, %s spawned with %s"), *GetName(), *Wind->GetName(), *WindRules->GetName());
	}
}
