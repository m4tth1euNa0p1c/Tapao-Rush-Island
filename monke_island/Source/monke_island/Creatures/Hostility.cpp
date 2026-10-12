// monke_island

#include "Creatures/Hostility.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/TikiCreature.h"
#include "Fort/FortSoldier.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<float> CVarHostilityScanInterval(
		TEXT("monke.Hostility.ScanInterval"), 0.5f,
		TEXT("Seconds between two scans of the characters that take sides (the soldiers and the warriors looking for a foe read the last scan)"));
}

EFaction Hostility::FactionOf(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return EFaction::None;
	}
	if (Actor->IsA<AFortSoldier>())
	{
		return EFaction::Soldier;
	}
	if (Actor->IsA<AGiantCrab>())
	{
		return EFaction::Crab;
	}
	if (Actor->IsA<ATikiCreature>())
	{
		return EFaction::Tiki;
	}
	// the islander (the bean of the players) by its class, any other pawn a player holds
	const APawn* Pawn = Cast<APawn>(Actor);
	if (Pawn && (Pawn->IsA<AIslanderCharacter>() || Pawn->IsPlayerControlled()))
	{
		return EFaction::Player;
	}
	return EFaction::None;
}

bool Hostility::AreFoes(EFaction A, EFaction B)
{
	return A != EFaction::None && B != EFaction::None && A != B;
}

bool Hostility::IsAlive(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const UIslanderHealthComponent* Health = Actor->FindComponentByClass<UIslanderHealthComponent>();
	return Health && !Health->IsDead();
}

bool Hostility::IsLivingFoe(EFaction SelfFaction, const AActor* Other)
{
	return AreFoes(SelfFaction, FactionOf(Other)) && IsAlive(Other);
}

bool Hostility::IsLivingFoe(const AActor* Self, const AActor* Other)
{
	return Self != Other && IsLivingFoe(FactionOf(Self), Other);
}

bool FHostileCombatant::IsLivingFoeOf(EFaction SelfFaction) const
{
	const UIslanderHealthComponent* Life = Health.Get();
	return Hostility::AreFoes(SelfFaction, Faction) && Character.IsValid() && Life && !Life->IsDead();
}

UHostilitySubsystem* UHostilitySubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHostilitySubsystem>() : nullptr;
}

bool UHostilitySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

const TArray<FHostileCombatant>& UHostilitySubsystem::GetCombatants()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (LastScanTime < 0.0 || Now - LastScanTime >= FMath::Max(0.0f, CVarHostilityScanInterval.GetValueOnGameThread()))
	{
		LastScanTime = Now;
		Scan();
	}
	return Combatants;
}

void UHostilitySubsystem::Scan()
{
	Combatants.Reset();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Character = *It;
		const EFaction Faction = Hostility::FactionOf(Character);
		if (Faction == EFaction::None)
		{
			continue;
		}
		FHostileCombatant& Entry = Combatants.AddDefaulted_GetRef();
		Entry.Character = Character;
		Entry.Health = Character->FindComponentByClass<UIslanderHealthComponent>();
		Entry.Faction = Faction;
	}
}
