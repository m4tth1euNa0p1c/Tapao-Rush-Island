// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/MusketRules.h"
#include "MusketWeapon.generated.h"

class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UBeanFirstPersonArmsComponent;
class UBeanPuppetComponent;
struct FInputActionValue;

/**
 *  Bean's flintlock musket (BP_Musket, inventory slot 1). One ball in the barrel, the rest in the pouch
 *  (DA_MusketRules: 23 per player); a shot empties the barrel and a full reload (about 6 s) puts the next ball in.
 *  The server owns the ammunition, validates the shot and traces it from the carrier's camera; clients play
 *  the poses and animations of the second PC: first person musket keys with the mittens on its sockets, the bean body
 *  (A_Bean_Musket_Reload / Fire, copied onto the puppet over the walk pose) and the lock bones in third person.
 *  Left click fires, right click shoulders, R reloads; the context is pushed only while the musket can be used,
 *  so carrying an object, swimming or lying knocked out gives the keys back to the bare hands.
 */
UCLASS()
class MONKE_ISLAND_API AMusketWeapon : public AInventoryItemActor
{
	GENERATED_BODY()

public:

	AMusketWeapon();

	/** Tunable rules (DA_MusketRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UMusketRules> Rules;

	/** Keys of the musket (left click, right click, R), pushed while it can be used */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MusketMappingContext;

	/** Above the character context so the clicks no longer punch or guard */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	int32 MusketMappingPriority = 2;

	/** Fire (left click) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> FireAction;

	/** Shoulder the musket (right click, held) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> AimAction;

	/** Reload (R) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> ReloadAction;

	/** Fires if loaded, else starts the reload (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Musket")
	void RequestFire();

	/** Starts the reload (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Musket")
	void RequestReload();

	/** Shoulders or lowers the musket (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Musket")
	void SetAiming(bool bInAiming);

	/** A ball is in the barrel */
	UFUNCTION(BlueprintPure, Category="Musket")
	bool IsLoaded() const { return bLoaded; }

	/** Balls left in the pouch (valid on the server and the carrier) */
	UFUNCTION(BlueprintPure, Category="Musket")
	int32 GetPouchBalls() const { return PouchBalls; }

	/** A reload is running */
	UFUNCTION(BlueprintPure, Category="Musket")
	bool IsReloading() const { return bReloading; }

	/** Progress of the running reload (0-1) */
	UFUNCTION(BlueprintPure, Category="Musket")
	float GetReloadProgress() const;

	/** The musket is shouldered */
	UFUNCTION(BlueprintPure, Category="Musket")
	bool IsAiming() const { return bAiming; }

	/** The musket can be used now (equipped, carrier standing, hands free, not swimming) */
	UFUNCTION(BlueprintPure, Category="Musket")
	bool IsUsable() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;
	virtual void OnAttachedToCarrier() override;

	UFUNCTION(Server, Reliable)
	void ServerFire();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bInAiming);

	UFUNCTION()
	void OnRep_FireCount();

	UFUNCTION()
	void OnRep_Reloading();

private:

	/** Server: the shot leaves after the lock time */
	void TraceShot();

	/** Server: the reload ends, a ball goes into the barrel */
	void CompleteReload();

	/** Server: stops a running reload (the ball stays in the pouch) */
	void CancelReload();

	/** Server: the recoil is over */
	void EndRecover();

	/** Plays the shot on this machine: recoil, sound, lock and body animations */
	void PlayFireEffects();

	/** Spawns the muzzle flame, the flash light and the powder smoke at the muzzle of the musket seen here */
	void SpawnShotEffects();

	/** Moves and fades the smoke puffs, puts out the flame */
	void UpdateShotEffects(float DeltaSeconds);

	/** Carrier's view jumps at the shot (local carrier) */
	void UpdateViewKick(float DeltaSeconds);

	/** Takes the shake roll off the carrier's view */
	void ResetViewRoll();

	/** The flame, the smoke or the view kick of the last shot are still playing */
	bool HasShotEffectsRunning() const;

	/** Third person: loops the carry animation on the hidden body when no action plays */
	void PlayBodyHold();

	/** Plays (or stops) the reload animations on this machine */
	void PlayReloadEffects(bool bStart);

	/** Binds the musket keys once, for the local carrier */
	void BindInput();

	/** Input handlers */
	void HandleFire();
	void HandleAimStarted();
	void HandleAimCompleted();
	void HandleReload();

	/** First person: blends the arms and the musket toward the current key */
	void UpdateFirstPerson(float DeltaSeconds, bool bShow);

	/** Third person: shows the musket in the hand and drives the bean's upper body with the musket animations */
	void UpdateThirdPerson(float DeltaSeconds, bool bShow);

	/** Point a first person mitten holds in the frame of the musket (cm): a socket plus offset, or a camera point */
	FVector HandHoldInMusket(FName Socket, const FVector& Offset, const FTransform& MusketToCamera) const;

	/** Plays the reload sounds the reload went past since the last frame */
	void UpdateReloadSounds();

	/** Third person: puts the musket at Grip (cm, relative to an unscaled HandBone) whatever the bone scale */
	void SetThirdPersonGrip(const FTransform& Grip);

	/** Third person: places the musket relative to the hand along the body animation that is playing */
	void UpdateThirdPersonGrip();

	/** Plays an animation on the musket meshes and the hidden body from Start (s), stretched to Duration (s) */
	void PlayAnimations(UAnimSequence* BodyAnimation, UAnimSequence* MusketAnimation, float Start, float Duration);

	/** Musket seen by its carrier in first person, attached to the camera */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** Musket in the bean's hand, seen by everyone else */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

	/** Hidden bean body that plays the reload and shot animations, copied onto the puppet */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> BodyAnimator;

	/** Muzzle flame (local, created at the first shot) */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlashComponent;

	/** Muzzle flash light (local, created at the first shot) */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;

	/** Powder smoke puffs (local pool, world space) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SmokeComponents;

	/** Fading material of each puff */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SmokeMaterials;

	/** Ball in the barrel */
	UPROPERTY(Replicated)
	bool bLoaded = false;

	/** Balls left in the pouch (carrier only) */
	UPROPERTY(Replicated)
	int32 PouchBalls = 0;

	/** A reload is running */
	UPROPERTY(ReplicatedUsing=OnRep_Reloading)
	bool bReloading = false;

	/** Server world time the reload started */
	UPROPERTY(Replicated)
	double ReloadStartTime = 0.0;

	/** Incremented by the server at each shot, replays it on the other machines */
	UPROPERTY(ReplicatedUsing=OnRep_FireCount)
	uint8 FireCount = 0;

	/** Musket shouldered (tighter spread) */
	UPROPERTY(Replicated)
	bool bAiming = false;

	/** Recoil after a shot, no action meanwhile (server) */
	bool bRecovering = false;

	/** Bean puppet of the carrier (third person body) */
	TWeakObjectPtr<UBeanPuppetComponent> Puppet;

	/** First person arms of the carrier */
	TWeakObjectPtr<UBeanFirstPersonArmsComponent> Arms;

	/** Current first person pose (blended) */
	FMusketFirstPersonKey CurrentPose;

	/** Next reload sound cue to play (index in ReloadSoundCues) */
	int32 NextReloadCue = 0;

	/** First person mittens in the frame of the musket (cm), blended toward their holds */
	FVector RightHandInMusket = FVector::ZeroVector;
	FVector LeftHandInMusket = FVector::ZeroVector;

	/** Recoil kick (1 at the shot, decays) */
	float Recoil = 0.0f;

	/** The body animation is driving the puppet until this world time */
	double BodyAnimationEndTime = 0.0;

	/** Field of view of the first person camera before aiming (0 = not saved yet) */
	float DefaultFieldOfView = 0.0f;

	/** One smoke puff in flight */
	struct FSmokePuff
	{
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 0.0f;
		float StartSize = 0.0f;
		float EndSize = 0.0f;
		bool bActive = false;
	};

	/** Puffs, same order as SmokeComponents */
	TArray<FSmokePuff> SmokePuffs;

	/** World time the flame goes out */
	double FlashEndTime = 0.0;

	/** View kick still to apply to the carrier's control rotation (pitch, yaw in deg) */
	FVector2D PendingViewKick = FVector2D::ZeroVector;

	/** Roll currently added to the carrier's control rotation by the shot shake (deg) */
	float AppliedViewRoll = 0.0f;

	/** Weight of the musket carry pose on the bean's upper body (third person) */
	float UpperBodyAlpha = 0.0f;

	/** The hidden body is looping the carry animation */
	bool bBodyHolding = false;

	/** Keys are bound */
	bool bInputBound = false;

	/** Shot direction randomness (server) */
	FRandomStream SpreadStream;

	FTimerHandle ShotTimer;
	FTimerHandle ReloadTimer;
	FTimerHandle RecoverTimer;
	FTimerHandle ShotEffectsTimer;
};
