// monke_island

#include "Foliage/WindLeavesSubsystem.h"
#include "Engine/World.h"
#include "Foliage/WindLeavesActor.h"
#include "Foliage/WindLeavesRules.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "monke_island.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWindLeaves(
		TEXT("monke.WindLeaves"), 1,
		TEXT("Leaves carried by the wind around the player: 1 on, 0 off (read when a world starts playing)"));
}

const TCHAR* UWindLeavesSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Environment/Wind/DA_WindLeavesRules.DA_WindLeavesRules");

bool UWindLeavesSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWindLeavesSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer || CVarWindLeaves.GetValueOnGameThread() == 0)
	{
		return;
	}

	UWindLeavesRules* Rules = LoadObject<UWindLeavesRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("WindLeaves: %s not found: default rules"), DefaultRulesPath);
		Rules = NewObject<UWindLeavesRules>(this);
		Rules->WindParameters = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Environment/Storm/MPC_Wind.MPC_Wind")));
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWindLeavesActor* Actor = InWorld.SpawnActor<AWindLeavesActor>(Params);
	if (Actor)
	{
		Actor->Init(Rules);
		LeavesActor = Actor;
	}
}
