// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Hostility.generated.h"

class ACharacter;
class UIslanderHealthComponent;

/** Side an actor fights for (owner request 2026-10-08: every side fights every other one) */
UENUM(BlueprintType)
enum class EFaction : uint8
{
	None,
	Player,
	Tiki,
	Soldier,
	Crab
};

namespace Hostility
{
	/** Side of an actor: a player's pawn (Player), a tiki creature (Tiki, the villagers too), a fort soldier (Soldier),
	 *  a giant crab (Crab); None for anything else. Dead or alive */
	MONKE_ISLAND_API EFaction FactionOf(const AActor* Actor);

	/** True when the two sides fight each other: every side but its own, and None fights nobody */
	MONKE_ISLAND_API bool AreFoes(EFaction A, EFaction B);

	/** True when Actor carries health and is not dead */
	MONKE_ISLAND_API bool IsAlive(const AActor* Actor);

	/** True when Other is alive and of a side SelfFaction fights */
	MONKE_ISLAND_API bool IsLivingFoe(EFaction SelfFaction, const AActor* Other);

	/** True when Other is alive and of a side Self fights (never Self itself) */
	MONKE_ISLAND_API bool IsLivingFoe(const AActor* Self, const AActor* Other);
}

/** A character that takes sides, as the registry last saw it */
struct FHostileCombatant
{
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UIslanderHealthComponent> Health;
	EFaction Faction = EFaction::None;

	/** True when it is still there, alive, and of a side SelfFaction fights */
	bool IsLivingFoeOf(EFaction SelfFaction) const;
};

/**
 *  Registry of the characters that take sides (server): the soldiers and the warriors looking for a foe read this
 *  list instead of each walking the actors of the world every time. The world is read again at most every
 *  monke.Hostility.ScanInterval seconds, so a newcomer is seen within that delay.
 */
UCLASS()
class MONKE_ISLAND_API UHostilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Subsystem of the world of an object, nullptr without a game world */
	static UHostilitySubsystem* Get(const UObject* WorldContext);

	/** Characters of a side (alive or not), read again when the last scan is older than the interval */
	const TArray<FHostileCombatant>& GetCombatants();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:

	/** Walks the characters of the world once */
	void Scan();

	TArray<FHostileCombatant> Combatants;

	/** World time of the last scan, negative before the first */
	double LastScanTime = -1.0;
};
