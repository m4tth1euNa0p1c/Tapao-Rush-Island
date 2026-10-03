// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 */
UCLASS(Blueprintable)
class MONKE_ISLAND_API A{{ClassName}} : public AActor
{
	GENERATED_BODY()

public:

	A{{ClassName}}();

protected:

	virtual void BeginPlay() override;

public:

	virtual void Tick(float DeltaSeconds) override;
};
