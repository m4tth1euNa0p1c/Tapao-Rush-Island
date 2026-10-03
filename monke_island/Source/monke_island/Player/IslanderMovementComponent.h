// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "IslanderMovementComponent.generated.h"

class UIslandWaterSubsystem;

/**
 *  Character movement tuned to a real-world adult (1 unit = 1 cm, gravity 9.8 m/s^2, 75 kg).
 *  Adds sprinting, wading (slower in shallow water) and swimming driven by the island sea level,
 *  so the whole lagoon and ocean are swimmable without placing water volumes.
 *  Lethal Company style stamina: sprinting, swimming and jumping spend it, walking and standing still
 *  restore it; once empty the character is exhausted (no sprint, slower, slow swim) until it recovers.
 *  Sprint, guard and flight are network predicted (compressed move flags); crouching uses the engine flag.
 */
UCLASS()
class MONKE_ISLAND_API UIslanderMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UIslanderMovementComponent();

	/** Max ground speed while sprinting (cm/s). A fit adult sprints at about 6-7 m/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Sprint", meta=(ClampMin="0"))
	float MaxSprintSpeed = 600.0f;

	/** Swim speed multiplier while sprinting */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="1"))
	float SprintSwimSpeedMultiplier = 1.5f;

	/** Water depth at the feet from which the character swims instead of walking (cm, about chest height) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="0"))
	float SwimStartDepth = 120.0f;

	/** Depth margin below SwimStartDepth before a swimmer stands up again (cm), avoids flickering */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="0"))
	float SwimExitHysteresis = 15.0f;

	/** Ground speed multiplier when wading with water at SwimStartDepth (linear from 1 when dry) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="0", ClampMax="1"))
	float WadingMinSpeedFactor = 0.4f;

	/** Flight speed multiplier while sprinting */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Flying", meta=(ClampMin="1"))
	float SprintFlySpeedMultiplier = 2.0f;

	/** Speed into a blocking surface (cm/s) above which a flying character crashes into ragdoll; 0 disables crashes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Flying", meta=(ClampMin="0"))
	float FlyingCrashSpeed = 700.0f;

	/** Maximum stamina (cahier §4: 100) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="1"))
	float MaxStamina = 100.0f;

	/** Stamina spent per second while sprinting on the ground or in the water */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0"))
	float SprintStaminaCost = 15.0f;

	/** Stamina spent per second while swimming (cahier §4: swimming spends stamina) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0"))
	float SwimStaminaCost = 4.0f;

	/** Stamina spent per second while treading water without moving */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0"))
	float SwimIdleStaminaCost = 2.0f;

	/** Stamina spent by a jump; a jump needs at least this much */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0"))
	float JumpStaminaCost = 10.0f;

	/** Stamina restored per second while walking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0"))
	float WalkStaminaRegen = 10.0f;

	/** Stamina restored per second while standing still */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0"))
	float IdleStaminaRegen = 20.0f;

	/** Delay after spending stamina before it restores (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0", Units="s"))
	float StaminaRegenDelay = 0.8f;

	/** Fraction of MaxStamina to recover before an exhausted character can sprint again */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0", ClampMax="1"))
	float ExhaustedRecoverFraction = 0.35f;

	/** Ground speed multiplier while exhausted */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0", ClampMax="1"))
	float ExhaustedSpeedFactor = 0.85f;

	/** Swim speed multiplier while exhausted (cahier §4: slow swim, no instant death) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Stamina", meta=(ClampMin="0", ClampMax="1"))
	float ExhaustedSwimSpeedFactor = 0.6f;

	/** Upward speed of a jump out of the water, enough to climb over a gunwale (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="0"))
	float WaterJumpZVelocity = 620.0f;

	/** Horizontal speed added toward the swimming direction by a jump out of the water (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="0"))
	float WaterJumpForwardSpeed = 150.0f;

	/** Body immersion (0 dry, 1 under) above which a swimmer is too deep to jump out of the water */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Swimming", meta=(ClampMin="0", ClampMax="1"))
	float WaterJumpMaxImmersion = 0.95f;

	/** Buoyancy of a swimmer out of stamina: below 1 the body slowly sinks (ADR-003 B-04) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Drowning", meta=(ClampMin="0"))
	float DrowningBuoyancy = 0.85f;

	/** Seconds out of stamina in the water before drowning hurts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Drowning", meta=(ClampMin="0", Units="s"))
	float DrownGraceTime = 3.0f;

	/** Health lost per second while drowning */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Drowning", meta=(ClampMin="0"))
	float DrownDamagePerSecond = 12.0f;

	/** Ground speed multiplier while guarding */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Guard", meta=(ClampMin="0", ClampMax="1"))
	float GuardSpeedFactor = 0.5f;

	/** Speed lost per kilogram carried (fraction, Lethal Company style weight) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Weight", meta=(ClampMin="0"))
	float SpeedLossPerKg = 0.008f;

	/** Lowest speed multiplier a heavy load can cause */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Weight", meta=(ClampMin="0", ClampMax="1"))
	float MinWeightSpeedFactor = 0.45f;

	/** Extra stamina cost per kilogram carried (fraction of the cost) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Weight", meta=(ClampMin="0"))
	float StaminaCostPerKg = 0.02f;

	/** Force a walking character pushes physics objects with (kg cm/s^2; 25000 = 250 N, a person leaning on a boat) */
	static constexpr float HumanPushForce = 25000.0f;

	/** Stamina spent per second and per kilogram carried while moving: carrying tires even at a walk (10 kg -> 5/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Weight", meta=(ClampMin="0"))
	float CarryStaminaPerKgMoving = 0.5f;

	/** Stamina spent per second and per kilogram carried while standing still (holding at arm's length) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Movement: Weight", meta=(ClampMin="0"))
	float CarryStaminaPerKgIdle = 0.15f;

	/** Stamina per second the load alone costs (0 when nothing is carried) */
	static float ComputeCarryStaminaCost(float CarriedKg, bool bMoving, float PerKgMoving, float PerKgIdle)
	{
		return FMath::Max(0.0f, CarriedKg) * (bMoving ? PerKgMoving : PerKgIdle);
	}

	/** Sets whether the character wants to sprint */
	void SetWantsToSprint(bool bInWantsToSprint) { bWantsToSprint = bInWantsToSprint; }

	/** Sets whether the character holds its guard */
	void SetWantsToGuard(bool bInWantsToGuard) { bWantsToGuard = bInWantsToGuard; }

	/** Sets whether the character wants to fly (applied by the movement update, predicted) */
	void SetWantsToFly(bool bInWantsToFly) { bWantsToFly = bInWantsToFly; }

	/** Returns true when flight is requested */
	bool WantsToFly() const { return bWantsToFly; }

	/** Returns true while sprinting on the ground or in the water */
	UFUNCTION(BlueprintPure, Category="Character Movement: Sprint")
	bool IsSprinting() const;

	/** Returns true while the guard is up */
	UFUNCTION(BlueprintPure, Category="Character Movement: Guard")
	bool IsGuarding() const;

	/** Current stamina */
	UFUNCTION(BlueprintPure, Category="Character Movement: Stamina")
	float GetStamina() const { return Stamina; }

	/** Current stamina in [0, 1] */
	UFUNCTION(BlueprintPure, Category="Character Movement: Stamina")
	float GetStaminaFraction() const { return MaxStamina > 0.0f ? Stamina / MaxStamina : 0.0f; }

	/** True while swimming without any stamina left: the body sinks and drowning starts after DrownGraceTime */
	UFUNCTION(BlueprintPure, Category="Character Movement: Drowning")
	bool IsDrowning() const { return IsSwimming() && Stamina <= 0.0f; }

	/** Advances the drowning clock; returns the health to remove for this step */
	float UpdateDrowning(float DeltaSeconds, bool bDrowning);

	/** True from the moment stamina runs out until it has recovered ExhaustedRecoverFraction */
	UFUNCTION(BlueprintPure, Category="Character Movement: Stamina")
	bool IsExhausted() const { return bExhausted; }

	/** Spends stamina if enough is left; returns false (and spends nothing) otherwise */
	UFUNCTION(BlueprintCallable, Category="Character Movement: Stamina")
	bool ConsumeStamina(float Amount);

	/** Advances stamina by DeltaSeconds for the given activity (spends or restores) */
	void UpdateStamina(float DeltaSeconds, bool bSprinting, bool bSwimming, bool bMoving);

	/** Weight currently carried (kg): slows the character and raises stamina costs */
	UFUNCTION(BlueprintCallable, Category="Character Movement: Weight")
	void SetCarriedWeight(float InKilograms) { CarriedWeight = FMath::Max(0.0f, InKilograms); }

	/** Weight currently carried (kg) */
	UFUNCTION(BlueprintPure, Category="Character Movement: Weight")
	float GetCarriedWeight() const { return CarriedWeight; }

	/** Returns the sea water depth above the bottom of the capsule (cm, 0 when dry) */
	UFUNCTION(BlueprintPure, Category="Character Movement: Swimming")
	float GetWaterDepthAtFeet() const;

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual float GetMaxSpeed() const override;

	virtual bool CanAttemptJump() const override;

	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	virtual bool IsInWater() const override;

	virtual float ImmersionDepth() const override;

protected:

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.0f, const FVector& MoveDelta = FVector::ZeroVector) override;

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

private:

	friend class FSavedMove_Islander;

	/** Speed multiplier of the carried weight */
	float GetWeightSpeedFactor() const;

	/** Sprint input state (predicted, FLAG_Custom_0) */
	bool bWantsToSprint = false;

	/** Guard input state (predicted, FLAG_Custom_1) */
	bool bWantsToGuard = false;

	/** Flight request (predicted, FLAG_Custom_2) */
	bool bWantsToFly = false;

	/** Current stamina */
	float Stamina = 100.0f;

	/** Out of stamina and not recovered yet */
	bool bExhausted = false;

	/** Time since stamina was last spent (s) */
	float TimeSinceStaminaSpent = 0.0f;

	/** Weight carried (kg) */
	float CarriedWeight = 0.0f;

	/** A jump out of the water is rising: do not fall back into swimming yet */
	bool bWaterJumping = false;

	/** Seconds spent drowning (out of stamina in the water) */
	float DrownTime = 0.0f;

	/** Buoyancy set in the defaults, restored once the swimmer has stamina again */
	float DefaultBuoyancy = 1.2f;

	/** A flying crash was detected and the ragdoll starts next tick */
	bool bCrashPending = false;

	/** Provides the sea level */
	UPROPERTY(Transient)
	TObjectPtr<UIslandWaterSubsystem> WaterSubsystem;
};
