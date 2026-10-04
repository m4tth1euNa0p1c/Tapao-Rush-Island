// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/TorchRules.h"
#include "TorchItem.generated.h"

class UBeanPuppetComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/** Move playing, replicated to the other machines (the counter restarts the same move) */
USTRUCT()
struct FTorchMoveState
{
	GENERATED_BODY()

	UPROPERTY()
	ETorchMove Move = ETorchMove::None;

	UPROPERTY()
	uint8 Counter = 0;
};

/**
 *  Bean's hand torch (T-099), inventory slot 3, held in the left hand. Key 3 takes it from the hip (and lights it with
 *  the flint the first time it is out), key 3 again puts it out and hangs it back. Right click held raises it (lights
 *  farther), left click a blow: damage, and a lit head sets the target on fire. Burns BurnTime seconds of fuel,
 *  goes out in the water. Server authoritative: lit state, fuel and moves replicate; the light flickers locally.
 *  The body animation is the library's (UBeanAnimDriverComponent plays A_Bean_Torch_* on the upper body).
 */
UCLASS()
class MONKE_ISLAND_API ATorchItem : public AInventoryItemActor
{
	GENERATED_BODY()

public:

	ATorchItem();

	/** Light, fuel, moves and keys (DA_TorchRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UTorchRules> Rules;

	/** Burning */
	UFUNCTION(BlueprintPure, Category="Torch")
	bool IsLit() const { return bLit; }

	/** Raised (right click held) */
	UFUNCTION(BlueprintPure, Category="Torch")
	bool IsRaised() const { return bRaised; }

	/** Fuel left (s) */
	UFUNCTION(BlueprintPure, Category="Torch")
	float GetFuel() const { return Fuel; }

	/** Move playing on this machine, its elapsed time and duration (for the body animation) */
	ETorchMove GetLocalMove() const { return LocalMove; }
	float GetLocalMoveElapsed() const;
	float GetLocalMoveDuration() const { return Rules ? Rules->GetMoveDuration(LocalMove) : 0.0f; }

	/** The torch can be used now (in hand, carrier standing and alive) */
	bool IsUsable() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;
	virtual void OnAttachedToCarrier() override;

	UFUNCTION(Server, Reliable)
	void ServerSwing();

	UFUNCTION(Server, Reliable)
	void ServerSetRaised(bool bInRaised);

	UFUNCTION()
	void OnRep_MoveState();

private:

	/** Server: starts a move for everyone */
	void StartMove(ETorchMove Move);

	/** Plays a move on this machine */
	void PlayMove(ETorchMove Move);

	/** Server: the flint catches */
	void FlameOn();

	/** Server: the blow reaches its arc */
	void ApplySwingHit();

	/** Server: one second of burn on a target */
	void Burn(TWeakObjectPtr<AActor> Target, int32 TicksLeft);

	void BindInput();
	void HandleSwing();
	void HandleRaiseStarted();
	void HandleRaiseCompleted();

	/** Places the torch in the hand or on the hip, after the puppet's pose */
	void UpdateAttachment();

	/** Flame and light of this frame */
	void UpdateFlame(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> TorchMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> FlameMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(ReplicatedUsing=OnRep_MoveState)
	FTorchMoveState MoveState;

	UPROPERTY(Replicated)
	bool bLit = false;

	UPROPERTY(Replicated)
	bool bRaised = false;

	UPROPERTY(Replicated)
	float Fuel = 300.0f;

	ETorchMove LocalMove = ETorchMove::None;
	double LocalMoveStart = 0.0;
	float RaiseAlpha = 0.0f;
	double SpawnTime = 0.0;
	bool bInputBound = false;
	FTimerHandle FlameTimer;
	FTimerHandle HitTimer;
	FTimerHandle IgniteTimer;
	TWeakObjectPtr<UBeanPuppetComponent> Puppet;
};
