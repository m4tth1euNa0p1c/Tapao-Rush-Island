// monke_island

#include "Save/SG_Campaign.h"

namespace
{
	/** Count map within bounds: valid keys, non-negative values */
	bool IsCountMapValid(const TMap<FName, int32>& Map, int32 MaxEntries)
	{
		if (Map.Num() > MaxEntries)
		{
			return false;
		}
		for (const TPair<FName, int32>& Pair : Map)
		{
			if (Pair.Key.IsNone() || Pair.Value < 0)
			{
				return false;
			}
		}
		return true;
	}
}

const FExpeditionLedgerEntry* USG_Campaign::FindLedgerEntry(const FGuid& ExpeditionId) const
{
	if (!ExpeditionId.IsValid())
	{
		return nullptr;
	}
	return ExpeditionLedger.FindByPredicate([&ExpeditionId](const FExpeditionLedgerEntry& Entry)
	{
		return Entry.ExpeditionId == ExpeditionId;
	});
}

bool USG_Campaign::IsExpeditionApplied(const FGuid& ExpeditionId) const
{
	return FindLedgerEntry(ExpeditionId) != nullptr;
}

bool USG_Campaign::Migrate()
{
	bool bMigrated = false;
	if (SchemaVersion < 2)
	{
		// 1 -> 2: bare ids become ledger entries; what they credited is unknown, so nothing is credited again
		for (const FGuid& Id : LegacyAppliedExpeditionIds)
		{
			if (Id.IsValid() && !IsExpeditionApplied(Id))
			{
				FExpeditionLedgerEntry& Entry = ExpeditionLedger.AddDefaulted_GetRef();
				Entry.ExpeditionId = Id;
				Entry.AppliedUtc = LastSavedUtc;
			}
		}
		LegacyAppliedExpeditionIds.Reset();
		SchemaVersion = 2;
		bMigrated = true;
	}
	return bMigrated;
}

bool USG_Campaign::ValidateLoadedData(int32 MaxMapEntries, int32 MaxLedgerEntries, int32 MaxDisplayNameLength, FString& OutReason) const
{
	if (!CampaignId.IsValid())
	{
		OutReason = TEXT("invalid campaign id");
		return false;
	}
	if (SchemaVersion < 1 || SchemaVersion > CurrentSchemaVersion)
	{
		OutReason = FString::Printf(TEXT("schema %d out of range"), SchemaVersion);
		return false;
	}
	if (DisplayName.Len() > MaxDisplayNameLength)
	{
		OutReason = TEXT("display name too long");
		return false;
	}
	if (Gold < 0)
	{
		OutReason = TEXT("negative gold");
		return false;
	}
	if (!IsCountMapValid(ReserveByCategory, MaxMapEntries) || !IsCountMapValid(HubEquipment, MaxMapEntries))
	{
		OutReason = TEXT("reserve or equipment out of bounds");
		return false;
	}
	if (ExpeditionInProgress.IsActive() && !IsCountMapValid(ExpeditionInProgress.CommittedEquipment, MaxMapEntries))
	{
		OutReason = TEXT("committed equipment out of bounds");
		return false;
	}
	if (ExpeditionLedger.Num() > MaxLedgerEntries || LegacyAppliedExpeditionIds.Num() > MaxLedgerEntries)
	{
		OutReason = TEXT("ledger too long");
		return false;
	}
	for (const FExpeditionLedgerEntry& Entry : ExpeditionLedger)
	{
		if (!Entry.ExpeditionId.IsValid() || Entry.ReserveDeltaByCategory.Num() > MaxMapEntries || !IsCountMapValid(Entry.LostEquipment, MaxMapEntries))
		{
			OutReason = TEXT("ledger entry out of bounds");
			return false;
		}
	}
	return true;
}
