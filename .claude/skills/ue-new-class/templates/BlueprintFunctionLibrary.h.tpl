// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 */
UCLASS()
class MONKE_ISLAND_API U{{ClassName}} : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Example pure helper callable from Blueprint */
	UFUNCTION(BlueprintPure, Category="monke_island", meta=(WorldContext="WorldContextObject"))
	static bool ExampleHelper(const UObject* WorldContextObject);
};
