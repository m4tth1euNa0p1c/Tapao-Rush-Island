// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/PirateMusketWeapon.h"
#include "BayonetMusketWeapon.generated.h"

class UBayonetMusketRules;
class UEnhancedInputComponent;
class UInputAction;
struct FBayonetThrust;

/**
 *  Bean's bayonet musket (BP_BayonetMusket, DA_BayonetMusketRules; owner 2026-10-11, T-335; model and clips T-334): the
 *  pirate musket, its shot and its reload unchanged, with a bayonet on the right click (the musket is fired from the
 *  hip, the aim key is free; ThrustAction overrides it):
 *  - the thrust (BayonetStab): a short lunge, the point strikes at StabHit in a short cone in front of the carrier;
 *  - on the run, the charge (BayonetCharge): a longer lunge, more damage, at ChargeHit.
 *  Both cost stamina and cut the reload (the ball stays in the pouch). The server decides everything (stamina, the
 *  gesture, the targets, the damage: UGameplayStatics::ApplyPointDamage with this weapon as the causer, as the
 *  cutlass); the carrier shows its thrust and its lunge at once and takes them back when the server refuses.
 */
UCLASS()
class MONKE_ISLAND_API ABayonetMusketWeapon : public APirateMusketWeapon
{
	GENERATED_BODY()

public:

	ABayonetMusketWeapon();

	/** Key of the bayonet; empty = the right click (AimAction: the musket is not aimed) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> ThrustAction;

	/** Thrusts the bayonet, or charges when the carrier runs (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Bayonet")
	void RequestThrust();

	/** A thrust or a charge is playing (the server's gesture) */
	UFUNCTION(BlueprintPure, Category="Bayonet")
	bool IsThrusting() const;

	/** The thrusts lower the aim too (when the rules allow aiming) */
	virtual bool LowersAim(EFirearmGesture Gesture) const override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

	/** Forgets the bindings of a former carrier: the next carrier binds its own (T-288) */
	virtual void OnInputBindingsRemoved() override;
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void OnActionFinished(EFirearmGesture Gesture) override;
	virtual void OnActionStopped(EFirearmGesture Gesture) override;

	/** No shot shown over a thrust the carrier shows before the server confirmed it */
	virtual bool PredictFire() const override;

	/** The carrier thrusts (bCharge: it asks for the charge, it runs); the server checks everything again */
	UFUNCTION(Server, Reliable)
	void ServerThrust(bool bCharge);

	/** The server refused the thrust the carrier shows: back to the carry, the lunge taken back */
	UFUNCTION(Client, Reliable)
	void ClientRejectThrust(bool bCharge);

	/** The point struck (bKill: and killed): the carrier feels it */
	UFUNCTION(Client, Unreliable)
	void ClientThrustStruck(bool bCharge, bool bKill);

	/** The point struck at Location: its sound on every machine (cosmetic, a lost packet loses it) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastThrustSound(FVector_NetQuantize Location, bool bKill);

private:

	/** Rules of the bayonet musket (Rules cast) */
	const UBayonetMusketRules* GetBayonetRules() const;

	/** Carrier: binds the bayonet key once on its input */
	void BindThrust();

	/** Input handler */
	void HandleThrustInput();

	/** The carrier runs fast enough for the charge (SpeedShare of ChargeMinSpeedCmps counts: the server's slack) */
	bool WantsCharge(float SpeedShare) const;

	/** The carrier has the stamina of the thrust */
	bool HasStaminaFor(const FBayonetThrust& Thrust) const;

	/** Server and carrier: the body faces the view (third person) */
	void FaceThrust();

	/** Server and carrier: the lunge toward the point (root motion), stopped short of an enemy ahead */
	void BeginLunge(const FBayonetThrust& Thrust);

	/** Distance to the nearest enemy ahead of the carrier along Forward within Range (cm), -1 when none */
	float NearestEnemyAhead(const FVector& Forward, float Range) const;

	/** Server: the point strikes now (once per thrust) */
	void Strike();

	/** Carrier: the camera shakes when the point strikes */
	void PlayStruckFeel(bool bCharge, bool bKill);

	/** Server: the thrust playing is the charge */
	bool bServerCharge = false;

	/** Server: the point of the thrust playing struck already (or the thrust was cut) */
	bool bStruck = true;

	/** Carrier: input component the bayonet key is bound on */
	TWeakObjectPtr<UEnhancedInputComponent> ThrustInput;
};
