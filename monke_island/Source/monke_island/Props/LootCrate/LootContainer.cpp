// monke_island

#include "Props/LootCrate/LootContainer.h"
#include "GameFramework/Actor.h"

void LootContainers::AppendWithContents(AActor* Actor, TArray<AActor*>& OutActors, int32 MaxDepth)
{
	AppendTree(Actor, OutActors, [](AActor* Node, TArray<AActor*>& OutChildren)
	{
		if (const ILootContainer* Container = Cast<ILootContainer>(Node))
		{
			Container->GetContainedActors(OutChildren);
		}
	}, MaxDepth);
}
