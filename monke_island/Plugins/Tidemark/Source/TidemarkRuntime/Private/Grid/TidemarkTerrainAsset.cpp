// Tidemark plugin

#include "Tidemark/Grid/TidemarkTerrainAsset.h"

const FPrimaryAssetType UTidemarkTerrainAsset::PrimaryAssetType(TEXT("TidemarkTerrain"));

bool FTidemarkCredit::IsComplete() const
{
	return !Producer.TrimStartAndEnd().IsEmpty()
		&& !License.TrimStartAndEnd().IsEmpty()
		&& !Url.TrimStartAndEnd().IsEmpty()
		&& !Date.TrimStartAndEnd().IsEmpty();
}

bool UTidemarkTerrainAsset::IsStale() const
{
	if (SourceHashHex.IsEmpty())
	{
		return true;
	}
	return !SourceHashHex.Equals(FTidemarkGrid::HashToHex(Grid.ComputeSourceHash()), ESearchCase::IgnoreCase);
}

bool UTidemarkTerrainAsset::HasCompleteCredits() const
{
	if (Credits.Num() == 0)
	{
		return false;
	}
	for (const FTidemarkCredit& Credit : Credits)
	{
		if (!Credit.IsComplete())
		{
			return false;
		}
	}
	return true;
}

void UTidemarkTerrainAsset::RefreshSourceHash()
{
	SourceHashHex = FTidemarkGrid::HashToHex(Grid.ComputeSourceHash());
	SchemaVersion = CurrentSchemaVersion;
}

FPrimaryAssetId UTidemarkTerrainAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
