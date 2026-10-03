// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 */
UCLASS()
class MONKE_ISLAND_API U{{ClassName}} : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;
};
