// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "GiantCrabBoss.generated.h"

class APawn;

/** What the bar of a boss shows to one player (T-261: the giant crab, the first boss; the bar is UI/BossHealthBar) */
struct FBossBarView
{
	/** Its name */
	FText Name;

	/** Health left, 0 to 1 */
	float HealthFraction = 1.0f;

	/** Health fractions where its later phases begin (marks on the bar) */
	TArray<float, TInlineAllocator<4>> PhaseMarks;

	/** Phase it is in (0: the first) */
	int32 Phase = 0;

	/** In a frenzy (its last phase) */
	bool bFrenzy = false;

	/** Dead, the bar lingers a little */
	bool bDead = false;
};

/** An actor that shows a boss bar to the players it fights (native only) */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UBossBarSource : public UInterface
{
	GENERATED_BODY()
};

/** A boss: its bar shows at the bottom of the screen of the players it fights */
class MONKE_ISLAND_API IBossBarSource
{
	GENERATED_BODY()

public:

	/** True when its bar shows to Viewer (it fights near it, or just died there); fills OutView */
	virtual bool GetBossBarView(const APawn* Viewer, FBossBarView& OutView) const = 0;
};

/**
 *  Bosses of the world on every machine (T-261): they sign in at their begin play and out at their end play, the HUD
 *  asks for the bar of its own player. Reads replicated state only; decides nothing.
 */
UCLASS()
class MONKE_ISLAND_API UBossBarSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Subsystem of the world of an object, nullptr without a game world */
	static UBossBarSubsystem* Get(const UObject* WorldContext);

	/** A boss signs in (an actor implementing IBossBarSource) */
	void Register(AActor* Boss);

	/** A boss signs out */
	void Unregister(AActor* Boss);

	/** The bar for Viewer: the nearest boss that shows it; false when none */
	bool FindBarFor(const APawn* Viewer, FBossBarView& OutView) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:

	/** Bosses signed in */
	TArray<TWeakObjectPtr<AActor>> Bosses;
};
