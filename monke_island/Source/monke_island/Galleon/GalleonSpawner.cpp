// monke_island

#include "Galleon/GalleonSpawner.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonRules.h"
#include "Boats/BoatActor.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	/** The barque of the start is waited for this many times, twice a second */
	constexpr int32 MaxAttempts = 40;
}

AGalleonSpawner::AGalleonSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	GalleonClass = AGalleonActor::StaticClass();
}

void AGalleonSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_GalleonRules, no galleon this game"), *GetName());
		return;
	}
	GetWorldTimerManager().SetTimer(Timer, this, &AGalleonSpawner::TryAnchor, 0.5f, true, FMath::Max(Rules->SpawnDelay, 0.1f));
}

void AGalleonSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(Timer);
	Super::EndPlay(EndPlayReason);
}

float AGalleonSpawner::DepthAt(const FVector2D& Point, float SeaLevel, const AActor* Ignored) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GalleonDepth), false, this);
	Params.AddIgnoredActor(Ignored);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	// from high above: land and its vegetation stand over the sea and count as no depth at all
	const FVector Start(Point.X, Point.Y, SeaLevel + 20000.0);
	const FVector End(Point.X, Point.Y, SeaLevel - Rules->MinDepthCm - 200.0);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params) ? SeaLevel - static_cast<float>(Hit.ImpactPoint.Z) : TNumericLimits<float>::Max();
}

bool AGalleonSpawner::FindAnchorage(const ABoatActor* Boat, FVector& OutLocation, float& OutYaw) const
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	const float Sea = Water ? Water->GetSeaLevel() : 0.0f;
	const FVector2D Arrival(Boat->GetActorLocation());
	const int32 Directions = FMath::Max(Rules->SearchDirections, 4);
	for (float Distance = Rules->MinDistanceFromArrival; Distance <= Rules->MaxDistanceFromArrival; Distance += Rules->SearchStep)
	{
		for (int32 Index = 0; Index < Directions; ++Index)
		{
			// the ship's +X points away from the arrival: its ladder side (-X) faces it; its bow (+Y) lies along the shore
			const float Angle = UE_TWO_PI * Index / Directions;
			const FVector2D Away(FMath::Cos(Angle), FMath::Sin(Angle));
			const FVector2D Along(-Away.Y, Away.X);
			const FVector2D Centre = Arrival + Away * Distance;
			bool bDeep = true;
			for (const FVector2D& Offset : { FVector2D(0, 0), FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0.5, 1), FVector2D(-0.5, 1), FVector2D(0.5, -1), FVector2D(-0.5, -1),
				FVector2D(0, 2.2), FVector2D(0, -2.2) })
			{
				// length along the shore, beam toward it; the last two probes cover the dock and the far side
				const FVector2D Point = Centre + Along * (Offset.X * Rules->FootprintHalfSize.X) + Away * (Offset.Y * Rules->FootprintHalfSize.Y);
				if (DepthAt(Point, Sea, Boat) < Rules->MinDepthCm)
				{
					bDeep = false;
					break;
				}
			}
			if (bDeep)
			{
				OutLocation = FVector(Centre.X, Centre.Y, Sea);
				OutYaw = FMath::RadiansToDegrees(Angle);
				return true;
			}
		}
	}
	return false;
}

void AGalleonSpawner::TryAnchor()
{
	TActorIterator<ABoatActor> BoatIt(GetWorld());
	ABoatActor* Boat = BoatIt ? *BoatIt : nullptr;
	if (!Boat)
	{
		if (++Attempts >= MaxAttempts)
		{
			GetWorldTimerManager().ClearTimer(Timer);
			UE_LOG(Logmonke_island, Warning, TEXT("%s: no barque at the start, no galleon"), *GetName());
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(Timer);

	FVector Location;
	float Yaw = 0.0f;
	if (!FindAnchorage(Boat, Location, Yaw))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no water %.0f cm deep within %.0f m of %s, no galleon"), *GetName(),
			Rules->MinDepthCm, Rules->MaxDistanceFromArrival / 100.0f, *Boat->GetName());
		return;
	}
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location);
	AGalleonActor* Galleon = GetWorld()->SpawnActorDeferred<AGalleonActor>(GalleonClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Galleon)
	{
		return;
	}
	Galleon->Rules = Rules;
	Galleon->FinishSpawning(Transform);
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s anchored %.0f m off %s (yaw %.0f)"), *GetName(), *Galleon->GetName(),
		FVector::Dist2D(Location, Boat->GetActorLocation()) / 100.0f, *Boat->GetName(), Yaw);

	// the crew on the deck first (they leave their seats), then the barque to the dock
	if (Rules->bPlayersStartOnDeck)
	{
		int32 Index = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACharacter* Character = It->Get() ? Cast<ACharacter>(It->Get()->GetPawn()) : nullptr)
			{
				Galleon->PlaceOnDeck(Character, Index++);
			}
		}
	}
	if (Rules->bMoorBoatAtStart)
	{
		Galleon->MoorBoat(Boat);
	}
}
