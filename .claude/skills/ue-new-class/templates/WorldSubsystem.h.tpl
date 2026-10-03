// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 */
UCLASS()
class MONKE_ISLAND_API U{{ClassName}} : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;
};
