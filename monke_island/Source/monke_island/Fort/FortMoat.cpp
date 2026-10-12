// monke_island

#include "Fort/FortMoat.h"
#include "Fort/FortGarrisonZone.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AFortMoat::AFortMoat()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
}

void AFortMoat::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (!Garrison || !Water)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no walls or no water (garrison %s, water %s), the moat does not drown"), *GetName(),
			*GetNameSafe(Garrison), *GetNameSafe(Water));
		return;
	}
	GetWorldTimerManager().SetTimer(CheckTimer, this, &AFortMoat::Check, CheckInterval, true);
}

void AFortMoat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	InWater.Reset();
	Super::EndPlay(EndPlayReason);
}

void AFortMoat::Check()
{
	if (!Garrison || !Water)
	{
		return;
	}
	FVector WaterOrigin;
	FVector WaterExtent;
	Water->GetActorBounds(false, WaterOrigin, WaterExtent);
	const double SurfaceZ = WaterOrigin.Z + WaterExtent.Z;
	const double Now = GetWorld()->GetTimeSeconds();
	const double Reach = FVector2D(WaterExtent).Size() + MoatWidthCm;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ACharacter* Character = It->Get() ? Cast<ACharacter>(It->Get()->GetPawn()) : nullptr;
		const TWeakObjectPtr<APawn> Key(Character);
		UIslanderHealthComponent* Health = Character ? Character->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
		if (!Health || Health->IsDead() || FVector::Dist2D(Character->GetActorLocation(), WaterOrigin) > Reach)
		{
			InWater.Remove(Key);
			continue;
		}
		const FVector Location = Character->GetActorLocation();
		const FVector Feet = Location - FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const bool bInsideWalls = Garrison->IsInside(Feet);
		const double ToWalls = bInsideWalls ? 0.0 : FVector::Dist2D(Feet, Garrison->ClampInside(Feet));
		if (!FortMoatMath::IsInMoatWater(bInsideWalls, ToWalls, Feet.Z, SurfaceZ, MoatWidthCm, DrownDepthCm))
		{
			InWater.Remove(Key);
			continue;
		}
		const double* Since = InWater.Find(Key);
		if (!Since)
		{
			InWater.Add(Key, Now);
			UE_LOG(Logmonke_island, Log, TEXT("%s: %s fell into the water of the moat (%.0f cm under the surface)"), *GetName(), *Character->GetName(),
				SurfaceZ - Feet.Z);
			continue;
		}
		if (Now - *Since >= DrownSeconds)
		{
			InWater.Remove(Key);
			// the water of the moat drowns even the invulnerable player of the test map (MonkeTestGameMode), where the
			// owner tries the fort
			if (!Character->CanBeDamaged())
			{
				Character->SetCanBeDamaged(true);
			}
			Health->ApplyHealthDamage(Health->GetHealth() + 1.0f, this);
			UE_LOG(Logmonke_island, Log, TEXT("%s: %s drowned in the moat"), *GetName(), *Character->GetName());
		}
	}
}
