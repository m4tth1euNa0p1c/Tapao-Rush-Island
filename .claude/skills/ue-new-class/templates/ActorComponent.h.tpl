// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 */
UCLASS(ClassGroup=(monke_island), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API U{{ClassName}} : public UActorComponent
{
	GENERATED_BODY()

public:

	U{{ClassName}}();

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
