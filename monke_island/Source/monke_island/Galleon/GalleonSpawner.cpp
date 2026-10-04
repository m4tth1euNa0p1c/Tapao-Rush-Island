// monke_island

#include "Galleon/GalleonSpawner.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonRules.h"
#include "Boats/BoatActor.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Expedition/Islands/ExpeditionIslandTypes.h"
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

	/** The island catalog is waited for this many times, twice a second, before the search around the arrival */
	constexpr int32 MaxCatalogWaits = 8;

	/** Mooring of the galleon given by the island data (role Galleon of the fragment whose Start anchorage is where the
	 *  barque waits, T-089 / fragments.json galleon_anchor); false when the catalog has none in the frame of this map */
	bool FindCataloguedAnchorage(const UObject* Context, const FVector& BoatLocation, float MaxStartDistance, FVector& OutLocation)
	{
		const UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(Context);
		if (!Islands)
		{
			return false;
		}
		const UExpeditionIslandDefinition* Played = nullptr;
		double BestDistSq = FMath::Square(static_cast<double>(MaxStartDistance));
		for (const UExpeditionIslandDefinition* Definition : Islands->GetOfferedDefinitions())
		{
			FExpeditionAnchorage Start;
			if (Definition && Definition->FindAnchorageByRole(EAnchorageRole::Start, Start))
			{
				// the data is in the frame of this map only if its start is where the barque really waits
				const double DistSq = FVector::DistSquared2D(Start.Transform.GetLocation(), BoatLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Played = Definition;
				}
			}
		}
		FExpeditionAnchorage Mooring;
		if (!Played || !Played->FindAnchorageByRole(EAnchorageRole::Galleon, Mooring))
		{
			return false;
		}
		OutLocation = Mooring.Transform.GetLocation();
		return true;
	}
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

	// where the land lies around the arrival: the ship anchors on the other side, never between the barque and the island
	FVector2D Land = FVector2D::ZeroVector;
	for (int32 Index = 0; Index < Directions; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / Directions;
		const FVector2D Ray(FMath::Cos(Angle), FMath::Sin(Angle));
		for (float Distance = Rules->SearchStep; Distance <= Rules->MaxDistanceFromArrival; Distance += Rules->SearchStep * 4.0f)
		{
			if (DepthAt(Arrival + Ray * Distance, Sea, Boat) <= 0.0f)
			{
				Land += Ray / FMath::Max(Distance, 1.0f);
				break;
			}
		}
	}
	const float Seaward = Land.IsNearlyZero() ? 0.0f : FMath::Atan2(-Land.Y, -Land.X);

	for (float Distance = Rules->MinDistanceFromArrival; Distance <= Rules->MaxDistanceFromArrival; Distance += Rules->SearchStep)
	{
		for (int32 Index = 0; Index < Directions; ++Index)
		{
			// seaward first, then alternately on each side of it
			const float Side = (Index % 2 == 0 ? 1.0f : -1.0f) * static_cast<float>((Index + 1) / 2);
			// the ship's +X points away from the arrival: its ladder side (-X) faces it; its bow (+Y) lies along the shore
			const float Angle = Seaward + UE_TWO_PI * Side / Directions;
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
	// the moorings are in the island catalog: a map played without the boot map has not loaded it yet
	if (UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
		Islands && !Islands->IsCatalogLoaded() && Islands->GetCatalogError() == EFragmentError::None && CatalogWaits < MaxCatalogWaits)
	{
		if (CatalogWaits++ == 0)
		{
			Islands->RequestCatalog();
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(Timer);

	FVector Location;
	float Yaw = 0.0f;
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	const float Sea = Water ? Water->GetSeaLevel() : 0.0f;
	FVector Catalogued;
	if (FindCataloguedAnchorage(this, Boat->GetActorLocation(), Rules->MaxDistanceFromArrival, Catalogued)
		&& DepthAt(FVector2D(Catalogued), Sea, Boat) >= Rules->MinDepthCm)
	{
		// the mooring of the island data, the ladder side turned to the arrival
		const FVector2D Away = (FVector2D(Catalogued) - FVector2D(Boat->GetActorLocation())).GetSafeNormal();
		Location = FVector(Catalogued.X, Catalogued.Y, Sea);
		Yaw = FMath::RadiansToDegrees(FMath::Atan2(Away.Y, Away.X));
		UE_LOG(Logmonke_island, Log, TEXT("%s: mooring of the island data"), *GetName());
	}
	else if (!FindAnchorage(Boat, Location, Yaw))
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
