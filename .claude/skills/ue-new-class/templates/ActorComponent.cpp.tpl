// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "monke_island.h"

U{{ClassName}}::U{{ClassName}}()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void U{{ClassName}}::BeginPlay()
{
	Super::BeginPlay();
}

void U{{ClassName}}::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
