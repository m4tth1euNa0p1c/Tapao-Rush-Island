// monke_island

#include "Expedition/Islands/IslandCatalog.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Expedition/Islands/FragmentRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FPrimaryAssetType UIslandCatalog::PrimaryAssetType(TEXT("IslandCatalog"));

FPrimaryAssetId UIslandCatalog::GetDefaultCatalogId()
{
	return FPrimaryAssetId(PrimaryAssetType, TEXT("DA_IslandCatalog"));
}

FPrimaryAssetId UIslandCatalog::GetPrimaryAssetId() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UIslandCatalog::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Issues;
	const EFragmentError Error = ExpeditionIslands::ValidateCatalog(this, &Issues);
	for (const FText& Issue : Issues)
	{
		if (Error != EFragmentError::None)
		{
			Context.AddError(Issue);
		}
		else
		{
			// Some entries are usable: the bad ones are skipped at runtime, flag them without blocking the save
			Context.AddWarning(Issue);
		}
	}
	if (Error != EFragmentError::None)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
