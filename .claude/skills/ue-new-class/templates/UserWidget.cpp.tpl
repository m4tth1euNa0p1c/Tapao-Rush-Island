// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "Components/TextBlock.h"

void U{{ClassName}}::NativeConstruct()
{
	Super::NativeConstruct();
}

void U{{ClassName}}::NativeDestruct()
{
	// unbind any delegates bound in NativeConstruct here
	Super::NativeDestruct();
}
