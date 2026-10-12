// monke_island

#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/FragmentRules.h"
#include "Expedition/Islands/IslandCatalog.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "MonkeFragments"

const TCHAR* const ExpeditionIslands::FragmentOptionKey = TEXT("Island");

namespace ExpeditionIslands::Private
{
	/** Fragment ids go into travel URLs: letters, digits and underscores only */
	static bool IsValidFragmentIdString(FStringView Value)
	{
		if (Value.IsEmpty() || Value.Len() > MaxFragmentIdLength)
		{
			return false;
		}
		for (const TCHAR Char : Value)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}

	/** Records a problem and keeps the first error */
	static void AddIssue(EFragmentError& FirstError, TArray<FText>* OutIssues, EFragmentError Error, const FText& Issue)
	{
		if (FirstError == EFragmentError::None)
		{
			FirstError = Error;
		}
		if (OutIssues)
		{
			OutIssues->Add(Issue);
		}
	}
}

bool ExpeditionIslands::IsValidFragmentId(FName FragmentId)
{
	if (FragmentId.IsNone())
	{
		return false;
	}
	TStringBuilder<FName::StringBufferSize> Builder;
	FragmentId.ToString(Builder);
	return Private::IsValidFragmentIdString(Builder.ToView());
}

EFragmentError ExpeditionIslands::ValidateDefinition(const UExpeditionIslandDefinition* Definition, const UFragmentRules* Rules, TArray<FText>* OutIssues)
{
	if (!Definition)
	{
		if (OutIssues)
		{
			OutIssues->Add(LOCTEXT("NoDefinition", "No island definition."));
		}
		return EFragmentError::FragmentNotFound;
	}

	const UFragmentRules& EffectiveRules = Rules ? *Rules : *GetDefault<UFragmentRules>();
	EFragmentError FirstError = EFragmentError::None;

	if (!IsValidFragmentId(Definition->FragmentId))
	{
		Private::AddIssue(FirstError, OutIssues, EFragmentError::InvalidFragmentId,
			FText::Format(LOCTEXT("BadFragmentId", "FragmentId '{0}' is empty or malformed (letters, digits and _ only, at most {1} characters)."),
				FText::FromName(Definition->FragmentId), MaxFragmentIdLength));
	}

	if (Definition->Map.IsNull())
	{
		Private::AddIssue(FirstError, OutIssues, EFragmentError::MapMissing, LOCTEXT("NoMap", "Map is not set."));
	}

	if (Definition->ZoneCount < EffectiveRules.MinZoneCount)
	{
		Private::AddIssue(FirstError, OutIssues, EFragmentError::InvalidZoneCount,
			FText::Format(LOCTEXT("FewZones", "ZoneCount is {0}, at least {1} required."), Definition->ZoneCount, EffectiveRules.MinZoneCount));
	}

	if (Definition->Anchorages.Num() < EffectiveRules.MinAnchorageCount)
	{
		Private::AddIssue(FirstError, OutIssues, EFragmentError::InvalidAnchorages,
			FText::Format(LOCTEXT("FewAnchorages", "{0} anchorages, at least {1} required."), Definition->Anchorages.Num(), EffectiveRules.MinAnchorageCount));
	}

	TSet<FName> AnchorageIds;
	bool bHasSubmersionAnchorage = false;
	for (const FExpeditionAnchorage& Anchorage : Definition->Anchorages)
	{
		bool bAlreadyThere = false;
		AnchorageIds.Add(Anchorage.AnchorageId, &bAlreadyThere);
		if (Anchorage.AnchorageId.IsNone() || bAlreadyThere)
		{
			Private::AddIssue(FirstError, OutIssues, EFragmentError::InvalidAnchorages,
				FText::Format(LOCTEXT("BadAnchorageId", "Anchorage id '{0}' is empty or duplicated."), FText::FromName(Anchorage.AnchorageId)));
		}
		bHasSubmersionAnchorage |= Anchorage.bAvailableDuringSubmersion;
	}
	if (EffectiveRules.bRequireSubmersionAnchorage && !bHasSubmersionAnchorage)
	{
		Private::AddIssue(FirstError, OutIssues, EFragmentError::InvalidAnchorages,
			LOCTEXT("NoSubmersionAnchorage", "No anchorage is reachable during the final submersion."));
	}

	return FirstError;
}

EFragmentError ExpeditionIslands::ValidateCatalog(const UIslandCatalog* Catalog, TArray<FText>* OutIssues)
{
	if (!Catalog)
	{
		if (OutIssues)
		{
			OutIssues->Add(LOCTEXT("NoCatalog", "No island catalog."));
		}
		return EFragmentError::CatalogNotFound;
	}
	if (Catalog->Fragments.IsEmpty())
	{
		if (OutIssues)
		{
			OutIssues->Add(LOCTEXT("EmptyCatalog", "The catalog lists no fragment."));
		}
		return EFragmentError::CatalogEmpty;
	}

	TSet<FPrimaryAssetId> Seen;
	int32 ValidCount = 0;
	for (const FPrimaryAssetId& Id : Catalog->Fragments)
	{
		bool bAlreadyThere = false;
		Seen.Add(Id, &bAlreadyThere);
		if (Id.PrimaryAssetType != UExpeditionIslandDefinition::PrimaryAssetType || !IsValidFragmentId(Id.PrimaryAssetName))
		{
			if (OutIssues)
			{
				OutIssues->Add(FText::Format(LOCTEXT("BadCatalogEntry", "Entry '{0}' is not an ExpeditionIsland fragment id."), FText::FromString(Id.ToString())));
			}
		}
		else if (bAlreadyThere)
		{
			if (OutIssues)
			{
				OutIssues->Add(FText::Format(LOCTEXT("DuplicateCatalogEntry", "Entry '{0}' is listed twice."), FText::FromString(Id.ToString())));
			}
		}
		else
		{
			++ValidCount;
		}
	}
	return ValidCount > 0 ? EFragmentError::None : EFragmentError::NoValidFragment;
}

TValueOrError<FString, EFragmentError> ExpeditionIslands::BuildTravelURL(const UExpeditionIslandDefinition* Definition)
{
	if (!Definition)
	{
		return MakeError(EFragmentError::FragmentNotFound);
	}
	if (!IsValidFragmentId(Definition->FragmentId))
	{
		return MakeError(EFragmentError::InvalidFragmentId);
	}
	const FString PackageName = Definition->Map.ToSoftObjectPath().GetLongPackageName();
	if (PackageName.IsEmpty())
	{
		return MakeError(EFragmentError::MapMissing);
	}
	return MakeValue(FString::Printf(TEXT("%s?%s=%s"), *PackageName, FragmentOptionKey, *Definition->FragmentId.ToString()));
}

int32 ExpeditionIslands::PickIndex(int32 Count, int32 Seed, int32 ExcludedIndex)
{
	if (Count <= 0)
	{
		return INDEX_NONE;
	}
	if (Count == 1)
	{
		return 0;
	}
	FRandomStream Stream(Seed);
	if (ExcludedIndex < 0 || ExcludedIndex >= Count)
	{
		return Stream.RandRange(0, Count - 1);
	}
	// Draw among the others, then skip over the excluded slot: uniform over the Count - 1 remaining fragments
	const int32 Drawn = Stream.RandRange(0, Count - 2);
	return Drawn >= ExcludedIndex ? Drawn + 1 : Drawn;
}

TValueOrError<FName, EFragmentError> ExpeditionIslands::PickFragmentId(TConstArrayView<FName> FragmentIds, int32 Seed, FName PreviousFragmentId)
{
	if (FragmentIds.IsEmpty())
	{
		return MakeError(EFragmentError::CatalogEmpty);
	}
	const int32 Excluded = PreviousFragmentId.IsNone() ? INDEX_NONE : FragmentIds.IndexOfByKey(PreviousFragmentId);
	const int32 Index = PickIndex(FragmentIds.Num(), Seed, Excluded);
	if (!FragmentIds.IsValidIndex(Index))
	{
		return MakeError(EFragmentError::CatalogEmpty);
	}
	return MakeValue(FragmentIds[Index]);
}

int32 ExpeditionIslands::PickWeightedIndex(TConstArrayView<float> Weights, int32 Seed, int32 ExcludedIndex)
{
	int32 Drawable = 0;
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Drawable += Weights[Index] > 0.0f ? 1 : 0;
	}
	if (Drawable == 0)
	{
		return INDEX_NONE;
	}
	// the excluded one stays out unless it is the only drawable one
	const bool bExclude = Weights.IsValidIndex(ExcludedIndex) && Weights[ExcludedIndex] > 0.0f && Drawable > 1;
	double Total = 0.0;
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Total += Weights[Index] > 0.0f && !(bExclude && Index == ExcludedIndex) ? Weights[Index] : 0.0;
	}
	FRandomStream Stream(Seed);
	double Roll = Stream.FRand() * Total;
	int32 Last = INDEX_NONE;
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		if (Weights[Index] <= 0.0f || (bExclude && Index == ExcludedIndex))
		{
			continue;
		}
		Last = Index;
		Roll -= Weights[Index];
		if (Roll < 0.0)
		{
			return Index;
		}
	}
	return Last;
}

TValueOrError<FName, EFragmentError> ExpeditionIslands::PickWeightedFragmentId(TConstArrayView<FName> FragmentIds, TConstArrayView<float> Weights,
	int32 Seed, FName PreviousFragmentId)
{
	if (FragmentIds.IsEmpty() || Weights.Num() != FragmentIds.Num())
	{
		return MakeError(EFragmentError::CatalogEmpty);
	}
	const int32 Excluded = PreviousFragmentId.IsNone() ? INDEX_NONE : FragmentIds.IndexOfByKey(PreviousFragmentId);
	const int32 Index = PickWeightedIndex(Weights, Seed, Excluded);
	if (!FragmentIds.IsValidIndex(Index))
	{
		return MakeError(EFragmentError::CatalogEmpty);
	}
	return MakeValue(FragmentIds[Index]);
}

FName ExpeditionIslands::ParseFragmentOption(const TCHAR* OptionValue)
{
	if (!OptionValue)
	{
		return NAME_None;
	}
	const FStringView Value(OptionValue);
	return Private::IsValidFragmentIdString(Value) ? FName(Value) : NAME_None;
}

#undef LOCTEXT_NAMESPACE
