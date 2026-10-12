// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Fort/FortSoldierRules.h"
#include "Fort/FortSoldierGoreMath.h"
#include "Fort/FortSoldierCorpseMath.h"
#include "Character/Bean/BeanLibraryAnimInstance.h"
#include "FortSoldier.generated.h"

class AFortGarrisonZone;
class UIslanderHealthComponent;
class UMaterialInstanceDynamic;
class UMobHealthBarComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;

/** What the soldier is doing */
UENUM(BlueprintType)
enum class EFortSoldierState : uint8
{
	/** Standing at a post */
	Post,
	/** Walking to the next post */
	Patrol,
	/** Spotted someone: turns to it */
	Alert,
	/** Runs toward the one it hunts (or its last known place), or stands facing it between two blows */
	Chase,
	/** Musket: aims, fires, reloads */
	Aim,
	Fire,
	Reload,
	/** Sabre: draws it, puts it back */
	Draw,
	Sheathe,
	/** A blow of cold steel */
	Melee,
	HitReact,
	Dead,
	/** Without the arm of its weapon (GoreBox panic, owner 2026-10-09): runs from its foe, round and round without one */
	Flee
};

/** The state every machine plays the clip of (a new Serial restarts it) */
USTRUCT()
struct FFortSoldierActionRep
{
	GENERATED_BODY()

	UPROPERTY()
	EFortSoldierState State = EFortSoldierState::Post;

	UPROPERTY()
	EFortSoldierBlow Blow = EFortSoldierBlow::None;

	UPROPERTY()
	uint8 Serial = 0;

	/** The sabre is in hand (the musket on the back) */
	UPROPERTY()
	bool bSabre = false;

	/** Pitch of the aim at the target (deg), for the musket shouldered on every machine */
	UPROPERTY()
	int8 AimPitch = 0;

	/** Zone of the body the last flinch is for (HitZones::EZone, 255 none) */
	UPROPERTY()
	uint8 HitZone = 255;

	/** What the flinch plays (EFortSoldierFlinch) */
	UPROPERTY()
	uint8 Flinch = 0;

	/** How it died (EFortSoldierDeath) */
	UPROPERTY()
	uint8 Death = 0;

	/** Its headwear was knocked off (the bone hidden, the hat flown) */
	UPROPERTY()
	bool bHatOff = false;

	/** Limbs torn off (FortSoldierGore::ELimbBit mask, owner 2026-10-09): their bones hidden and their pieces flown on
	 *  every machine, its stance (one arm, panic, crawl) on every machine */
	UPROPERTY()
	uint8 LostLimbs = 0;

	/** It was crawling when it died: its body keeps the crawl stance (T-332); a leg torn off by the blow that killed it
	 *  standing leaves its body standing */
	UPROPERTY()
	bool bDiedCrawling = false;
};

/** A clip being played by a soldier, where it is and how fast it runs */
struct FFortSoldierClipPlay
{
	const UAnimSequence* Clip = nullptr;
	float Time = 0.0f;
	float Rate = 1.0f;
};

/** A puff of black powder smoke of a soldier's shot */
struct FFortSoldierSmokePuff
{
	TWeakObjectPtr<UStaticMeshComponent> Mesh;
	TWeakObjectPtr<UMaterialInstanceDynamic> Material;
	float Age = 0.0f;
};

/**
 *  Bean soldier of a French fort (maquette bean-francais: seven uniforms on SK_Bean_Body_Skeleton; campaign day 6,
 *  T-205; owner request 2026-10-07: seven of them in the fort of L_Test_Flat). Server authoritative: it walks between
 *  the posts of its fort (AFortGarrisonZone), spots the foes it sees (players, tiki, the giant crab: every side fights
 *  every other one, Creatures/Hostility, owner 2026-10-08), warns the garrison, fires its musket (Shots rounds, a
 *  reload after each), fights with the bayonet or the sabre when pressed or out of shots, and never leaves the fort.
 *  Without a fort (the arena of L_Test_Flat) it stands on guard, fights what comes, and keeps to the ring it was put
 *  in (SetLeash). Paths on the navigation mesh when there is one, straight walking otherwise. No AnimBP: the bean
 *  library anim instance plays the frame it builds (locomotion, the clip of its state, the upper body while it runs).
 *  Stealth (T-262): a crouched player, or any player at night, is seen only at contact (UStealthSubsystem::CanSee);
 *  a noise of a player it hears (running steps, a voice, a shot) makes it walk there to look, and it gives up after
 *  LoseTargetSeconds without a clue.
 *  Guards of the buildings (T-332): a soldier posted by its fort inside a building stands at its post facing the door,
 *  fights from its building and its doorstep (BuildingHoldRadiusCm) and answers only the alarms close to it.
 */
UCLASS()
class MONKE_ISLAND_API AFortSoldier : public ACharacter
{
	GENERATED_BODY()

public:

	AFortSoldier();

	/** Rules of the garrison (DA_FortSoldierRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Fort Soldier")
	TObjectPtr<UFortSoldierRules> Rules;

	/** Kind of soldier (fusilier, grenadier, sergeant...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier")
	EFortSoldierRole Kind = EFortSoldierRole::Fusilier;

	/** Uniform (SK_BeanFR_*); none = the mesh of the blueprint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Skin, Category="Fort Soldier")
	TObjectPtr<USkeletalMesh> Skin;

	/** First post it goes to (index into the fort's patrol points), -1 = drawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier")
	int32 FirstPost = -1;

	/** Headwear that flies when a blow knocks it off (SM_BeanFR_<Skin>_Hat); none = the hat bone is only hidden */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> HatMesh;

	/** Head that flies when it is taken off (SM_BeanFR_<Skin>_Head); none = the head bone is only hidden */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> HeadMesh;

	/** Limbs that fly when they are torn off (SM_BeanFR_<Skin>_Arm_L / _Arm_R / _Leg_L / _Leg_R, pivot at the socket
	 *  GoreLimb_<Module>; owner 2026-10-09); none = the limb bone is only hidden */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> ArmLMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> ArmRMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> LegLMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> LegRMesh;

	/** Upper body that flies when a blade cuts the soldier in two (SM_BeanFR_<Skin>_Torso_Upper); none = only hidden */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort Soldier|Gore")
	TObjectPtr<UStaticMesh> TorsoUpperMesh;

	/** Returns the health component */
	UFUNCTION(BlueprintPure, Category="Fort Soldier")
	UIslanderHealthComponent* GetHealth() const { return Health; }

	/** True once dead */
	UFUNCTION(BlueprintPure, Category="Fort Soldier")
	bool IsDead() const;

	/** True while its dead body lies at rest and is no longer animated on this machine (T-323) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier")
	bool IsCorpseFrozen() const { return bCorpseFrozen; }

	/** What it is doing (replicated) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier")
	EFortSoldierState GetState() const { return Action.State; }

	/** Who it hunts (server) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier")
	AActor* GetTarget() const { return Target.Get(); }

	/** Shots left (server) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier")
	int32 GetShotsLeft() const { return ShotsLeft; }

	/** Post it walks to or stands at (server, -1 = none) */
	int32 GetPost() const { return Post; }

	/** Limbs torn off (FortSoldierGore::ELimbBit mask, replicated) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier|Gore")
	uint8 GetLostLimbs() const { return Action.LostLimbs; }

	/** True on the ground without a leg: it drags itself, no blow nor shot (replicated) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier|Gore")
	bool IsCrawling() const;

	/** True without the arm of its weapon: it runs in a panic, no blow nor shot (replicated) */
	UFUNCTION(BlueprintPure, Category="Fort Soldier|Gore")
	bool IsPanicking() const;

	/** True while it deals a sabre blow (a blade, for the body it strikes) */
	bool IsSabreBlow() const;

	/** True when Actor is someone the soldiers fight: a living thing of another side (a player, a tiki, the giant
	 *  crab; Creatures/Hostility) */
	static bool IsEnemy(const AActor* Actor);

	/** Server: the fort it belongs to (AFortGarrisonZone, when it starts inside) */
	void JoinGarrison(AFortGarrisonZone* Zone);

	/** Server, before it begins: a guard of a building of its fort (T-332): it stands at the building post Index of the
	 *  zone (World, facing Yaw), keeps within DA_FortSoldierRules.BuildingHoldRadiusCm of it and answers only the alarms
	 *  of the fort close to it (BuildingAlarmRadiusCm) */
	void AssignBuildingPost(int32 Index, const FVector& World, float Yaw);

	/** True for a guard of a building (server) */
	bool IsBuildingGuard() const { return BuildingPost != INDEX_NONE; }

	/** Server: the ring it keeps to when it has no fort (the arena it starts in, owner 2026-10-08): its goals and
	 *  its steps stay inside; Radius 0 frees it */
	void SetLeash(const FVector& Center, float Radius);

	/** Server: another soldier spotted InTarget near From; true when this one takes the hunt */
	bool HearAlarm(AActor* InTarget, const FVector& From);

	/** Server: the fort put it back inside (its path is dropped) */
	void OnPutBackInside();

	/** Server: a firing squad (AExecutionScene): it stands, ignores the players, faces Victim, shoulders at AimAt and
	 *  fires at FireAt (world times); the ball always strikes, kills and throws the body at Push (cm/s) */
	void StartFiringSquad(AActor* Victim, double AimAt, double FireAt, const FVector& Push);

	/** True in a firing squad */
	bool IsFiringSquad() const { return bFiringSquad; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: flinches, takes the one who hurt it as its target, warns the fort */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void PostInitializeComponents() override;

	/** A dead soldier is thrown no more (the giant crab's blows and throw, a grenade's blast): its body lies where it
	 *  fell (owner 2026-10-10: "fix les corps des soldats français volent après la mort") */
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;

	/** A soldier killed in the air (thrown by the giant crab, knocked back) falls to the ground first, then lies still */
	virtual void Landed(const FHitResult& Hit) override;

	/** A correction of its place received while its dead body is frozen wakes it: the movement smooths the mesh to it (T-323) */
	virtual void PostNetReceiveLocationAndRotation() override;

protected:

	virtual void BeginPlay() override;

private:

	/** Health, damage and death */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderHealthComponent> Health;

	/** Health bar over the head once hurt (local players) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMobHealthBarComponent> HealthBar;

	/** Musket, bayonet, sabre and scabbard */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> Musket;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BayonetMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Sabre;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Scabbard;

	/** The state and blow every machine plays */
	UPROPERTY(ReplicatedUsing=OnRep_Action)
	FFortSoldierActionRep Action;

	UFUNCTION()
	void OnRep_Action();

	UFUNCTION()
	void OnRep_Skin();

	/** Death on every machine */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	// ------------------------------------------------------------------ server: network (T-317, review T-268 R-28)

	/** The update rates and the relevancy distance of the rules (Network), those of a living or of a dead soldier */
	void ApplyNetRates();

	/** After its death: dormant once its body lies still, removed after DeadLifetimeSeconds out of the crew's sight */
	void UpdateDeadNet();

	/** Distance to the nearest player's pawn (cm), a large value without one */
	double NearestPlayerDistance() const;

	/** World time of its death (server), negative while alive */
	double DeathTime = -1.0;

	/** Next look at the dead soldier (UpdateDeadNet) */
	FTimerHandle DeadNetTimer;

	// ------------------------------------------------------------------ every machine: a body at rest (T-323)

	/** A dead soldier: counts the time its body spends at rest, and stops animating it once at rest long enough */
	void UpdateCorpseRest(float DeltaSeconds);

	/** What its body does now (FortSoldierCorpse::IsAtRest) */
	FortSoldierCorpse::FRestFacts GatherRestFacts() const;

	/** Stops the ticks of the body at rest: its own (the animation frame), the skeleton's (pose and bones), the musket's
	 *  and the movement's; the engine's physics blend of a ragdoll keeps moving its bones when pushed */
	void FreezeCorpse();

	/** Its ticks run again (a look of the gore, a new detail level, a correction of its place): the rest starts over */
	void WakeCorpse(const TCHAR* Why);

	/** While frozen: the detail level the renderer wants, the switch turned off */
	void WatchFrozenCorpse();

	/** Its body stopped animating on this machine */
	bool bCorpseFrozen = false;

	/** Time and ticks spent at rest since its death or its last wake */
	FortSoldierCorpse::FRestClock CorpseRest;

	/** Looks at the frozen body (WatchFrozenCorpse) */
	FTimerHandle CorpseWatchTimer;

	// ------------------------------------------------------------------ server: decisions

	/** One step of the decisions */
	void ServerThink(float DeltaSeconds);

	/** Looks for the foes it sees (the nearest one, whatever its side); keeps, takes or loses its target */
	void Sense();

	/** True when it sees Actor from its eyes (in its view cone when bNeedView, or close), nothing in the way */
	bool CanSee(const AActor* Actor, bool bNeedView) const;

	/** Chooses what to do against its target (or goes back to its posts) */
	void Decide();

	/** Runs toward its target (or where it saw it last), stopping StopEdge cm from it */
	void ChaseFoe(float StopEdge);

	/** Goes to the next free post of the patrol */
	void GoToNextPost();

	/** Walks or runs to Goal (held on its ground: the fort, or the ring) */
	void MoveTo(const FVector& Goal, bool bRun);

	/** Speed of a walk or a run on the ground (cm/s): the crawl speed without a leg */
	float GroundSpeed(bool bRun) const;

	/** Panic (no arm to hold a weapon): runs from the one it hunts, round and round without one (the Flee state) */
	void Flee();

	/** One step of a straight walk (no navigation mesh there), never off its ground */
	void DirectStep();

	/** True when a wall stands right ahead in Direction (both traces, at the waist and the chest: a ramp or a step is no wall) */
	bool IsWallAhead(const FVector& Direction) const;

	/** True when Location is on the ground it keeps to: its fort, else its ring; anywhere without either */
	bool IsOnOwnGround(const FVector& Location) const;

	/** Location held on that ground */
	FVector ClampToOwnGround(const FVector& Location) const;

	/** Stops walking */
	void StopMoving();

	/** True when the last move is over (reached, or no way) */
	bool HasArrived() const;

	/** Turns toward Point (YawOffset deg to its right) at the turn rate; returns the angle left (deg) */
	float TurnToward(const FVector& Point, float DeltaSeconds, float YawOffset = 0.0f);

	/** Sets the replicated pitch of the aim at Point */
	void SetAimPitchToward(const FVector& Point);

	/** Firing squad: the orders of the scene (aim, fire, stand) */
	void SquadThink(float DeltaSeconds);

	/** Firing squad: the ball that kills */
	void SquadShoot();

	/** Enters a state (and its clip on every machine) */
	void SetState(EFortSoldierState NewState, EFortSoldierBlow Blow = EFortSoldierBlow::None);

	/** Seconds since the state began */
	float StateTime() const;

	/** Fires the ball: it strikes by chance when the target is in sight, else it passes beside */
	void Shoot();

	/** The blow under way: turn, lunge, hit windows */
	void UpdateBlow(float DeltaSeconds);

	/** Next blow of steel: the bayonet, or a sabre blow drawn by the weights */
	EFortSoldierBlow PickBlow() const;

	/** Uses the bayonet rather than the sabre now (its kind, shots left) */
	bool WantsBayonet() const;

	/** Edge distance to the target (cm), a large value without one */
	float TargetEdge() const;

	/** Damage scale of its kind */
	float RoleDamage() const;

	// ------------------------------------------------------------------ every machine: look

	/** Puts the gear where the state wants it (musket in hand or on the back, sabre in hand or sheathed) */
	void ApplyGear();

	/** Attaches a piece of gear to a bone of the body at Grip (cm from an unscaled bone); returns the bone's scale */
	FVector AttachGear(USceneComponent* Gear, FName Bone, const FTransform& Grip);

	/** The musket at this frame: in the hand (the grip of the rules), shouldered toward the target while it aims and
	 *  fires, stood up and held by the barrel while it reloads; blended from a pose to the next */
	void UpdateMusketGrip(float DeltaSeconds);

	/** Where the state wants the musket */
	enum class EMusketPose : uint8 { Hand, Aim, Reload };
	EMusketPose MusketPose = EMusketPose::Hand;

	/** The musket in the soldier's frame when the pose changed, and how far the blend went (0 → 1) */
	FTransform MusketFromActor = FTransform::Identity;
	float MusketPoseAlpha = 1.0f;

	/** Clips of the shot and the reload (the player's musket when the rules name it) */
	UAnimSequence* GetFireClip() const;
	UAnimSequence* GetReloadClip() const;

	/** Moment of the fire clip the shot state starts at (the trigger; the aim holds the clip up to there) (s) */
	float GetFireStartTime() const;

	/** Seconds from the start of the shot state to the ball leaving */
	float GetShotDelay() const;

	/** Builds the animation frame: locomotion, the clip of the state, the upper body */
	void UpdateAnimation(float DeltaSeconds);

	/** Clip of a state (and blow), none for the locomotion states */
	UAnimSequence* GetStateClip(EFortSoldierState State, EFortSoldierBlow Blow) const;

	/** Length of a clip (s), 0 without one */
	static float ClipLength(const UAnimSequence* Clip);

	/** Bone a blow struck: the bone of its hit, else the bone its line passes nearest to (HitBoneRadiusCm) */
	FName BoneOfHit(const FDamageEvent& DamageEvent) const;

	/** Zone of the body a blow struck (HitZones::EZone), from BoneOfHit */
	uint8 ZoneOfHit(const FDamageEvent& DamageEvent) const;

	/** The head bone for a blow on the lower share of the hat bone (the face), Bone otherwise */
	FName HeadOrHat(FName Bone, const FHitResult& Hit) const;

	/** The blow being dealt, known before the health may kill (server): its bone and zone, where it struck and from
	 *  where, the death it would cause */
	FName PendingHitBone;
	uint8 PendingHitZone = 255;
	FVector PendingHitPoint = FVector::ZeroVector;
	FVector PendingHitDirection = FVector::ZeroVector;
	EFortSoldierDeath PendingDeath = EFortSoldierDeath::Plain;

	// ------------------------------------------------------------------ limbs torn off (server, owner 2026-10-09)

	/** True when the blow of DamageCauser is a blade: the player's cutlass when its move cuts, the sabre of another soldier */
	bool IsBladeBlow(const AActor* DamageCauser, bool bShot) const;

	/** The blows of a point damage on the arms and legs wear them down (the pellets of a shot by bone times the weapon's
	 *  LimbDamageScale, a blade by the bone it struck); OutTorn gets the limbs this damage tears off */
	void WearLimbs(const FDamageEvent& DamageEvent, float DamageAmount, TArray<int32, TInlineAllocator<4>>& OutTorn);

	/** Tears a limb off (FortSoldierGore::ELimb): the replicated mask, the musket and the shots gone with an arm, the
	 *  fall with a leg; false when it is gone already */
	bool TearLimb(int32 Limb);

	/** True when this limb is gone */
	bool IsLimbLost(int32 Limb) const;

	/** A crawling soldier dies of its wounds (the crawl death) */
	void BleedOut();

	/** Damage taken by each limb (FortSoldierGore::ELimb order) */
	TArray<float> LimbDamage;

	/** World time it fell to crawl */
	double CrawlStart = 0.0;

	// ------------------------------------------------------------------ GoreBox look (every machine, owner 2026-10-08)

	/** Blood of a blow at Location along Direction on every machine (the tiki blood subsystem with BloodRules) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBloodHit(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction);

	/** Applies the look of the action: the hat knocked off, the head taken off, the burst ragdoll (timed on the clips) */
	void ApplyGoreLook();

	/** The hat bone hidden and the HatMesh flying */
	void KnockHatOff();

	/** The head bone hidden, the HeadMesh flying, the blood at the neck */
	void TakeHeadOff();

	/** The body thrown back as a ragdoll, the head off, chunks of blood */
	void BurstBody();

	/** A piece of the body (hat, head, a limb) flying from its bone at Velocity (cm/s), kept in Pieces; nullptr without a mesh */
	UStaticMeshComponent* LaunchPiece(UStaticMesh* PieceMesh, FName Bone, const FVector& Offset, const FVector& Velocity);

	/** A piece flying from a place in the world (its turn kept, its own size) at Velocity (cm/s); nullptr without a mesh */
	UStaticMeshComponent* LaunchPieceAt(UStaticMesh* PieceMesh, const FTransform& World, const FVector& Velocity);

	/** Pieces flown off the body */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	/** Serial of the death whose look was applied; what is already gone */
	uint8 GoreSerial = 255;
	bool bHatHidden = false;
	bool bHeadHidden = false;
	bool bBurst = false;

	/** Timers of the hat and of the death looks */
	FTimerHandle HatTimer;
	FTimerHandle DeathTimer;

	// ------------------------------------------------------------------ limbs look (every machine, owner 2026-10-09)

	/** The stance of the lost limbs on the body: the capsule low and the mesh at its feet while it crawls, the crawl
	 *  speed (every machine, a dedicated server too) */
	void ApplyStance();

	/** A limb gone: its bone hidden; fresh (torn now, not met already torn by a player joining late), its piece flies,
	 *  the stump bleeds and the gear of that arm falls */
	void TearLimbLook(int32 Limb, bool bFresh);

	/** The gear lost with an arm flies off: the musket with the first arm, the sabre with the right one when in hand */
	void DropGearLook(int32 Limb);

	/** The body cut in two (the bisected death): the upper body hidden, its piece flying with the head, the hat and
	 *  the arms still on, the waist bleeding */
	void CutInTwo();

	/** Hides again what a new mesh showed (the skin received after the action by a player joining late) */
	void ReapplyHiddenBones();

	/** The GoreBox locomotion of a wounded soldier (crawling, panicking) and its play rate; nullptr when whole or
	 *  without the clip */
	const UAnimSequence* WoundedLocomotion(FortSoldierGore::EStance Stance, float Speed, float& OutRate) const;

	/** First bone of a limb (the rules) and its piece (the blueprint) */
	FName LimbBone(int32 Limb) const;
	UStaticMesh* LimbPiece(int32 Limb) const;

	/** Limbs whose look was applied (ELimbBit mask); the gear flown; the body cut in two */
	uint8 AppliedLost = 0;
	bool bMusketFlown = false;
	bool bSabreFlown = false;
	bool bBisected = false;

	/** Stance applied to the capsule and the mesh (-1 none yet, 0 standing, 1 crawling), and the standing sizes */
	int8 StanceApplied = -1;
	float StandingHalfHeight = 55.0f;
	FVector StandingMeshOffset = FVector::ZeroVector;

	/** The bounds of a body lying on the ground (crawling or dead) on every machine: LyingBoundsScale (T-332) */
	void ApplyBodyBounds();

	/** Bounds scale of the blueprint (standing), -1 until read */
	float StandingBoundsScale = -1.0f;

	/** Muzzle in the world */
	FVector GetMuzzle() const;

	/** Shot on every machine: report, flame and smoke */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot(FVector_NetQuantize Muzzle, FVector_NetQuantizeNormal Direction);

	/** Sound of a moment of the fight on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(USoundBase* Sound, FVector_NetQuantize Location);

	/** Swells and fades the smoke puffs, puts out the flame */
	void UpdateEffects(float DeltaSeconds);

	// ------------------------------------------------------------------ server state

	/** The fort it belongs to */
	TWeakObjectPtr<AFortGarrisonZone> Garrison;

	/** Ring it keeps to without a fort: centre and radius (cm), 0 = free */
	FVector LeashCenter = FVector::ZeroVector;
	float LeashRadius = 0.0f;

	/** Building post it guards (index into its fort's BuildingPosts, -1 = the patrol of the court and the ramparts),
	 *  where it is and the yaw it faces there (T-332) */
	int32 BuildingPost = INDEX_NONE;
	FVector BuildingPostLocation = FVector::ZeroVector;
	float BuildingPostYaw = 0.0f;

	/** Who it hunts, where and when it saw it last, whether it sees it now */
	TWeakObjectPtr<AActor> Target;
	FVector LastSeenAt = FVector::ZeroVector;
	double LastSeenTime = -1000.0;
	bool bSeesTarget = false;

	/** It only heard the one it hunts (running steps, a voice, a shot) and never saw it since: it walks there to look
	 *  rather than runs (T-262) */
	bool bHeardOnly = false;

	/** Time of the newest noise it took: each noise is taken once (T-262) */
	double HeardNoiseTime = -1.0e9;

	/** Shots left, and the musket loaded */
	int32 ShotsLeft = 0;
	bool bLoaded = true;

	/** Post it walks to or stands at */
	int32 Post = INDEX_NONE;

	/** World times: end of the stay at the post, next look, next path, next blow, next flinch, last fight */
	double PostUntil = 0.0;
	double NextSense = 0.0;
	double NextRepath = 0.0;
	double NextBlow = 0.0;
	double NextHitReact = 0.0;
	double LastFight = -1000.0;

	/** Time drawn for the aim under way (s) */
	float AimSeconds = 1.0f;

	/** Firing squad: on, its orders (world times), the push of its ball, and the shot fired */
	bool bFiringSquad = false;
	bool bSquadFired = false;
	double SquadAimAt = 0.0;
	double SquadFireAt = 0.0;
	FVector SquadPush = FVector::ZeroVector;

	/** World time the state began */
	double StateStart = 0.0;

	/** The ball of the shot under way left; the gear of the draw or sheathe moved; hit windows that struck */
	bool bShotDone = false;
	bool bGearSwapped = false;
	TSet<int32> StruckWindows;

	/** Goal of the move under way; walking straight (no navigation mesh there) */
	FVector MoveGoal = FVector::ZeroVector;
	bool bMoving = false;
	bool bDirectMove = false;

	/** Walking straight: when its progress is next looked at, and how far it then was from its goal (cm) */
	double DirectProgressCheckAt = 0.0;
	double DirectLastLeft = 0.0;

	// ------------------------------------------------------------------ every machine: animation and effects

	/** Locomotion going out and coming in, and the weight of the one coming in */
	FFortSoldierClipPlay BaseFrom;
	FFortSoldierClipPlay BaseTo;
	float BaseAlpha = 1.0f;

	/** Clip of the state over the locomotion, its weight, whether the state still plays it; serial it started for */
	FFortSoldierClipPlay StateClip;
	float StateClipWeight = 0.0f;
	bool bStateClipActive = false;
	uint8 PlayedSerial = 255;

	/** Upper body layer and its weight */
	FFortSoldierClipPlay Upper;
	float UpperWeight = 0.0f;

	FBeanAnimFrame Frame;

	/** Where the gear was last put: -1 nowhere yet, 0 musket in hand, 1 sabre in hand */
	int32 GearApplied = -1;

	/** Scale of the hand bone the musket hangs from (the bean bones carry 100) */
	FVector MusketBoneScale = FVector::OneVector;

	/** Smoke of the last shots, and the flame */
	TArray<FFortSoldierSmokePuff> Puffs;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Flash;

	float FlashLeft = 0.0f;
};
