// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "monke_island.h"

A{{ClassName}}::A{{ClassName}}()
{
	// Disable ticking by default; enable only if the actor really needs it
	PrimaryActorTick.bCanEverTick = false;
}

void A{{ClassName}}::BeginPlay()
{
	Super::BeginPlay();
}

void A{{ClassName}}::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}
