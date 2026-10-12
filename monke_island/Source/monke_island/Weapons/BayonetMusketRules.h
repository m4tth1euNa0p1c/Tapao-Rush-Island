// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Weapons/PirateMusketRules.h"
#include "BayonetMusketRules.generated.h"

class UCameraShakeBase;
class USoundBase;

/** One use of the bayonet: the thrust (BayonetStab) or the charge (BayonetCharge), all tunable (DA_BayonetMusketRules) */
USTRUCT(BlueprintType)
struct FBayonetThrust
{
	GENERATED_BODY()

	/** Damage dealt to each target the point strikes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0"))
	float Damage = 55.0f;

	/** Reach of the point from the carrier's centre to the nearest point of a target's body (cm, horizontal) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="10", Units="cm"))
	float ReachCm = 150.0f;

	/** Half angle of the cone in front of the carrier where the point strikes (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="1", ClampMax="90", Units="deg"))
	float HalfAngleDegrees = 30.0f;

	/** Targets one use strikes at most, the one right in front of the point first */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="1", ClampMax="8"))
	int32 MaxTargets = 1;

	/** Stamina it costs, share of the carrier's MaxStamina (refused below that) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", ClampMax="1"))
	float StaminaCostFraction = 0.12f;

	/** The carrier lunges this far toward the point (cm, over LungeSeconds; stopped short of an enemy ahead) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm"))
	float LungeDistanceCm = 40.0f;

	/** Time of the lunge (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0.01", Units="s"))
	float LungeSeconds = 0.18f;

	/** Speed given to a character the point strikes and does not kill, along the thrust (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackSpeed = 220.0f;

	/** Upward speed given with that push (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm/s"))
	float KnockbackUpSpeed = 50.0f;

	/** Impulse given to a physics body the point strikes (kg cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0"))
	float PhysicsImpulse = 30000.0f;

	/** Event of the gesture's clip where the point strikes (StabHit, ChargeHit; none in the clip: at its end) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	FName HitEvent = TEXT("StabHit");

	/** Scale of the carrier's camera shake when the point strikes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0"))
	float HitShakeScale = 1.0f;
};

/**
 *  Tunable rules of the bean's bayonet musket (DA_BayonetMusketRules, BP_BayonetMusket; owner 2026-10-11: "fusil
 *  mousquet avec baïonnette et utilisation de baïonnette", T-335; model and clips T-334). The pirate musket's shot and
 *  reload, unchanged (UPirateMusketRules), plus the bayonet on the right click (the musket is fired from the hip: the
 *  aim key is free): a thrust (BayonetStab) that strikes at StabHit in a short cone in front of the carrier, and, on the
 *  run, the charge (BayonetCharge): a longer lunge, more damage, at ChargeHit. Both cost stamina. The point deals its
 *  blow as the cutlass does (UGameplayStatics::ApplyPointDamage, this weapon the causer): tikis, French soldiers and the
 *  giant crab take it.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBayonetMusketRules : public UPirateMusketRules
{
	GENERATED_BODY()

public:

	UBayonetMusketRules();

	/** The thrust (right click; BayonetStab) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	FBayonetThrust Stab;

	/** The charge (right click while running; BayonetCharge): a longer lunge, more damage, more stamina */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	FBayonetThrust Charge;

	/** The carrier charges when it runs (sprint) or moves at least this fast (cm/s; 0 = the sprint only) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm/s"))
	float ChargeMinSpeedCmps = 450.0f;

	/** The server takes a charge the carrier asked for at this share of ChargeMinSpeedCmps (its speed changed a little
	 *  since the press), else it thrusts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", ClampMax="1"))
	float ChargeSpeedTolerance = 0.75f;

	/** A thrust cuts the musket's reload (the ball stays in the pouch) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	bool bThrustCutsReload = true;

	/** Targets farther above or below the carrier's centre are out of reach (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm"))
	float HeightToleranceCm = 140.0f;

	/** The lunge stops this far short of the nearest enemy ahead (cm): no lunge through it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm"))
	float LungeBlockDistanceCm = 70.0f;

	/** Speed the carrier keeps at most when the lunge ends (cm/s): a charge does not fling it on */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0", Units="cm/s"))
	float LungeExitSpeedCmps = 600.0f;

	/** The body turns to face the thrust in third person (the point goes where the view looks) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	bool bFaceThrust = true;

	/** Object types the point strikes (pawns, physics bodies) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	TArray<TEnumAsByte<EObjectTypeQuery>> HitObjectTypes;

	/** The point needs a free line from the carrier's eyes to the target on this channel: no thrust through a wall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	TEnumAsByte<ECollisionChannel> SightChannel = ECC_Visibility;

	/** The bone the point strikes is the one its line (the eyes, through where it enters, this far into the body) passes
	 *  nearest to, within this distance (cm); farther, the body without a bone (the musket's shot keeps BoneHitRadiusCm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="1", Units="cm"))
	float ThrustBoneRadiusCm = 36.0f;

	/** Sound where the point strikes (heard around it; none = silent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	TObjectPtr<USoundBase> ThrustHitSound;

	/** Sound where the point kills (none = ThrustHitSound) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	TObjectPtr<USoundBase> ThrustKillSound;

	/** Shake of the carrier's camera when the point strikes (UCutlassHitCameraShake by default, none when empty) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet")
	TSubclassOf<UCameraShakeBase> HitCameraShake;

	/** That shake, times this when the point kills */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bayonet", meta=(ClampMin="0"))
	float KillShakeMultiplier = 1.25f;

	/** The thrust or the charge, by its gesture */
	const FBayonetThrust& GetThrust(bool bCharge) const { return bCharge ? Charge : Stab; }
};
