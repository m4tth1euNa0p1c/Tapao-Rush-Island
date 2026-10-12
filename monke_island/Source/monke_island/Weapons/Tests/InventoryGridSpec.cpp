// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Math/RandomStream.h"
#include "Weapons/InventoryGrid.h"
#include "Weapons/InventoryItemDefinition.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.InventoryGrid
BEGIN_DEFINE_SPEC(FInventoryGridSpec, "monke_island.Weapons.InventoryGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UInventoryItemDefinition* Sword = nullptr;
	UInventoryItemDefinition* Musket = nullptr;
	UInventoryItemDefinition* Shells = nullptr;

	/** BarNum + BagNum empty cells */
	TArray<FInventoryCell> MakeCells(int32 Num) const
	{
		TArray<FInventoryCell> Cells;
		Cells.SetNum(Num);
		return Cells;
	}

	void Put(TArray<FInventoryCell>& Cells, int32 Index, UInventoryItemDefinition* Definition, int32 Count) const
	{
		Cells[Index].Definition = Definition;
		Cells[Index].Count = Count;
		Cells[Index].Source = Index;
	}
END_DEFINE_SPEC(FInventoryGridSpec)

void FInventoryGridSpec::Define()
{
	BeforeEach([this]()
	{
		Sword = NewObject<UInventoryItemDefinition>();
		Musket = NewObject<UInventoryItemDefinition>();
		Shells = NewObject<UInventoryItemDefinition>();
		Shells->MaxStack = 30;
		Sword->AddToRoot();
		Musket->AddToRoot();
		Shells->AddToRoot();
	});

	AfterEach([this]()
	{
		Sword->RemoveFromRoot();
		Musket->RemoveFromRoot();
		Shells->RemoveFromRoot();
		Sword = Musket = Shells = nullptr;
	});

	It("moves a whole stack with its actor onto an empty cell", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(6);
		Put(Cells, 0, Sword, 1);
		TestEqual(TEXT("moved"), InventoryGrid::Move(Cells, 0, 5, 1), 1);
		TestTrue(TEXT("source empty"), Cells[0].IsEmpty());
		TestTrue(TEXT("sword in 6"), Cells[5].Definition == Sword && Cells[5].Count == 1);
		TestEqual(TEXT("same actor"), Cells[5].Source, 0);
		TestTrue(TEXT("valid"), InventoryGrid::IsValid(Cells));
	});

	It("swaps two different items, whole stacks only", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(6);
		Put(Cells, 0, Sword, 1);
		Put(Cells, 1, Shells, 12);
		Put(Cells, 2, Musket, 1);
		TestEqual(TEXT("swap"), InventoryGrid::Move(Cells, 0, 2, 1), 1);
		TestTrue(TEXT("musket in 1"), Cells[0].Definition == Musket && Cells[0].Source == 2);
		TestTrue(TEXT("sword in 3"), Cells[2].Definition == Sword && Cells[2].Source == 0);
		TestEqual(TEXT("part of a stack onto another item refused"), InventoryGrid::Move(Cells, 1, 0, 5), 0);
		TestEqual(TEXT("shells untouched"), Cells[1].Count, 12);
	});

	It("takes half a stack (right click) and leaves a new stack", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(6);
		Put(Cells, 4, Shells, 13);
		const int32 Half = (Cells[4].Count + 1) / 2;
		TestEqual(TEXT("bigger half moved"), InventoryGrid::Move(Cells, 4, 5, Half), 7);
		TestEqual(TEXT("6 left"), Cells[4].Count, 6);
		TestEqual(TEXT("7 there"), Cells[5].Count, 7);
		TestEqual(TEXT("old actor stays"), Cells[4].Source, 4);
		TestEqual(TEXT("new stack needs an actor"), Cells[5].Source, INDEX_NONE);
		TestEqual(TEXT("one put"), InventoryGrid::Move(Cells, 5, 0, 1), 1);
		TestEqual(TEXT("13 shells in all"), InventoryGrid::CountOf(Cells, Shells), 13);
		TestTrue(TEXT("valid"), InventoryGrid::IsValid(Cells));
	});

	It("joins stacks up to the stack limit and keeps the rest", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(6);
		Put(Cells, 0, Shells, 25);
		Put(Cells, 1, Shells, 20);
		TestEqual(TEXT("5 fit"), InventoryGrid::Move(Cells, 1, 0, 20), 5);
		TestEqual(TEXT("full stack"), Cells[0].Count, 30);
		TestEqual(TEXT("15 stay"), Cells[1].Count, 15);
		TestEqual(TEXT("full: nothing"), InventoryGrid::Move(Cells, 1, 0, 15), 0);
		Put(Cells, 2, Shells, 3);
		TestEqual(TEXT("3 join"), InventoryGrid::Move(Cells, 2, 1, 3), 3);
		TestTrue(TEXT("emptied stack cleared (its actor goes)"), Cells[2].IsEmpty() && Cells[2].Source == INDEX_NONE);
		TestEqual(TEXT("48 shells"), InventoryGrid::CountOf(Cells, Shells), 48);
	});

	It("never stacks single items", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(4);
		Put(Cells, 0, Sword, 1);
		Put(Cells, 1, Sword, 1);
		TestEqual(TEXT("two swords swap, no stack"), InventoryGrid::Move(Cells, 0, 1, 1), 1);
		TestEqual(TEXT("still one each"), Cells[0].Count + Cells[1].Count, 2);
		TestEqual(TEXT("2 swords"), InventoryGrid::CountOf(Cells, Sword), 2);
	});

	It("quick moves between the bar and the bag, joining stacks first", [this]()
	{
		// bar 0-3, bag 4-9
		TArray<FInventoryCell> Cells = MakeCells(10);
		Put(Cells, 1, Shells, 20);
		Put(Cells, 6, Shells, 25);
		Put(Cells, 0, Sword, 1);
		TestEqual(TEXT("20 sent"), InventoryGrid::QuickMove(Cells, 1, 4), 20);
		TestEqual(TEXT("stack of the bag full"), Cells[6].Count, 30);
		TestTrue(TEXT("rest in the first free bag cell"), Cells[4].Definition == Shells && Cells[4].Count == 15);
		TestTrue(TEXT("bar cell emptied"), Cells[1].IsEmpty());
		TestEqual(TEXT("sword to the bag"), InventoryGrid::QuickMove(Cells, 0, 4), 1);
		TestTrue(TEXT("sword in the bag"), Cells[5].Definition == Sword);
		TestEqual(TEXT("back to the bar"), InventoryGrid::QuickMove(Cells, 5, 4), 1);
		TestTrue(TEXT("sword in the first bar cell"), Cells[0].Definition == Sword && Cells[0].Source == 0);
		TestEqual(TEXT("45 shells"), InventoryGrid::CountOf(Cells, Shells), 45);
		TestTrue(TEXT("valid"), InventoryGrid::IsValid(Cells));
	});

	It("gathers identical items into a cell (double click)", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(8);
		Put(Cells, 2, Shells, 4);
		Put(Cells, 0, Shells, 10);
		Put(Cells, 5, Shells, 22);
		Put(Cells, 6, Sword, 1);
		TestEqual(TEXT("26 gathered up to 30"), InventoryGrid::Gather(Cells, 2), 26);
		TestEqual(TEXT("full"), Cells[2].Count, 30);
		TestEqual(TEXT("36 in all"), InventoryGrid::CountOf(Cells, Shells), 36);
		TestTrue(TEXT("sword untouched"), Cells[6].Definition == Sword);
		TestEqual(TEXT("a single item gathers nothing"), InventoryGrid::Gather(Cells, 6), 0);
	});

	It("places a new item: stack with room, then the bar, then the bag, else full", [this]()
	{
		TArray<FInventoryCell> Cells = MakeCells(6);
		Put(Cells, 5, Shells, 29);
		Put(Cells, 0, Sword, 1);
		TestEqual(TEXT("joins the stack in the bag"), InventoryGrid::FindPlacement(Cells, Shells, 4), 5);
		TestEqual(TEXT("first free bar cell"), InventoryGrid::FindPlacement(Cells, Musket, 4), 1);
		Put(Cells, 1, Musket, 1);
		Put(Cells, 2, Musket, 1);
		Put(Cells, 3, Musket, 1);
		TestEqual(TEXT("bar full: the bag"), InventoryGrid::FindPlacement(Cells, Musket, 4), 4);
		Put(Cells, 4, Musket, 1);
		Cells[5].Count = 30;
		TestEqual(TEXT("full"), InventoryGrid::FindPlacement(Cells, Shells, 4), INDEX_NONE);
	});

	It("never duplicates nor loses an item, whatever the clicks", [this]()
	{
		FRandomStream Random(166);
		TArray<FInventoryCell> Cells = MakeCells(22);
		Put(Cells, 0, Sword, 1);
		Put(Cells, 1, Musket, 1);
		Put(Cells, 2, Shells, 30);
		Put(Cells, 9, Shells, 17);
		Put(Cells, 15, Shells, 8);
		bool bAlwaysValid = true;
		for (int32 Step = 0; Step < 5000; ++Step)
		{
			const int32 From = Random.RandRange(-1, 22);
			const int32 To = Random.RandRange(-1, 22);
			switch (Random.RandRange(0, 3))
			{
			case 0: InventoryGrid::Move(Cells, From, To, Random.RandRange(-2, 35)); break;
			case 1: InventoryGrid::QuickMove(Cells, From, 4); break;
			case 2: InventoryGrid::Gather(Cells, From); break;
			default:
				// the same request twice in a row (two quick clicks)
				InventoryGrid::Move(Cells, From, To, 1);
				InventoryGrid::Move(Cells, From, To, 1);
				break;
			}
			bAlwaysValid &= InventoryGrid::IsValid(Cells);
			// the Sources of the pure rules are indices of the state before one operation: renumber as the component does
			for (int32 Index = 0; Index < Cells.Num(); ++Index)
			{
				Cells[Index].Source = Cells[Index].IsEmpty() ? INDEX_NONE : Index;
			}
		}
		TestTrue(TEXT("rules held at every step"), bAlwaysValid);
		TestEqual(TEXT("one sword"), InventoryGrid::CountOf(Cells, Sword), 1);
		TestEqual(TEXT("one musket"), InventoryGrid::CountOf(Cells, Musket), 1);
		TestEqual(TEXT("55 shells"), InventoryGrid::CountOf(Cells, Shells), 55);
	});
}

#endif
