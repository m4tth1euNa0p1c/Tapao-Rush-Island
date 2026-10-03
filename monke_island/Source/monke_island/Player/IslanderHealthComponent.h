// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

	/** Maximum health */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="1"))
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

	/** Broadcast on every client and the server when health reaches zero */
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

	/** Removes health (server only); returns the damage actually applied */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	float ApplyHealthDamage(float Amount, AActor* DamageCauser);

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

	/** Fall damage of a landing: none up to SafeSpeed, MaxDamage from LethalSpeed, linear in between */
	static float ComputeFallDamage(float LandingSpeed, float SafeSpeed, float LethalSpeed, float MaxDamage);

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

	/** Current health */
	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health = 100.0f;

	/** Health reached zero */
	UPROPERTY(Replicated)
	bool bDead = false;

	/** Actor that dealt the killing blow, for OnDeath on clients */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> LastDamageCauser;
};
