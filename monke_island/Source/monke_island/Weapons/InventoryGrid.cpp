// monke_island

#include "Weapons/InventoryGrid.h"
#include "Weapons/InventoryItemDefinition.h"

namespace InventoryGrid
{
	int32 GetMaxStack(const FInventoryCell& Cell)
	{
		return UInventoryItemDefinition::GetMaxStack(Cell.Definition);
	}

	bool CanStack(const FInventoryCell& A, const FInventoryCell& B)
	{
		return !A.IsEmpty() && !B.IsEmpty() && A.Definition == B.Definition && GetMaxStack(A) > 1;
	}

	int32 Move(TArray<FInventoryCell>& Cells, int32 From, int32 To, int32 Count)
	{
		if (!Cells.IsValidIndex(From) || !Cells.IsValidIndex(To) || From == To || Cells[From].IsEmpty() || Count <= 0)
		{
			return 0;
		}
		FInventoryCell& Src = Cells[From];
		FInventoryCell& Dst = Cells[To];
		Count = FMath::Min(Count, Src.Count);

		if (Dst.IsEmpty())
		{
			if (Count == Src.Count)
			{
				// the whole stack, its actor with it
				Dst = Src;
				Src.Clear();
			}
			else
			{
				// a part: a new stack, a new actor
				Dst.Definition = Src.Definition;
				Dst.Count = Count;
				Dst.Source = INDEX_NONE;
				Src.Count -= Count;
			}
			return Count;
		}

		if (CanStack(Src, Dst))
		{
			const int32 Moved = FMath::Min(Count, GetMaxStack(Dst) - Dst.Count);
			if (Moved <= 0)
			{
				return 0;
			}
			Dst.Count += Moved;
			Src.Count -= Moved;
			if (Src.Count <= 0)
			{
				// its actor is not needed any more
				Src.Clear();
			}
			return Moved;
		}

		// another item: swap, whole stacks only (a part has nowhere to leave the other stack)
		if (Count != Src.Count)
		{
			return 0;
		}
		Swap(Src, Dst);
		return Count;
	}

	int32 QuickMove(TArray<FInventoryCell>& Cells, int32 From, int32 BarNum)
	{
		if (!Cells.IsValidIndex(From) || Cells[From].IsEmpty())
		{
			return 0;
		}
		BarNum = FMath::Clamp(BarNum, 0, Cells.Num());
		const bool bFromBar = From < BarNum;
		const int32 First = bFromBar ? BarNum : 0;
		const int32 Last = bFromBar ? Cells.Num() : BarNum;

		int32 Moved = 0;
		// stacks of the same item first
		for (int32 Index = First; Index < Last && !Cells[From].IsEmpty(); ++Index)
		{
			if (CanStack(Cells[From], Cells[Index]))
			{
				Moved += Move(Cells, From, Index, Cells[From].Count);
			}
		}
		// then the first empty cell takes what is left, actor included
		for (int32 Index = First; Index < Last && !Cells[From].IsEmpty(); ++Index)
		{
			if (Cells[Index].IsEmpty())
			{
				Moved += Move(Cells, From, Index, Cells[From].Count);
			}
		}
		return Moved;
	}

	int32 Gather(TArray<FInventoryCell>& Cells, int32 Into)
	{
		if (!Cells.IsValidIndex(Into) || Cells[Into].IsEmpty() || GetMaxStack(Cells[Into]) <= 1)
		{
			return 0;
		}
		int32 Gathered = 0;
		for (int32 Index = 0; Index < Cells.Num() && Cells[Into].Count < GetMaxStack(Cells[Into]); ++Index)
		{
			if (Index != Into && CanStack(Cells[Index], Cells[Into]))
			{
				Gathered += Move(Cells, Index, Into, Cells[Index].Count);
			}
		}
		return Gathered;
	}

	int32 FindPlacement(const TArray<FInventoryCell>& Cells, const UInventoryItemDefinition* Definition, int32 BarNum)
	{
		if (!Definition)
		{
			return INDEX_NONE;
		}
		BarNum = FMath::Clamp(BarNum, 0, Cells.Num());
		if (UInventoryItemDefinition::GetMaxStack(Definition) > 1)
		{
			for (int32 Index = 0; Index < Cells.Num(); ++Index)
			{
				const FInventoryCell& Cell = Cells[Index];
				if (!Cell.IsEmpty() && Cell.Definition == Definition && Cell.Count < GetMaxStack(Cell))
				{
					return Index;
				}
			}
		}
		// array order is the bar, then the bag
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			if (Cells[Index].IsEmpty())
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 CountOf(const TArray<FInventoryCell>& Cells, const UInventoryItemDefinition* Definition)
	{
		int32 Count = 0;
		for (const FInventoryCell& Cell : Cells)
		{
			Count += !Cell.IsEmpty() && Cell.Definition == Definition ? Cell.Count : 0;
		}
		return Count;
	}

	bool IsValid(const TArray<FInventoryCell>& Cells)
	{
		TSet<int32> Sources;
		for (const FInventoryCell& Cell : Cells)
		{
			if (Cell.IsEmpty())
			{
				if (Cell.Definition != nullptr || Cell.Count != 0 || Cell.Source != INDEX_NONE)
				{
					return false;
				}
				continue;
			}
			if (Cell.Count > GetMaxStack(Cell))
			{
				return false;
			}
			if (Cell.Source != INDEX_NONE)
			{
				bool bAlreadyThere = false;
				Sources.Add(Cell.Source, &bAlreadyThere);
				if (bAlreadyThere)
				{
					return false;
				}
			}
		}
		return true;
	}
}
