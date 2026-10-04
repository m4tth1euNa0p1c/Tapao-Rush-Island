// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/CutlassRules.h"
#include "CutlassWeapon.generated.h"

class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;
class UBeanFirstPersonArmsComponent;
class UBeanPuppetComponent;
class UIslanderMovementComponent;

/** Move played by the server, replicated to the other players */
USTRUCT()
struct FCutlassMoveState
{
	GENERATED_BODY()

	/** Move being played */
	UPROPERTY()
	ECutlassMove Move = ECutlassMove::None;

	/** Incremented at each move, so the same blow twice replays */
	UPROPERTY()
	uint8 Counter = 0;
};

/** A blow that landed, shown to its author (damage number) */
struct FCutlassHitMarker
{
	FVector Location = FVector::ZeroVector;
	float Damage = 0.0f;
	double Time = 0.0;
	bool bKill = false;
};

/**
 *  Bean's boarding cutlass (BP_Cutlass, inventory slot 2), from the second PC's maquettes bean-cutlass,
 *  bean-blade-test and bean-cutlass-anim. Key 2 draws it from the scabbard on the left hip (key 2 again sheathes it).
 *  Left click: combo right cut -> backhand cut -> thrust; left click held 0.4 s: vertical finisher; right click held:
 *  parry (80 % of a frontal hit). Each blow and each parried hit costs 11 % of the stamina, none below that.
 *  The server validates every blow, sweeps the blade's arc at the hit time and damages the creatures (PvE);
 *  every machine plays the grip tracks of the second PC: the blade in the bean's hand and the right arm reaching it
 *  in third person, the cutlass and the first person arm in first person.
 */
UCLASS()
class MONKE_ISLAND_API ACutlassWeapon : public AInventoryItemActor
{
	GENERATED_BODY()

public:

	ACutlassWeapon();

	/** Tunable rules (DA_CutlassRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cutlass")
	TObjectPtr<UCutlassRules> Rules;

	/** Keys of the cutlass (left click, right click), pushed while it can be used */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> CutlassMappingContext;

	/** Above the character context so the clicks no longer punch or guard */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	int32 CutlassMappingPriority = 2;

	/** Attack (left click; held for the finisher) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> AttackAction;

	/** Parry (right click, held) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> ParryAction;

	/** Plays a blow: the next combo blow, or the finisher (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Cutlass")
	void RequestAttack(bool bFinisher);

	/** Raises or lowers the parry (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Cutlass")
	void SetParrying(bool bInParrying);

	/** The parry is up */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	bool IsParrying() const { return bParrying; }

	/** Edge sharpness, 1 = new (valid on the server and the carrier) */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	float GetSharpness() const { return Sharpness; }

	/** Move playing on this machine */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	ECutlassMove GetLocalMove() const { return LocalMove; }

	/** Time since the move playing on this machine started (s), 0 without a move */
	float GetLocalMoveElapsed() const;

	/** Duration of the move playing on this machine as played (s), 0 without a move */
	float GetLocalMoveDuration() const;

	/** Charge of the held left click toward the finisher (0-1), carrier only */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	float GetChargeProgress() const;

	/** The cutlass can be used now (equipped, carrier standing, hands free, not swimming) */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	bool IsUsable() const;

	/** Recent blows of the local carrier, for the damage numbers of the HUD */
	const TArray<FCutlassHitMarker>& GetHitMarkers() const { return HitMarkers; }

	/** Server: damage the parry lets through */
	virtual float ModifyIncomingDamage(float Damage, const AActor* DamageCauser) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;
	virtual void OnAttachedToCarrier() override;

	UFUNCTION(Server, Reliable)
	void ServerBeginCharge();

	UFUNCTION(Server, Reliable)
	void ServerAttack(ECutlassMove Move);

	UFUNCTION(Server, Reliable)
	void ServerSetParrying(bool bInParrying);

	/** The server refused a blow the carrier already started */
	UFUNCTION(Client, Reliable)
	void ClientRejectAttack();

	/** Blows that landed, for the carrier's damage numbers */
	UFUNCTION(Client, Unreliable)
	void ClientShowHits(const TArray<FVector_NetQuantize>& Locations, const TArray<float>& Damages, const TArray<bool>& Kills);

	/** Server: a blade sound heard by every player (hit, kill, parry) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBladeSound(ECutlassSound Sound, FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_MoveState();

private:

	/** Plays a cutlass sound on this machine */
	void PlayBladeSound(ECutlassSound Sound, const FVector& Location) const;

	/** Server: the blade reaches its arc, damages what is there */
	void ApplyHit();

	/** Plays a move on this machine from its start */
	void PlayMove(ECutlassMove Move);

	/** Server: accepts a blow now (no move running, stamina) */
	bool CanAttackNow() const;

	/** Binds the cutlass keys once, for the local carrier */
	void BindInput();

	/** Input handlers */
	void HandleAttackStarted();
	void HandleAttackCompleted();
	void HandleParryStarted();
	void HandleParryCompleted();

	/** Carrier's stamina */
	UIslanderMovementComponent* GetCarrierMovement() const;

	/** Grip and scabbard pose of this frame (bean mesh space) */
	FCutlassKey ComputeTargetPose() const;

	/** Bean mesh space transform -> puppet component space, following the pelvis */
	FTransform FollowPelvis(const FTransform& MeshSpace) const;

	/** First person: cutlass in front of the camera and the right arm on its grip */
	void UpdateFirstPerson(bool bShow);

	/** Third person: cutlass in the hand (or sheathed), scabbard on the hip, right arm reaching the grip */
	void UpdateThirdPerson(bool bShowBlade, bool bDriveArm);

	/** True while the Bean animation library drives the carrier's body (the hand carries the blade) */
	bool IsLibraryAnimated() const;

	/** Third person: bends the bean's right arm so the grip in its mitten lands on Target (puppet component space);
	 *  returns where the grip ended (Target, or the nearest point the arm reaches) */
	FVector ReachArm(const FVector& Target);

	/** Cutlass seen by its carrier in first person, attached to the camera */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> FirstPersonMesh;

	/** Cutlass on the bean, seen by everyone else (sheathed when put away) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> ThirdPersonMesh;

	/** Scabbard on the bean's left hip */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> ScabbardMesh;

	/** Move played by the server (the carrier predicts its own) */
	UPROPERTY(ReplicatedUsing=OnRep_MoveState)
	FCutlassMoveState MoveState;

	/** Parry up (the carrier predicts it) */
	UPROPERTY(Replicated)
	bool bParrying = false;

	/** Edge sharpness (server and carrier) */
	UPROPERTY(Replicated)
	float Sharpness = 1.0f;

	/** Move playing on this machine and its start (local world time) */
	ECutlassMove LocalMove = ECutlassMove::None;
	double LocalMoveStart = 0.0;

	/** Server: move running, its end, the last blow of the combo and its end (world time) */
	ECutlassMove ServerMove = ECutlassMove::None;
	double ServerMoveEnd = 0.0;
	ECutlassMove LastComboMove = ECutlassMove::None;
	double LastComboEnd = -1.0e6;

	/** Server: the left click went down at this world time (finisher check), negative when not charging */
	double ServerChargeStart = -1.0;

	/** Carrier: the left click went down at this world time, negative when released */
	double LocalChargeStart = -1.0;

	/** Carrier: a click arrived during a blow, played when it ends */
	bool bQueuedAttack = false;

	/** Carrier: the held click already gave the finisher */
	bool bChargeSpent = false;

	/** Carrier's last combo blow and its end (prediction of the next one) */
	ECutlassMove LocalLastCombo = ECutlassMove::None;
	double LocalLastComboEnd = -1.0e6;

	/** Damage numbers of the carrier */
	TArray<FCutlassHitMarker> HitMarkers;

	/** Bean puppet of the carrier (third person body) */
	TWeakObjectPtr<UBeanPuppetComponent> Puppet;

	/** First person arms of the carrier */
	TWeakObjectPtr<UBeanFirstPersonArmsComponent> Arms;

	/** Pose shown (blended toward the target outside the moves) */
	FCutlassKey CurrentPose;

	/** Animation library driver of the carrier, if any */
	TWeakObjectPtr<class UBeanAnimDriverComponent> AnimDriver;

	/** Pelvis of the puppet in its reference pose (component space) */
	FTransform PelvisReference = FTransform::Identity;
	bool bHasPelvisReference = false;

	/** The first person arms are driven by the cutlass */
	bool bArmsDriven = false;

	/** The missing arm bones were reported once */
	bool bWarnedMissingBones = false;

	/** Keys are bound */
	bool bInputBound = false;

	/** World time of BeginPlay: an item equipped on arrival skips the draw */
	double SpawnTime = 0.0;

	FTimerHandle HitTimer;
};
