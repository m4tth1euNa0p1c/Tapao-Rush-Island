// monke_island

#include "Misc/AutomationTest.h"
#include "Cargo/CargoComponent.h"
#include "Cargo/CargoLedger.h"
#include "Cargo/CargoRules.h"

#if WITH_AUTOMATION_TESTS

namespace CargoSpecUtils
{
	/** Builds an entry with a fresh id */
	static FCargoEntry MakeEntry(EResourceCategory Category, float MassKg, int32 Value, int32 Quantity = 1)
	{
		FCargoEntry Entry;
		Entry.Id = FResourceId::NewId();
		Entry.Category = Category;
		Entry.MassKg = MassKg;
		Entry.Value = Value;
		Entry.Quantity = Quantity;
		return Entry;
	}

	/** Ledger configured with the cahier defaults (UCargoRules class defaults) */
	static FCargoLedger MakeDefaultLedger()
	{
		const UCargoRules* Rules = GetDefault<UCargoRules>();
		FCargoLedger Ledger;
		Ledger.Configure(Rules->SlotCount, Rules->MaxMassKg);
		return Ledger;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Cargo
BEGIN_DEFINE_SPEC(FCargoLedgerSpec, "monke_island.Cargo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FCargoLedger Ledger;
END_DEFINE_SPEC(FCargoLedgerSpec)

void FCargoLedgerSpec::Define()
{
	using namespace CargoSpecUtils;

	BeforeEach([this]()
	{
		Ledger = MakeDefaultLedger();
	});

	Describe("Rules", [this]()
	{
		It("defaults to the cahier capacity: 12 slots and 150 kg", [this]()
		{
			const UCargoRules* Rules = GetDefault<UCargoRules>();
			TestEqual(TEXT("slots"), Rules->SlotCount, 12);
			TestEqual(TEXT("mass"), Rules->MaxMassKg, 150.0f);
			TestEqual(TEXT("ledger slots"), Ledger.Slots.Num(), 12);
			TestEqual(TEXT("ledger mass"), Ledger.MaxMassKg, 150.0f);
		});

		It("never drops a stowed object when the capacity shrinks", [this]()
		{
			int32 Slot = INDEX_NONE;
			Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Free, 9, nullptr, Slot);
			TestEqual(TEXT("kept up to the occupied slot"), Ledger.Configure(4, 150.0f), 10);
			TestTrue(TEXT("still stowed"), Ledger.IsSlotOccupied(9));
			TestEqual(TEXT("grows for the +4 upgrade"), Ledger.Configure(16, 150.0f), 16);
			TestEqual(TEXT("bounded"), Ledger.Configure(100000, 150.0f), FCargoLedger::MaxSlotCount);
		});

		It("generates distinct resource ids", [this]()
		{
			const FResourceId A = FResourceId::NewId();
			const FResourceId B = FResourceId::NewId();
			TestTrue(TEXT("valid"), A.IsValid() && B.IsValid());
			TestNotEqual(TEXT("distinct"), A, B);
			TestFalse(TEXT("default invalid"), FResourceId().IsValid());
		});
	});

	Describe("Stow", [this]()
	{
		It("counts a stowed resource by category, value and mass", [this]()
		{
			int32 Slot = INDEX_NONE;
			TestEqual(TEXT("wood"), Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Carried, INDEX_NONE, nullptr, Slot), ECargoRefusal::None);
			TestEqual(TEXT("first free slot"), Slot, 0);
			TestEqual(TEXT("chest"), Ledger.Stow(MakeEntry(EResourceCategory::Riches, 20.0f, 100), EResourceState::Free, 5, nullptr, Slot), ECargoRefusal::None);
			TestEqual(TEXT("aimed slot"), Slot, 5);

			const FCargoTotals Totals = Ledger.ComputeTotals();
			TestEqual(TEXT("wood qty"), Totals.GetQuantity(EResourceCategory::Wood), 1);
			TestEqual(TEXT("riches qty"), Totals.GetQuantity(EResourceCategory::Riches), 1);
			TestEqual(TEXT("riches value"), Totals.GetValue(EResourceCategory::Riches), 100);
			TestEqual(TEXT("total value"), Totals.TotalValue, 100);
			TestEqual(TEXT("mass"), Totals.MassKg, 30.0f);
			TestEqual(TEXT("count"), Totals.StowedCount, 2);
		});

		It("refuses an occupied slot without changing anything", [this]()
		{
			int32 Slot = INDEX_NONE;
			Ledger.Stow(MakeEntry(EResourceCategory::Food, 5.0f, 0), EResourceState::Free, 3, nullptr, Slot);
			const FCargoEntry Other = MakeEntry(EResourceCategory::Food, 5.0f, 0);
			TestEqual(TEXT("taken"), Ledger.Stow(Other, EResourceState::Free, 3, nullptr, Slot), ECargoRefusal::SlotTaken);
			TestEqual(TEXT("not counted"), Ledger.FindSlotOf(Other.Id), static_cast<int32>(INDEX_NONE));
			TestEqual(TEXT("one stowed"), Ledger.ComputeTotals().StowedCount, 1);
		});

		It("refuses an invalid slot", [this]()
		{
			int32 Slot = INDEX_NONE;
			TestEqual(TEXT("out of range"), Ledger.Stow(MakeEntry(EResourceCategory::Food, 5.0f, 0), EResourceState::Free, 12, nullptr, Slot), ECargoRefusal::InvalidSlot);
			TestEqual(TEXT("negative"), Ledger.Stow(MakeEntry(EResourceCategory::Food, 5.0f, 0), EResourceState::Free, -3, nullptr, Slot), ECargoRefusal::InvalidSlot);
		});

		It("never counts the same resource twice", [this]()
		{
			int32 Slot = INDEX_NONE;
			const FCargoEntry Log = MakeEntry(EResourceCategory::Wood, 10.0f, 0);
			Ledger.Stow(Log, EResourceState::Free, 0, nullptr, Slot);
			TestEqual(TEXT("same id, other slot"), Ledger.Stow(Log, EResourceState::Free, 1, nullptr, Slot), ECargoRefusal::AlreadyStowed);
			TestEqual(TEXT("state Stowed"), Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Stowed, 2, nullptr, Slot), ECargoRefusal::AlreadyStowed);
			TestEqual(TEXT("one wood"), Ledger.ComputeTotals().WoodQuantity, 1);
		});

		It("refuses a lost resource or an invalid one", [this]()
		{
			int32 Slot = INDEX_NONE;
			TestEqual(TEXT("lost"), Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Lost, 0, nullptr, Slot), ECargoRefusal::ResourceLost);
			TestEqual(TEXT("no id"), Ledger.Stow(FCargoEntry(), EResourceState::Free, 0, nullptr, Slot), ECargoRefusal::InvalidResource);
			TestEqual(TEXT("no quantity"), Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0, 0), EResourceState::Free, 0, nullptr, Slot), ECargoRefusal::InvalidResource);
			TestEqual(TEXT("nothing stowed"), Ledger.ComputeTotals().StowedCount, 0);
		});

		It("refuses everything while locked", [this]()
		{
			int32 Slot = INDEX_NONE;
			Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Free, 0, nullptr, Slot);
			Ledger.bLocked = true;
			TestEqual(TEXT("stow"), Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Free, 1, nullptr, Slot), ECargoRefusal::Locked);
			FCargoSlot Removed;
			TestEqual(TEXT("unstow"), Ledger.Unstow(0, Removed), ECargoRefusal::Locked);
			TestEqual(TEXT("still one"), Ledger.ComputeTotals().StowedCount, 1);
		});
	});

	Describe("Recette 5 - full boat", [this]()
	{
		It("refuses the 13th object clearly and keeps the 12 others", [this]()
		{
			int32 Slot = INDEX_NONE;
			for (int32 Index = 0; Index < 12; ++Index)
			{
				TestEqual(TEXT("fits"), Ledger.Stow(MakeEntry(EResourceCategory::Food, 5.0f, 0), EResourceState::Free, INDEX_NONE, nullptr, Slot), ECargoRefusal::None);
			}
			const FCargoEntry Extra = MakeEntry(EResourceCategory::Food, 5.0f, 0);
			TestEqual(TEXT("no free slot"), Ledger.Stow(Extra, EResourceState::Free, INDEX_NONE, nullptr, Slot), ECargoRefusal::NoFreeSlot);
			TestEqual(TEXT("aimed at a taken slot when full"), Ledger.Stow(Extra, EResourceState::Free, 4, nullptr, Slot), ECargoRefusal::NoFreeSlot);
			TestFalse(TEXT("readable message"), FCargoLedger::GetRefusalText(ECargoRefusal::NoFreeSlot).IsEmpty());
			TestEqual(TEXT("12 kept"), Ledger.ComputeTotals().FoodQuantity, 12);
			TestEqual(TEXT("extra not counted"), Ledger.FindSlotOf(Extra.Id), static_cast<int32>(INDEX_NONE));
		});

		It("refuses an object above 150 kg and keeps the load", [this]()
		{
			int32 Slot = INDEX_NONE;
			for (int32 Index = 0; Index < 7; ++Index)
			{
				Ledger.Stow(MakeEntry(EResourceCategory::Riches, 20.0f, 100), EResourceState::Free, INDEX_NONE, nullptr, Slot);
			}
			TestEqual(TEXT("140 kg"), Ledger.GetMassKg(), 140.0f);
			TestEqual(TEXT("+20 kg refused"), Ledger.Stow(MakeEntry(EResourceCategory::Riches, 20.0f, 100), EResourceState::Free, INDEX_NONE, nullptr, Slot), ECargoRefusal::TooHeavy);
			TestEqual(TEXT("+10 kg fits exactly"), Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Free, INDEX_NONE, nullptr, Slot), ECargoRefusal::None);
			TestEqual(TEXT("150 kg"), Ledger.GetMassKg(), 150.0f);
			TestFalse(TEXT("readable message"), FCargoLedger::GetRefusalText(ECargoRefusal::TooHeavy).IsEmpty());
		});
	});

	Describe("Recette 6 - repeated stow and unstow", [this]()
	{
		It("keeps quantity and value unchanged", [this]()
		{
			int32 Slot = INDEX_NONE;
			const FCargoEntry Chest = MakeEntry(EResourceCategory::Riches, 20.0f, 100);
			const FCargoEntry Log = MakeEntry(EResourceCategory::Wood, 10.0f, 0);
			Ledger.Stow(Log, EResourceState::Free, INDEX_NONE, nullptr, Slot);
			const FCargoTotals Reference = [&]()
			{
				FCargoLedger Copy = Ledger;
				int32 S = INDEX_NONE;
				Copy.Stow(Chest, EResourceState::Free, INDEX_NONE, nullptr, S);
				return Copy.ComputeTotals();
			}();

			for (int32 Round = 0; Round < 25; ++Round)
			{
				TestEqual(TEXT("stow"), Ledger.Stow(Chest, EResourceState::Free, INDEX_NONE, nullptr, Slot), ECargoRefusal::None);
				FCargoSlot Removed;
				TestEqual(TEXT("unstow"), Ledger.Unstow(Slot, Removed), ECargoRefusal::None);
				TestEqual(TEXT("same entry back"), Removed.Entry.Id, Chest.Id);
				TestEqual(TEXT("value kept"), Removed.Entry.Value, 100);
			}
			Ledger.Stow(Chest, EResourceState::Free, INDEX_NONE, nullptr, Slot);

			const FCargoTotals Totals = Ledger.ComputeTotals();
			TestEqual(TEXT("riches qty"), Totals.RichesQuantity, Reference.RichesQuantity);
			TestEqual(TEXT("riches value"), Totals.RichesValue, Reference.RichesValue);
			TestEqual(TEXT("wood qty"), Totals.WoodQuantity, Reference.WoodQuantity);
			TestEqual(TEXT("mass"), Totals.MassKg, Reference.MassKg);
			TestEqual(TEXT("count"), Totals.StowedCount, 2);
		});

		It("refuses to unstow an empty slot", [this]()
		{
			FCargoSlot Removed;
			TestEqual(TEXT("empty"), Ledger.Unstow(0, Removed), ECargoRefusal::SlotEmpty);
			TestEqual(TEXT("invalid"), Ledger.Unstow(99, Removed), ECargoRefusal::InvalidSlot);
		});
	});

	Describe("Recette 20 - quota then unstow", [this]()
	{
		It("updates the totals when an object leaves after the quota is met", [this]()
		{
			int32 Slot = INDEX_NONE;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Ledger.Stow(MakeEntry(EResourceCategory::Wood, 10.0f, 0), EResourceState::Free, INDEX_NONE, nullptr, Slot);
			}
			TestEqual(TEXT("4 wood (quota V1)"), Ledger.ComputeTotals().WoodQuantity, 4);

			FCargoSlot Removed;
			Ledger.Unstow(2, Removed);
			const FCargoTotals Totals = Ledger.ComputeTotals();
			TestEqual(TEXT("3 wood"), Totals.WoodQuantity, 3);
			TestEqual(TEXT("30 kg"), Totals.MassKg, 30.0f);
			TestTrue(TEXT("slot free again"), !Ledger.IsSlotOccupied(2));
		});
	});

	Describe("Snapshot", [this]()
	{
		It("is a frozen copy unaffected by later changes", [this]()
		{
			int32 Slot = INDEX_NONE;
			Ledger.Stow(MakeEntry(EResourceCategory::Riches, 20.0f, 100), EResourceState::Free, INDEX_NONE, nullptr, Slot);
			Ledger.Stow(MakeEntry(EResourceCategory::Food, 5.0f, 0), EResourceState::Free, INDEX_NONE, nullptr, Slot);
			const FCargoSnapshot Snapshot = Ledger.MakeSnapshot();

			FCargoSlot Removed;
			Ledger.Unstow(0, Removed);
			Ledger.RemoveAll();

			TestEqual(TEXT("entries"), Snapshot.Entries.Num(), 2);
			TestEqual(TEXT("riches value"), Snapshot.Totals.RichesValue, 100);
			TestEqual(TEXT("food"), Snapshot.Totals.FoodQuantity, 1);
			TestEqual(TEXT("ledger empty"), Ledger.ComputeTotals().StowedCount, 0);
		});
	});

	Describe("Layout", [this]()
	{
		It("centres a 2 x 6 grid on the origin", [this]()
		{
			const FVector2D Spacing(50.0f, 60.0f);
			const FVector Origin(0.0f, 0.0f, 10.0f);
			const FVector First = CargoLayout::ComputeSlotOffset(0, 12, 2, Spacing, Origin);
			const FVector Last = CargoLayout::ComputeSlotOffset(11, 12, 2, Spacing, Origin);
			TestEqual(TEXT("first X"), First.X, -125.0, 0.01);
			TestEqual(TEXT("first Y"), First.Y, -30.0, 0.01);
			TestEqual(TEXT("last X"), Last.X, 125.0, 0.01);
			TestEqual(TEXT("last Y"), Last.Y, 30.0, 0.01);
			TestEqual(TEXT("height"), First.Z, 10.0, 0.01);
		});
	});

	Describe("Component", [this]()
	{
		It("refuses to stow outside the server with a readable message", [this]()
		{
			UCargoComponent* Cargo = NewObject<UCargoComponent>();
			FText Refusal;
			TestFalse(TEXT("no owner, no authority"), Cargo->TryStow(nullptr, 0, Refusal));
			TestFalse(TEXT("message"), Refusal.IsEmpty());
			TestFalse(TEXT("unstow"), Cargo->TryUnstow(0, Refusal));
			TestEqual(TEXT("nothing counted"), Cargo->GetTotals().StowedCount, 0);
		});
	});
}

#endif
