// monke_island

#include "Arena/ArenaZone.h"
#include "Creatures/TikiWarrior.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "monke_island.h"

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
	if (!ConfinedClass)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no ConfinedClass, nothing is held in the zone"), *GetName());
		return;
	}
	CheckMembers();
	GetWorldTimerManager().SetTimer(CheckTimer, this, &AArenaZone::CheckMembers, CheckInterval, true);
	UE_LOG(Logmonke_island, Log, TEXT("%s: arena of %.0f cm, %d %s held inside"), *GetName(), Radius, Members.Num(), *ConfinedClass->GetName());
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
	if (!World || !ConfinedClass)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const float HoldRadius = FMath::Max(0.0f, Radius - InnerMargin);
	for (TActorIterator<ACharacter> It(World, ConfinedClass); It; ++It)
	{
		ACharacter* Character = *It;
		const TWeakObjectPtr<ACharacter> Key(Character);
		if (Ignored.Contains(Key))
		{
			continue;
		}
		if (!Members.Contains(Key))
		{
			// first sight: a mob that starts outside is never pulled in
			(IsInside(Character->GetActorLocation()) ? Members : Ignored).Add(Key);
			continue;
		}

		const ATikiCreature* Tiki = Cast<ATikiCreature>(Character);
		if (Tiki && Tiki->IsDead())
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
		}
	}
}
