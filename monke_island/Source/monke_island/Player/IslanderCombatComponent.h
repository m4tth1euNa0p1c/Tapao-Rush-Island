// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "IslanderCombatComponent.generated.h"

class ACharacter;
class UIslanderMovementComponent;

/** Phase of the melee attack */
UENUM(BlueprintType)
enum class EIslanderAttackPhase : uint8
{
	Idle,
	/** Visible wind-up (cahier §5: preparation the target can read) */
	Windup,
	/** The blow lands */
	Strike,
	/** Arm comes back, cooldown running */
	Recover
};

/**
 *  Bare-hand melee for a player: punch on left click, guard while right click is held.
 *  The owning client predicts the visuals, the server validates cooldown and stamina, sweeps the hit,
 *  applies damage and knockback (cahier §5: server-validated melee with cooldown). The guard is a
 *  predicted movement flag (slower walk) that cuts frontal melee damage at a stamina cost per blocked hit.
 */
UCLASS(ClassGroup=(Islander), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UIslanderCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UIslanderCombatComponent();

	/** Damage of a punch */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0"))
	float PunchDamage = 20.0f;

	/** Reach of a punch from the capsule surface (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0"))
	float PunchReach = 90.0f;

	/** Radius of the punch sweep (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="1"))
	float PunchRadius = 30.0f;

	/** Wind-up before the blow lands (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0", Units="s"))
	float PunchWindup = 0.25f;

	/** Time the arm stays extended (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0", Units="s"))
	float PunchStrikeTime = 0.12f;

	/** Time from the start of an attack before the next one (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0", Units="s"))
	float PunchCooldown = 0.7f;

	/** Stamina spent by a punch; no punch without it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0"))
	float PunchStaminaCost = 10.0f;

	/** Horizontal speed given to a punched character (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0"))
	float PunchKnockback = 350.0f;

	/** Impulse given to a punched physics object (kg cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch", meta=(ClampMin="0"))
	float PunchImpulse = 30000.0f;

	/** Object types a punch can hit (pawns and physics bodies; not the vegetation) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Punch")
	TArray<TEnumAsByte<EObjectTypeQuery>> PunchObjectTypes;

	/** Damage multiplier of a blocked frontal hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Guard", meta=(ClampMin="0", ClampMax="1"))
	float GuardDamageMultiplier = 0.25f;

	/** Stamina lost per blocked hit; without enough stamina the guard breaks and the hit lands fully */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Guard", meta=(ClampMin="0"))
	float GuardStaminaPerBlock = 15.0f;

	/** Cosine of the half angle in front of the guard that blocks (0.2 = about 78 degrees each side) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Guard", meta=(ClampMin="-1", ClampMax="1"))
	float GuardFrontDot = 0.2f;

	/** Starts a punch (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void RequestPunch();

	/** Raises or lowers the guard (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetGuard(bool bInGuard);

	/** Current attack phase (predicted on the owner, replicated to others) */
	UFUNCTION(BlueprintPure, Category="Combat")
	EIslanderAttackPhase GetAttackPhase() const { return AttackPhase; }

	/** True while the guard is up */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsGuarding() const;

	/** Damage a guard lets through: full when down, broken or hit from behind */
	static float ComputeGuardedDamage(float Damage, bool bGuarding, float FacingDot, float FrontDot, float Multiplier, bool bHasStamina);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	/** Server side of RequestPunch */
	UFUNCTION(Server, Reliable)
	void ServerPunch();

	/** Server side of SetGuard, for the other players' visuals */
	UFUNCTION(Server, Reliable)
	void ServerSetGuard(bool bInGuard);

	/** Replays the punch phases on the other clients */
	UFUNCTION()
	void OnRep_PunchCount();

private:

	/** True when a punch may start now (cooldown, stamina, state) */
	bool CanPunch() const;

	/** Runs the wind-up / strike / recover timeline; the server applies the hit */
	void StartPunchTimeline(bool bApplyHit);

	/** Server: sweeps in front of the character and hits what is there */
	void ApplyPunchHit();

	/** Phase setters driven by timers */
	void EnterStrike();
	void EnterRecover();
	void EnterIdle();

	ACharacter* GetCharacter() const;
	UIslanderMovementComponent* GetIslanderMovement() const;

	/** Current attack phase */
	EIslanderAttackPhase AttackPhase = EIslanderAttackPhase::Idle;

	/** Incremented by the server for each punch, replays it on the other clients */
	UPROPERTY(ReplicatedUsing=OnRep_PunchCount)
	uint8 PunchCount = 0;

	/** Guard state for the other players (the owner reads its movement flag) */
	UPROPERTY(Replicated)
	bool bReplicatedGuard = false;

	/** World time of the last punch start */
	double LastPunchTime = -1.0e6;

	/** The server applies the hit when the strike starts */
	bool bPendingHit = false;

	FTimerHandle PhaseTimer;
};
