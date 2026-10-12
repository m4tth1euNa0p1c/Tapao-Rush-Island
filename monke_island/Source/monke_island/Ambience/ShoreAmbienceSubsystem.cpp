// monke_island

#include "Ambience/ShoreAmbienceSubsystem.h"
#include "Ambience/ShoreAmbienceActor.h"
#include "Ambience/ShoreAmbienceRules.h"
#include "monke_island.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarShoreAmbience(
		TEXT("monke.ShoreAmbience"), 1,
		TEXT("Waves on the shore and open sea around the player: 1 on, 0 off (read when a world starts playing)"));
}

const TCHAR* UShoreAmbienceSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Environment/Shore/DA_ShoreAmbienceRules.DA_ShoreAmbienceRules");

bool UShoreAmbienceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UShoreAmbienceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer || CVarShoreAmbience.GetValueOnGameThread() == 0)
	{
		return;
	}
	UShoreAmbienceRules* Rules = LoadObject<UShoreAmbienceRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("ShoreAmbience: %s not found, no waves"), DefaultRulesPath);
		return;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AShoreAmbienceActor* Actor = InWorld.SpawnActor<AShoreAmbienceActor>(Params))
	{
		Actor->Init(Rules);
		AmbienceActor = Actor;
	}
}
