// monke_island

#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "MonkeFragments"

const FPrimaryAssetType UExpeditionIslandDefinition::PrimaryAssetType(TEXT("ExpeditionIsland"));

FPrimaryAssetId UExpeditionIslandDefinition::MakePrimaryAssetId(FName InFragmentId)
{
	return FPrimaryAssetId(PrimaryAssetType, InFragmentId);
}

FPrimaryAssetId UExpeditionIslandDefinition::GetPrimaryAssetId() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// The class is the type itself: its default object is not an island
		return FPrimaryAssetId();
	}
	// Named by the stable fragment id; a fresh asset without id still registers under its own name until it gets one
	return FPrimaryAssetId(PrimaryAssetType, FragmentId.IsNone() ? GetFName() : FragmentId);
}

bool UExpeditionIslandDefinition::FindAnchorageByRole(EAnchorageRole Role, FExpeditionAnchorage& OutAnchorage) const
{
	const FExpeditionAnchorage* Found = Anchorages.FindByPredicate([Role](const FExpeditionAnchorage& Anchorage) { return Anchorage.Role == Role; });
	if (!Found)
	{
		return false;
	}
	OutAnchorage = *Found;
	return true;
}

#if WITH_EDITOR
EDataValidationResult UExpeditionIslandDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Issues;
	if (ExpeditionIslands::ValidateDefinition(this, nullptr, &Issues) != EFragmentError::None)
	{
		for (const FText& Issue : Issues)
		{
			Context.AddError(Issue);
		}
		Result = EDataValidationResult::Invalid;
	}

	if (!FragmentId.ToString().StartsWith(TEXT("FRG_")))
	{
		Context.AddWarning(LOCTEXT("FragmentPrefix", "FragmentId should follow FRG_<Ile>_<NN> (ADR-004)."));
	}
	if (DisplayName.IsEmptyOrWhitespace())
	{
		Context.AddWarning(LOCTEXT("NoDisplayName", "DisplayName is empty: the hub would show nothing."));
	}
	if (DataCredits.IsEmptyOrWhitespace())
	{
		Context.AddWarning(LOCTEXT("NoCredits", "DataCredits is empty: cite Te Fenua / DAF (Etalab 2.0) and the IRD atlas."));
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
