// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Creatures/TikiCombat.h"
#include "TikiCreature.generated.h"

class ATikiDirector;
class UAnimSequenceBase;
class UIslanderHealthComponent;
class UMobHealthBarComponent;
class USoundAttenuation;
class USoundBase;
class UTikiAnimInstance;
class UTikiDismemberSet;
class UTikiShredDemoSet;
class ATikiShredDemo;
class UTikiRules;
enum class ETikiLimbLoss : uint8;
struct FStealthNoise;
struct FTikiNetRules;

/** One-shot clip played on every machine */
UENUM(BlueprintType)
enum class ETikiAction : uint8
{
	None,
	Attack,
	Hit,
	Collect,
	Cower,
	Emerge,
	/** Villager activity with the decor (well, storehouse, chat): the basket clip, looped for one take */
	Work,
	/** Villager kneeling at the shrine: the crouch clip, looped for one take */
	Pray,
	/** Villager sitting on its heels in front of the engawa: the crouch clip, looped for one take */
	Rest,
	/** Warrior spotting a player: its growl, no clip */
	Roar,
	/** Villager running away: its scream, no clip */
	Scream,
	/** Warrior spotting a player: the alert clip and its growl */
	Alert,
	/** Warrior taunting a player out of its reach */
	Taunt,
	/** Heavy hit */
	Stagger,
	/** Hit from behind */
	HitBack,
	/** Its prey down */
	Victory,
	/** Villager startled by an alarm */
	Startle,
	/** Villager looking around */
	LookAround,
	/** Villager waving at a player */
	Greet,
	/** Villager gesture in a chat (variant = which one) */
	Gesture,
	/** Hit in a zone of the body (variant = HitZones::EZone): the GoreBox clips of the realistic mobs (owner 2026-10-08) */
	HitZone,
	/** Warrior tactics (T-275): a feint, its hiss and gesture (Stalker) */
	Feint,
	/** Warrior tactics (T-275): the heavy blow, its growl and slow preparation (Stalker) */
	HeavyAttack,
	/** Warrior tactics (T-275): the claw opens to seize, its clacks (Bulwark) */
	Grab,
	/** Warrior tactics (T-275): its guard broken, it staggers (Bulwark) */
	GuardBreak,
	/** Warrior tactics (T-275): the howl that calls the others (Watcher) */
	Howl,
	/** Warrior tactics (T-275): it breathes the ash in, then throws it (Watcher) */
	Cast,
	/** Warrior tactics (T-275): wounded and alone, it runs away with a cry, no clip (Stalker) */
	Flee
};

/**
 *  Last one-shot action of a creature, replicated (T-306, review R-27): a lost packet, or a creature that becomes
 *  relevant during the preparation of its blow, still shows the telegraph. A client starts it as far into its clip as
 *  the server is, so the blow lands on its screen when the server deals it; an action long over is not replayed.
 */
USTRUCT()
struct FTikiActionRep
{
	GENERATED_BODY()

	/** The action */
	UPROPERTY()
	ETikiAction Action = ETikiAction::None;

	/** Which of its variants (blow, gesture, zone) */
	UPROPERTY()
	uint8 Variant = 0;

	/** Bumped by the server at each action: the same action twice still plays twice */
	UPROPERTY()
	uint8 Serial = 0;

	/** Server world time the action started (s) */
	UPROPERTY()
	float StartServerTime = 0.0f;
};

/** Clips of a creature (A_<Name>_*), played without an AnimBP */
USTRUCT(BlueprintType)
struct FTikiAnimSet
{
	GENERATED_BODY()

	/** Standing still, looped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Idle;

	/** Walking, looped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Walk;

	/** Running, looped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Run;

	/** Blow with its visible preparation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Attack;

	/** Hit reaction, also used for the leap out of a bush */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Hit;

	/** Death, holds the last frame */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Death;

	/** Picking a coconut up (villager: Talk, the basket offered) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Collect;

	/** Fear (villager) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Cower;

	/** Idle while it fights (warriors with a target), looped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> IdleCombat;

	/** Spotting a player (warriors) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Alert;

	/** Taunting a player out of its reach (warriors) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Taunt;

	/** Other blows, one drawn at random with Attack (sweep, lunge, overhead...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TArray<TObjectPtr<UAnimSequenceBase>> AttackVariants;

	/** Hit from behind */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> HitBack;

	/** Heavy hit: a cut or a strong blow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Stagger;

	/** Its prey down (warriors) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Victory;

	/** Startled by an alarm, before it runs (villagers) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Startle;

	/** Looking around now and then (villagers) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> LookAround;

	/** Waving at a player who comes close (villagers) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> Greet;

	/** Gestures of a chat on the square: give, point, nod, shake the head, cheer (villagers) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TArray<TObjectPtr<UAnimSequenceBase>> Gestures;

	/** Gathering at a work activity (villagers), looped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	TObjectPtr<UAnimSequenceBase> WorkGather;

	/** Reactions to the bean's musket and sabre (T-150), filled by scripts/character/set_combat_reactions.py */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims")
	FTikiCombatReactions Combat;

	/** Hits by zone of the body (A_<Mob>_Hit_Head / _Torso / _Arm_L / _Arm_R / _Leg_L / _Leg_R, the GoreBox clips of the
	 *  realistic mobs, owner 2026-10-08): a blow the creature survives plays the one of the bone it struck; Hit when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Zones")
	TObjectPtr<UAnimSequenceBase> HitHead;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Zones")
	TObjectPtr<UAnimSequenceBase> HitTorso;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Zones")
	TObjectPtr<UAnimSequenceBase> HitArmL;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Zones")
	TObjectPtr<UAnimSequenceBase> HitArmR;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Zones")
	TObjectPtr<UAnimSequenceBase> HitLegL;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Zones")
	TObjectPtr<UAnimSequenceBase> HitLegR;

	/** Clip of a zone (HitZones::EZone), null when none is set */
	UAnimSequenceBase* GetZoneHit(uint8 Zone) const
	{
		switch (Zone)
		{
		case 0: return HitHead;
		case 1: return HitTorso;
		case 2: return HitArmL;
		case 3: return HitArmR;
		case 4: return HitLegL;
		case 5: return HitLegR;
		default: return nullptr;
		}
	}

	/** Locomotion of a creature with a leg torn off (A_<Mob>_Legless_Crawl_Loop, looped; the GoreBox clips of the
	 *  realistic mobs, owner 2026-10-09): it drags itself; empty, the walk and run as before */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Gore")
	TObjectPtr<UAnimSequenceBase> LeglessCrawl;

	/** A crawling creature that does not move writhes on the ground (A_<Mob>_Writhe_Ground_Loop, looped); empty, the
	 *  crawl loop */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Gore")
	TObjectPtr<UAnimSequenceBase> WritheGround;

	/** A creature without both arms runs in a panic (A_<Mob>_Armless_Panic_Loop, looped), no blow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Gore")
	TObjectPtr<UAnimSequenceBase> ArmlessPanic;

	/** Blow of a creature with one arm torn off (A_<Mob>_Armless_Fight_On); empty, its attack clips */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Gore")
	TObjectPtr<UAnimSequenceBase> ArmlessFightOn;

	/** Speed the crawl clip does not slide at (cm/s; the maquette: 18); 0 = played as authored */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims|Gore", meta=(ClampMin="0"))
	float CrawlSpeed = 18.0f;

	/** Speed the walk clip does not slide at (cm/s, from the animation manifest); 0 = WalkClipSpeed of the rules */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims", meta=(ClampMin="0"))
	float WalkSpeed = 0.0f;

	/** Speed the run clip does not slide at (cm/s); 0 = RunClipSpeed of the rules */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anims", meta=(ClampMin="0"))
	float RunSpeed = 0.0f;
};

/** Voice of a creature: random variants by event, played on every machine (CC0 creature sounds, art/SOURCES.md) */
USTRUCT(BlueprintType)
struct FTikiVoice
{
	GENERATED_BODY()

	/** Leaping out of a bush */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Emerge;

	/** Spotting a player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Aggro;

	/** The blow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Attack;

	/** Hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Hurt;

	/** Death */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Death;

	/** Now and then, near a player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Idle;

	/** Running away (villagers) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TArray<TObjectPtr<USoundBase>> Flee;

	/** Pitch of this creature (deeper below 1), a little varied at each sound */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice", meta=(ClampMin="0.25", ClampMax="4"))
	float Pitch = 1.0f;

	/** Volume of this creature */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice", meta=(ClampMin="0"))
	float Volume = 1.0f;

	/** How the voice fades with distance (ATT_Tiki_Voice) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Shortest and longest pause between two idle sounds (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice", meta=(ClampMin="1", Units="s"))
	float IdleIntervalMin = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice", meta=(ClampMin="1", Units="s"))
	float IdleIntervalMax = 16.0f;

	/** Idle sounds only within this distance of the local camera (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Voice", meta=(ClampMin="0"))
	float IdleDistance = 2500.0f;
};

/**
 *  Base of the tiki creatures (villagers and warriors): a character moved by its own server logic (no NavMesh on
 *  the 1:1 atoll), health from UIslanderHealthComponent, clips crossfaded by UTikiAnimInstance from the replicated
 *  movement and one-shot actions, a voice, and a ragdoll at death. Server authoritative; clients only pick the
 *  clips and play the sounds.
 */
UCLASS(Abstract)
class MONKE_ISLAND_API ATikiCreature : public ACharacter
{
	GENERATED_BODY()

public:

	ATikiCreature();

	/** Rules shared by the creatures (DA_TikiRules); the director sets them on spawn, they replicate once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Tiki")
	TObjectPtr<UTikiRules> Rules;

	/** Voice of this creature */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki")
	FTikiVoice Voice;

	/** Clips of this creature */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki")
	FTikiAnimSet Anims;

	/** Weapon one can pick up, thrown out of the hand at death: BP_Pickup_<Weapon> (AMobWeaponPickup, T-161: "Ramasser"
	 *  then a melee weapon in a slot; the BP_Drop_* physics props before); none: the corpse keeps it in its hand. It
	 *  stays in the hand of an arm cut off before */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki|Death")
	TSubclassOf<AActor> DroppedWeaponClass;

	/** Bone the held weapon is skinned to: the dropped one appears at its grip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki|Death")
	FName DroppedWeaponBone = TEXT("weapon_r");

	/** Correction of the dropped weapon in its own frame. It is already placed as held: the static weapons are in grip
	 *  space with their long axis on +Z (scripts/character/tiki/export.py), and that axis follows the held weapon from
	 *  the grip toward its middle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki|Death")
	FTransform DroppedWeaponOffset;

	/** Speed the dropped weapon flies out of the hand with, along the killing blow (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki|Death", meta=(ClampMin="0"))
	float DroppedWeaponThrowSpeed = 200.0f;

	/** Closed modules of the body (DA_<Mob>_DismemberSet, T-155): once a part is cut off, the body and its pieces are
	 *  drawn with them, every cut showing flesh and bone; none (or an incomplete set): the bones are hidden (T-072) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki|Dismember")
	TObjectPtr<UTikiDismemberSet> DismemberSet;

	/** Baked shredding demos of this mob (DA_<Mob>_ShredDemoSet, T-168): a lethal burst, close shot or sabre cut with a
	 *  demo replaces the mob and its corpse by the fragments of the pack; none: the reactions and modules as before */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki|Dismember")
	TObjectPtr<UTikiShredDemoSet> ShredDemoSet;

	/** Returns the health component */
	UFUNCTION(BlueprintPure, Category="Tiki")
	UIslanderHealthComponent* GetHealth() const { return Health; }

	/** True once dead */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsDead() const;

	/** Director that spawned this creature (server) */
	void SetDirector(ATikiDirector* InDirector);

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: records what dealt the blow (weapon, distance, stroke) before the health takes it, for the combat
	 *  reaction picked at death */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** True while a combat reaction clip plays on this machine (T-150) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsCombatReactionPlaying() const;

	/** Server: takes this bone off the living creature with what hangs from it (an arm and its weapon) and drops it
	 *  as a corpse piece; false when it is gone already or unknown */
	bool CutOffLimb(FName Bone, const FVector& Direction);

	/** True once the arm holding the weapon was cut off */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsMaimed() const;

	/** Legs torn off this creature (server, T-167): it limps, then drags itself */
	UFUNCTION(BlueprintPure, Category="Tiki")
	int32 GetLegsLost() const { return LegsLost; }

	/** Damage taken by each limb of the rules (DA_TikiRules Limbs, same order; server, T-167) */
	const TArray<float>& GetLimbDamage() const { return LimbDamage; }

	/** Limbs of the rules torn or cut off whose loss is of this kind (from the replicated cut bones: every machine) */
	int32 CountLimbsLost(ETikiLimbLoss Loss) const;

	/** True with a leg torn off: it drags itself (Anims.LeglessCrawl), dies crawling (Combat.LeglessCrawlDeath) */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsCrawling() const;

	/** True with both arms torn off: it only runs in a panic (Anims.ArmlessPanic), no blow */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsArmless() const;

	/** True when the blow of this damage causer cuts: a cutlass swing whose attack has bCut (server) */
	static bool IsCutBlow(const AActor* DamageCauser);

	/** Debug (server): the next blow cuts, whatever deals it */
	void MarkNextBlowCut() { bNextBlowCut = true; }

protected:

	virtual void BeginPlay() override;

	/** Server decisions, every tick while alive */
	virtual void ServerThink(float DeltaSeconds) {}

	/** Server: the creature took damage from Causer (a player pawn, usually) */
	virtual void OnDamaged(AActor* Causer, float Damage) {}

	/** Server: the creature died; Killer dealt the last blow (a player pawn, or the weapon or projectile of one) */
	virtual void OnKilled(AActor* Killer) {}

	/** Pawn behind a damage causer: the pawn itself, else the instigator or the owner of a weapon or projectile;
	 *  nullptr when none */
	static APawn* PawnOfCauser(AActor* Causer);

	/** Limb a cut blow takes off this creature (LimbBones of the rules), NAME_None when none is left */
	FName PickLimbToCut() const;

	/** Share of its speed a creature keeps with the legs it lost (1 with both legs, T-167) */
	float GetLimbSpeedScale() const;

	/** Plays one of the sounds at random, attached to the body, with the pitch and volume of the voice (every
	 *  machine; nothing on a dedicated server) */
	void PlayVoice(const TArray<TObjectPtr<USoundBase>>& Sounds) const;

	/** The body goes limp: its physics asset simulates, thrown along the last blow; false without a physics asset */
	bool StartRagdoll();

	/** Health of this creature from the rules */
	virtual float GetRulesHealth() const { return 100.0f; }

	/** Network rates of this kind of creature from the rules (T-312): the warriors' unless a subclass says otherwise;
	 *  nullptr without rules */
	virtual const FTikiNetRules* GetNetRules() const;

	/** Server: walks or runs toward a point; returns the horizontal distance left (cm) */
	float MoveToward(const FVector& Target, float Speed);

	/** Water depth above the ground at a location (cm), 0 on dry land */
	float WaterDepthAt(const FVector& Location) const;

	/** A swimmer, or a player standing in water deeper than a tiki wades: tikis do not follow it into the sea */
	bool IsOutOfReachInWater(const APawn* Pawn) const;

	/** Server: stops and turns toward a point */
	void FaceToward(const FVector& Target);

	/** Server: plays a one-shot clip on every machine (Variant picks among the variants of the action), through the
	 *  replicated action state (T-306): a client that receives it late starts it that far in */
	void PlayAction(ETikiAction Action, uint8 Variant = 0);

	/** Starts a one-shot clip and its sound locally (every machine, from the replicated action); a subclass handles its
	 *  own actions and passes the others on. ActionAge tells how late this machine starts it */
	virtual void PlayLocalAction(ETikiAction Action, uint8 Variant);

	/** How late this machine starts the action PlayLocalAction plays now (s): 0 on the server, the replication delay
	 *  on a client. Its clip starts that far in and ends when the server's does; its sounds are left out once older
	 *  than ActionLateSoundSeconds of the rules (T-306) */
	float ActionAge = 0.0f;

	/** True when a one-shot action may start on this machine: not dead, no combat reaction playing */
	bool CanPlayAction() const;

	/** Plays Clip once at PlayRate for Duration seconds (its length at that rate when 0) on this machine: the
	 *  locomotion resumes after it */
	void PlayOneShot(UAnimSequenceBase* Clip, float Duration, float PlayRate = 1.0f);

	/** True when Clip can play on this body: same skeleton as its mesh (a clip of another mob is never played) */
	bool IsClipForBody(const UAnimSequenceBase* Clip) const;

	/** Clip of this creature standing still (a warrior in a fight stands on guard) */
	virtual UAnimSequenceBase* GetIdleClip() const { return Anims.Idle; }

	/** Looped clip for a body moving at Speed in another way than the walk and run (a side step, a walk backward, T-275);
	 *  nullptr keeps the walk and run. OutClipSpeed: the speed it does not slide at (cm/s), 0 = as authored */
	virtual UAnimSequenceBase* GetDirectionalClip(float Speed, float& OutClipSpeed) const { return nullptr; }

	/** True for a looped locomotion clip (crossfaded slowly, never restarted): idle, walk, run, crawl, and those a
	 *  subclass adds */
	virtual bool IsLocomotionClip(const UAnimSequenceBase* Clip) const;

	/** True while a one-shot clip plays */
	bool IsActionPlaying() const;

	/** Pawn of a living player nearest to this creature within MaxDistance, or nullptr; bMustSee: only a player it sees
	 *  (CanSpot: a crouched player, or any player at night, only at contact, T-262) */
	APawn* FindNearestPlayer(float MaxDistance, bool bMustSee = false) const;

	/** Nearest living foe of this creature within MaxDistance (Creatures/Hostility: a player, a fort soldier, the giant
	 *  crab...), or nullptr; bMustSee: only a foe it sees (CanSpot, T-262) */
	APawn* FindNearestFoe(float MaxDistance, bool bMustSee = false) const;

	/** True when this creature sees Target within Range by the stealth rules (UStealthSubsystem::CanSee, T-262): a
	 *  crouched player, or any player at night, only at contact; anything else in range. A warrior also sees the flame
	 *  of a lit torch at night (T-275) */
	virtual bool CanSpot(const AActor* Target, float Range) const;

	/** Server: the newest noise of a player this creature hears now (UStealthSubsystem::FindHeardNoise, the radius
	 *  times HearingScale), at most every ListenIntervalSeconds of DA_StealthRules; false otherwise (T-262) */
	bool Listen(float HearingScale, FStealthNoise& OutNoise);

	/** True when Pawn is a living player pawn */
	static bool IsLivingPlayer(const APawn* Pawn);

	/** Director that spawned this creature (server) */
	TWeakObjectPtr<ATikiDirector> Director;

	/** Point the creature tries to reach, for the stuck check (server) */
	FVector StuckCheckLocation = FVector::ZeroVector;

	/** World time of the last stuck check (server) */
	double StuckCheckTime = 0.0;

	/** True when the creature did not move 30 cm toward its goal in the last 2 s (server) */
	bool UpdateStuck(bool bWantsToMove);

private:

	/** Health, damage and death */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderHealthComponent> Health;

	/** Health bar above the creature once it is hurt (local players) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMobHealthBarComponent> HealthBar;

	/** Item held in the hand (weapon, basket), follows the body pose */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> HeldItem;

	/** Routes engine damage to OnDamaged (server) */
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** Death on every machine */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** At death: the hand lets go of its weapon on every machine, the server spawns the pickup (DroppedWeaponClass) */
	void DropWeapon();

	/** Last one-shot action started by the server (T-306, review R-27: was an unreliable multicast without state) */
	UPROPERTY(ReplicatedUsing=OnRep_ActionRep)
	FTikiActionRep ActionRep;

	/** Serial of the last action started on this machine */
	uint8 LocalActionSerial = 0;

	/** Starts the replicated action on a client */
	UFUNCTION()
	void OnRep_ActionRep();

	/** Client: starts the replicated action once, as far into it as the server is; nothing for an action long over */
	void StartReplicatedAction();

	/** Network frequencies and relevancy of this creature from the rules (server, T-312) */
	void ApplyNetRules();

	/** Picks the locomotion clip for the current speed (every machine) */
	void UpdateAnimation();

	/** Plays a clip on the body if it is not already playing, from StartTime (s), at PlayRate */
	void PlayClip(UAnimSequenceBase* Clip, bool bLoop, float StartTime = 0.0f, float PlayRate = 1.0f);

	/** Clip currently on the body */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentClip;

	/** World time the one-shot clip ends */
	double ActionEndTime = 0.0;

	/** The death clip is on */
	bool bDeathPlayed = false;

	/** Bones cut off this living creature, hidden on every machine */
	UPROPERTY(ReplicatedUsing=OnRep_CutBones)
	TArray<FName> CutBones;

	/** Hides the cut bones on every machine */
	UFUNCTION()
	void OnRep_CutBones();

	/** Hides the cut bones of the body, and the held item once its hand is gone */
	void ApplyCutBones();

	/** Modules drawn instead of the body mesh once a part is cut off, following its pose (every machine, T-155) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> DismemberModules;

	/** Draws the body with the modules the cut bones leave, the body mesh hidden; false (whole mesh shown, modules
	 *  removed) when the set cannot close these cuts (T-155) */
	bool ApplyDismemberModules();

	/** Blood of a hit (T-153): the musket or blade impact at Location along Direction, on every machine (cosmetic,
	 *  lost packets only lose a spray) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBloodHit(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction);

	/** Blood of a shot whose projectiles entered the body at Locations (T-167: one spray per shotgun pellet), along
	 *  Direction, on every machine (cosmetic) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBloodHits(const TArray<FVector_NetQuantize>& Locations, FVector_NetQuantizeNormal Direction);

	/** Blood of a bone just cut off this body (T-153): the jet out of the stump, chunks and a splat (every machine) */
	void SpawnStumpBlood(FName Bone);

	/** Cut bones whose blood this machine has spawned */
	int32 BloodCutCount = 0;

	/** Server: replaces the dead body by its corpse, or by two halves after a cut blow */
	void BecomeCorpse();

	/** The last blow was a cut (IsCutBlow), read when the body becomes a corpse (server) */
	bool bLastBlowCut = false;

	/** Debug: the next blow cuts (server) */
	bool bNextBlowCut = false;

	/** Blow being dealt, recorded by TakeDamage before the health takes it (server) */
	TikiCombat::FBlow PendingBlow;

	/** The blow being dealt cuts (cutlass move with bCut, or the debug flag) (server) */
	bool bPendingBlowCut = false;

	/** PendingBlow is the blow being dealt (server, inside TakeDamage) */
	bool bHasPendingBlow = false;

	/** Horizontal direction of the blow being dealt (server) */
	FVector PendingBlowDirection = FVector::ZeroVector;

	/** Bone the blow being dealt struck (point damage), None otherwise (server) */
	FName PendingHitBone;

	/** The blow that started the lethal reaction cuts: its limb comes off at the contact (server) */
	bool bReactionCuts = false;

	/** Margin past its clip a replicated reaction is still started (s): a player joining late does not replay it */
	static constexpr double ReactionStaleMargin = 0.25;

	/** Combat reaction started by the server, replicated to start it on every machine */
	UPROPERTY(ReplicatedUsing=OnRep_CombatReaction)
	FTikiCombatReactionState CombatReaction;

	/** Serial of the last reaction started on this machine */
	uint8 LocalReactionSerial = 0;

	/** World time the combat reaction clip ends on this machine */
	double ReactionEndTime = 0.0;

	/** World time the server resumes the creature's decisions after a reaction it survives */
	double ThinkSuspendedUntil = 0.0;

	/** A lethal reaction clip plays instead of the immediate ragdoll on this machine */
	bool bLethalReactionStarted = false;

	/** Length of the death clip played to the ground (s), 0 when the body went limp instead (T-151) */
	float DeathClipLength = 0.0f;

	/** The corpse has replaced this body (server): once only */
	bool bBecameCorpse = false;

	/** Timers of a lethal reaction: ragdoll, weapon drop (every machine), limb cut (server) */
	FTimerHandle ReactionRagdollTimer;
	FTimerHandle ReactionDropTimer;
	FTimerHandle ReactionCutTimer;

	/** The blow being dealt is predicted lethal with a shredding demo: its limb hits tear nothing, the demo shows the
	 *  whole body flying apart (server, T-168) */
	bool bPendingShredDemo = false;

	/** Demo spawned on this machine in place of the body (T-168) */
	TWeakObjectPtr<ATikiShredDemo> ShredDemo;

	/** Server: demo replacing a lethal blow whose reaction is Reaction (T-150 pick, None when no clip), None when the
	 *  mob keeps its reaction (no demo, body already cut, rules off) */
	ETikiCombatClip ChooseShredDemo(ETikiCombatClip Reaction) const;

	/** Replaces the body by the demo of the replicated reaction on this machine: hides the mob, spawns the demo (not on
	 *  a dedicated server); false when the demo cannot spawn here (the reaction clip plays instead) */
	bool StartLocalShredDemo();

	/** Starts the reaction of the replicated state on a client */
	UFUNCTION()
	void OnRep_CombatReaction();

	/** Server: picks the reaction to the blow being dealt (None: the behaviour without combat clips) */
	ETikiCombatClip ChooseCombatReaction(bool bLethal) const;

	/** Server: replicates a reaction and starts it here; bShredDemo: the mob is replaced by its demo of Clip (T-168) */
	void StartCombatReaction(ETikiCombatClip Clip, bool bShredDemo = false);

	/** Plays a reaction clip from its contact on this machine; a lethal one holds the ragdoll and the weapon drop */
	void PlayLocalCombatReaction(ETikiCombatClip Clip);

	/** End of a lethal reaction: the ragdoll takes over (every machine) */
	void FinishLethalReaction();

	/** Server: the limb of a lethal reaction comes off at its contact */
	void CutReactionLimb();

	/** Server: takes a bone off the body (living or dying) as a corpse piece; false when gone or unknown */
	bool DetachLimb(FName Bone, const FVector& Direction);

	/** Damage taken by each limb of the rules, same order as Rules->Limbs (server, T-167) */
	TArray<float> LimbDamage;

	/** Legs torn off (server, T-167) */
	int32 LegsLost = 0;

	/** Limbs cut off during the blow being dealt (server, T-167) */
	int32 BlowCuts = 0;

	/** Limbs the blow being dealt shreds off if it kills (close shotgun shot; server, T-167) */
	int32 BlowShredLimbs = 0;

	/** The lethal reaction's blow shredded the body already: its burst takes no other limb (server, T-167) */
	bool bReactionShredded = false;

	/** Server: the hits of a gun shot on the bones fill the health of their limbs, and the limbs past it are torn
	 *  off; a head torn off raises the damage to a lethal one (T-167) */
	void ApplyLimbHits(const FDamageEvent& DamageEvent, float& InOutDamage);

	/** Server: tears off a limb of the rules (an arm lets its weapon fall first); false when it is gone already */
	bool TearLimb(int32 Limb, const FVector& Direction);

	/** Server: limbs of the rules still on the body */
	bool IsLimbGone(int32 Limb) const;

	/** Limb of the rules a bone belongs to (the bone or an ancestor starts it), INDEX_NONE for the body */
	int32 LimbOfBone(FName Bone) const;

	/** Bone a plain point damage struck: its bone, else the one its trace passes nearest to (LimbHitRadius) */
	FName ResolveHitBone(const FHitResult& Hit) const;

	/** A limb of the rules still on the body, drawn at random, INDEX_NONE when none */
	int32 PickShredLimb() const;

	/** Lethal reaction started by the server, read at its contact (server) */
	ETikiCombatClip ReactionClip = ETikiCombatClip::None;

	/** Horizontal direction of the last blow (server) */
	FVector LastBlowDirection = FVector::ZeroVector;

	/** Timer of BecomeCorpse (server) */
	FTimerHandle CorpseTimer;

	/** Server, instead of its decisions once both arms are gone (owner 2026-10-09): runs from its nearest foe, round
	 *  and round without one, screaming now and then */
	void PanicThink(float DeltaSeconds);

	/** Where the panic runs to, when a new way is drawn, when it screams again (server) */
	FVector PanicGoal = FVector::ZeroVector;
	double NextPanicGoalTime = 0.0;
	double NextPanicScreamTime = 0.0;

	/** The waist bleeds at the contact of the bisected death (every machine) */
	void SpawnWaistBlood();

	/** Timer of that blood */
	FTimerHandle ReactionBloodTimer;

	/** World time of the next idle sound (every machine) */
	double NextIdleVoiceTime = 0.0;

	/** World time this creature listens again for the noises of the players (server, T-262) */
	double NextListenTime = 0.0;

	/** Crossfading animation instance of the body, none when a Blueprint replaced it */
	UTikiAnimInstance* GetTikiAnim() const;
};
