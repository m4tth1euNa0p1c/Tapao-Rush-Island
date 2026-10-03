// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "monke_island.h"

bool U{{ClassName}}::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create in game worlds (PIE + standalone), not in editor preview worlds
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void U{{ClassName}}::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void U{{ClassName}}::Deinitialize()
{
	Super::Deinitialize();
}
