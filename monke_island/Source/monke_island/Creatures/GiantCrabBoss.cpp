// monke_island

#include "Creatures/GiantCrabBoss.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UBossBarSubsystem* UBossBarSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBossBarSubsystem>() : nullptr;
}

bool UBossBarSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void UBossBarSubsystem::Register(AActor* Boss)
{
	if (Boss && Cast<IBossBarSource>(Boss))
	{
		Bosses.AddUnique(Boss);
	}
}

void UBossBarSubsystem::Unregister(AActor* Boss)
{
	Bosses.RemoveAll([Boss](const TWeakObjectPtr<AActor>& Item) { return !Item.IsValid() || Item.Get() == Boss; });
}

bool UBossBarSubsystem::FindBarFor(const APawn* Viewer, FBossBarView& OutView) const
{
	if (!Viewer)
	{
		return false;
	}
	double NearestSq = TNumericLimits<double>::Max();
	bool bFound = false;
	for (const TWeakObjectPtr<AActor>& Item : Bosses)
	{
		const AActor* Boss = Item.Get();
		const IBossBarSource* Source = Cast<IBossBarSource>(Boss);
		FBossBarView View;
		if (!Source || !Source->GetBossBarView(Viewer, View))
		{
			continue;
		}
		const double DistanceSq = FVector::DistSquared(Boss->GetActorLocation(), Viewer->GetActorLocation());
		if (DistanceSq < NearestSq)
		{
			NearestSq = DistanceSq;
			OutView = MoveTemp(View);
			bFound = true;
		}
	}
	return bFound;
}
