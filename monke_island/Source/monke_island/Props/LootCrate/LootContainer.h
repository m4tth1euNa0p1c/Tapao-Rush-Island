// monke_island

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LootContainer.generated.h"

class APawn;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class ULootContainer : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Something that holds loot inside it (T-258: the loot crate). A player holding an object (grab component) puts it in
 *  with the interaction key aimed at the container; what lies inside travels with it, hidden, and counts wherever the
 *  container is counted: the barque's cargo, the galleon's cargo and the bilan (LootContainers::AppendWithContents).
 *  Server authoritative: StoreItem runs on the server only, after the grab component re-checked reach and sight.
 */
class MONKE_ISLAND_API ILootContainer
{
	GENERATED_BODY()

public:

	/** Objects inside now (server; on a client, those it still knows) */
	virtual void GetContainedActors(TArray<AActor*>& OutActors) const = 0;

	/** True when this object can go in now: its kind, its size, the room and the mass left (every machine, the server decides) */
	virtual bool CanStoreItem(const APawn* User, const AActor* Item) const = 0;

	/** Server: puts the object inside (hidden, carried along); false when refused */
	virtual bool StoreItem(APawn* User, AActor* Item) = 0;

	/** Action shown next to the key while User holds Item aimed at the container, e.g. "Ranger dans la caisse (3/12)" */
	virtual FText GetStorePrompt(const APawn* User, const AActor* Item) const = 0;
};

/** What a container holds, counted with it */
namespace LootContainers
{
	/**
	 *  Adds Node, then what it holds (GetChildren(Node, OutChildren)), depth first and at most MaxDepth containers deep;
	 *  a node already listed is never added twice (no loop through a container that would hold itself)
	 */
	template <typename NodeType, typename ChildrenFunc>
	void AppendTree(NodeType* Node, TArray<NodeType*>& OutNodes, ChildrenFunc&& GetChildren, int32 MaxDepth)
	{
		if (!Node || OutNodes.Contains(Node))
		{
			return;
		}
		OutNodes.Add(Node);
		if (MaxDepth <= 0)
		{
			return;
		}
		TArray<NodeType*> Children;
		GetChildren(Node, Children);
		for (NodeType* Child : Children)
		{
			AppendTree(Child, OutNodes, GetChildren, MaxDepth - 1);
		}
	}

	/** Adds Actor, then, when it is a container (ILootContainer), what it holds, nested containers included */
	MONKE_ISLAND_API void AppendWithContents(AActor* Actor, TArray<AActor*>& OutActors, int32 MaxDepth = 4);
}
