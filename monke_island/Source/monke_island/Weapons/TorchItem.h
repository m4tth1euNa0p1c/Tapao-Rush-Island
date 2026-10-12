// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/TorchRules.h"
#include "TorchItem.generated.h"

class UBeanFirstPersonArmsComponent;
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
 *  goes out in the water. Server authoritative: lit state and moves replicate; the fuel replicates only when the torch
 *  is lit or put out (the fuel left then and the server time it was lit), every machine computes the fuel left from
 *  them (network review R-40); the light flickers locally. Low net frequency, every change forces an update.
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

	/** Fuel left (s), computed from the last lighting on every machine */
	UFUNCTION(BlueprintPure, Category="Torch")
	float GetFuel() const;

	/** Fuel left (s) at server time Now: FuelAtChange, burnt down since LitTime while lit, never below zero */
	static float ComputeFuel(float FuelAtChange, double LitTime, double Now, bool bInLit);

	/** Move playing on this machine, its elapsed time and duration (for the body animation) */
	ETorchMove GetLocalMove() const { return LocalMove; }
	float GetLocalMoveElapsed() const;
	float GetLocalMoveDuration() const { return Rules ? Rules->GetMoveDuration(LocalMove) : 0.0f; }

	/** The torch can be used now (in hand, carrier standing and alive) */
	bool IsUsable() const;

	/** In first person the carrier sees the torch's own view model in the first person arms, not the body's arms (T-330) */
	bool UsesFirstPersonViewModel() const { return Rules && Rules->bFirstPersonViewModel; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;

	/** Forgets the bindings of a former carrier: the next carrier binds its own (T-288) */
	virtual void OnInputBindingsRemoved() override;
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

	/** Server: the flame goes out, the fuel left kept */
	void PutOut();

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

	/** First person torch of the carrier's own view: shown or not, its pose, the left first person arm on its grip and
	 *  the light on its flame (T-330) */
	void UpdateFirstPerson(float DeltaSeconds);

	/** Pose of the first person torch for the move playing and the raise, relative to the camera */
	FTransform ComputeFirstPersonPose() const;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> TorchMesh;

	/** First person torch, seen by its carrier only, attached to its first person camera (T-330) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> FirstPersonTorch;

	/** Flame of the first person torch */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> FirstPersonFlame;

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

	/** Fuel left (s) when the torch was last lit or put out (set by the server; sent only then) */
	UPROPERTY(Replicated)
	float FuelAtChange = 300.0f;

	/** Server world time the torch was lit (GetServerWorldTimeSeconds): while lit, the fuel burns down from FuelAtChange */
	UPROPERTY(Replicated)
	double LitServerTime = 0.0;

	ETorchMove LocalMove = ETorchMove::None;
	double LocalMoveStart = 0.0;
	float RaiseAlpha = 0.0f;
	double SpawnTime = 0.0;
	bool bInputBound = false;
	FTimerHandle FlameTimer;
	FTimerHandle HitTimer;
	FTimerHandle IgniteTimer;
	TWeakObjectPtr<UBeanPuppetComponent> Puppet;

	/** Carrier's procedural first person arms (the left one holds the first person torch) */
	TWeakObjectPtr<UBeanFirstPersonArmsComponent> Arms;

	/** First person pose shown (blended toward the target of the frame) */
	FTransform FirstPersonPose = FTransform::Identity;

	/** The first person torch drives the arms / the light rides on its flame */
	bool bArmsDriven = false;
	bool bLightOnFirstPerson = false;

	/** Walk bob of the first person torch: phase and weight */
	float BobPhase = 0.0f;
	float BobWeight = 0.0f;
};
