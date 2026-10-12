// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SharkRules.generated.h"

/**
 *  Tunable rules of the shark (DA_SharkRules, cahier §8): patrol, detection, chase, attack and return.
 *  It targets swimming players only and never leaves water deep enough for it.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API USharkRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** A swimming player is noticed up to this distance (cm, cahier §8: 12 m) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Senses", meta=(ClampMin="0"))
	float DetectionRadius = 1200.0f;

	/** Pause, turned toward the swimmer, before the chase starts (s): the danger is readable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Senses", meta=(ClampMin="0", Units="s"))
	float DetectionDelay = 0.8f;

	/** The chase is given up beyond this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Senses", meta=(ClampMin="0"))
	float LoseRadius = 2500.0f;

	/** The chase is given up after this long without a bite (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Senses", meta=(ClampMin="0", Units="s"))
	float ChaseGiveUpTime = 15.0f;

	/** Damage of a bite (cahier §8: 25) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Attack", meta=(ClampMin="0"))
	float AttackDamage = 25.0f;

	/** Time between two bites (s, cahier §8: 2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Attack", meta=(ClampMin="0.1", Units="s"))
	float AttackInterval = 2.0f;

	/** Distance from the snout at which the bite lands (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Attack", meta=(ClampMin="0"))
	float AttackRange = 170.0f;

	/** Speed given to the bitten swimmer, away from the shark (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Attack", meta=(ClampMin="0"))
	float BiteKnockback = 300.0f;

	/** Cruise speed on patrol (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="0"))
	float PatrolSpeed = 220.0f;

	/** Speed in the chase (cm/s); a swimmer is slower, a sprinting swimmer about as fast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="0"))
	float ChaseSpeed = 520.0f;

	/** Speed back to the patrol area (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="0"))
	float ReturnSpeed = 320.0f;

	/** Turn rate (degrees per second) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="1"))
	float TurnRateDeg = 110.0f;

	/** Radius of the patrol area around the spawn point (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="100"))
	float PatrolRadius = 3000.0f;

	/** Water shallower than this is out of reach (cm from the surface to the bottom) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="0"))
	float MinWaterDepth = 140.0f;

	/** Cruise depth of the back under the surface (cm); the dorsal fin shows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Movement", meta=(ClampMin="0"))
	float CruiseDepth = 35.0f;

	/** Health of the shark */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark", meta=(ClampMin="1"))
	float Health = 150.0f;

	/** Time a dead shark takes to sink before it disappears (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark", meta=(ClampMin="0", Units="s"))
	float CorpseLifetime = 10.0f;

	/** Network updates per second of the shark (T-312; the engine default is 100): a client carries it on by its speed
	 *  between two updates and glides its body over the corrections */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Network", meta=(ClampMin="1", ClampMax="100"))
	float NetUpdateFrequency = 20.0f;

	/** Time a client takes to glide the body onto a new replicated place (s); 0 = it jumps there (review R-34) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Network", meta=(ClampMin="0", Units="s"))
	float ClientSmoothTime = 0.12f;

	/** Longest a client carries the shark on by its last replicated speed without news (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Network", meta=(ClampMin="0", Units="s"))
	float ClientExtrapolateTime = 0.25f;

	/** A correction farther than this is not glided: the body jumps (a spawn, a teleport) (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark|Network", meta=(ClampMin="0", Units="cm"))
	float ClientMaxSmoothDistance = 400.0f;
};
