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
	return PickIndex(Count, Seed, MakeArrayView(&ExcludedIndex, 1));
}

int32 ExpeditionIslands::PickIndex(int32 Count, int32 Seed, TConstArrayView<int32> RecentIndices)
{
	if (Count <= 0)
	{
		return INDEX_NONE;
	}
	TArray<int32> LastPlayed;
	LastPlayed.Init(INDEX_NONE, Count);
	for (int32 Position = 0; Position < RecentIndices.Num(); ++Position)
	{
		if (LastPlayed.IsValidIndex(RecentIndices[Position]))
		{
			LastPlayed[RecentIndices[Position]] = Position;
		}
	}
	int32 Oldest = LastPlayed[0];
	for (const int32 Position : LastPlayed)
	{
		Oldest = FMath::Min(Oldest, Position);
	}
	TArray<int32> Candidates;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (LastPlayed[Index] == Oldest)
		{
			Candidates.Add(Index);
		}
	}
	FRandomStream Stream(Seed);
	return Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
}

TValueOrError<FName, EFragmentError> ExpeditionIslands::PickFragmentId(TConstArrayView<FName> FragmentIds, int32 Seed, TConstArrayView<FName> RecentIds, int32 HistoryCount)
{
	if (FragmentIds.IsEmpty())
	{
		return MakeError(EFragmentError::CatalogEmpty);
	}
	TArray<int32> Indices;
	const int32 First = FMath::Max(0, RecentIds.Num() - FMath::Clamp(HistoryCount, 0, 1024));
	for (int32 Position = First; Position < RecentIds.Num(); ++Position)
	{
		Indices.Add(FragmentIds.IndexOfByKey(RecentIds[Position]));
	}
	return MakeValue(FragmentIds[PickIndex(FragmentIds.Num(), Seed, Indices)]);
}

void ExpeditionIslands::RememberPlayedIsland(TArray<FName>& RecentIds, FName FragmentId, int32 HistoryCount)
{
	const int32 Capacity = FMath::Clamp(HistoryCount, 0, 1024);
	if (Capacity > 0 && IsValidFragmentId(FragmentId))
	{
		RecentIds.Add(FragmentId);
	}
	if (RecentIds.Num() > Capacity)
	{
		RecentIds.RemoveAt(0, RecentIds.Num() - Capacity, EAllowShrinking::No);
	}
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
