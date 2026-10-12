// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DeathCause.h"
#include "IslanderHealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FIslanderHealthChangedSignature, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIslanderDeathSignature, AActor*, Killer);

/**
 *  Health of a player or creature (cahier §4: 100). Server authoritative and replicated.
 *  Any UGameplayStatics::ApplyDamage on the owner lands here; also computes fall damage.
 */
UCLASS(ClassGroup=(Islander), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UIslanderHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UIslanderHealthComponent();

	/** Maximum health (replicated: creatures set it from their rules on the server) */
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category="Health", meta=(ClampMin="1"))
	float MaxHealth = 100.0f;

	/** Landing speed under which a fall does no damage (cm/s); 1000 cm/s is a drop of about 5 m at 9.8 m/s^2 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health|Fall", meta=(ClampMin="0"))
	float SafeFallSpeed = 1000.0f;

	/** Landing speed from which a fall is lethal (cm/s); 2000 cm/s is a drop of about 20 m */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health|Fall", meta=(ClampMin="0"))
	float LethalFallSpeed = 2000.0f;

	/** Broadcast on every client and the server when health changes */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FIslanderHealthChangedSignature OnHealthChanged;

	/** Broadcast on every client and the server when health reaches zero; a client that receives the owner already dead
	 *  hears it once, one frame after BeginPlay (see WasDeadOnArrival) */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FIslanderDeathSignature OnDeath;

	/** Current health */
	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth() const { return Health; }

	/** Current health in [0, 1] */
	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthFraction() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	/** True once health reached zero */
	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return bDead; }

	/**
	 *  True on a client that received its owner already dead (it joined late, or the owner came back into relevance):
	 *  the death this client is told of then, one frame after BeginPlay so the owner's own bindings hear it, is the
	 *  replay of an old one, to settle the body without its cry nor its blood (network review R-26)
	 */
	UFUNCTION(BlueprintPure, Category="Health")
	bool WasDeadOnArrival() const { return bDeadOnArrival; }

	/** How the owner died (recorded by the server on the fatal blow, replicated with the death): the cause the death
	 *  screen tells (T-135); unset while alive */
	UFUNCTION(BlueprintPure, Category="Health")
	FDeathRecord GetDeathRecord() const { return DeathRecord; }

	/** Removes health (server only); returns the damage actually applied. Cause: what the caller knows of the blow (a
	 *  fall, drowning, the deluge), recorded if it kills */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	float ApplyHealthDamage(float Amount, AActor* DamageCauser, EDeathCause Cause = EDeathCause::Unknown);

	/** Restores health (server only), capped at MaxHealth and refused once dead; returns the health actually restored */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	float ApplyHeal(float Amount, AActor* Source);

	/** Health a heal of Amount actually restores from CurrentHealth (never above MaxHealth, never negative) */
	static float ComputeHeal(float CurrentHealth, float MaxHealth, float Amount);

	/** Applies the damage of a landing at this vertical speed (server only, cm/s, positive downward) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health|Fall")
	float ApplyFallDamage(float LandingSpeed);

	/** Restores full health and clears death (server only), e.g. on respawn */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	void ResetHealth();

	/** Multiplies the maximum health and refills it (server only): the difficulty of the day on a creature that just
	 *  appeared (docs/features/campagne-dix-jours.md); nothing at 1 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	void ScaleMaxHealth(float Scale);

	/** Fall damage of a landing: none up to SafeSpeed, MaxDamage from LethalSpeed, linear in between */
	static float ComputeFallDamage(float LandingSpeed, float SafeSpeed, float LethalSpeed, float MaxDamage);

	/** Damage after the difficulty of the day: a player struck by a mob, a soldier or the crab (anything no player
	 *  holds) takes MobDamageScale times the blow; a creature struck, or a player struck by a player or by the island
	 *  (a fall, the moat), takes it as is */
	static float ScaleMobDamage(float Damage, float MobDamageScale, bool bOwnerIsPlayer, bool bFromMob);

	/** True when the actor is a pawn a player holds */
	static bool IsPlayerPawn(const AActor* Actor);

	/** True when the blow comes from a pawn no player holds (a mob, a soldier, the crab: itself, or a weapon or a
	 *  projectile of its), and not from a player's controller */
	static bool IsMobCauser(const AController* InstigatedBy, const AActor* DamageCauser);

	/** Record of a fatal blow (T-135): Cause as given, else Player for a player's blow; the pawn behind the blow (the
	 *  instigator's, the causer itself, or the pawn that fired or holds it), else the causer; the damage type */
	static FDeathRecord MakeDeathRecord(EDeathCause Cause, const AActor* DamageCauser, const AController* InstigatedBy, const UDamageType* DamageType, float Time);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	/** Routes engine damage to ApplyHealthDamage */
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** Replays health changes on clients */
	UFUNCTION()
	void OnRep_Health(float OldHealth);

private:

	/** ApplyHealthDamage with what the engine knows of the blow (its instigator and damage type) */
	float ApplyDamageFrom(float Amount, AActor* DamageCauser, const AController* InstigatedBy, const UDamageType* DamageType, EDeathCause Cause);

	/** How the owner died: set on the server before the death notifications, cleared by ResetHealth (T-135) */
	UPROPERTY(Replicated)
	FDeathRecord DeathRecord;

	/** Current health */
	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health = 100.0f;

	/** Health reached zero */
	UPROPERTY(Replicated)
	bool bDead = false;

	/** Actor that dealt the killing blow, for OnDeath on clients */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> LastDamageCauser;

	/** Client: the owner was already dead when it began play here (see WasDeadOnArrival) */
	bool bDeadOnArrival = false;
};
