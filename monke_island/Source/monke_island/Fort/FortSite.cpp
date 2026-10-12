// monke_island

#include "Fort/FortSite.h"
#include "Fort/FortSiteRules.h"
#include "monke_island.h"
#include "Components/SphereComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AFortSite::AFortSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	Tags.Add(TEXT("TikiQuietZone"));

	QuietZone = CreateDefaultSubobject<USphereComponent>(TEXT("QuietZone"));
	QuietZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	QuietZone->SetSphereRadius(7000.0f);
	RootComponent = QuietZone;
}

void AFortSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFortSite, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AFortSite, LevelTransform, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AFortSite, LevelName, COND_InitialOnly);
}

bool AFortSite::IsBuilt() const
{
	return Streamed && Streamed->IsLevelVisible();
}

void AFortSite::BeginPlay()
{
	Super::BeginPlay();
	if (!Rules || Rules->FortLevel.IsNull() || LevelName.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no fort level (rules %s, level name '%s')"), *GetName(), *GetNameSafe(Rules), *LevelName);
		return;
	}
	QuietZone->SetSphereRadius(Rules->QuietRadiusCm);
	// the vegetation first, then the fort: its ground is the plateau of the level, the palms of the atoll keep off it
	Clearing.Clear(this, GetActorLocation(), Rules->ClearRadiusCm);
	bool bLoaded = false;
	Streamed = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, Rules->FortLevel, LevelTransform, bLoaded, LevelName);
	UE_LOG(Logmonke_island, Log, TEXT("%s: fort %s at %s (yaw %.0f), %d vegetation instances cleared%s"), *GetName(), *LevelName,
		*LevelTransform.GetLocation().ToCompactString(), LevelTransform.Rotator().Yaw, Clearing.Num(),
		bLoaded && Streamed ? TEXT("") : TEXT(", its level did not load"));
}

void AFortSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Streamed)
	{
		Streamed->SetIsRequestingUnloadAndRemoval(true);
		Streamed = nullptr;
	}
	Clearing.Restore();
	Super::EndPlay(EndPlayReason);
}
