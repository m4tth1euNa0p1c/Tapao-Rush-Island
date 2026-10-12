// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Run/RunLoadout.h"
#include "Run/RunSaveGame.h"
#include "Run/SaveSlotFile.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace RunLoadoutSpecHelpers
{
	/** A mob weapon the save lets come back (URunSaveGame::IsAllowedItemDefinition) */
	const TCHAR* SpearPath = TEXT("/Game/MonkeIsland/Weapons/MobWeapons/DA_Item_Spear.DA_Item_Spear");

	/** Writer of the build before T-314: the loadout items have neither BaseValue nor Value (format 1) */
	class FWithoutItemValuesWriter final : public FMemoryWriter
	{
	public:

		explicit FWithoutItemValuesWriter(TArray<uint8>& InBytes)
			: FMemoryWriter(InBytes, /*bIsPersistent*/ true)
		{
		}

		virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
		{
			return InProperty && InProperty->GetOwnerStruct() == FRunLoadoutItem::StaticStruct()
				&& (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FRunLoadoutItem, BaseValue)
					|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FRunLoadoutItem, Value));
		}
	};

	/** A run whose player carries a spear found at x1.5 (100 -> 150) and knocked about (110 left), and a musket */
	URunSaveGame* MakeRun()
	{
		URunSaveGame* Run = NewObject<URunSaveGame>(GetTransientPackage());
		Run->State.bActive = true;
		Run->State.ZoneIndex = 1;
		Run->State.DaysPlayed = 2;
		FRunLoadout& Loadout = Run->Loadouts.Add(TEXT("Player-1"));
		FRunLoadoutItem& Spear = Loadout.Items.AddDefaulted_GetRef();
		Spear.Slot = 0;
		Spear.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(SpearPath));
		Spear.SetKeptValue(150, 110);
		FRunLoadoutItem& Musket = Loadout.Items.AddDefaulted_GetRef();
		Musket.Slot = 1;
		Musket.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Weapons/Musket/DA_Item_Musket.DA_Item_Musket")));
		Musket.LoadedRounds = 1;
		Musket.ReserveRounds = 12;
		return Run;
	}

	/**
	 *  The whole file the build before T-314 wrote for this run: envelope of schema 1 (the layout of MonkeSaveSlot::Encode,
	 *  its size and checksum made for this payload), then the engine header and the properties without the item values.
	 *  Empty when the engine header could not be told from the properties.
	 */
	TArray<uint8> EncodeFormat1(URunSaveGame& Run)
	{
		// the engine header is what UGameplayStatics::SaveGameToMemory writes before the properties
		TArray<uint8> Whole;
		UGameplayStatics::SaveGameToMemory(&Run, Whole);
		TArray<uint8> Properties;
		{
			FMemoryWriter Writer(Properties, /*bIsPersistent*/ true);
			FObjectAndNameAsStringProxyArchive Ar(Writer, /*bInLoadIfFindFails*/ false);
			Run.Serialize(Ar);
		}
		const int32 HeaderBytes = Whole.Num() - Properties.Num();
		if (HeaderBytes <= 0 || FMemory::Memcmp(Whole.GetData() + HeaderBytes, Properties.GetData(), Properties.Num()) != 0)
		{
			return TArray<uint8>();
		}
		TArray<uint8> Payload(Whole.GetData(), HeaderBytes);
		{
			TArray<uint8> Older;
			FWithoutItemValuesWriter Writer(Older);
			FObjectAndNameAsStringProxyArchive Ar(Writer, /*bInLoadIfFindFails*/ false);
			Run.Serialize(Ar);
			Payload.Append(Older);
		}

		// int32 0 | uint32 magic | uint16 envelope version | uint16 flags | int32 schema, then this payload's size and CRC32
		const TValueOrError<TArray<uint8>, ESaveSlotError> Model = MonkeSaveSlot::Encode(Run, 1);
		if (Model.HasError() || Model.GetValue().Num() < 24)
		{
			return TArray<uint8>();
		}
		TArray<uint8> Bytes(Model.GetValue().GetData(), 16);
		FMemoryWriter Writer(Bytes);
		Writer.Seek(16);
		int32 PayloadBytes = Payload.Num();
		uint32 Crc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());
		Writer << PayloadBytes << Crc;
		Bytes.Append(Payload);
		return Bytes;
	}

	/** Reads a whole file as a run, null when it is refused */
	URunSaveGame* Decode(const TArray<uint8>& Bytes)
	{
		bool bLegacy = false;
		const TValueOrError<USaveGame*, ESaveSlotError> Decoded = MonkeSaveSlot::Decode(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion, bLegacy);
		return Decoded.HasValue() ? Cast<URunSaveGame>(Decoded.GetValue()) : nullptr;
	}

	/** The item of a slot in the loadout of Player-1, null when absent */
	const FRunLoadoutItem* FindItem(const URunSaveGame* Run, int32 Slot)
	{
		const FRunLoadout* Loadout = Run ? Run->Loadouts.Find(TEXT("Player-1")) : nullptr;
		return Loadout ? Loadout->Items.FindByPredicate([Slot](const FRunLoadoutItem& Item) { return Item.Slot == Slot; }) : nullptr;
	}
}

// T-314 (network review R-39 of docs/audit/revue-reseau-T268.md, after T-309): the loot value of a mob weapon goes from an
// island to the next through the run on disk, and a run written before T-314 is still read.
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Run.Loadout
BEGIN_DEFINE_SPEC(FRunLoadoutSpec, "monke_island.Run.Loadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRunLoadoutSpec)

void FRunLoadoutSpec::Define()
{
	using namespace RunLoadoutSpecHelpers;

	Describe("Value", [this]()
	{
		It("has no value of its own by default (a bought weapon, a save older than T-314)", [this]()
		{
			const FRunLoadoutItem Item;
			int32 Base = 7;
			int32 Value = 7;
			TestFalse(TEXT("no value"), Item.GetKeptValue(Base, Value));
			TestEqual(TEXT("BaseValue"), Item.BaseValue, FRunLoadoutItem::NoValue);
			TestEqual(TEXT("Value"), Item.Value, FRunLoadoutItem::NoValue);
		});

		It("keeps the value it is given, the value left never above its base", [this]()
		{
			FRunLoadoutItem Item;
			Item.SetKeptValue(150, 110);
			int32 Base = 0;
			int32 Value = 0;
			TestTrue(TEXT("a value"), Item.GetKeptValue(Base, Value));
			TestEqual(TEXT("base, zone multiplier included"), Base, 150);
			TestEqual(TEXT("value left"), Value, 110);

			Item.SetKeptValue(80, 200);
			TestTrue(TEXT("still a value"), Item.GetKeptValue(Base, Value));
			TestEqual(TEXT("value left at most the base"), Value, 80);
			Item.SetKeptValue(-5, -1);
			TestTrue(TEXT("a broken weapon is worth nothing, not 'no value'"), Item.GetKeptValue(Base, Value) && Base == 0 && Value == 0);
		});

		It("reads half a value as none", [this]()
		{
			FRunLoadoutItem Item;
			Item.BaseValue = 150;
			int32 Base = 0;
			int32 Value = 0;
			TestFalse(TEXT("Value missing"), Item.GetKeptValue(Base, Value));
			Item.BaseValue = FRunLoadoutItem::NoValue;
			Item.Value = 40;
			TestFalse(TEXT("BaseValue missing"), Item.GetKeptValue(Base, Value));
		});
	});

	Describe("Save", [this]()
	{
		It("writes format 2 and reads the value back", [this]()
		{
			TestEqual(TEXT("format of T-314"), URunSaveGame::CurrentVersion, 2);
			const TValueOrError<TArray<uint8>, ESaveSlotError> Bytes = MonkeSaveSlot::Encode(*MakeRun(), URunSaveGame::CurrentVersion);
			if (!TestTrue(TEXT("encoded"), Bytes.HasValue()))
			{
				return;
			}
			const URunSaveGame* Run = Decode(Bytes.GetValue());
			const FRunLoadoutItem* Spear = FindItem(Run, 0);
			const FRunLoadoutItem* Musket = FindItem(Run, 1);
			if (!TestNotNull(TEXT("the spear comes back"), Spear) || !TestNotNull(TEXT("the musket comes back"), Musket))
			{
				return;
			}
			int32 Base = 0;
			int32 Value = 0;
			TestTrue(TEXT("the spear keeps its value"), Spear->GetKeptValue(Base, Value) && Base == 150 && Value == 110);
			TestFalse(TEXT("the musket has none"), Musket->GetKeptValue(Base, Value));
			TestEqual(TEXT("its rounds are kept as before"), Musket->ReserveRounds, 12);
		});

		It("reads a run of format 1, written before T-314, with no value and nothing else lost", [this]()
		{
			const TArray<uint8> Bytes = EncodeFormat1(*MakeRun());
			if (!TestTrue(TEXT("a file of format 1 (engine header told from the properties)"), Bytes.Num() > 24))
			{
				return;
			}
			TestEqual(TEXT("checked whole: format 1 is older, not refused"), MonkeSaveSlot::Check(Bytes, URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion),
				ESaveSlotError::None);
			URunSaveGame* Run = Decode(Bytes);
			if (!TestNotNull(TEXT("read"), Run))
			{
				return;
			}
			int32 Fixes = 0;
			TestTrue(TEXT("accepted"), Run->SanitizeLoaded(Fixes));
			TestEqual(TEXT("nothing to fix: no value is not a damage"), Fixes, 0);
			const FRunLoadoutItem* Spear = FindItem(Run, 0);
			const FRunLoadoutItem* Musket = FindItem(Run, 1);
			if (!TestNotNull(TEXT("the spear comes back"), Spear) || !TestNotNull(TEXT("the musket comes back"), Musket))
			{
				return;
			}
			int32 Base = 0;
			int32 Value = 0;
			TestFalse(TEXT("the spear has no value: its pickup will take the value of its class, as before T-314"), Spear->GetKeptValue(Base, Value));
			TestEqual(TEXT("BaseValue left at its default"), Spear->BaseValue, FRunLoadoutItem::NoValue);
			TestEqual(TEXT("rounds"), Musket->ReserveRounds, 12);
			TestEqual(TEXT("loaded"), Musket->LoadedRounds, 1);
			TestEqual(TEXT("days played"), Run->State.DaysPlayed, 2);
		});

		It("bounds the values a file gives (security.md §3)", [this]()
		{
			URunSaveGame* Run = MakeRun();
			FRunLoadout& Loadout = Run->Loadouts.FindChecked(TEXT("Player-1"));
			Loadout.Items[0].BaseValue = URunSaveGame::MaxCrowns;
			Loadout.Items[0].Value = URunSaveGame::MaxCrowns;
			FRunLoadoutItem& Over = Loadout.Items.AddDefaulted_GetRef();
			Over.Slot = 2;
			Over.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(SpearPath));
			Over.BaseValue = 50;
			Over.Value = 900;
			FRunLoadoutItem& Half = Loadout.Items.AddDefaulted_GetRef();
			Half.Slot = 3;
			Half.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(SpearPath));
			Half.BaseValue = 70;
			int32 Fixes = 0;
			TestTrue(TEXT("kept"), Run->SanitizeLoaded(Fixes));
			TestEqual(TEXT("the largest value a run counts is allowed"), Loadout.Items[0].BaseValue, URunSaveGame::MaxCrowns);
			const FRunLoadoutItem* Clamped = FindItem(Run, 2);
			const FRunLoadoutItem* None = FindItem(Run, 3);
			TestTrue(TEXT("value left brought down to its base"), Clamped && Clamped->Value == 50 && Clamped->BaseValue == 50);
			TestTrue(TEXT("half a value is none"), None && None->BaseValue == FRunLoadoutItem::NoValue && None->Value == FRunLoadoutItem::NoValue);
			TestEqual(TEXT("two changes counted"), Fixes, 2);
		});
	});
}

#endif
