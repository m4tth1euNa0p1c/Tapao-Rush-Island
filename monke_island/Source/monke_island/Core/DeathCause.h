// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "Templates/SubclassOf.h"
#include "DeathCause.generated.h"

class AActor;
class APlayerState;

/**
 *  What killed a player (T-135 death screen; the first values are those of T-129, telemetry, still on dev/T-129).
 *  The callers that know it give it (a fall, drowning, the deluge); a blow of a player gives Player; anything else
 *  stays Unknown and is told by the classes of the death record (UDeathRules).
 */
UENUM(BlueprintType)
enum class EDeathCause : uint8
{
	/** Not given by the blow: the classes of the record tell it */
	Unknown,
	/** A landing too hard */
	Fall,
	/** Out of stamina in the water */
	Drowning,
	/** A falling coconut */
	Coconut,
	/** A warrior of the island */
	Tiki,
	/** A shark */
	Shark,
	/** Ashore at the end of the deluge (ADR-006) */
	Flood,
	/** A blow of a player: a teammate, or oneself */
	Player,
};

/**
 *  How a player died (cahier §19: "Mort : cause courte"), recorded by the server on the fatal blow
 *  (UIslanderHealthComponent) and replicated with the death: every machine can tell the cause, even once the killer
 *  is gone. Classes rather than actors: a coconut or a projectile may be destroyed by then.
 */
USTRUCT(BlueprintType)
struct FDeathRecord
{
	GENERATED_BODY()

	/** Cause given by the blow; Unknown: the classes tell it */
	UPROPERTY(BlueprintReadOnly, Category="Death")
	EDeathCause Cause = EDeathCause::Unknown;

	/** Class of the pawn behind the fatal blow (a creature, a soldier, a player), or of the actor that dealt it when no
	 *  pawn is behind it (a falling coconut, the moat, the deluge) */
	UPROPERTY(BlueprintReadOnly, Category="Death")
	TSubclassOf<AActor> KillerClass;

	/** Class of the actor that dealt the blow (the creature itself, a weapon, a projectile) */
	UPROPERTY(BlueprintReadOnly, Category="Death")
	TSubclassOf<AActor> CauserClass;

	/** Damage type of the blow, none when the blow came without one */
	UPROPERTY(BlueprintReadOnly, Category="Death")
	TSubclassOf<UDamageType> DamageType;

	/** Player whose blow it was (a teammate, or the dead player himself), nullptr for the island and its creatures */
	UPROPERTY(BlueprintReadOnly, Category="Death")
	TObjectPtr<APlayerState> KillerPlayer;

	/** Server time of the death (s), negative while alive */
	UPROPERTY(BlueprintReadOnly, Category="Death")
	float Time = -1.0f;

	/** A death was recorded */
	bool IsSet() const { return Time >= 0.0f; }
};
