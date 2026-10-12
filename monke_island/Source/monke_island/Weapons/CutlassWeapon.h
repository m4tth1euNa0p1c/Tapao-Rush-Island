// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/CutlassRules.h"
#include "Weapons/CutlassMath.h"
#include "CutlassWeapon.generated.h"

class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;
class UBeanFirstPersonArmsComponent;
class UBeanPuppetComponent;
class UIslanderMovementComponent;
class UCharacterMovementComponent;
class UPrimitiveComponent;

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
 *  parry (80 % of a frontal hit). Each blow costs 8 % of the stamina, each parried hit 11 %, no blow below that.
 *  T-152: a click during a blow is buffered and chains as soon as the blow can be cancelled (after its active frames),
 *  the recovery is shorter, the bean walks slowly during a blow, steps in on the cuts and turns softly toward the nearest
 *  enemy; the striker feels each hit (hit-stop, camera shake).
 *  The server validates every blow, sweeps the blade along its track during the active frames (plus the arc at the hit
 *  time as a safety net), hits each target once per blow and damages the creatures (PvE);
 *  every machine plays the grip tracks of the second PC: the blade in the bean's hand and the right arm reaching it
 *  in third person; in first person (T-239) the cutlass and the arm meshes follow their own camera space tracks
 *  (guard low at the right, blows sweeping the view, parry across it with the left hand, breath, run bob, recoil of a
 *  blocked hit), as long as the animation library leaves the first person arms to the cutlass.
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

	/** Position in the track of the move playing on this machine (s), 0 without a move. An attack's recovery is played
	 *  faster than its track: this is the track time, so Elapsed / Duration is the phase of the move */
	float GetLocalMoveElapsed() const;

	/** Track length of the move playing on this machine (s; the draw and the sheathe: their played time), 0 without a move */
	float GetLocalMoveDuration() const;

	/** Charge of the held left click toward the finisher (0-1), carrier only */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	float GetChargeProgress() const;

	/** The cutlass can be used now (equipped, carrier standing, hands free, not swimming) */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	bool IsUsable() const;

	/** Recent blows of the local carrier, for the damage numbers of the HUD */
	const TArray<FCutlassHitMarker>& GetHitMarkers() const { return HitMarkers; }

	/** First person: the cutlass draws itself and the arm meshes in front of the camera (DA_CutlassRules); the animation
	 *  library should then keep the body arms hidden for the owner */
	UFUNCTION(BlueprintPure, Category="Cutlass")
	bool UsesFirstPersonViewModel() const;

	/** Server: damage the parry lets through */
	virtual float ModifyIncomingDamage(float Damage, const AActor* DamageCauser) override;

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
	void ServerBeginCharge();

	UFUNCTION(Server, Reliable)
	void ServerAttack(ECutlassMove Move);

	UFUNCTION(Server, Reliable)
	void ServerSetParrying(bool bInParrying);

	/** The server refused a blow the carrier already started */
	UFUNCTION(Client, Reliable)
	void ClientRejectAttack();

	/** Blows that landed, for the carrier's damage numbers and its feel of the hit (hit-stop, camera shake) */
	UFUNCTION(Client, Unreliable)
	void ClientShowHits(const TArray<FVector_NetQuantize>& Locations, const TArray<float>& Damages, const TArray<bool>& Kills,
		const TArray<AActor*>& Struck, bool bAnyKill);

	/** Server: a blade sound heard by every player (hit, kill, parry) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBladeSound(ECutlassSound Sound, FVector_NetQuantize Location);

	/** The parry just stopped a hit: the carrier feels it (blade recoil in first person, camera shake) */
	UFUNCTION(Client, Unreliable)
	void ClientParryBlocked();

	UFUNCTION()
	void OnRep_MoveState();

private:

	/** Plays a cutlass sound on this machine */
	void PlayBladeSound(ECutlassSound Sound, const FVector& Location) const;

	/** Blows of one server frame, sent together to the carrier and the sound */
	struct FHitBatch
	{
		TArray<FVector_NetQuantize> Locations;
		TArray<float> Damages;
		TArray<bool> Kills;
		TArray<AActor*> Struck;
		FVector BodyLocation = FVector::ZeroVector;
		bool bStruckBody = false;
		bool bAnyKill = false;
	};

	/** Server: sweeps the blade of the running blow from the last active frame swept to Now */
	void UpdateServerBlade(double Now);

	/** Server: sweeps the blade along its track from track time From to To, the bean frame going from FrameFrom to FrameTo */
	void SweepBlade(const FCutlassAttack& Attack, float From, float To, const FTransform& FrameFrom, const FTransform& FrameTo, FHitBatch& Batch);

	/** Server: the arc in front of the bean at the hit time, for what the blade sweep missed */
	void CheckArc(const FCutlassAttack& Attack, FHitBatch& Batch);

	/** Server: one blow on one target (each target once per blow) */
	void StrikeTarget(AActor* Target, UPrimitiveComponent* Component, const FVector& ImpactPoint, const FCutlassAttack& Attack, FHitBatch& Batch);

	/** Server: sound for everyone, damage numbers and feel for the carrier */
	void SendHits(FHitBatch& Batch);

	/** Bean mesh space -> world, for the blade keys (no pelvis sway: the server's blade) */
	FTransform GetBladeFrame() const;

	/** Living enemies within Radius of the carrier (not the players, unless friendly fire) */
	void GatherEnemies(float Radius, TArray<AActor*>& OutEnemies) const;

	/** Owner and server: direction of the blow (aim, soft aim toward an enemy) and the lunge */
	void BeginStrike(ECutlassMove Move);

	/** Owner and server: turns the bean (third person) or the camera (first person) toward the blow */
	void UpdateStrikeTurn(float DeltaSeconds);

	/** Owner and server: slower walk from the start of a blow to its last active frame */
	void SetAttackSlowdown(bool bSlow);

	/** Owner and server: the parry is up when asked and the move allows it */
	void UpdateParry(double Now);

	/** The carrier asks to move (input, or the replicated speed for the other players) or leaves the ground */
	bool IsCarrierTryingToMove() const;

	/** Carrier: hit-stop of the blade and the struck enemies, camera shake */
	void PlayHitFeel(const TArray<AActor*>& Struck, bool bKill);

	/** Puts the struck enemies back at normal speed after the hit-stop */
	void ReleaseHitStop();

	/** Carrier: recoil of the blade and light camera shake when the parry stops a hit */
	void PlayBlockFeel();

	/** Windows of the move playing on this machine */
	CutlassMath::FMoveWindows GetLocalWindows() const;

	/** Played time of the move on this machine (s) */
	float GetLocalPlayed() const;

	/** Plays a move on this machine from its start */
	void PlayMove(ECutlassMove Move);

	/** Ends the move on this machine (cancelled, refused or over) */
	void StopLocalMove();

	/** Server: accepts a blow now (the running one can be cancelled, stamina) */
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

	/** First person pose of this frame (camera space): the move's track, else the parry, the charge or the guard */
	FCutlassViewKey ComputeViewPose() const;

	/** First person: advances the breath, the run bob and the recoil of a blocked hit, and returns their offset */
	CutlassMath::FViewSway UpdateViewSway(float DeltaSeconds);

	/** First person: cutlass in front of the camera on its own tracks, the right mitten on its grip, the left one backing the parry */
	void UpdateFirstPerson(bool bShow, float DeltaSeconds);

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

	/** Server: move running and its start, the last blow of the combo and its end (world time) */
	ECutlassMove ServerMove = ECutlassMove::None;
	double ServerMoveStart = 0.0;
	ECutlassMove LastComboMove = ECutlassMove::None;
	double LastComboEnd = -1.0e6;

	/** Server: the next blow, and the parry, may start from these world times */
	double ServerAttackOpen = 0.0;
	double ServerDefensiveOpen = 0.0;

	/** Server: blade sweep of the running blow (last track time swept and its bean frame) */
	float ServerBladeTime = 0.0f;
	FTransform ServerBladeFrame = FTransform::Identity;
	bool bServerBladeStarted = false;
	bool bServerBladeDone = true;
	bool bServerArcDone = true;

	/** Server: targets the running blow already hit */
	TSet<TWeakObjectPtr<AActor>> ServerBlowTargets;

	/** Server: the left click went down at this world time (finisher check), negative when not charging */
	double ServerChargeStart = -1.0;

	/** Carrier: the left click went down at this world time, negative when released */
	double LocalChargeStart = -1.0;

	/** Carrier: a click during a blow (world time, negative when none), played when the blow can be cancelled */
	double BufferedPressTime = -1.0;
	bool bBufferedFinisher = false;

	/** Carrier: the next blow and the parry may start from these world times (this machine's clock) */
	double LocalAttackOpen = 0.0;
	double LocalDefensiveOpen = 0.0;

	/** Owner and server: the right click is held (the parry rises when the move allows it) */
	bool bParryWanted = false;

	/** Owner and server: yaw the running blow strikes toward, and the turn toward it is running */
	float StrikeYaw = 0.0f;
	bool bStrikeTurning = false;

	/** Owner and server: the soft aim found an enemy for the running blow */
	bool bStrikeAssisted = false;

	/** Owner and server: walking speed before the blow slowed it, negative when not slowed, and the movement slowed */
	float SlowedBaseWalkSpeed = -1.0f;
	TWeakObjectPtr<UCharacterMovementComponent> SlowedMovement;

	/** Carrier: the blade stays still until this world time (hit-stop) */
	double LocalHitStopUntil = 0.0;

	/** Carrier: struck enemies slowed by the hit-stop, until this world time */
	TArray<TWeakObjectPtr<AActor>> HitStopActors;
	double HitStopActorsUntil = 0.0;

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

	/** First person pose shown (camera space; blended toward the target outside the moves) */
	FCutlassViewKey ViewPose;

	/** Counts the moves played on this machine (PlayMove), so the first person view blends each new track in from the pose
	 *  it shows (ViewMoveStartPose) */
	uint32 LocalMoveSerial = 0;
	uint32 ViewMoveSerial = 0;
	FCutlassViewKey ViewMoveStartPose;

	/** First person: phase of the breath and of the stride (cycles), weight of the run bob (0-1, lags the speed) */
	float BreathPhase = 0.0f;
	float BobPhase = 0.0f;
	float BobWeight = 0.0f;

	/** First person: world time the parry stopped a hit, negative when no recoil is playing */
	double BlockRecoilStart = -1.0;

	/** First person: the left mitten is on the blade (1) or out of view (0), blended */
	float LeftHandWeight = 0.0f;

	/** The animation library hiding the first person view model was reported once */
	bool bWarnedBodyArms = false;

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
};
