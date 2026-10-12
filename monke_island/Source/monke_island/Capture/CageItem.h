// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Capture/CaptureItem.h"
#include "CageItem.generated.h"

/**
 *  The cage of the inventory (T-336, DA_Item_Cage): carried folded in the right hand. On the beach (ground just above
 *  the sea with the sea near, DA_CaptureRules "Beach"), the interaction key sets it down in front of the player: the
 *  placing gesture plays (A_Bean_Cage_Place), the cage stands at its CagePlaced moment (ACaptureCage, two minutes) and
 *  the item leaves the inventory at the end of the gesture; it comes back when the cage goes (ACaptureCage::ReturnCage).
 *  The owner previews the spot (the prompt "F : poser la cage" or why not); the server looks for the spot itself from
 *  the carrier and checks it again when the cage stands (another cage set meanwhile wins).
 */
UCLASS()
class MONKE_ISLAND_API ACageItem : public ACaptureItem
{
	GENERATED_BODY()

public:

	ACageItem();

	/** Where a cage would stand now in front of the carrier, and why not (None when it can); the owner's preview and the
	 *  server's check alike */
	ECaptureRefusal FindSpot(FTransform& OutSpot) const;

	virtual bool GetPrompt(FCaptureHudLine& Out) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual UStaticMesh* GetHeldMesh() const override;
	virtual FTransform GetHeldGrip() const override;
	virtual bool ShouldShowHeldMesh() const override;
	virtual bool GetHeldOverride(FTransform& OutWorld) const override;
	virtual bool WantsUseKey() const override;
	virtual void OnUsePressed() override;
	virtual void RefreshLocal() override;
	virtual void OnEquippedChanged() override;

	/** Asks the server to set the cage down */
	UFUNCTION(Server, Reliable)
	void ServerPlaceCage();

private:

	/** Room for a cage on its floor at Spot: CageTooClose (another cage), NoRoom (a wall, a tree, a body), else None */
	ECaptureRefusal CheckRoom(const FTransform& Spot) const;

	/** Server: the cage stands (CagePlaced moment of the gesture) */
	void DeployCage();

	/** Server: the gesture ended: the item leaves the inventory (a cage stands), or stays (the spot was lost) */
	void FinishPlacing();

	/** Server: cuts a placing under way (put away, dropped, the carrier fell) */
	void CancelPlacing();

	/** The cage left the hand (it stands on the beach) */
	UPROPERTY(Replicated)
	bool bDeployed = false;

	/** Server: the spot chosen when the gesture started */
	FTransform PendingSpot = FTransform::Identity;
	bool bPlacing = false;

	FTimerHandle DeployTimer;
	FTimerHandle FinishTimer;

	/** Owner: the last check of the spot */
	ECaptureRefusal LocalRefusal = ECaptureRefusal::NotOnBeach;

	/** Owner: the crosshair is on something to use with the key (the interaction keeps it) */
	bool bInteractableInView = false;
};
