// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmWeapon.h"
#include "MachineGunWeapon.generated.h"

class UAudioComponent;
class UEnhancedInputComponent;
class UIslanderMovementComponent;
class UMachineGunRules;
class UNiagaraComponent;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** What the server knows of the gun, replicated compactly to every machine (T-273) */
USTRUCT()
struct FMachineGunState
{
	GENERATED_BODY()

	/** Rounds left on the belt in the gun */
	UPROPERTY()
	uint16 BeltRounds = 0;

	/** Heat of the barrel, 0-255 for 0-1 (MachineGunMath::PackHeat) */
	UPROPERTY()
	uint8 Heat = 0;

	/** The crank turns: the gun fires now */
	UPROPERTY()
	bool bFiring = false;

	/** Overheated: no fire until the barrel cooled down to OverheatRecoverHeat */
	UPROPERTY()
	bool bOverheated = false;
};

/**
 *  Bean's crank machine gun, a heavy belt-fed gun of 1918 with a water jacket (BP_MachineGun, DA_MachineGunRules; owner
 *  2026-10-10, T-273; model and clips T-272). Fire held turns the crank: FireStart, then the FireLoop clip loops while
 *  the rounds leave one by one at the cadence of the crank (MachineGunMath: MinCadenceRpm up to MaxCadenceRpm in
 *  SpinUpSeconds), FireEnd once the trigger is let go. Every round heats the barrel; at OverheatHeat the fire is cut,
 *  the Overheat clip vents the steam, and the gun waits until the barrel cooled. R swaps the box and feeds a new belt
 *  (Reload, the rounds count at BeltIn). Heavy: walk and run are slower while it is in hand.
 *  Network: the server owns the trigger, the cadence, the belt, the heat and every hit (one AFirearmWeapon::FireProjectiles
 *  per round, the tracers of the base every Tracer.EveryNth); FMachineGunState replicates compactly. Every machine draws
 *  its own rounds at the cadence (flash, flickering light, sound, cases, links): the listen server from its real rounds,
 *  the others from the replicated crank, the carrier's client from its own trigger at once (its crank gestures too).
 */
UCLASS()
class MONKE_ISLAND_API AMachineGunWeapon : public AFirearmWeapon
{
	GENERATED_BODY()

public:

	AMachineGunWeapon();

	/** Heat of the barrel (0 cold, 1 overheated), as this machine knows it */
	UFUNCTION(BlueprintPure, Category="Machine Gun")
	float GetHeat() const;

	/** The gun overheated: no fire until it cooled */
	UFUNCTION(BlueprintPure, Category="Machine Gun")
	bool IsOverheated() const { return GunState.bOverheated; }

	/** The crank turns on the server: the gun fires now */
	UFUNCTION(BlueprintPure, Category="Machine Gun")
	bool IsFiring() const { return GunState.bFiring; }

	/** Lets go of the trigger (owning client or server): the crank stops */
	UFUNCTION(BlueprintCallable, Category="Machine Gun")
	void RequestCeaseFire();

	virtual int32 GetLoadedRounds() const override { return GunState.BeltRounds; }
	virtual int32 GetReserveRounds() const override { return ReserveRounds; }
	virtual int32 GetMaxRounds() const override;
	virtual void SetRounds(int32 Loaded, int32 Reserve) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;

	/** Forgets the bindings of a former carrier: the next carrier binds its own (T-288) */
	virtual void OnInputBindingsRemoved() override;
	virtual void OnAttachedToCarrier() override;
	virtual void ServerHandleFire() override;
	virtual void ServerHandleReload() override;
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void OnActionFinished(EFirearmGesture Gesture) override;
	virtual void OnActionStopped(EFirearmGesture Gesture) override;
	virtual float GetGestureRate(const FFirearmClip& Clip) const override;
	virtual EFirearmGesture DesiredLoop() const override;
	virtual float GetShotSpreadDegrees(bool bMoving) const override;
	virtual void OnCosmeticEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void TickCosmetics(float DeltaSeconds) override;
	virtual bool HasCosmeticsRunning() const override;

	/** The carrier let go of the trigger */
	UFUNCTION(Server, Reliable)
	void ServerCeaseFire();

private:

	/** One case or link thrown out of the gun (local, world space) */
	struct FBrass
	{
		FVector Velocity = FVector::ZeroVector;
		FRotator Spin = FRotator::ZeroRotator;
		float Age = 0.0f;
		bool bActive = false;
		bool bResting = false;
		bool bBounced = false;
	};

	// ---------------------------------------------------------------- server

	/** Server: trigger, crank, rounds, heat and overheat for this frame (DeltaSeconds 0: the state changed now) */
	void ServerTickGun(float DeltaSeconds);

	/** Server: the crank starts turning (FireStart) */
	void BeginBurst(double Now);

	/** Server: the crank stops; bPlayEnd plays FireEnd (not when a reload or an overheat takes over) */
	void EndBurst(bool bPlayEnd);

	/** Server: one round leaves the belt: the hit (FireProjectiles), the heat, the bloom; the belt or the barrel may
	 *  end the burst */
	void FireRound(double Now);

	/** Server: the barrel overheated: fire cut, Overheat clip */
	void Overheat();

	/** Server: R or an empty belt: the box swapped and a new belt fed (false when there is nothing to load or now is
	 *  not the time) */
	bool BeginReload();

	/** Server: the new belt is in the feed */
	void LoadBelt();

	/** Server: the heat of the barrel, packed into the replicated state */
	void SetHeat(float NewHeat);

	// ---------------------------------------------------------------- carrier

	/** Carrier: binds the press and the release of the fire key (the base binds the press to RequestFire) */
	void BindTrigger();

	/** Carrier: fire pressed (the crank shown at once on a client) */
	void HandleTriggerPressed();

	/** Carrier: fire released */
	void HandleTriggerReleased();

	/** Carrier's client: the gun would fire now (the replicated belt, heat and gesture; not while running) */
	bool PredictsFire() const;

	/** This machine shows the gun firing: the carrier's client its own trigger, every other machine the server's crank */
	bool IsFiringHere() const;

	/** The carrier runs (sprint) */
	bool IsCarrierRunning() const;

	/** The carrier moves faster than the steady speed of the rules */
	bool IsCarrierMoving() const;

	/** Every machine: the carrier walks and runs slower while the gun is in hand */
	void ApplyCarrySpeed(bool bInHand);

	// ---------------------------------------------------------------- cosmetics (every machine)

	/** A round seen here: flash, light, sound and muzzle blast of the base, the case and the link, the kick of the view */
	void PlayRoundCosmetics();

	/** Throws a case (or a link) out of the gun seen here */
	void EjectBrass(bool bLink);

	/** Reload: the old ammunition box falls from the gun seen here */
	void DropBox();

	/** A free piece of a pool for Mesh (created up to Max, else the oldest one), placed at Location with Velocity */
	void LaunchBrass(UStaticMesh* Mesh, int32 Max, TArray<TObjectPtr<UStaticMeshComponent>>& Components, TArray<FBrass>& Items,
		const FVector& Location, const FRotator& Rotation, const FVector& Velocity, const FRotator& Spin);

	/** Pieces fall, bounce once (Bounce, a clink at ClinkVolume when it is not 0), rest, then go after LifeSeconds */
	void UpdateBrass(float DeltaSeconds, TArray<TObjectPtr<UStaticMeshComponent>>& Components, TArray<FBrass>& Items, float ClinkVolume,
		float LifeSeconds, float Bounce);

	/** The heat parameter on the glowing material slots of a weapon mesh */
	void SetGlow(USkeletalMeshComponent* Mesh, float Value) const;

	/** Glow of the barrel (material parameter and light), steam out of the cap while hot */
	void UpdateHeatLook(float DeltaSeconds);

	/** Burst loop, steam loop */
	void UpdateAudio(float DeltaSeconds);

	/** Carrier: the recoil reaches the view */
	void UpdateRecoil(float DeltaSeconds);

	/** The belt bone hidden while the belt is empty */
	void UpdateBelt();

	/** The burst starts or ends on this machine (loop and tail sounds) */
	void OnBurstChangedHere(bool bFiring);

	/** Steam vents out of the cap and the jacket (overheat) */
	void VentSteam();

	/** Put away: loops, steam and glow light stop (the falling cases end on their own) */
	void StopCosmetics();

	/** Glow last set on a weapon mesh (0 by default) */
	float& AppliedGlowFor(const USkeletalMeshComponent* Mesh);

	/** Rules of the gun (Rules cast once at begin play) */
	UPROPERTY(Transient)
	TObjectPtr<UMachineGunRules> GunRules;

	/** Belt, heat, crank and overheat as the server has them */
	UPROPERTY(Replicated)
	FMachineGunState GunState;

	/** Rounds left in the boxes (carrier only) */
	UPROPERTY(Replicated)
	int32 ReserveRounds = 0;

	/** Server: fire held */
	bool bTriggerHeld = false;

	/** Server: heat of the barrel (full precision; GunState.Heat is its packed copy) */
	float ServerHeat = 0.0f;

	/** Server: world time the heat was last brought up to date (the barrel cools while the gun is put away too) */
	double HeatTime = 0.0;

	/** Server: crank speed (0 at rest, 1 full speed) and the clock of the rounds */
	float ServerCrank = 0.0f;
	float ServerClock = 0.0f;

	/** Server: world time of the last round */
	double LastRoundTime = -100.0;

	/** Server: spread added by the burst (deg) */
	float Bloom = 0.0f;

	/** Server: the new belt went in during the Reload playing */
	bool bBeltLoadedThisReload = false;

	/** Server: the belt ran dry during the burst: the reload follows FireEnd */
	bool bReloadAfterFireEnd = false;

	/** Carrier: fire held on this machine */
	bool bLocalTriggerHeld = false;

	/** Carrier: input component the release of the trigger is bound on */
	TWeakObjectPtr<UEnhancedInputComponent> TriggerInput;

	/** Remote machines: crank and round clock of the rounds drawn here, world time the burst shown here started */
	float CosmeticCrank = 0.0f;
	float CosmeticClock = 0.0f;
	double LastCosmeticRoundTime = -100.0;
	double CosmeticBurstStart = 0.0;

	/** The burst shown here (loop and tail sounds) */
	bool bBurstShownHere = false;

	/** The overheat shown here (its sound, its vent) */
	bool bOverheatShownHere = false;

	/** Heat drawn here (eased toward the replicated value) */
	float DisplayedHeat = 0.0f;

	/** Glow set on each weapon mesh (first and third person) */
	TWeakObjectPtr<const USkeletalMeshComponent> GlowMeshes[2];
	float GlowValues[2] = { 0.0f, 0.0f };

	/** Carrier: recoil still to bring to the view (pitch, yaw in deg) and the drift of its sideways walk */
	FVector2D PendingRecoil = FVector2D::ZeroVector;
	float RecoilDrift = 0.0f;

	/** World time of the last clink of a case */
	double LastCasingSoundTime = -100.0;

	/** The loaded belt is hidden */
	bool bBeltHidden = false;

	/** Movement whose speed this gun slowed */
	TWeakObjectPtr<UIslanderMovementComponent> SlowedMovement;

	/** Cases and links thrown out (local pools, same order as their states) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CasingComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LinkComponents;

	/** Ammunition boxes let go during the reloads (local) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BoxComponents;

	TArray<FBrass> Casings;
	TArray<FBrass> Links;
	TArray<FBrass> Boxes;

	/** Crank and feed clatter while the crank turns */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MechanismAudio;

	/** Simmering of a hot jacket */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SteamAudio;

	/** Wisps out of the steam cap of a hot gun */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SteamComponent;

	/** Light of the glowing jacket */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> HeatLight;
};
