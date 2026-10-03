// monke_island

#pragma once

#include "CoreMinimal.h"
#include "{{ClassName}}.generated.h"

/** {{Description}} */
UENUM(BlueprintType)
enum class E{{EnumName}} : uint8
{
	None
};

/** {{Description}} */
USTRUCT(BlueprintType)
struct F{{StructName}}
{
	GENERATED_BODY()

	/** Example field */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data")
	float Value = 0.0f;
};
