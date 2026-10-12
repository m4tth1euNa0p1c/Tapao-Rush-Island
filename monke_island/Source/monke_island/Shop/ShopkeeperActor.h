// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "Shop/ShopTypes.h"
#include "ShopkeeperActor.generated.h"

class UAnimSequence;
class UCapsuleComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UShopCatalog;

/**
 *  Bean merchant standing on the galleon deck (owner request 2026-10-06, ADR-007 §2 "quartier marchand"): F opens the
 *  TAPAO shop screen of the player who asked; a purchase goes through that player's inventory RPC and is judged here,
 *  on the server: offer, reach, the crew's treasury (URunSubsystem), a free cell. Not a pawn (the interaction trace
 *  ignores pawns): a capsule that blocks, the bean body, its outfit following the body, an idle loop.
 *  Spawned by AGalleonActor (UGalleonRules::ShopkeeperClass) or placed by hand.
 */
UCLASS()
class MONKE_ISLAND_API AShopkeeperActor : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	AShopkeeperActor();

	/** What is sold (DA_ShopCatalog); DefaultCatalog when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<UShopCatalog> Catalog;

	/** Catalog used when Catalog is empty */
	UPROPERTY(EditDefaultsOnly, Category="Shop")
	TSoftObjectPtr<UShopCatalog> DefaultCatalog;

	/** Bean body and outfit, idle loop */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<USkeletalMesh> OutfitMesh;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	/** Prompt next to the interaction key */
	UPROPERTY(EditAnywhere, Category="Shop")
	FText Prompt = NSLOCTEXT("Shop", "Prompt", "Parler au marchand");

	/** The catalog in use (Catalog, else DefaultCatalog), null when none */
	UFUNCTION(BlueprintPure, Category="Shop")
	UShopCatalog* GetCatalog() const;

	/** Server: judges and does a purchase for Buyer (pays from the treasury, gives the item) */
	EShopResult Sell(APawn* Buyer, int32 Offer);

	// IIslanderInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Outfit;

private:

	/** Server: last purchase time of each buyer (rate limit) */
	TMap<TWeakObjectPtr<APawn>, double> LastPurchase;
};
