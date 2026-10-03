// monke_island

#include "Expedition/Pacification/RoyalFlag.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ARoyalFlag::ARoyalFlag()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	// few flags per expedition, and the whole team must see a planted one
	bAlwaysRelevant = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Mesh->SetMobility(EComponentMobility::Movable);
	RootComponent = Mesh;
}

void ARoyalFlag::InitializePlanted(FName InZoneId, APawn* InPlanter)
{
	ZoneId = InZoneId;
	Planter = InPlanter;
}

void ARoyalFlag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ARoyalFlag, ZoneId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ARoyalFlag, Planter, COND_InitialOnly);
}
