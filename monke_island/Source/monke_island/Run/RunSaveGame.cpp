// monke_island

#include "Run/RunSaveGame.h"
#include "Misc/PackageName.h"

namespace
{
	/** Folder of the item definitions (DA_Item_*, docs/conventions/asset-naming.md) */
	const TCHAR* ItemDefinitionRoot = TEXT("/Game/MonkeIsland/");

	/** Prefix of an item definition asset */
	const TCHAR* ItemDefinitionPrefix = TEXT("DA_Item_");

	/** Clamps Value into [Min, Max], counting a change */
	void Bound(int32& Value, int32 Min, int32 Max, int32& Fixes)
	{
		const int32 Clamped = FMath::Clamp(Value, Min, Max);
		if (Clamped != Value)
		{
			Value = Clamped;
			++Fixes;
		}
	}

	/** An enum value read from a file is one of its names, else the fallback */
	template <typename TEnum>
	void BoundEnum(TEnum& Value, TEnum Last, TEnum Fallback, int32& Fixes)
	{
		if (static_cast<uint8>(Value) > static_cast<uint8>(Last))
		{
			Value = Fallback;
			++Fixes;
		}
	}

	/** A date that formats without trouble (0001 to 9999) */
	void BoundDate(FDateTime& Value, int32& Fixes)
	{
		if (Value.GetTicks() < FDateTime::MinValue().GetTicks() || Value.GetTicks() > FDateTime::MaxValue().GetTicks())
		{
			Value = FDateTime::MinValue();
			++Fixes;
		}
	}

	/** The loot value of a loadout item read from a file (T-314): none unless both halves are there (a save of format 1 has
	 *  neither), then BaseValue at most MaxCrowns and Value at most BaseValue */
	void BoundKeptValue(FRunLoadoutItem& Item, int32& Fixes)
	{
		int32 KeptBase = 0;
		int32 Kept = 0;
		if (!Item.GetKeptValue(KeptBase, Kept))
		{
			if (Item.BaseValue != FRunLoadoutItem::NoValue || Item.Value != FRunLoadoutItem::NoValue)
			{
				Item.BaseValue = FRunLoadoutItem::NoValue;
				Item.Value = FRunLoadoutItem::NoValue;
				++Fixes;
			}
			return;
		}
		Bound(Item.BaseValue, 0, URunSaveGame::MaxCrowns, Fixes);
		Bound(Item.Value, 0, Item.BaseValue, Fixes);
	}
}

bool URunSaveGame::IsAllowedItemDefinition(const FSoftObjectPath& Path)
{
	if (Path.IsNull() || !Path.GetSubPathString().IsEmpty())
	{
		return false;
	}
	const FString Package = Path.GetLongPackageName();
	return Package.StartsWith(ItemDefinitionRoot, ESearchCase::CaseSensitive) && Path.GetAssetName().StartsWith(ItemDefinitionPrefix, ESearchCase::CaseSensitive)
		&& !Package.Contains(TEXT("..")) && FPackageName::IsValidLongPackageName(Package);
}

bool URunSaveGame::SanitizeLoaded(int32& OutFixes)
{
	OutFixes = 0;
	if (Version > CurrentVersion || Loadouts.Num() > MaxLoadouts)
	{
		return false;
	}
	Bound(Version, 1, CurrentVersion, OutFixes);

	Bound(State.ZoneIndex, 0, MaxDays, OutFixes);
	Bound(State.ContractNumber, 1, MaxDays, OutFixes);
	Bound(State.DayInContract, 1, MaxDays, OutFixes);
	Bound(State.DaysPlayed, 0, MaxDays, OutFixes);
	Bound(State.Quota, 0, MaxCrowns, OutFixes);
	Bound(State.ContractValue, 0, MaxCrowns, OutFixes);
	Bound(State.Treasury, 0, MaxCrowns, OutFixes);
	Bound(State.TotalValue, 0, MaxCrowns, OutFixes);
	Bound(State.FailedContracts, 0, MaxDays, OutFixes);
	BoundEnum(State.Outcome, ERunOutcome::Victory, ERunOutcome::InProgress, OutFixes);

	BoundEnum(LastDayReport.Verdict, ERunDayVerdict::Executed, ERunDayVerdict::Invalid, OutFixes);
	Bound(LastDayReport.ZoneIndex, 0, MaxDays, OutFixes);
	Bound(LastDayReport.Day, 1, MaxDays, OutFixes);
	Bound(LastDayReport.DayValue, 0, MaxCrowns, OutFixes);
	Bound(LastDayReport.Deaths, 0, MaxDays, OutFixes);
	Bound(LastDayReport.MedicalFees, 0, MaxCrowns, OutFixes);
	Bound(LastDayReport.ContractValue, 0, MaxCrowns, OutFixes);
	Bound(LastDayReport.Quota, 0, MaxCrowns, OutFixes);
	Bound(LastDayReport.Payout, 0, MaxCrowns, OutFixes);
	Bound(LastDayReport.Penalty, 0, MaxCrowns, OutFixes);

	for (auto It = Loadouts.CreateIterator(); It; ++It)
	{
		FRunLoadout& Loadout = It.Value();
		if (It.Key().IsEmpty() || It.Key().Len() > MaxPlayerKeyChars || Loadout.Items.Num() > MaxLoadoutItems)
		{
			return false;
		}
		Bound(Loadout.ActiveSlot, INDEX_NONE, MaxLoadoutItems - 1, OutFixes);
		for (int32 Index = Loadout.Items.Num() - 1; Index >= 0; --Index)
		{
			FRunLoadoutItem& Item = Loadout.Items[Index];
			// only an item of the game comes back: the restore loads this path (UIslanderInventoryComponent::RestoreLoadout)
			if (!IsAllowedItemDefinition(Item.Definition.ToSoftObjectPath()))
			{
				Loadout.Items.RemoveAt(Index);
				++OutFixes;
				continue;
			}
			Bound(Item.Slot, 0, MaxLoadoutItems - 1, OutFixes);
			Bound(Item.Count, 1, MaxItemCount, OutFixes);
			Bound(Item.LoadedRounds, INDEX_NONE, MaxRounds, OutFixes);
			Bound(Item.ReserveRounds, INDEX_NONE, MaxRounds, OutFixes);
			BoundKeptValue(Item, OutFixes);
		}
	}
	BoundDate(Saved, OutFixes);
	return true;
}
