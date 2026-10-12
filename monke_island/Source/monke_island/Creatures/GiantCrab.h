// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/GiantCrabBoss.h"
#include "Creatures/GiantCrabLag.h"
#include "Creatures/GiantCrabTypes.h"
#include "GameFramework/Character.h"
#include "GiantCrab.generated.h"

class AArenaZone;
class ATikiDirector;
class UAnimSequenceBase;
class UGiantCrabBodyComponent;
class UGiantCrabFeedbackComponent;
class UGiantCrabRules;
class UIslanderHealthComponent;
class UMobHealthBarComponent;
class UStealthSubsystem;
struct FGiantCrabAttackRules;
struct FGiantCrabPhaseRules;
struct FGiantCrabWeakPoint;
struct FHitResult;

/** The state every machine plays the clip of (a new Serial restarts it) */
USTRUCT()
struct FGiantCrabActionRep
{
	GENERATED_BODY()

	UPROPERTY()
	EGiantCrabState State = EGiantCrabState::Idle;

	UPROPERTY()
	EGiantCrabAttack Attack = EGiantCrabAttack::None;

	UPROPERTY()
	uint8 Serial = 0;

	/** On its back (turned over): it dies on its back */
	UPROPERTY()
	bool bOnBack = false;

	/** Play rate of the clip of the state (the phase quickens its blows, T-261) */
	UPROPERTY()
	float PlayRate = 1.0f;

	/** Server world time the state began (T-305): a machine that receives it late plays its clip and its moments as
	 *  far in as it is old, a player who joins or sees the crab again does not replay it */
	UPROPERTY()
	float StartServerTime = 0.0f;
};

/**
 *  Giant crab boss "Tupa Nui, Gardien du récif" (T-211, kit crabe-geant of the second PC; boss v2 T-261), server
 *  authoritative: it hunts the nearest living thing of its arena it senses (player, villager, tiki warrior, soldier;
 *  another giant crab is spared; UStealthSubsystem sight and hearing, T-262), closes in sideways with its claws to the
 *  front, strikes (claw swipe, claw slam with a shock wave, grab then throw; from phase 2 stomps of its flanks and
 *  charges across the arena, combos), raises its claws against blows from the front, turns over after hits on its weak
 *  points and lies on its back, belly offered, before it gets up. Whoever stays under its shell is crushed by a belly
 *  slam. Phases by health (roar, quicker, tougher, frenzy with red eyes). It never leaves the AArenaZone it starts in
 *  (nor its leash without one).
 *  No AnimBP: UTikiAnimInstance crossfades the kit's clips; no NavMesh: it walks straight on the sand. Hit bodies on its
 *  bones and upright pillars that stop the characters (UGiantCrabBodyComponent), whose moves ignore its animated bodies;
 *  the server frees one pinned in its shapes (T-328, GiantCrabPawns.cpp). Steps and cues on every machine that plays
 *  sound (UGiantCrabFeedbackComponent), its bar at the bottom of the screen of the players it fights (IBossBarSource,
 *  UI/BossHealthBar).
 */
UCLASS()
class MONKE_ISLAND_API AGiantCrab : public ACharacter, public IBossBarSource
{
	GENERATED_BODY()

public:

	AGiantCrab();

	/** Rules of the crab (DA_GiantCrabRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Giant Crab")
	TObjectPtr<UGiantCrabRules> Rules;

	/** Returns the health component */
	UFUNCTION(BlueprintPure, Category="Giant Crab")
	UIslanderHealthComponent* GetHealth() const { return Health; }

	/** True once dead */
	UFUNCTION(BlueprintPure, Category="Giant Crab")
	bool IsDead() const;

	/** What it is doing now (replicated) */
	UFUNCTION(BlueprintPure, Category="Giant Crab")
	EGiantCrabState GetState() const { return Action.State; }

	/** What it hunts (server) */
	UFUNCTION(BlueprintPure, Category="Giant Crab")
	AActor* GetTarget() const { return Target.Get(); }

	/** Phase it is in, 0 for the first (replicated) */
	UFUNCTION(BlueprintPure, Category="Giant Crab")
	int32 GetPhase() const { return Phase; }

	/** True while it fights: a prey, or hurt a moment ago (replicated) */
	UFUNCTION(BlueprintPure, Category="Giant Crab")
	bool IsEngaged() const { return bEngaged; }

	/** Rules of the phase it is in, nullptr without phases */
	const FGiantCrabPhaseRules* GetPhaseRules() const;

	/** Moments of an action (sorted by time): slam, stomps, belly slam, roar and its wave, charge, death. The server
	 *  hurts at these times, every machine plays their cues */
	void GetActionMoments(const FGiantCrabActionRep& InAction, TArray<FGiantCrabMoment>& OutMoments) const;

	/** Where a moment happens (in front of the claws, at a flank, under the shell, at the mouth) */
	FVector GetMomentLocation(const FGiantCrabMoment& Moment) const;

	/** Point of the ground under its centre */
	FVector GetGroundPoint() const;

	/** True when Actor is a living thing the crab hunts: a character with health, alive, not a giant crab */
	static bool IsPrey(const AActor* Actor);

	/** Server: it rises out of the sand (UGiantCrabEmergenceSubsystem): no collision on any machine meanwhile (R-31) */
	void SetRising(bool bInRising);

	/** True while it rises out of the sand (replicated) */
	bool IsRising() const { return bRising; }

	/** The one hanging from its claw (replicated), nullptr when none */
	ACharacter* GetHeldPrey() const { return HeldPrey; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: the part struck, raised claws, weak points, the phase change the blow before the health takes it */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** The pushes of the blows it takes are scaled by KnockbackTakenScale (0: an 850 kg boss does not budge) */
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;

	/** The boss bar of the players inside its arena (or near it) while it fights, and a little after its death */
	virtual bool GetBossBarView(const APawn* Viewer, FBossBarView& OutView) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Prey gathered for one decision (a few: no allocation) */
	using FPreyList = TArray<ACharacter*, TInlineAllocator<16>>;

	/** Health, damage and death */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderHealthComponent> Health;

	/** Health bar above the shell once it is hurt (local players), when the rules show no boss bar */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMobHealthBarComponent> HealthBar;

	/** Hit bodies on its bones */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UGiantCrabBodyComponent> Bodies;

	/** Steps, cues and looks on the machines that play sound */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UGiantCrabFeedbackComponent> Feedback;

	/** The state and blow every machine plays */
	UPROPERTY(ReplicatedUsing=OnRep_Action)
	FGiantCrabActionRep Action;

	UFUNCTION()
	void OnRep_Action();

	/** Phase it is in */
	UPROPERTY(ReplicatedUsing=OnRep_Phase)
	uint8 Phase = 0;

	UFUNCTION()
	void OnRep_Phase();

	/** It fights (the boss bar) */
	UPROPERTY(Replicated)
	bool bEngaged = false;

	/** The one caught by a grab, hanging from its claw until the throw (T-304, R-23): the server holds it there, and
	 *  the machine of a player held holds its own pawn there too, its movement off, instead of waiting for corrections */
	UPROPERTY(ReplicatedUsing=OnRep_HeldPrey)
	TObjectPtr<ACharacter> HeldPrey;

	UFUNCTION()
	void OnRep_HeldPrey();

	/** It rises out of the sand: no collision on any machine (the server alone switched it off before, R-31) */
	UPROPERTY(ReplicatedUsing=OnRep_Rising)
	bool bRising = false;

	UFUNCTION()
	void OnRep_Rising();

	/** Death on every machine */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** Server: one step of the decisions */
	void ServerThink(float DeltaSeconds);

	/** Server: the nearest prey it senses inside the arena (or the leash), the current one kept unless another is much
	 *  closer, or kept a while once it is no longer sensed; the maker of a noise it hears when it senses nobody */
	void ChooseTarget();

	/** Server: it sees or feels Prey (Gap: between the bodies) */
	bool SensesPrey(const ACharacter* Prey, float Gap, const UStealthSubsystem* Stealth, ATikiDirector* Bushes) const;

	/** Server: the living prey of the registry (every side but the crabs) */
	void GatherPrey(FPreyList& OutPrey) const;

	/** Server: faces the target and walks to it, sideways; strikes when it is in reach and in front, charges from afar */
	void Approach(float DeltaSeconds);

	/** Server: the blow under way (lunge, hit window, shock wave, grab, combo) */
	void UpdateAttack(float DeltaSeconds);

	/** Server: the throw after a catch */
	void UpdateThrow();

	/** Server: the roar of a new phase */
	void UpdateRoar();

	/** Server: the wind-up of a charge, then the charge */
	void UpdateChargeWindup(float DeltaSeconds);

	void UpdateCharge(float DeltaSeconds);

	/** Server: the belly slam until the crab stands again */
	void UpdateBellySlam();

	/** Server: how long each prey has stood under its shell; true when it starts a belly slam */
	bool CheckUnderShell(float DeltaSeconds);

	/** Server: enters a state (and its clip on every machine) */
	void SetState(EGiantCrabState NewState, EGiantCrabAttack Attack = EGiantCrabAttack::None);

	/** Server: the damage at the moments of the action under way */
	void FireServerMoments();

	/** Server: turns toward a point at the turn rate (times RateScale); returns the angle left (deg) */
	float TurnToward(const FVector& Point, float DeltaSeconds, float RateScale = 1.0f);

	/** Server: walks along a ground direction at the speed of its kind of step (sideways or forward), or at Speed when
	 *  given, held inside the arena */
	void Walk(const FVector& Direction, float Speed = 0.0f);

	/** Server: draws the next blow (the weights, the phase's stomp and charge), whose reach the approach aims for */
	void PlanAttack();

	/** Server: chains the next blow of a combo after From; true when it did */
	bool TryCombo(EGiantCrabAttack From);

	/** Server: the phase of its health (never back), and its roar */
	void UpdatePhase();

	/** Next blow (server) */
	EGiantCrabAttack Planned = EGiantCrabAttack::None;

	/** Server: hurts the prey in the cone of the blow (once each per blow); returns the first one struck */
	AActor* StrikeCone(const FGiantCrabAttackRules& Blow);

	/** Server: shock wave around Center: damage, push, ragdoll of the players within KnockoutRadius */
	void Shockwave(const FVector& Center, float Radius, float Damage, float PushSpeed, float PushUp, float KnockoutRadius = 0.0f);

	/** Server: a leg stamped on a flank (-1 left, +1 right) */
	void StompImpact(int8 Side);

	/** Server: the body on the sand: those under it crushed, a wave around */
	void BellySlamImpact();

	/** Server: those in the way of the charge */
	void ChargeStrikes();

	/** Server: throws the one caught by the grab */
	void ReleaseThrow();

	/** Server: Prey hangs from the claw until the throw: its movement off, the moves its machine sends meanwhile not
	 *  corrected (its machine holds it at its own claw) */
	void CatchPrey(ACharacter* Prey);

	/** Server: lets go of the one held; bFree: it falls free (unless dead or knocked out meanwhile: its ragdoll owns
	 *  its movement) */
	void ReleaseHeldPrey(bool bFree);

	/** Server: the one held is still held (alive, not knocked out, the throw under way), else let go */
	void UpdateHeldPrey();

	/** Client: holds its own pawn when the server holds it, lets it go when the server does */
	void SyncLocalHeld();

	/** Every machine: places the one held under the claw */
	void HoldAtClaw(ACharacter* Held) const;

	/** True for a player knocked out into a ragdoll (its ragdoll owns its movement) */
	static bool IsKnockedOut(const AActor* Actor);

	/** Every machine: the pillars stop the pawns but while it charges (and while it rises, with its collision) */
	void ApplyBodyBlocking();

	/** Every machine, every Pawns.CheckSeconds (T-328): the moves of the characters around it ignore its animated bodies;
	 *  the server frees the ones pinned in its shapes for Pawns.PinnedSeconds */
	void UpdatePawns(float DeltaSeconds);

	/** Server: moves a pinned character out, just out of the shape when it pushes level (Away, Depth), else out of its
	 *  footprint around it, on the ground, inside its ground; true when it did. bReport: says it in the journal when it
	 *  finds no spot */
	bool FreePinnedPawn(ACharacter* Pawn, const FVector& Away, float Depth, FName Shape, float PinnedSeconds, bool bReport);

	/** Time since the last look at the characters around it (s, every machine) */
	float PawnClock = 0.0f;

	/** Characters pressed into its shapes and for how long (server, s) */
	TMap<TWeakObjectPtr<ACharacter>, float> PinnedTimes;

	/** Server: records where its weak points are (the shots of distant players are judged on them, T-305) */
	void RecordWeakPoints();

	/** Server: how far back a shot of this instigator is judged (its round trip and view delay, bounded by the rules;
	 *  0 for the host and the mobs) */
	float GetInstigatorRewind(const AController* EventInstigator) const;

	/** Server: the weak point a shot or blow of a distant player struck as that player saw the crab (the ray of the
	 *  shot, else the point, against the recorded weak points Rewind seconds back); nullptr when none */
	const FGiantCrabWeakPoint* FindLaggedWeakPoint(const FHitResult& Hit, const FVector& Point, float Rewind) const;

	/** Seconds since the action under way began on the server (0 on the server and for a fresh action) */
	float ActionAge() const;

	/** Server: hurts a prey by a blow of the crab (the phase's damage) */
	void HurtPrey(ACharacter* Prey, float Damage);

	/** Server: throws a prey: a player knocked out into a ragdoll when bKnockout, anything else launched */
	void Fling(ACharacter* Prey, const FVector& Velocity, bool bKnockout);

	/** Rules of a blow */
	const FGiantCrabAttackRules* GetBlow(EGiantCrabAttack Attack) const;

	/** Clip of a blow (the stomp borrows the hit reaction until T-260 delivers its own) */
	UAnimSequenceBase* GetAttackClip(EGiantCrabAttack Attack) const;

	/** Clip of a state (and blow) */
	UAnimSequenceBase* GetStateClip(EGiantCrabState State, EGiantCrabAttack Attack) const;

	/** Phase rules or neutral ones */
	const FGiantCrabPhaseRules& CurrentPhase() const;

	/** Every machine: plays the clip of the replicated state, or the walk that fits the body's speed */
	void UpdateClips();

	/** Plays a clip from StartTime, told to the feedback (its notifies) */
	void PlayClip(UAnimSequenceBase* Clip, bool bLoop, float Rate, bool bRestart, float StartTime = 0.0f);

	/** Seconds since the state began (server) */
	float StateTime() const;

	/** Seconds into the clip of the state: StateTime at its play rate */
	float ClipTime() const;

	/** Length of a clip (s), 0 without one */
	static float ClipLength(const UAnimSequenceBase* Clip);

	/** Time of a notify of a clip (s at play rate 1), Fallback without it */
	static float FindNotifyTime(const UAnimSequenceBase* Clip, FName Notify, float Fallback);

	/** Where the crab may stand: the arena around it, else its leash */
	bool IsInTerritory(const FVector& Location) const;

	/** Arena the crab started in */
	TWeakObjectPtr<AArenaZone> Arena;

	/** Where it started (centre of the leash without an arena) */
	FVector Home = FVector::ZeroVector;

	/** What it hunts (server) */
	TWeakObjectPtr<AActor> Target;

	/** The pawn this machine holds at the claw, its movement switched off here (server: HeldPrey; client: its own pawn) */
	TWeakObjectPtr<ACharacter> LocalHeld;

	/** The server ignored the moves of the one held: what it did before (restored at the release) */
	bool bHeldPreyIgnoredErrors = false;

	/** Recent places of the weak points, in the order of the rules (server) */
	FGiantCrabPoseHistory WeakPointHistory;

	/** Next recording of the weak points (server, world time) */
	double NextWeakPointSample = 0.0;

	/** World times the claw of the last slam was stuck in the sand (server; empty when To < From) */
	double StuckClawFrom = 0.0;
	double StuckClawTo = -1.0;

	/** Prey already struck by the blow or the charge under way (server) */
	TSet<TWeakObjectPtr<AActor>> Struck;

	/** Prey under the shell and for how long (server, s) */
	TMap<TWeakObjectPtr<ACharacter>, float> UnderShell;

	/** Time since the last look under the shell (server, s) */
	float UnderShellClock = 0.0f;

	/** Moments of the action under way, and how many are done (server) */
	TArray<FGiantCrabMoment> ServerMoments;
	int32 ServerMomentsDone = 0;

	/** World time the state began (server) */
	double StateStart = 0.0;

	/** Clip time of the blow at the last step (server): a hit window shorter than a frame is not skipped */
	float PreviousClipTime = -1.0f;

	/** World times: next choice of target, next blow allowed, next claw clack, next hit reaction, next charge, next
	 *  belly slam, end of the breath after a charge, last time it sensed its prey, last time it was hurt (server);
	 *  its death on this machine */
	double NextRetarget = 0.0;
	double NextAttack = 0.0;
	double NextTaunt = 0.0;
	double NextHitReact = 0.0;
	double NextCharge = 0.0;
	double NextBellySlam = 0.0;
	double StunnedUntil = 0.0;
	double LastSensedTarget = 0.0;
	double LastHurtTime = -1000.0;
	double LocalDeathTime = -1.0;

	/** Recent weak point hits (server) */
	TArray<double> WeakHits;

	/** The shock wave of the slam under way went off, the throw was released (server) */
	bool bShockwaveDone = false;
	bool bThrowDone = false;

	/** A new phase came while it lay on its back: it roars once it stands again (server) */
	bool bRoarPending = false;

	/** Side it circles its prey on (+1 / -1, server) */
	float CircleSign = 1.0f;

	/** Charge: its direction and end, ground covered (server) */
	FVector ChargeDirection = FVector::ForwardVector;
	FVector ChargeEnd = FVector::ZeroVector;

	/** Last clip asked of the animation, and the serial it was asked for (every machine) */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PlayedClip;

	uint8 PlayedSerial = 255;

	/** A blow of the crab landed on someone: its cue on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCue(EGiantCrabCue Cue, FVector_NetQuantize Location);
};
