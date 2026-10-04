// monke_island

#include "Storm/WindStormActor.h"
#include "Components/SphereComponent.h"
#include "Storm/WindMath.h"
#include "Storm/WindRules.h"
#include "DayNight/DayNightCycleActor.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "monke_island.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

namespace
{
	/** Name of the wind drift root motion on characters */
	const FName StormDriftName(TEXT("StormWindDrift"));

	/** Actors with this tag are not pushed by the wind (dense objects in free fall, e.g. a falling coconut) */
	const FName WindShelteredTag(TEXT("WindSheltered"));
}

AWindStormActor::AWindStormActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	EngineWind = CreateDefaultSubobject<UWindDirectionalSourceComponent>(TEXT("EngineWind"));
	EngineWind->SetupAttachment(RootComponent);
	EngineWind->SetMobility(EComponentMobility::Movable);
	EngineWind->Strength = 0.0f;
}

void AWindStormActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_WindRules: using the default rules"), *GetName());
		Rules = NewObject<UWindRules>(this);
	}
	MaterialParameters = Rules->MaterialParameters.LoadSynchronous();
}

AWindStormActor* AWindStormActor::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AWindStormActor> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FVector AWindStormActor::WindAt(const UWindRules& InRules, double Elapsed, double DeadlineStartElapsed)
{
	const float Mean = WindMath::MeanStrengthAt(InRules, Elapsed, DeadlineStartElapsed);
	const float Phase = static_cast<float>(Elapsed * InRules.GustFrequency);
	// two octaves of noise inside the gust: long surges and short bursts, never below a third of the mean
	const float Turbulence = 0.65f * FMath::PerlinNoise1D(Phase) + 0.35f * FMath::PerlinNoise1D(Phase * 3.1f + 17.0f);
	const float Strength = Mean * FMath::Max(0.33f, 1.0f + InRules.GustAmplitude * Turbulence * 1.6f);
	const float Yaw = InRules.BlowTowardYawDeg + InRules.DirectionSwingDeg * FMath::PerlinNoise1D(Phase * 0.4f + 41.0f) * 1.6f;
	return FRotator(0.0, Yaw, 0.0).Vector() * Strength;
}

int32 AWindStormActor::GetGustLevel() const
{
	return Rules ? WindMath::LevelOf(*Rules, GustIndex) : 0;
}

bool AWindStormActor::IsBlowing() const
{
	return Rules && Wind.Size() >= Rules->ForceThreshold;
}

float AWindStormActor::GetShelterFactor(const AActor* Actor) const
{
	if (!Rules || !Actor)
	{
		return 1.0f;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// stale entries of destroyed actors are dropped by RefreshPushedBodies, once per refresh
	FWindShelterSample& Sample = ShelterCache.FindOrAdd(Actor);
	if (Now - Sample.Time >= Rules->ShelterRefreshInterval)
	{
		FVector Origin;
		FVector Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		Sample.Factor = ComputeShelter(Actor, FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z));
		Sample.Time = Now;
	}
	return Sample.Factor;
}

const TArray<FSphere>& AWindStormActor::GetCalmZones() const
{
	// the galleon and the water around it (owner request 2026-10-04): no wind while the crew loads the ship
	static const FName CalmZoneTag(TEXT("WindCalmZone"));
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - CalmZonesTime >= 1.0 || Now < CalmZonesTime)
	{
		CalmZonesTime = Now;
		CalmZones.Reset();
		for (TActorIterator<AActor> It(World); World && It; ++It)
		{
			if (It->ActorHasTag(CalmZoneTag))
			{
				if (const USphereComponent* Sphere = It->FindComponentByClass<USphereComponent>())
				{
					CalmZones.Emplace(Sphere->GetComponentLocation(), Sphere->GetScaledSphereRadius());
				}
			}
		}
	}
	return CalmZones;
}

float AWindStormActor::ComputeShelter(const AActor* Actor, const FVector& Feet) const
{
	// calm zones (the galleon and the water around it): no wind at all there
	for (const FSphere& Zone : GetCalmZones())
	{
		if (FVector::DistSquared2D(Feet, Zone.Center) <= FMath::Square(Zone.W))
		{
			return 0.0f;
		}
	}
	const FVector Direction = Wind.GetSafeNormal2D();
	if (Direction.IsNearlyZero() || Rules->ShelterDistance <= 0.0f)
	{
		return 1.0f;
	}
	// look upwind, at chest height: a building or a bush there breaks the wind
	const FVector Start = Feet + FVector(0.0, 0.0, Rules->ShelterHeight);
	const FVector End = Start - Direction * Rules->ShelterDistance;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WindShelter), false, Actor);
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(Rules->ShelterTraceRadius), Params);

	float Factor = 1.0f;
	for (const FHitResult& Hit : Hits)
	{
		const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		const FString Name = Mesh->GetName();
		if (WindMath::HasKeyword(Name, Rules->BuildingMeshKeywords))
		{
			Factor = FMath::Min(Factor, Rules->BuildingShelterFactor);
		}
		else if (WindMath::HasKeyword(Name, Rules->BushMeshKeywords))
		{
			Factor = FMath::Min(Factor, Rules->BushShelterFactor);
		}
	}
	return Factor;
}

FVector AWindStormActor::GetWindAccelerationOn(const AActor* Actor) const
{
	const UPrimitiveComponent* Body = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
	if (!Rules || !Body || Actor->ActorHasTag(WindShelteredTag) || Wind.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	const FVector Extent = Body->Bounds.BoxExtent;
	const float AreaM2 = static_cast<float>(4.0 * FMath::Max(Extent.X, Extent.Y) * Extent.Z / 10000.0);
	const float MassKg = FMath::Max(Body->GetMass(), 0.1f);
	const float Strength = static_cast<float>(Wind.Size()) * GetShelterFactor(Actor);
	const float Acceleration = FMath::Min(Rules->WindPressurePa * Strength * Strength * AreaM2 / MassKg, Rules->MaxObjectAcceleration);
	return Wind.GetSafeNormal() * Acceleration * 100.0f;
}

bool AWindStormActor::ResolveClock()
{
	if (ClockSource != EHazardClockSource::Undecided)
	{
		return true;
	}
	ClockSource = HazardClock::ResolveForWorld(Rules->ClockMode, GetWorld());
	if (ClockSource == EHazardClockSource::Undecided)
	{
		return false;
	}
	HazardClock::LogDecision(this, TEXT("the wind"), Rules->ClockMode, ClockSource);
	if (ClockSource == EHazardClockSource::Expedition)
	{
		Expedition = AExpeditionGameState::Get(this);
		UE_LOG(Logmonke_island, Log, TEXT("%s: the ultra violent wind sets in %.0f s before the submersion (DeadlineDay ignored)"),
			*GetName(), Rules->ExpeditionStormLeadSeconds);
	}
	return true;
}

void AWindStormActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Rules)
	{
		return;
	}
	if (!ResolveClock())
	{
		// A client waits for the replicated game state before choosing its clock
		return;
	}
	if (ClockSource == EHazardClockSource::Disabled)
	{
		Wind = FVector::ZeroVector;
		EngineWind->SetStrength(0.0f);
		EngineWind->SetSpeed(0.0f);
		SetActorTickEnabled(false);
		return;
	}

	ADayNightCycleActor* Clock = ADayNightCycleActor::Get(this);
	const int32 Day = Clock ? Clock->GetDayNumber() : 1;
	double Elapsed = 0.0;
	double DeadlineStart = -1.0;
	bool bDeadlineNow = false;
	if (ClockSource == EHazardClockSource::Expedition)
	{
		// One clock: gusts count from the start of the expedition, the ultra violent wind follows the submersion
		if (const AExpeditionGameState* GameState = Expedition.Get())
		{
			const FExpeditionClockState& State = GameState->GetClockState();
			const double Now = GameState->GetServerWorldTimeSeconds();
			const bool bConfirmed = GameState->GetPhase() >= EExpeditionPhase::Submerging
				|| (GameState->HasAuthority() && State.bStarted && Now >= State.FloodStartTime);
			Elapsed = State.bStarted ? FMath::Max(0.0, Now - State.StartTime) : 0.0;
			const double StormSeconds = WindMath::ExpeditionStormSeconds(State, Now, Rules->ExpeditionStormLeadSeconds, bConfirmed);
			DeadlineStart = WindMath::DeadlineStartElapsed(Elapsed, StormSeconds);
			bDeadlineNow = StormSeconds >= 0.0;
		}
	}
	else
	{
		Elapsed = Clock ? Clock->GetElapsedSeconds() : GetWorld()->GetTimeSeconds();
		DeadlineStart = Clock ? Clock->GetDayStartElapsed(Rules->DeadlineDay) : -1.0;
		bDeadlineNow = Clock && Day >= Rules->DeadlineDay;
	}

	const int32 PreviousGust = GustIndex;
	WindMath::MeanStrengthAt(*Rules, Elapsed, DeadlineStart, &GustIndex);
	if (GustIndex != PreviousGust && GustIndex != INDEX_NONE)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Gust %d (level %d, day %d)"), GustIndex + 1, GetGustLevel(), Day);
	}
	if (bDeadlineNow != bDeadline)
	{
		if (ClockSource == EHazardClockSource::Expedition)
		{
			UE_LOG(Logmonke_island, Log, TEXT("%s: the ultra violent wind %s (expedition clock, %.0f s before the submersion)"), *GetName(),
				bDeadlineNow ? TEXT("sets in") : TEXT("waits: the submersion was moved back"), Rules->ExpeditionStormLeadSeconds);
		}
		else if (bDeadlineNow)
		{
			UE_LOG(Logmonke_island, Log, TEXT("Day %d: the ultra violent wind sets in"), Day);
		}
	}
	bDeadline = bDeadlineNow;
	Wind = WindAt(*Rules, Elapsed, DeadlineStart);
	const float Strength = static_cast<float>(Wind.Size());

	EngineWind->SetWorldRotation(Wind.IsNearlyZero() ? FRotator::ZeroRotator : Wind.Rotation());
	EngineWind->SetStrength(Strength);
	EngineWind->SetSpeed(Rules->VisualWindSpeed * Strength);

	// foliage sway (palm trunks bend, fronds flutter) follows the gusts
	if (UMaterialParameterCollectionInstance* Parameters = MaterialParameters ? GetWorld()->GetParameterCollectionInstance(MaterialParameters) : nullptr)
	{
		const FVector Direction = Wind.GetSafeNormal2D();
		Parameters->SetScalarParameterValue(TEXT("WindStrength"), Strength);
		Parameters->SetVectorParameterValue(TEXT("WindDirection"), FLinearColor(Direction.X, Direction.Y, 0.0f, 0.0f));
	}

	if (Clock)
	{
		// the sky darkens with the gusts, fully from a full storm (strength 1) on
		Clock->SetStormFactor(Rules->StormSkyFactor * FMath::Clamp((Strength - Rules->ForceThreshold) / (1.0f - Rules->ForceThreshold), 0.0f, 1.0f));
	}

	if (Strength >= Rules->ForceThreshold)
	{
		ApplyForces(DeltaSeconds);
	}
	else
	{
		// calm: no list to keep, the next gust searches the bodies again on its first frame
		PushedBodies.Reset();
		PushedBodiesTimer = 0.0f;
	}
}

void AWindStormActor::ApplyForces(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const bool bServer = HasAuthority();

	// characters: an additive constant-force root motion, refreshed often; it survives the ground braking and is
	// predicted like any root motion (applied by the server and by the owning client)
	DriftTimer -= DeltaSeconds;
	if (DriftTimer <= 0.0f)
	{
		DriftTimer = Rules->DriftRefreshInterval;
		const FVector OpenDrift = FVector(Wind.X, Wind.Y, 0.0) * Rules->CharacterDriftSpeed;
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Character = *It;
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			if (!Movement || !(bServer || Character->IsLocallyControlled()))
			{
				continue;
			}
			// only on the ground: in the water the swim physics owns the body, in the air the jump does
			if (!Movement->IsMovingOnGround())
			{
				Movement->RemoveRootMotionSource(StormDriftName);
				continue;
			}
			Movement->RemoveRootMotionSource(StormDriftName);
			// behind a building or a bush the wind barely pushes
			const FVector Drift = OpenDrift * GetShelterFactor(Character);
			if (Drift.IsNearlyZero())
			{
				continue;
			}
			TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
			Source->InstanceName = StormDriftName;
			Source->AccumulateMode = ERootMotionAccumulateMode::Additive;
			Source->Priority = 5;
			Source->Force = Drift;
			Source->Duration = Rules->DriftRefreshInterval * 1.5f;
			Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
			Movement->ApplyRootMotionSource(Source);
		}
	}

	if (!bServer)
	{
		return;
	}
	// free physics objects: pressure times exposed area, applied by the server; WindSheltered actors (boats) are excluded
	// the drag works on the speed relative to the air: an object carried as fast as the wind is no longer pushed
	PushedBodiesTimer -= DeltaSeconds;
	if (PushedBodiesTimer <= 0.0f)
	{
		PushedBodiesTimer = Rules->PushedBodiesRefreshInterval;
		RefreshPushedBodies();
	}
	const float ReferenceSpeed = FMath::Max(Rules->VisualWindSpeed, 1.0f);
	const FVector AirVelocity = Wind * ReferenceSpeed;
	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : PushedBodies)
	{
		UPrimitiveComponent* Body = Weak.Get();
		AActor* BodyOwner = Body ? Body->GetOwner() : nullptr;
		if (!BodyOwner || !Body->IsSimulatingPhysics() || BodyOwner->ActorHasTag(WindShelteredTag))
		{
			continue;
		}
		const FVector Relative = AirVelocity - FVector(Body->GetPhysicsLinearVelocity().X, Body->GetPhysicsLinearVelocity().Y, 0.0) / 100.0;
		const float RelativeSpeed = static_cast<float>(Relative.Size());
		if (RelativeSpeed < UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector Extent = Body->Bounds.BoxExtent;
		const float AreaM2 = static_cast<float>(4.0 * FMath::Max(Extent.X, Extent.Y) * Extent.Z / 10000.0);
		const float MassKg = FMath::Max(Body->GetMass(), 0.1f);
		const float Pressure = Rules->WindPressurePa * FMath::Square(RelativeSpeed / ReferenceSpeed) * FMath::Square(GetShelterFactor(BodyOwner));
		const float Acceleration = FMath::Min(Pressure * AreaM2 / MassKg, Rules->MaxObjectAcceleration);
		Body->AddForce(Relative / RelativeSpeed * Acceleration * 100.0f, NAME_None, true);
	}
}

void AWindStormActor::RefreshPushedBodies()
{
	UWorld* World = GetWorld();
	for (auto It = ShelterCache.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	TArray<FVector, TInlineAllocator<8>> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	PushedBodies.Reset();
	const double RadiusSquared = FMath::Square(static_cast<double>(Rules->ObjectPushRadius));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(It->GetRootComponent());
		if (!Body || !Body->IsSimulatingPhysics() || It->IsA<ACharacter>())
		{
			continue;
		}
		const FVector Location = Body->GetComponentLocation();
		if (Players.ContainsByPredicate([&](const FVector& Player) { return FVector::DistSquared(Player, Location) <= RadiusSquared; }))
		{
			PushedBodies.Add(Body);
		}
	}
}
