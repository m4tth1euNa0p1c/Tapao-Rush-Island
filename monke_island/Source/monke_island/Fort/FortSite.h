// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camps/SiteClearing.h"
#include "FortSite.generated.h"

class UFortSiteRules;
class ULevelStreamingDynamic;
class USphereComponent;

/**
 *  A French fort built on the land of the atoll (owner 2026-10-08), spawned by UFortSiteSubsystem on the server and
 *  replicated: on every machine it streams in the land fort level (DA_FortSiteRules.FortLevel) under the same name, at
 *  the transform the server measured, and clears the vegetation of its ground. The garrison, the loot and the prisoners
 *  are the actors of that level (replicated from the server once the client sees the level). Tagged TikiQuietZone: no
 *  tiki, camp, marae nor temple on it.
 */
UCLASS()
class MONKE_ISLAND_API AFortSite : public AActor
{
	GENERATED_BODY()

public:

	AFortSite();

	/** Rules of the fort (set by the subsystem before the fort finishes spawning, replicated with it) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Fort")
	TObjectPtr<UFortSiteRules> Rules;

	/** Origin of the fort level (world), measured by the server */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Fort")
	FTransform LevelTransform;

	/** Name of the streamed level, the same on every machine */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Fort")
	FString LevelName;

	/** True once the fort level is in the world */
	UFUNCTION(BlueprintPure, Category="Fort")
	bool IsBuilt() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Zone of the fort, tagged TikiQuietZone */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> QuietZone;

	/** The fort level streamed in */
	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> Streamed;

	FSiteClearing Clearing;
};
