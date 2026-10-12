// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmWeapon.h"
#include "GrenadeWeapon.generated.h"

class UGrenadeRules;

/**
 *  Bean's grenades (BP_Grenade, DA_GrenadeRules; owner 2026-10-09, pack bean-grenade: 13 gestures at 30 fps). The bag
 *  is carried as a firearm whose "round" is the grenade in hand: Fire lobs it (short throw), AimFire throws it far;
 *  the clip plays from its start (the wind-up), the fuse is lit at its Light event and the AGrenadeProjectile leaves
 *  at its Fire event. Reload takes the next grenade out of the bag (HammerCocked), by itself after a throw; an empty
 *  bag clicks (DryFire), or throws the bag away when bDiscardWhenEmpty. A throw cut after the fuse was lit (swim,
 *  knocked out) lets the grenade roll at the feet. Server authoritative: the server owns the bag and spawns the
 *  projectile; the carrier sees its wind-up at once.
 */
UCLASS()
class MONKE_ISLAND_API AGrenadeWeapon : public AFirearmWeapon
{
	GENERATED_BODY()

public:

	AGrenadeWeapon();

	/** A grenade is in hand */
	UFUNCTION(BlueprintPure, Category="Grenade")
	bool IsGrenadeInHand() const { return bInHand; }

	/** Grenades left in the bag (valid on the server and the carrier) */
	UFUNCTION(BlueprintPure, Category="Grenade")
	int32 GetBagGrenades() const { return BagGrenades; }

	virtual int32 GetLoadedRounds() const override { return bInHand ? 1 : 0; }
	virtual int32 GetReserveRounds() const override { return BagGrenades; }
	virtual int32 GetMaxRounds() const override;
	virtual void SetRounds(int32 Loaded, int32 Reserve) override { bInHand = Loaded > 0; BagGrenades = FMath::Max(0, Reserve); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void OnEquippedChanged() override;
	virtual void ServerHandleFire() override;
	virtual void ServerHandleReload() override;
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void OnActionFinished(EFirearmGesture Gesture) override;
	virtual void OnActionStopped(EFirearmGesture Gesture) override;
	virtual bool PredictFire() const override;
	virtual float FireClipStart(EFirearmGesture Gesture) const override;
	virtual void OnCosmeticEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void TickCosmetics(float DeltaSeconds) override;

private:

	/** Server: does what the trigger decided (GrenadeMath::ETrigger) */
	void Execute(uint8 Trigger);

	/** Server: the grenade in the wind-up leaves the hand now (bDrop: let go at the feet) */
	void ThrowGrenade(bool bDrop);

	/** Rules of the grenade (Rules cast once) */
	const UGrenadeRules* GetGrenadeRules() const;

	/** Every machine: the grenade in hand is shown or not */
	void ShowGrenadeInHand(bool bShown);

	/** Grenade in hand */
	UPROPERTY(Replicated)
	bool bInHand = false;

	/** Grenades left in the bag (carrier only) */
	UPROPERTY(Replicated)
	int32 BagGrenades = 0;

	/** Server: the wind-up plays, the grenade has not left yet */
	bool bThrowPending = false;

	/** Server: the throw under way is the long one (aimed) */
	bool bThrowLong = false;

	/** Server: world time the fuse of the grenade in hand was lit (< 0: not lit) */
	double LitTime = -1.0;

	/** Local world time the hand was last shown or hidden by a clip event (the replicated state waits on it) */
	double HandCosmeticTime = -100.0;

	/** The grenade in hand is shown on this machine */
	bool bHandShown = true;
};
