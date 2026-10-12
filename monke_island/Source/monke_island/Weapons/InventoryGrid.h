// monke_island

#pragma once

#include "CoreMinimal.h"

class UInventoryItemDefinition;

/**
 *  One cell of the inventory as the pure rules see it (T-166): what lies there, how many, and which item actor of the
 *  state before the operation represents it (Source = its old cell, INDEX_NONE = a new actor must be spawned).
 *  The component turns the cells back into actors: an old actor no cell points to any more is destroyed.
 */
struct FInventoryCell
{
	const UInventoryItemDefinition* Definition = nullptr;
	int32 Count = 0;
	int32 Source = INDEX_NONE;

	bool IsEmpty() const { return Definition == nullptr || Count <= 0; }

	void Clear() { *this = FInventoryCell(); }
};

/**
 *  Pure rules of the Minecraft-like inventory (T-166, lot L5): the quick bar is the first BarNum cells, the bag the
 *  others. No world, no actor: every function keeps the number of each item (no duplication, no loss) and each
 *  Source is held by one cell at most. Run on the server only; the client predicts nothing with them but the cursor.
 */
namespace InventoryGrid
{
	/** Stack limit of a cell's item */
	MONKE_ISLAND_API int32 GetMaxStack(const FInventoryCell& Cell);

	/** Two cells hold the same stackable item */
	MONKE_ISLAND_API bool CanStack(const FInventoryCell& A, const FInventoryCell& B);

	/**
	 *  Moves Count items of From onto To (left click, right click, keys 1-4). To empty: they go there (a part leaves
	 *  a new stack). Same stackable item: they join up to the stack limit. Another item: the two cells swap, only when
	 *  the whole stack moves. Returns the number moved (a swap counts the whole stack), 0 when refused.
	 */
	MONKE_ISLAND_API int32 Move(TArray<FInventoryCell>& Cells, int32 From, int32 To, int32 Count);

	/** Shift + click: sends the stack of From to the other section (bar <-> bag), joining stacks first. Returns the number moved */
	MONKE_ISLAND_API int32 QuickMove(TArray<FInventoryCell>& Cells, int32 From, int32 BarNum);

	/** Double click: gathers the identical items of the other cells into Into, up to its stack limit (bar first). Returns the number gathered */
	MONKE_ISLAND_API int32 Gather(TArray<FInventoryCell>& Cells, int32 Into);

	/**
	 *  Cell a new item goes to (pick up): a stack of the same item with room (bar first), then the first empty cell of
	 *  the bar, then of the bag. INDEX_NONE when nothing fits.
	 */
	MONKE_ISLAND_API int32 FindPlacement(const TArray<FInventoryCell>& Cells, const UInventoryItemDefinition* Definition, int32 BarNum);

	/** Number of a given item over every cell */
	MONKE_ISLAND_API int32 CountOf(const TArray<FInventoryCell>& Cells, const UInventoryItemDefinition* Definition);

	/** The rules hold: counts within 1..MaxStack, empty cells cleared, each Source once at most */
	MONKE_ISLAND_API bool IsValid(const TArray<FInventoryCell>& Cells);
}
