// monke_island

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PacificationParticipant.generated.h"

/** Optional interface for actors that take part in a pacification (designated threats, officiants) */
UINTERFACE(MinimalAPI, BlueprintType)
class UPacificationParticipant : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Lets an actor tell the pacification zone whether it still counts as alive (a creature lying dead, a player down).
 *  Actors without this interface are alive while they are valid and not being destroyed.
 */
class MONKE_ISLAND_API IPacificationParticipant
{
	GENERATED_BODY()

public:

	/** Whether this actor still counts as alive for the pacification rules */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Pacification")
	bool IsPacificationAlive() const;
};
