// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoyalFlag.generated.h"

class APawn;
class UStaticMeshComponent;

/**
 *  The British flag planted on a zone socket (cahier §6.2). Spawned by the server through APacificationZone,
 *  replicated and always relevant so the whole team sees it. Gives no reward on its own: only the sermon does.
 */
UCLASS(Blueprintable)
class MONKE_ISLAND_API ARoyalFlag : public AActor
{
	GENERATED_BODY()

public:

	ARoyalFlag();

	/** Records which zone and which player planted the flag (server, right after spawning) */
	void InitializePlanted(FName InZoneId, APawn* InPlanter);

	/** Zone this flag was planted on */
	UFUNCTION(BlueprintPure, Category="Flag")
	FName GetZoneId() const { return ZoneId; }

	/** Player who planted the flag (null once that pawn is gone) */
	UFUNCTION(BlueprintPure, Category="Flag")
	APawn* GetPlanter() const { return Planter; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	/** Pole and cloth of the flag (mesh set in the Blueprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Flag")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Zone this flag was planted on */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Flag")
	FName ZoneId;

	/** Player who planted the flag */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Flag")
	TObjectPtr<APawn> Planter;
};
