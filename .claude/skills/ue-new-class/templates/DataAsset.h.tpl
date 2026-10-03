// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 *  Data-only asset: designers tune values in the editor, code reads them.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API U{{ClassName}} : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("{{ClassName}}"), GetFName());
	}
};
