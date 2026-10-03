// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ValuableComponent.generated.h"

class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FValuableValueChangedSignature, int32, NewValue, int32, Loss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FValuableBrokenSignature);

/**
 *  R.E.P.O. style valuable: the object loses value on every hard impact and breaks at zero.
 *  Server authoritative (cahier §5: a client never chooses the value of loot); the value replicates.
 */
UCLASS(ClassGroup=(Items), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UValuableComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UValuableComponent();

	/** Value of the intact object */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Valuable", meta=(ClampMin="0"))
	int32 BaseValue = 100;

	/** Impact speed under which nothing is lost (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Valuable", meta=(ClampMin="0"))
	float SafeImpactSpeed = 250.0f;

	/** Impact speed that costs MaxLossPerHit of the base value (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Valuable", meta=(ClampMin="0"))
	float HeavyImpactSpeed = 1200.0f;

	/** Fraction of the base value lost by a heavy impact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Valuable", meta=(ClampMin="0", ClampMax="1"))
	float MaxLossPerHit = 0.35f;

	/** Minimum time between two counted impacts (s), one bounce is one hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Valuable", meta=(ClampMin="0", Units="s"))
	float HitCooldown = 0.25f;

	/** Destroy the object when its value reaches zero */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Valuable")
	bool bBreakAtZero = true;

	/** Broadcast on every machine when the value changes */
	UPROPERTY(BlueprintAssignable, Category="Valuable")
	FValuableValueChangedSignature OnValueChanged;

	/** Broadcast on the server when the object breaks */
	UPROPERTY(BlueprintAssignable, Category="Valuable")
	FValuableBrokenSignature OnBroken;

	/** Current value */
	UFUNCTION(BlueprintPure, Category="Valuable")
	int32 GetValue() const { return Value; }

	/** Removes value (server only); returns the loss actually applied */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Valuable")
	int32 ApplyValueLoss(int32 Loss);

	/** Value lost by an impact at this speed */
	static int32 ComputeImpactLoss(float ImpactSpeed, float SafeSpeed, float HeavySpeed, float MaxLossFraction, int32 InBaseValue);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	/** Rigid body hits of the owner's root (server) */
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Plays value changes on clients */
	UFUNCTION()
	void OnRep_Value(int32 OldValue);

private:

	/** Current value */
	UPROPERTY(ReplicatedUsing=OnRep_Value)
	int32 Value = 100;

	/** World time of the last counted hit */
	double LastHitTime = -1.0e6;
};
