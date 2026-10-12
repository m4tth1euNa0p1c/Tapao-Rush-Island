// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmWeapon.h"
#include "PistolWeapon.generated.h"

class UPistolRules;

/**
 *  Bean's pirate pistol (BP_Pistol, DA_PistolRules; second PC: bean-pirate-pistol). One hand, one charge: fire when
 *  loaded and cocked, then a full reload (about 7 s: paper cartridge, ramrod, hammer cocked at HammerCocked).
 *  Fire on a lowered hammer cocks it and shoots in the same press (bAutoCockOnFire, else the Cock gesture first),
 *  fire on an empty cocked pistol without charges clicks (DryFire).
 *  Double pistol (BP_DoublePistol, DA_DoublePistolRules, owner 2026-10-09): UPistolRules::Barrels = 2, two charges
 *  loaded, Fire then AimFire / Fire shoot the second barrel (its flame at SecondMuzzleSocket), the reload fills both.
 *  The server owns the charges; the ball is a trace from the carrier's eyes (AFirearmWeapon::FireProjectiles).
 */
UCLASS()
class MONKE_ISLAND_API APistolWeapon : public AFirearmWeapon
{
	GENERATED_BODY()

public:

	APistolWeapon();

	/** A charge is in a barrel */
	UFUNCTION(BlueprintPure, Category="Pistol")
	bool IsLoaded() const { return LoadedCharges > 0; }

	/** Charges in the barrels (0 to Barrels) */
	UFUNCTION(BlueprintPure, Category="Pistol")
	int32 GetLoadedCharges() const { return LoadedCharges; }

	/** The hammer is cocked */
	UFUNCTION(BlueprintPure, Category="Pistol")
	bool IsCocked() const { return bCocked; }

	/** Barrel the last shot left from (0 the first, 1 the second of a double pistol) */
	UFUNCTION(BlueprintPure, Category="Pistol")
	int32 GetLastShotBarrel() const { return LastShotBarrel; }

	virtual int32 GetLoadedRounds() const override { return LoadedCharges; }
	virtual int32 GetReserveRounds() const override { return ReserveCharges; }
	virtual int32 GetMaxRounds() const override;
	virtual void SetRounds(int32 Loaded, int32 Reserve) override;

	/** The reload lowers the aim too (network review R-38) */
	virtual bool LowersAim(EFirearmGesture Gesture) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void ServerHandleFire() override;
	virtual void ServerHandleReload() override;
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void OnActionFinished(EFirearmGesture Gesture) override;
	virtual bool PredictFire() const override;
	virtual void OnShotPredicted(EFirearmGesture Shot) override;
	virtual FName GetShotMuzzleSocket() const override;

private:

	/** Server: does what the trigger decided */
	void Execute(uint8 Trigger);

	/** Rules of the pistol (Rules cast once) */
	const UPistolRules* GetPistolRules() const;

	/** Barrels of the rules (1 without rules) */
	int32 GetBarrels() const;

	/** Charges in the barrels */
	UPROPERTY(Replicated)
	int32 LoadedCharges = 0;

	/** Hammer cocked */
	UPROPERTY(Replicated)
	bool bCocked = false;

	/** Charges left in the pouch (carrier only) */
	UPROPERTY(Replicated)
	int32 ReserveCharges = 0;

	/** Barrel of the last shot (the muzzle its flame comes out of); the carrier guesses it for its predicted shot */
	UPROPERTY(Replicated)
	uint8 LastShotBarrel = 0;
};
