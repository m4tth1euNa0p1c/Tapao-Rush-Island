// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/FirearmRules.h"
#include "FirearmWeapon.generated.h"

class UBeanFirstPersonArmsComponent;
class UBeanPuppetComponent;
class UInputAction;
class UInputMappingContext;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FFirearmDamageEvent;

/** The gesture the server is playing, replicated so every machine plays it from the same time */
USTRUCT()
struct FFirearmAction
{
	GENERATED_BODY()

	/** Gesture (None: the last one was cut, back to the carry loops) */
	UPROPERTY()
	EFirearmGesture Gesture = EFirearmGesture::None;

	/** Incremented at each new gesture (a repeated ReloadInsert replays) */
	UPROPERTY()
	uint8 Serial = 0;

	/** Clip time the gesture started from (s; the shots start at their Fire event) */
	UPROPERTY()
	float ClipStart = 0.0f;

	/** Server world time it started */
	UPROPERTY()
	double ServerStartTime = 0.0;
};

/**
 *  Base of the pirate firearms of the second PC (APistolWeapon, AShotgunWeapon): an inventory item whose gestures are
 *  the synchronized clips of the pack. For each gesture the bean body clip and the weapon clip play together, from the
 *  same time, the weapon root on the bean root (motion baked in the bean's space: no second attachment to the hand):
 *  - first person: the pack's arms (SK_Bean_<Weapon>_FP) and the weapon hang from the camera at the inverse of the
 *    pack's FP camera, owner only; the procedural arms of the bean are hidden meanwhile;
 *  - third person: a hidden bean body plays the clip and drives the puppet's upper body (as the musket), and the
 *    weapon keeps, relative to the puppet's real AnchorBone, the place it has relative to that bone in the clip.
 *  The server owns the ammunition and the gestures that change it (shot, reload, cock, pump) and runs their logic at
 *  the events of the manifest; the carry loops (Idle, Walk, Sprint, AimIdle) and the aim transitions are local.
 *  Empty weapon (owner 2026-10-09): a press on fire with nothing loaded and nothing in the pouch plays Discard (or
 *  DryFire without that clip) and the weapon is thrown to the ground as a pickup, its counters kept; it fires again
 *  once ammunition of its kind (AAmmoPickup, the merchant) is picked up: AddAmmo.
 */
UCLASS(Abstract)
class MONKE_ISLAND_API AFirearmWeapon : public AInventoryItemActor
{
	GENERATED_BODY()

public:

	AFirearmWeapon();

	/** Tunable rules (DA_PistolRules / DA_ShotgunRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firearm")
	TObjectPtr<UFirearmRules> Rules;

	/** Keys of the weapon (left click, right click, R), pushed while it can be used (IMC_Musket fits) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Above the character context so the clicks no longer punch or guard */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	int32 MappingPriority = 2;

	/** Fire (left click) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> FireAction;

	/** Aim (right click, held) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> AimAction;

	/** Reload (R) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> ReloadAction;

	/** Inspect the weapon (optional) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> InspectAction;

	/** Fires, or does what a press on fire does now (cock, reload, dry fire, throw the empty weapon away) (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Firearm")
	void RequestFire();

	/** Reloads (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Firearm")
	void RequestReload();

	/** Plays the Inspect gesture (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Firearm")
	void RequestInspect();

	/**
	 *  The carrier asks to aim (right click held) or lets go (owning client or server). The wish is kept by the carrier
	 *  and by the server alike; both aim while the weapon may be aimed (see LowersAim), so a reload lowers the aim on
	 *  both and both raise it again while the click is still held (network review R-38)
	 */
	UFUNCTION(BlueprintCallable, Category="Firearm")
	void SetAiming(bool bInAiming);

	/**
	 *  A gesture during which the weapon is not aimed, read by the server and by the carrier from the replicated
	 *  gesture: the inspection and the throw of an empty weapon; the pistol and the shotgun add their reload (the musket
	 *  its own through CanAim)
	 */
	virtual bool LowersAim(EFirearmGesture Gesture) const;

	/** The weapon is aimed */
	UFUNCTION(BlueprintPure, Category="Firearm")
	bool IsAiming() const { return bAiming; }

	/** Carrier: the scope fills the view (bHideFirstPersonWhenAimed, the bolt rifle): the first person weapon is hidden, a
	 *  HUD may draw the scope's ring */
	UFUNCTION(BlueprintPure, Category="Firearm")
	bool IsScopeView() const { return bScopeViewShown; }

	/** The weapon can be used now (equipped, carrier standing, hands free, not swimming) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	bool IsUsable() const;

	/** Gesture the server plays now (None: carrying) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	EFirearmGesture GetServerGesture() const;

	/** Progress of the gesture the server plays now (0-1; 0 when carrying) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	float GetServerGestureProgress() const;

	/** Seconds left of the gesture the server plays now (0 when carrying): the reload time of the HUD */
	UFUNCTION(BlueprintPure, Category="Firearm")
	float GetServerGestureSecondsLeft() const;

	/** Rounds ready to fire (in the barrel / the gun) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	virtual int32 GetLoadedRounds() const { return 0; }

	/** Rounds left in the pouch (valid on the server and the carrier) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	virtual int32 GetReserveRounds() const { return 0; }

	/** Rounds the weapon and its pouch hold in all, the loaded ones included (MaxCharges, MaxShells, MaxBalls...) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	virtual int32 GetMaxRounds() const { return 0; }

	/** Server: sets the rounds loaded and in the pouch (a weapon carried over from the day before, ADR-007) */
	virtual void SetRounds(int32 Loaded, int32 Reserve) {}

	/** Ammunition this weapon takes (Rules->AmmoKind; None without rules) */
	UFUNCTION(BlueprintPure, Category="Firearm|Ammo")
	EAmmoKind GetAmmoKind() const;

	/** Rounds of Kind this weapon holds, loaded and in the pouch (0 for another kind) */
	UFUNCTION(BlueprintPure, Category="Firearm|Ammo")
	int32 GetAmmo(EAmmoKind Kind) const;

	/** Rounds of its kind the pouch can still take */
	UFUNCTION(BlueprintPure, Category="Firearm|Ammo")
	int32 GetAmmoRoom() const;

	/** Server: puts up to Count rounds of Kind in the pouch (AAmmoPickup, the merchant); the number taken (0 for
	 *  another kind or a full pouch) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Firearm|Ammo")
	int32 AddAmmo(EAmmoKind Kind, int32 Count);

	/** The weapon is being thrown away (Discard plays, or the DryFire that stands for it) */
	UFUNCTION(BlueprintPure, Category="Firearm")
	bool IsDiscarding() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;

	/** Forgets the bindings of a former carrier: the next carrier binds its own (T-288) */
	virtual void OnInputBindingsRemoved() override;
	virtual void OnAttachedToCarrier() override;

	/** Server: a press on fire (the weapon is usable, not empty with an empty pouch when it is thrown away then) */
	virtual void ServerHandleFire() {}

	/** Server: a press on reload (the weapon is usable) */
	virtual void ServerHandleReload() {}

	/** Server: an event of the manifest is reached in the gesture the server plays */
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) {}

	/** Server: the gesture played to its end */
	virtual void OnActionFinished(EFirearmGesture Gesture) {}

	/** Server: the gesture was cut (another one, put away, swimming...) */
	virtual void OnActionStopped(EFirearmGesture Gesture) {}

	/** Carrier, before the server answers: a press on fire would fire now (the shot is shown at once) */
	virtual bool PredictFire() const { return false; }

	/** Carrier: the shot Shot is shown before the server answers (a double pistol guesses its barrel) */
	virtual void OnShotPredicted(EFirearmGesture Shot) {}

	/** The weapon may be aimed now (the musket not while it reloads) */
	virtual bool CanAim() const { return true; }

	/** Every machine: an event of the gesture played here (sounds, flash, shell); the base plays the shot and the sounds */
	virtual void OnCosmeticEvent(EFirearmGesture Gesture, FName Event);

	/** Every machine: per frame, after the gestures */
	virtual void TickCosmetics(float DeltaSeconds) {}

	/** Every machine: effects still playing (the weapon keeps ticking after it is put away until they end) */
	virtual bool HasCosmeticsRunning() const { return false; }

	/** Weapon socket the flame, light and blast of the shot shown now come out of (a double pistol: its second muzzle) */
	virtual FName GetShotMuzzleSocket() const;

	/** Server: plays Gesture from ClipStart (s) on every machine and schedules its events; false if the pack has no clip */
	bool StartAction(EFirearmGesture Gesture, float ClipStart = 0.0f);

	/** Server: cuts the gesture playing (the carry loops come back) */
	void StopAction();

	/** Server: a gesture is playing (no other action until it ends) */
	bool IsBusy() const;

	/** Server: Count projectiles from the carrier's eyes in the spread cone; the hits of one target add up into one
	 *  ApplyPointDamage (this weapon as DamageCauser) */
	void FireProjectiles(int32 Count);

	/** Server: what this weapon adds to a shot on a target at Distance (cm) before it is dealt (T-167: the shotgun
	 *  shreds at close range) */
	virtual void DescribeShot(FFirearmDamageEvent& Shot, float Distance) {}

	/** Clip time the Fire / AimFire clip starts from (default: its shot event, so the shot is immediate) */
	virtual float FireClipStart(EFirearmGesture Gesture) const;

	/** Speed a gesture plays at, logic and animations together (default: the clip's PlayRate; the musket stretches
	 *  its reload over ReloadSeconds) */
	virtual float GetGestureRate(const FFirearmClip& Clip) const { return FMath::Max(Clip.PlayRate, 0.1f); }

	/** Carry loop for the carrier's state (Idle, Walk, Sprint, AimIdle; the machine gun turns its crank while it fires) */
	virtual EFirearmGesture DesiredLoop() const;

	/** Server: spread half angle of a shot (deg): the stance (aimed or steady, else on the move); the machine gun adds
	 *  the bloom of its burst */
	virtual float GetShotSpreadDegrees(bool bMoving) const;

	/** Carrier, before the server answers: plays Gesture here at once; the server's own start of it is not replayed */
	void PredictGesture(EFirearmGesture Gesture, float ClipStart);

	/** Carrier: the server refused the gesture predicted here (a bayonet thrust without stamina): back to the carry loops */
	void CancelPredictedGesture(EFirearmGesture Gesture);

	/** Gesture played on this machine now (a gesture predicted by the carrier included; None between two) */
	EFirearmGesture GetLocalGesture() const { return PlayingGesture; }

	/** The weapon mesh seen on this machine (first person for its carrier in that view, else third person) */
	USkeletalMeshComponent* GetVisibleWeaponMesh() const;

	/** Shows or hides the weapon meshes of both views without touching the view logic (a grenade thrown: the hand is
	 *  empty until the next one is taken) */
	void SetWeaponMeshVisible(bool bVisible);

	/** Hides (or shows) bones of the weapon on both views: a part of the weapon gone from the hand while the rest stays */
	void SetWeaponBonesHidden(const TArray<FName>& Bones, bool bHideThem);

	UFUNCTION(Server, Reliable)
	void ServerFire();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerInspect();

	/** The carrier's wish to aim (right click held or let go), not the aim itself: the server keeps it and applies the
	 *  same rule as the carrier (R-38); a new wish to aim cuts the inspection */
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bInAiming);

	UFUNCTION()
	void OnRep_Action();

	/** Where the projectiles of a shot struck the world (not a character: those bleed), with the hit normals, on
	 *  every machine: a puff of dust at each (T-167; cosmetic, a lost packet loses the puffs) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotImpacts(const TArray<FVector_NetQuantize>& Locations, const TArray<FVector_NetQuantizeNormal>& Normals);

	/** Where the tracer rounds of a shot end (a hit, or the end of the range), on every machine: each draws its rounds
	 *  from its own visible muzzle (owner 2026-10-10; cosmetic, a lost packet loses the tracers) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotTracers(const TArray<FVector_NetQuantize>& Ends);

	/** Rounds fired by this weapon (server): which ones carry a tracer (Tracer.EveryNth) */
	int32 RoundsFired = 0;

	/** Aim direction of the carrier (control rotation for the server and the carrier, replicated aim elsewhere) */
	FVector GetCarrierAim() const;

	/** Shot direction randomness (server) */
	FRandomStream SpreadStream;

private:

	/** Server: the empty weapon is thrown away: Discard, or DryFire then the throw at its end (no clip: at once) */
	void StartDiscard();

	/** Server: the weapon leaves the hand as a pickup tossed to the right and down (the inventory drops the slot) */
	void ReleaseWeapon();

	/** Server and carrier: aimed when the carrier wants it and the weapon may be aimed now (FirearmMath::IsAimed) */
	void RefreshAim();

	/** Server: an event timer of the gesture Serial fired */
	void HandleServerEvent(uint8 InSerial, FName Event);

	/** Server: the gesture Serial reached its end */
	void HandleServerFinished(uint8 InSerial);

	/** Plays the replicated gesture here (from where it is now: late joiners, lag) */
	void ApplyServerAction();

	/** Plays Gesture on the meshes of this machine from ClipStart */
	void PlayGesture(EFirearmGesture Gesture, float ClipStart, bool bFromServer);

	/** Starts the four clips of a gesture at Time on the meshes that exist here */
	void ApplyClipToMeshes(const FFirearmClip& Clip, float Time);

	/** Clip time of the gesture playing here */
	float GetPlayingClipTime() const;

	/** Local gesture flow: events crossed, end of the one-shots, aim transitions, loops */
	void UpdateGestures();

	/** First person: shows the pack's arms and weapon on the camera, field of view */
	void UpdateFirstPerson(float DeltaSeconds, bool bShow);

	/** Third person: drives the puppet's upper body and places the weapon on its anchor bone */
	void UpdateThirdPerson(float DeltaSeconds, bool bShow);

	/** Muzzle flame, light, shot sound and view kick */
	void PlayShotEffects();

	/** Binds the keys once, for the local carrier */
	void BindInput();

	/** Input handlers */
	void HandleFireInput();
	void HandleAimStarted();
	void HandleAimCompleted();
	void HandleReloadInput();
	void HandleInspectInput();

	/** Puts the first person field of view back */
	void RestoreFieldOfView();

	/** Carrier: hides the first person weapon and arms while a scope fills the view, shows them back after */
	void SetScopeView(bool bInScopeView);

	/** Reads, once, the weapon pose of the Idle and AimIdle clips (first frame) to frame them on the view */
	void MeasureFirstPersonPoses();

	/** Weapon pose of the first frame of Gesture's FP clip: from the weapon's rest space to the rig; false if unknown */
	bool MeasureWeaponPose(EFirearmGesture Gesture, FTransform& OutPose);

	/** Rig placement under the camera for the hip (0) to aimed (1) framing */
	FTransform ComputeRigTransform(float InAimAlpha) const;

	/** Pack camera (FP scenes) under the player's camera: the arms and the weapon hang from it */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> FirstPersonRig;

	/** First person arms of the pack (owner only) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	/** Weapon seen by its carrier in first person (owner only) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonWeapon;

	/** Weapon in the bean's hands, seen by everyone else (and by the carrier in third person) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> ThirdPersonWeapon;

	/** Hidden bean body that plays the third person clips, copied onto the puppet's upper body */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> BodyAnimator;

	/** Muzzle flame (local, created at the first shot) */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlashComponent;

	/** Muzzle light (local, created at the first shot) */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;

	/** Gesture the server plays */
	UPROPERTY(ReplicatedUsing=OnRep_Action)
	FFirearmAction Action;

	/** Aimed (tighter spread, aim clips): computed by the server and the carrier, replicated to the others */
	UPROPERTY(Replicated)
	bool bAiming = false;

	/** The carrier wants to aim (right click held): set on its machine, mirrored on the server by ServerSetAiming */
	bool bAimWanted = false;

	/** Server: world time the gesture ends */
	double ActionEndTime = 0.0;

	/** Server: timers of the gesture's events and end */
	TArray<FTimerHandle> ActionTimers;

	/** Server: DryFire plays in place of a missing Discard clip; the weapon is thrown at its end */
	bool bDiscardPending = false;

	/** Gesture played on this machine */
	EFirearmGesture PlayingGesture = EFirearmGesture::None;

	/** Its clip start, start time (local world time), rate, length and loop */
	float PlayingClipStart = 0.0f;
	double PlayingStartTime = 0.0;
	float PlayingRate = 1.0f;
	float PlayingDuration = 0.0f;
	bool bPlayingLoop = false;

	/** The gesture played here came from the server (a cut gives the loops back) */
	bool bPlayingFromServer = false;

	/** Clip time of the last cosmetic events played */
	float LastCosmeticTime = 0.0f;

	/** Serial of the server gesture played here */
	uint8 PlayedSerial = 0;

	/** Carrier: shot shown before the server answered (gesture, local time) */
	EFirearmGesture PredictedGesture = EFirearmGesture::None;
	double PredictedTime = -100.0;

	/** Aim state at the last frame (aim in / out transitions) */
	bool bLastAiming = false;

	/** Carrier's horizontal speed smoothed over LoopSpeedSmoothingSeconds: chooses the carry loop (T-330) */
	float SmoothedCarrierSpeed = 0.0f;

	/** Carry loop wanted in place of the one playing, and since when (local world time): it plays once it was wanted
	 *  for LoopConfirmSeconds (T-330) */
	EFirearmGesture PendingLoop = EFirearmGesture::None;
	double PendingLoopSince = 0.0;

	/** Bean puppet of the carrier (third person body) */
	TWeakObjectPtr<UBeanPuppetComponent> Puppet;

	/** Procedural first person arms of the carrier (hidden while the pack's arms show) */
	TWeakObjectPtr<UBeanFirstPersonArmsComponent> Arms;

	/** Weight of the gestures on the puppet's upper body */
	float UpperBodyAlpha = 0.0f;

	/** The puppet override was given back */
	bool bPuppetReleased = true;

	/** Field of view of the first person camera before the weapon changed it (0 = not saved) */
	float DefaultFieldOfView = 0.0f;

	/** World time the flame goes out */
	double FlashEndTime = 0.0;

	/** View kick still to apply to the carrier's control rotation (pitch, yaw in deg) */
	FVector2D PendingViewKick = FVector2D::ZeroVector;

	/** Keys are bound */
	bool bInputBound = false;

	/** Weapon pose of the Idle / AimIdle FP clips (rest space to rig) and the weapon points read in its rest pose */
	FTransform HipWeaponPose;
	FTransform AimWeaponPose;
	FVector GripInWeapon = FVector::ZeroVector;
	FVector MuzzleInWeapon = FVector::ZeroVector;

	/** Point of the weapon put at AimMuzzleLocation when aimed: the AimSightSocket (a scope's eyepiece), else the muzzle */
	FVector AimPointInWeapon = FVector::ZeroVector;

	/** The scope fills the view: the first person weapon and arms are hidden (bHideFirstPersonWhenAimed) */
	bool bScopeViewShown = false;

	/** The poses above were read (bHipPoseValid / bAimPoseValid: found) */
	bool bPosesMeasured = false;
	bool bHipPoseValid = false;
	bool bAimPoseValid = false;

	/** First person framing: 0 hip, 1 aimed */
	float AimAlpha = 0.0f;
};
