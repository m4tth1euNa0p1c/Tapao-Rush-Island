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

/**
 *  Third person islander with real-world proportions (1.80 m, 75 kg).
 *  Walks, sprints, jumps, wades and swims on the atoll; a double tap on jump toggles flight, and crashing
 *  while flying turns the body into a ragdoll. Mesh, animation and input assets are set in the Blueprint.
 *  Lethal Company style controls: stamina-limited sprint and jump, crouch toggle (also dives while swimming,
 *  jump rises), punch on left click, guard on right click, health with fall damage, death and respawn.
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

	/** Turns the body into a ragdoll launched at the given velocity; it stands up again once it has settled */
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
	bool IsFirstPerson() const { return bFirstPerson; }

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

	/** Crouch input action: toggles crouching, held while swimming it dives */
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
	bool bStartInFirstPerson = false;

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

	/** Bone whose position places the capsule when standing up */
	UPROPERTY(EditDefaultsOnly, Category="Movement|Ragdoll")
	FName RagdollPelvisBone = TEXT("pelvis");

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

	/** Stands up from the ragdoll once it has settled, otherwise checks again later */
	void TryRecoverFromRagdoll();

	/** Starts sprinting */
	void SprintStart();

	/** Stops sprinting */
	void SprintEnd();

	/** Crouch pressed: toggles crouching on foot */
	void CrouchPressed();

	/** Crouch held: dives while swimming */
	void CrouchHeld();

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

	/** Plays the tumble ragdoll on every machine */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastTumble(FVector_NetQuantize10 Velocity);

	/** Plays the death ragdoll on every machine */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathRagdoll(FVector_NetQuantize10 Velocity);

	/** Server: replaces the dead pawn by a fresh one at a player start */
	void Respawn();

	virtual void BeginPlay() override;

	/** Applies the view mode: active camera and body rotation (first person: the body follows the view yaw) */
	void ApplyViewMode(bool bInFirstPerson, bool bFirstPersonCamera);

	/** Keeps the server's rotation mode in step with the owner's view */
	UFUNCTION(Server, Reliable)
	void ServerSetFirstPerson(bool bInFirstPerson);

private:

	/** World time of the last jump press, for the double tap */
	double LastJumpPressTime = -1.0e6;

	/** True while the body is a ragdoll */
	bool bIsRagdoll = false;

	/** World time the ragdoll started */
	double RagdollStartTime = 0.0;

	/** Capsule collision restored when standing up */
	ECollisionEnabled::Type CapsuleCollision = ECollisionEnabled::QueryAndPhysics;

	/** Mesh placement and collision restored when standing up */
	FTransform MeshRelativeTransform;
	FName MeshCollisionProfile;

	/** Recovery checks while ragdolling */
	FTimerHandle RagdollTimer;

	/** Respawn after death */
	FTimerHandle RespawnTimer;

	/** First person view selected */
	bool bFirstPerson = false;
};
