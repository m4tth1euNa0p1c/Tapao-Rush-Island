// monke_island

#include "Arena/ArenaZone.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/TikiWarrior.h"
#include "Fort/FortSoldier.h"
#include "Player/IslanderHealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "monke_island.h"

namespace
{
	/** Spots tried on the ground for one soldier */
	constexpr int32 GroundTries = 8;

	/** A spawned character stands this much above the ground it was found on (cm) */
	constexpr float SpawnLift = 5.0f;
}

bool ArenaZoneMath::ClampIntoCircle(const FVector& Center, float Radius, FVector& InOutLocation)
{
	const FVector2D Offset(InOutLocation.X - Center.X, InOutLocation.Y - Center.Y);
	const double Distance = Offset.Size();
	if (Distance <= Radius)
	{
		return false;
	}
	const FVector2D Direction = Distance > UE_KINDA_SMALL_NUMBER ? Offset / Distance : FVector2D(1.0, 0.0);
	const double Kept = FMath::Max(0.0, static_cast<double>(Radius));
	InOutLocation.X = Center.X + Direction.X * Kept;
	InOutLocation.Y = Center.Y + Direction.Y * Kept;
	return true;
}

AArenaZone::AArenaZone()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	ConfinedClass = ATikiWarrior::StaticClass();
	ExtraConfinedClasses.Add(AGiantCrab::StaticClass());
	ExtraConfinedClasses.Add(AFortSoldier::StaticClass());
}

bool AArenaZone::IsConfined(const ACharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	if (ConfinedClass && Character->IsA(ConfinedClass))
	{
		return true;
	}
	return ExtraConfinedClasses.ContainsByPredicate([Character](const TSubclassOf<ACharacter>& Class) { return Class && Character->IsA(Class); });
}

bool AArenaZone::IsInside(const FVector& Location) const
{
	const FVector Center = GetActorLocation();
	return FVector2D(Location.X - Center.X, Location.Y - Center.Y).SizeSquared() <= FMath::Square(static_cast<double>(Radius));
}

void AArenaZone::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	const int32 Soldiers = SpawnSoldiers();
	if (!ConfinedClass && ExtraConfinedClasses.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no ConfinedClass, nothing is held in the zone"), *GetName());
		return;
	}
	CheckMembers();
	GetWorldTimerManager().SetTimer(CheckTimer, this, &AArenaZone::CheckMembers, CheckInterval, true);
	UE_LOG(Logmonke_island, Log, TEXT("%s: arena of %.0f cm, %d member(s) held inside (%s + %d other kind(s)), %d soldier(s) spawned"), *GetName(), Radius,
		Members.Num(), ConfinedClass ? *ConfinedClass->GetName() : TEXT("-"), ExtraConfinedClasses.Num(), Soldiers);
}

int32 AArenaZone::SpawnSoldiers()
{
	TArray<TSubclassOf<AFortSoldier>> Kinds;
	for (const TSubclassOf<AFortSoldier>& Kind : SoldierClasses)
	{
		if (Kind)
		{
			Kinds.Add(Kind);
		}
	}
	// nothing set on the actor: no soldier
	if (Kinds.IsEmpty() || SoldierCount <= 0)
	{
		return 0;
	}
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation();
	const float SpawnRadius = FMath::Max(0.0f, Radius - SoldierSpawnMargin);
	const float HoldRadius = FMath::Max(0.0f, Radius - InnerMargin);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < SoldierCount; ++Index)
	{
		FVector Ground;
		if (!FindGroundPoint(SpawnRadius, Ground))
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s: no ground found for soldier %d of %d"), *GetName(), Index + 1, SoldierCount);
			continue;
		}
		const TSubclassOf<AFortSoldier> Kind = Kinds[FMath::RandRange(0, Kinds.Num() - 1)];
		const float HalfHeight = Kind->GetDefaultObject<ACharacter>()->GetDefaultHalfHeight();
		const FTransform Transform(FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), Ground + FVector(0.0f, 0.0f, HalfHeight + SpawnLift));
		AFortSoldier* Soldier = World->SpawnActorDeferred<AFortSoldier>(Kind, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Soldier)
		{
			continue;
		}
		// no fort around it: the ring keeps its goals and its steps inside (the zone holds it in any case)
		Soldier->SetLeash(Center, HoldRadius);
		Soldier->FinishSpawning(Transform);
		++Spawned;
		UE_LOG(Logmonke_island, Log, TEXT("%s: soldier %s (%s) in the arena at %s"), *GetName(), *Soldier->GetName(), *Kind->GetName(), *Ground.ToCompactString());
	}
	return Spawned;
}

bool AArenaZone::FindGroundPoint(float SpawnRadius, FVector& OutPoint) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FVector Center = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaGround), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (int32 Try = 0; Try < GroundTries; ++Try)
	{
		// drawn evenly over the disc
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = SpawnRadius * FMath::Sqrt(FMath::FRand());
		const FVector Spot = Center + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, Spot + FVector(0.0f, 0.0f, GroundTraceHeight), Spot - FVector(0.0f, 0.0f, GroundTraceHeight), Objects, Params))
		{
			OutPoint = Hit.ImpactPoint;
			return true;
		}
	}
	return false;
}

void AArenaZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	Members.Reset();
	Ignored.Reset();
	Super::EndPlay(EndPlayReason);
}

void AArenaZone::CheckMembers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const float HoldRadius = FMath::Max(0.0f, Radius - InnerMargin);
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		if (!IsConfined(Character))
		{
			continue;
		}
		const TWeakObjectPtr<ACharacter> Key(Character);
		if (Ignored.Contains(Key))
		{
			continue;
		}
		if (!Members.Contains(Key))
		{
			// first sight: a mob that starts outside is never pulled in
			const bool bInside = IsInside(Character->GetActorLocation());
			(bInside ? Members : Ignored).Add(Key);
			// a soldier of the ring (no fort around it) keeps its goals and its steps inside
			if (AFortSoldier* Soldier = bInside ? Cast<AFortSoldier>(Character) : nullptr)
			{
				Soldier->SetLeash(Center, HoldRadius);
			}
			continue;
		}

		// a body on the ground stays where it fell
		const UIslanderHealthComponent* Life = Character->FindComponentByClass<UIslanderHealthComponent>();
		if (Life && Life->IsDead())
		{
			continue;
		}

		FVector Location = Character->GetActorLocation();
		if (ArenaZoneMath::ClampIntoCircle(Center, HoldRadius, Location))
		{
			Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
			if (AFortSoldier* Soldier = Cast<AFortSoldier>(Character))
			{
				// its path is dropped, as when its fort puts it back
				Soldier->OnPutBackInside();
			}
		}
	}
}
