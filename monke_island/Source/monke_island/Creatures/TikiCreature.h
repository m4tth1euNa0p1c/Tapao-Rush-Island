// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TikiCreature.generated.h"

class ATikiDirector;
class UAnimSequenceBase;
class UIslanderHealthComponent;
class UMobHealthBarComponent;
class USoundAttenuation;
class USoundBase;
class UTikiAnimInstance;
class UTikiRules;

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
	Gesture
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

	/** Weapon one can pick up, thrown out of the hand at death (BP_Drop_*); none: the corpse keeps it in its hand. It
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

	/** Server: takes this bone off the living creature with what hangs from it (an arm and its weapon) and drops it
	 *  as a corpse piece; false when it is gone already or unknown */
	bool CutOffLimb(FName Bone, const FVector& Direction);

	/** True once the arm holding the weapon was cut off */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsMaimed() const;

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

	/** Plays one of the sounds at random, attached to the body, with the pitch and volume of the voice (every
	 *  machine; nothing on a dedicated server) */
	void PlayVoice(const TArray<TObjectPtr<USoundBase>>& Sounds) const;

	/** The body goes limp: its physics asset simulates, thrown along the last blow; false without a physics asset */
	bool StartRagdoll();

	/** Health of this creature from the rules */
	virtual float GetRulesHealth() const { return 100.0f; }

	/** Server: walks or runs toward a point; returns the horizontal distance left (cm) */
	float MoveToward(const FVector& Target, float Speed);

	/** Water depth above the ground at a location (cm), 0 on dry land */
	float WaterDepthAt(const FVector& Location) const;

	/** A swimmer, or a player standing in water deeper than a tiki wades: tikis do not follow it into the sea */
	bool IsOutOfReachInWater(const APawn* Pawn) const;

	/** Server: stops and turns toward a point */
	void FaceToward(const FVector& Target);

	/** Server: plays a one-shot clip on every machine (Variant picks among the variants of the action) */
	void PlayAction(ETikiAction Action, uint8 Variant = 0);

	/** Clip of this creature standing still (a warrior in a fight stands on guard) */
	virtual UAnimSequenceBase* GetIdleClip() const { return Anims.Idle; }

	/** True while a one-shot clip plays */
	bool IsActionPlaying() const;

	/** Pawn of a living player nearest to this creature within MaxDistance, or nullptr */
	APawn* FindNearestPlayer(float MaxDistance) const;

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

	/** Plays the action clip on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAction(ETikiAction Action, uint8 Variant);

	/** Starts a one-shot clip locally */
	void PlayLocalAction(ETikiAction Action, uint8 Variant);

	/** Picks the locomotion clip for the current speed (every machine) */
	void UpdateAnimation();

	/** Plays a clip on the body if it is not already playing */
	void PlayClip(UAnimSequenceBase* Clip, bool bLoop);

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

	/** Server: replaces the dead body by its corpse, or by two halves after a cut blow */
	void BecomeCorpse();

	/** The last blow was a cut (IsCutBlow), read when the body becomes a corpse (server) */
	bool bLastBlowCut = false;

	/** Debug: the next blow cuts (server) */
	bool bNextBlowCut = false;

	/** Horizontal direction of the last blow (server) */
	FVector LastBlowDirection = FVector::ZeroVector;

	/** Timer of BecomeCorpse (server) */
	FTimerHandle CorpseTimer;

	/** World time of the next idle sound (every machine) */
	double NextIdleVoiceTime = 0.0;

	/** Crossfading animation instance of the body, none when a Blueprint replaced it */
	UTikiAnimInstance* GetTikiAnim() const;
};
