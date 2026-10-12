// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmWeapon.h"
#include "PirateMusketWeapon.generated.h"

class UPirateMusketRules;

/**
 *  Bean's redesigned flintlock musket (BP_PirateMusket, DA_PirateMusketRules; T-170, second PC: bean-musket v0.2 and
 *  bean-musket-anim v0.2). The gameplay of the old musket (AMusketWeapon) on the clips of the pack:
 *  - one ball in the barrel, the rest in the pouch (MaxBalls); fire pulls the trigger (the Fire clip starts at its
 *    Trigger event) and the ball leaves at the Shot event, after the lock time;
 *  - an empty barrel needs the full reload butt on the ground (GroundReload, stretched over ReloadSeconds); the ball
 *    goes into the barrel at its end only: swimming, grabbing, being knocked out or putting the musket away cuts it
 *    and the ball stays in the pouch.
 *  The server owns the ammunition; the shot is a trace from the carrier's eyes (AFirearmWeapon::FireProjectiles).
 */
UCLASS()
class MONKE_ISLAND_API APirateMusketWeapon : public AFirearmWeapon
{
	GENERATED_BODY()

public:

	APirateMusketWeapon();

	/** A ball is in the barrel */
	UFUNCTION(BlueprintPure, Category="Musket")
	bool IsLoaded() const { return bLoaded; }

	/** Balls left in the pouch (valid on the server and the carrier) */
	UFUNCTION(BlueprintPure, Category="Musket")
	int32 GetPouchBalls() const { return PouchBalls; }

	/** A reload is running */
	UFUNCTION(BlueprintPure, Category="Musket")
	bool IsReloading() const;

	/** Progress of the running reload (0-1) */
	UFUNCTION(BlueprintPure, Category="Musket")
	float GetReloadProgress() const;

	virtual int32 GetLoadedRounds() const override { return bLoaded ? 1 : 0; }
	virtual int32 GetReserveRounds() const override { return PouchBalls; }
	virtual int32 GetMaxRounds() const override;
	virtual void SetRounds(int32 Loaded, int32 Reserve) override { bLoaded = Loaded > 0; PouchBalls = FMath::Max(0, Reserve); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void ServerHandleFire() override;
	virtual void ServerHandleReload() override;
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void OnActionFinished(EFirearmGesture Gesture) override;
	virtual void OnActionStopped(EFirearmGesture Gesture) override;
	virtual bool PredictFire() const override;
	virtual bool CanAim() const override;
	virtual float FireClipStart(EFirearmGesture Gesture) const override;
	virtual float GetGestureRate(const FFirearmClip& Clip) const override;

	/** Projectiles one charge throws (1; the blunderbuss throws its PelletCount) */
	virtual int32 GetProjectilesPerShot() const { return 1; }

	/** Rules of the musket (Rules cast once) */
	const UPirateMusketRules* GetMusketRules() const;

private:

	/** Server: the ball of the pulled trigger leaves now */
	void ReleaseShot();

	/** Ball in the barrel */
	UPROPERTY(Replicated)
	bool bLoaded = false;

	/** Balls left in the pouch (carrier only) */
	UPROPERTY(Replicated)
	int32 PouchBalls = 0;

	/** Server: the trigger is pulled, the ball has not left yet */
	bool bShotPending = false;
};
