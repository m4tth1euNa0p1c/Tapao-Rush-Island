// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Character.h"
#include "IslanderCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UIslanderMovementComponent;
class UIslanderHealthComponent;
class UIslanderCombatComponent;
class UIslanderGrabComponent;
struct FInputActionValue;

/** Knockout of an islander as the server decides it (T-287, review T-268 R-22): every machine plays the ragdoll from
 *  the server's start, the server alone stands the body up and every machine stands it up at the same place */
USTRUCT()
struct FIslanderKnockoutRep
{
	GENERATED_BODY()

	/** The server's body is a ragdoll (knocked out, or dead) */
	UPROPERTY()
	bool bActive = false;

	/** Dead: the body never stands up */
	UPROPERTY()
	bool bDead = false;

	/** Bumped at each knockout (0: none yet) */
	UPROPERTY()
	uint8 Serial = 0;

	/** Server world time the knockout began (AGameStateBase::GetServerWorldTimeSeconds) */
	UPROPERTY()
	float StartServerTime = 0.0f;

	/** Capsule location when the knockout began, and the velocity the body was thrown with */
	UPROPERTY()
	FVector_NetQuantize10 StartLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 StartVelocity = FVector::ZeroVector;

	/** Where the server stood the body up the last time, and the yaw it faced */
	UPROPERTY()
	FVector_NetQuantize10 RecoverLocation = FVector::ZeroVector;

	UPROPERTY()
	float RecoverYaw = 0.0f;
};

/**
 *  Third person islander with real-world proportions (1.80 m, 75 kg).
 *  Walks, sprints, jumps, wades and swims on the atoll; a double tap on jump toggles flight, and crashing
 *  while flying turns the body into a ragdoll. Mesh, animation and input assets are set in the Blueprint.
 *  Lethal Company style controls: stamina-limited sprint and jump, crouch while the crouch key is held (left Ctrl,
 *  Counter-Strike style, T-262: crouched, the mobs and villagers see the player only at contact; the key also dives
 *  while swimming, jump rises), punch on left click, guard on right click, health with fall damage, death and respawn.
 */
UCLASS(abstract)
class MONKE_ISLAND_API AIslanderCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom keeping the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** First person camera (R toggles); tagged "FirstPerson" so a skin can move it to its eyes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Health, fall damage and death */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderHealthComponent> HealthComponent;

	/** Punch and guard */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderCombatComponent> CombatComponent;

	/** R.E.P.O. style physics grab */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderGrabComponent> GrabComponent;

public:

	AIslanderCharacter(const FObjectInitializer& ObjectInitializer);

	/** Returns the islander movement component */
	UFUNCTION(BlueprintPure, Category="Movement")
	UIslanderMovementComponent* GetIslanderMovement() const;

	/** Switches between flying and falling */
	UFUNCTION(BlueprintCallable, Category="Movement|Flying")
	void ToggleFlying();

	/** Returns true while flying */
	UFUNCTION(BlueprintPure, Category="Movement|Flying")
	bool IsFlying() const;

	/** Server: turns the body into a ragdoll launched at the given velocity, on every machine; it stands up again once
	 *  it has settled on the server. A client cannot knock itself out (its crash or tumble comes from the server) */
	UFUNCTION(BlueprintCallable, Category="Movement|Ragdoll")
	void StartRagdoll(FVector InitialVelocity);

	/** Returns true while the body is a ragdoll */
	UFUNCTION(BlueprintPure, Category="Movement|Ragdoll")
	bool IsRagdoll() const { return bIsRagdoll; }

	/** Returns the health component */
	UFUNCTION(BlueprintPure, Category="Health")
	UIslanderHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Returns the grab component */
	UFUNCTION(BlueprintPure, Category="Grab")
	UIslanderGrabComponent* GetGrabComponent() const { return GrabComponent; }

	/** Returns the combat component */
	UFUNCTION(BlueprintPure, Category="Combat")
	UIslanderCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/** Switches between the third person and the first person view */
	UFUNCTION(BlueprintCallable, Category="Camera")
	void ToggleView();

	/** True while the first person view is selected */
	UFUNCTION(BlueprintPure, Category="Camera")
	bool IsFirstPerson() const { return bFirstPerson && !bForceThirdPerson; }

	/** Forces the third person view while true (seated in a boat), the player's choice comes back afterwards */
	void SetForceThirdPerson(bool bForce);

	/** Returns the first person camera */
	UFUNCTION(BlueprintPure, Category="Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	/** Knocks the character out into a ragdoll on every machine (server only), e.g. hit by a falling coconut;
	 *  ignored when already knocked out or dead; the held object is released. It stands up again as after a tumble */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Movement|Ragdoll")
	void Knockout(FVector Velocity);

	/** Toggles crouching (not while swimming, flying or knocked out) */
	UFUNCTION(BlueprintCallable, Category="Movement|Crouch")
	void ToggleCrouch();

	virtual void Landed(const FHitResult& Hit) override;

	virtual void PostInitializeComponents() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Mask filter of the bodies animated on every machine (the giant crab's hit bodies, T-305): in a network session
	 *  the islanders' own moves ignore them, since they are not where the server has them; fixed shapes on the crab
	 *  stop them instead. Alone (NM_Standalone) the bodies stop them as before */
	static constexpr uint8 AnimatedHitBodyMaskFilter = 1;

protected:

	/** Input mapping contexts added while a local player controls this character (e.g. sprint) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TArray<TObjectPtr<UInputMappingContext>> CharacterMappingContexts;

	/** Move input action */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Gamepad look input action */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	/** Mouse look input action */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	/** Jump input action */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Sprint input action (hold) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> SprintAction;

	/** Crouch input action (left Ctrl, IA_Crouch): crouches while held on foot (T-262), dives while held in the water */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Attack input action (left click): punch */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> AttackAction;

	/** Guard input action (right click, hold) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> GuardAction;

	/** Interaction input action (F, owner choice over the cahier's E): lifts / lets go the physics object in view,
	 *  or uses the interactable in view (boat, chest...) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> GrabAction;

	/** Drop input action (G, cahier §4): lets go of the held object */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> DropAction;

	/** Hold distance input action (mouse wheel, axis 1D) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> HoldDistanceAction;

	/** Tumble input action (X): falls over on purpose, R.E.P.O. style */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> TumbleAction;

	/** Push given when tumbling on purpose (cm/s, forward) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0"))
	float TumbleForwardSpeed = 250.0f;

	/** View toggle input action (R): third / first person */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> ToggleViewAction;

	/** Start in the first person view (cahier E-01) */
	UPROPERTY(EditDefaultsOnly, Category="Camera")
	bool bStartInFirstPerson = true;

	/** The player may switch between first and third person with its key (R); off since the owner's request of
	 *  2026-10-07 ("enlève l'option R pour changer de vue"): the game stays in first person */
	UPROPERTY(EditDefaultsOnly, Category="Camera")
	bool bAllowViewToggle = false;

	/** Delay between death and respawn (s); 0 disables respawning */
	UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ClampMin="0", Units="s"))
	float RespawnDelay = 6.0f;

	/** Max delay between two jump presses that toggles flight (s) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Flying", meta=(ClampMin="0.05", Units="s"))
	float FlyDoubleTapTime = 0.3f;

	/** Minimum time spent as a ragdoll before standing up (s) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0", Units="s"))
	float RagdollMinDuration = 2.5f;

	/** The ragdoll stands up after this time even if it is still moving, e.g. sinking (s) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0", Units="s"))
	float RagdollMaxDuration = 8.0f;

	/** The ragdoll stands up once its pelvis moves slower than this (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0"))
	float RagdollSettledSpeed = 30.0f;

	/** Fastest a ragdoll body may be pushed out of an overlap (cm/s): falling over on a boat or against a
	 *  creature must not catapult the body (engine default: unlimited) (T-063) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0"))
	float RagdollMaxDepenetrationVelocity = 150.0f;

	/** Buoyancy of a ragdoll in the water (knocked out or dead): above 1 the body floats, a little under the surface */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0"))
	float RagdollBuoyancy = 1.25f;

	/** Water drag on a floating ragdoll (1/s): it settles instead of bobbing */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0"))
	float RagdollWaterDrag = 1.5f;

	/** Depth over which a ragdoll body goes from dry to fully immersed (cm) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="1", Units="cm"))
	float RagdollFloatDepth = 20.0f;

	/** Bone whose position places the capsule when standing up */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll")
	FName RagdollPelvisBone = TEXT("pelvis");

	/** Server: while knocked out, the capsule follows the pelvis of the ragdoll this often (s, 0: it stays where the
	 *  knockout began): the blows, shots and mobs of the server find the body where it lies (T-287) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0", Units="s"))
	float RagdollFollowInterval = 0.1f;

	/** Server: the capsule follows the pelvis only once it is this far from it: a body at rest sends nothing (cm) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0", Units="cm"))
	float RagdollFollowMinStepCm = 5.0f;

	/** Client: a knockout received at most this late starts as far into its flight as the server's (the throw of the
	 *  giant crab lands at the same place); older, for a player who joins or sees the body late, the body drops where
	 *  the capsule is (s) */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll", meta=(ClampMin="0", Units="s"))
	float RagdollCatchUpMaxSeconds = 0.4f;

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Moves relative to the camera; swimming follows the view pitch so looking down dives */
	void MoveInput(const FInputActionValue& Value);

	/** Rotates the camera */
	void LookInput(const FInputActionValue& Value);

	/** Jump pressed: jumps, or toggles flight on a double tap */
	void JumpPressed();

	/** Jump held: climbs while flying */
	void JumpHeld();

	/** Jump released */
	void JumpReleased();

	/** Server: stands up from the ragdoll once it has settled, otherwise checks again later; every machine stands up
	 *  at the server's place (KnockoutRep) */
	void TryRecoverFromRagdoll();

	/** Starts sprinting */
	void SprintStart();

	/** Stops sprinting */
	void SprintEnd();

	/** Crouch pressed: crouches on foot (T-262) */
	void CrouchPressed();

	/** Crouch held: dives while swimming; on foot, crouches as soon as it can (landed, out of the water, up again) */
	void CrouchHeld();

	/** Crouch released: stands up (T-262) */
	void CrouchReleased();

	/** Attack pressed: punch */
	void AttackPressed();

	/** Guard pressed / released */
	void GuardPressed();
	void GuardReleased();

	/** Health reached zero (every machine) */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** Grab pressed / released, drop, wheel */
	void GrabPressed();
	void GrabReleased();
	void DropPressed();
	void HoldDistanceInput(const FInputActionValue& Value);

	/** Tumble pressed: asks the server to fall over */
	void TumblePressed();

	/** Server side of the tumble */
	UFUNCTION(Server, Reliable)
	void ServerTumble();

	/** The knockout state of the server changed: the ragdoll starts here, or stands up where the server's did */
	UFUNCTION()
	void OnRep_Knockout();

	/** Plays the death ragdoll on every machine */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathRagdoll(FVector_NetQuantize10 Velocity);

	/** Server: replaces the dead pawn by a fresh one at a player start */
	void Respawn();

	virtual void BeginPlay() override;

	/** Ticks only while ragdolling: keeps the body afloat in the water */
	virtual void Tick(float DeltaSeconds) override;

	/** Buoyancy and drag on every ragdoll body under the water surface (all machines simulate the ragdoll) */
	void ApplyRagdollBuoyancy();

	/** Applies the view mode: active camera and body rotation (first person: the body follows the view yaw) */
	void ApplyViewMode(bool bInFirstPerson, bool bFirstPersonCamera);

	/** Keeps the server's rotation mode in step with the owner's view */
	UFUNCTION(Server, Reliable)
	void ServerSetFirstPerson(bool bInFirstPerson);

private:

	/** World time of the last jump press, for the double tap */
	double LastJumpPressTime = -1.0e6;

	/** True while the body is a ragdoll (this machine) */
	bool bIsRagdoll = false;

	/** The body is a corpse here: it never stands up */
	bool bDeathRagdoll = false;

	/** Knockout as the server decides it (T-287): the ragdoll and the place it stands up, for every machine and for
	 *  the players who join or see the body late */
	UPROPERTY(ReplicatedUsing=OnRep_Knockout)
	FIslanderKnockoutRep KnockoutRep;

	/** World time the ragdoll started */
	double RagdollStartTime = 0.0;

	/** Capsule collision restored when standing up */
	ECollisionEnabled::Type CapsuleCollision = ECollisionEnabled::QueryAndPhysics;

	/** Mesh placement and collision restored when standing up */
	FTransform MeshRelativeTransform;
	FName MeshCollisionProfile;

	/** Recovery checks while ragdolling (server) */
	FTimerHandle RagdollTimer;

	/** The capsule follows the pelvis while ragdolling (server) */
	FTimerHandle RagdollFollowTimer;

	/** Server: knocks the body out on every machine (KnockoutRep), then stands it up once it has settled */
	void BeginServerKnockout(const FVector& Velocity);

	/** Client: the ragdoll of the server's knockout starts here, as far into its flight as it is old */
	void StartKnockoutFromServer();

	/** This machine: the body becomes a ragdoll launched at InitialVelocity; false when it cannot (no physics asset:
	 *  it falls instead) */
	bool StartLocalRagdoll(const FVector& InitialVelocity);

	/** This machine: the body stands up at Location facing Yaw */
	void StopLocalRagdoll(const FVector& Location, float Yaw);

	/** Server: the capsule moves to the pelvis of the ragdoll (no collision, no sweep) */
	void FollowRagdoll();

	/** Pelvis of the ragdoll in the world, read from its physics body (fresh even when the mesh is not rendered) */
	FVector GetRagdollPelvisLocation() const;

	/** Server world time on this machine (its estimate on a client; the world time without a game state) */
	double GetServerNow() const;

	/** Respawn after death */
	FTimerHandle RespawnTimer;

	/** First person view selected */
	bool bFirstPerson = false;

	/** Third person forced by the situation (seated in a boat) */
	bool bForceThirdPerson = false;

	/** The crouch key is held (local player, T-262) */
	bool bCrouchKeyHeld = false;

	/** Crouches for the held key when it can: on the ground, not knocked out, not held by the flag ceremony */
	void CrouchFromKey();

	/** True while the flag ceremony holds the bean as it is */
	bool IsRitualLocked() const;
};
