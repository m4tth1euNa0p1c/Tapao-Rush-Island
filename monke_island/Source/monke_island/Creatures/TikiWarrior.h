// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiTacticsTypes.h"
#include "Stealth/StealthMath.h"
#include "TikiWarrior.generated.h"

class UTikiTactics;
struct FStreamableHandle;

/** Why a warrior is on the island (server) */
UENUM(BlueprintType)
enum class ETikiWarriorRole : uint8
{
	/** Came out at night, hunts the players nearby, goes back into the ground at dawn */
	Night,
	/** Called by a hit villager, hunts that player only */
	Defender,
	/** Designated threat of a village (cahier §6.2): waits by the bush it came out of, hunts the players who come
	 *  close, walks back to its post; stays at dawn and is not replaced once killed */
	Guardian,
	/** Lay in wait in a bush of a bush zone: leaps out when a player comes near, hunts the players nearby around its
	 *  bush and goes back into it when nobody is left */
	Ambusher
};

/**
 *  Tiki warrior (Guerrier Coco, Gardien Crabe, Veilleur des Cendres): leaps out of a bush, runs at its target and
 *  strikes in melee after a visible preparation (cahier §5). Its targets are the living foes of its side (players,
 *  fort soldiers, the giant crab: Creatures/Hostility, owner 2026-10-08), the nearest it sees first. Stealth (T-262):
 *  a crouched player, or any player at night, is seen only at contact; a hunted player out of sight is followed a
 *  moment, then looked for where it was seen last; a noise of a player (running steps, voice, shot) sends it there,
 *  walking, to look around, and it forgets after NoiseMemorySeconds (DA_StealthRules). It strikes and shoots only
 *  at a foe it sees. Server authoritative: the server moves it, sweeps the blow and applies the damage; every machine
 *  plays the clips.
 *  Roles (T-275): each mob fights in its own way (UTikiTactics, created on the server from its archetype): the Guerrier
 *  Coco stalks, the Gardien Crabe guards and seizes, the Veilleur des Cendres blinds from a distance and howls. A class
 *  without a role (Brawler) keeps the classic melee.
 */
UCLASS()
class MONKE_ISLAND_API ATikiWarrior : public ATikiCreature
{
	GENERATED_BODY()

	friend class UTikiTactics;

public:

	ATikiWarrior();

	/** Role of this warrior (T-275): Auto reads it from its class name (UTikiRules::Pack.Archetypes); a level instance
	 *  may force one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki|Tactics")
	ETikiArchetype Archetype = ETikiArchetype::Auto;

	/** Role it plays: Archetype, else the one of its class name (every machine) */
	ETikiArchetype GetArchetype() const;

	/** What its tactics are doing, replicated (T-275) */
	const FTikiTacticRep& GetTactic() const { return Tactic; }

	/** True while it attacks its foe: a blow under way, a charge, a heavy blow, a grab, a cast (the pack counts it) */
	bool IsCommitted() const;

	/** Server: a watcher's howl (T-275): unless it hunts a foe already, it runs to look where that foe was seen; it
	 *  attacks only if it then sees it */
	void HearHowl(const FVector& Where, APawn* Suspect);

	/** True when Pawn holds a lit torch in its hand; bOutRaised: held up (any machine) */
	static bool HoldsLitTorch(const APawn* Pawn, bool& bOutRaised);

	virtual void Tick(float DeltaSeconds) override;

	/** Server: the guard of its role may take a blow first (T-275); then the creature takes it */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** WarriorRole and first target, set by the director on spawn (server) */
	void InitWarrior(ETikiWarriorRole InRole, APawn* InTarget, const FVector& InLeapDirection);

	/** WarriorRole of this warrior (server) */
	ETikiWarriorRole GetRole() const { return WarriorRole; }

	/** Current target (server) */
	APawn* GetTarget() const { return Target.Get(); }

	/** Goes back into the ground (server): at dawn, or when no player is left nearby */
	void Burrow();

	/** Hunts this foe now (server): a guardian of an alarmed village */
	void Engage(APawn* InTarget);

	/** Place a guardian keeps when it has no target, and how far it wanders around it (server) */
	void SetPost(const FVector& InPost, float InRadius) { Post = InPost; PostRadius = InRadius; }

	/** Server, before it finishes spawning: it comes out with a musket (the armed tiki of the villages, T-214) */
	void SetArmed(bool bInArmed);

	/** It carries a musket */
	bool IsArmed() const { return bArmed; }

	/** Balls left in its pouch (server) */
	int32 GetShotsLeft() const { return ShotsLeft; }

	/** Server: goes to look where this noise was made (a monster leaping out of its bush at a noise, T-262) */
	void HearNoise(const FStealthNoise& Noise);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void ServerThink(float DeltaSeconds) override;

	virtual void OnDamaged(AActor* Causer, float Damage) override;

	virtual float GetRulesHealth() const override;

	virtual UAnimSequenceBase* GetIdleClip() const override;

	virtual void PlayLocalAction(ETikiAction Action, uint8 Variant) override;

	virtual UAnimSequenceBase* GetDirectionalClip(float Speed, float& OutClipSpeed) const override;

	virtual bool IsLocomotionClip(const UAnimSequenceBase* Clip) const override;

	virtual bool CanSpot(const AActor* Seen, float Range) const override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: sweeps in front and hits the foes there (the classic blow) */
	void StrikeNow();

	/** Server: sweeps Reach in front, Radius wide, and hits the foes there with Damage and a push of Knockback; the foes
	 *  struck (T-275: the heavy blow of the Coco) */
	int32 StrikeWith(float Reach, float Radius, float Damage, float Knockback);

	/** Server: starts the classic blow at Victim: its preparation, then StrikeNow */
	void BeginStrike(APawn* Victim, double Now);

	/** True when the classic blow may start now (its cooldown is over) */
	bool CanStartStrike(double Now) const;

	/** Server: replicates what the tactics do; the body turns to its movement unless it faces its foe (bStrafe) */
	void SetTacticRep(const FTikiTacticRep& Rep);

	/** Server: creates the tactics of its role (none for a Brawler) */
	void CreateTactics();

	/** Every machine: a cue of the tactics at a place (TikiWarriorCue: a blow blocked by the guard, the claw closing) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTacticCue(uint8 Cue, FVector_NetQuantize Location);

	/** Plays one of Sounds (none: nothing) attached to the body, or at Location when given, with the role's
	 *  attenuation (bFar: the howl's); nothing on a dedicated server */
	void PlayTacticSound(const TArray<TObjectPtr<USoundBase>>& Sounds, bool bFar = false, const FVector* Location = nullptr) const;

	/** Clips and sounds of its role, none for a Brawler or without rules */
	const FTikiTacticClips* GetTacticClips() const;
	const FTikiTacticSounds* GetTacticSounds() const;

	/** A clip of its role when it can play on this body, else nullptr */
	UAnimSequenceBase* TacticClip(UAnimSequenceBase* Clip) const;

	/** True when the flame of a lit torch shows Seen to it at night within Range (T-275) */
	bool IsRevealedByTorch(const AActor* Seen, float Range) const;

	/** Loads the effects of its role in the background (sparks of the guard, cloud and embers of the ash) */
	void PreloadTacticEffects();

	/** Tactics of its role (server; none for a Brawler and on the clients) */
	UPROPERTY(Transient)
	TObjectPtr<UTikiTactics> Tactics;

	/** What its tactics are doing (replicated: clips, guard pose, stalk sounds) */
	UPROPERTY(Replicated)
	FTikiTacticRep Tactic;

	/** Role read from the class name once (every machine) */
	mutable ETikiArchetype ResolvedArchetype = ETikiArchetype::Auto;

	/** World time of the next stalk sound (every machine) */
	double NextStalkSoundTime = 0.0;

	/** Background load of the effects of its role */
	TSharedPtr<FStreamableHandle> EffectsLoad;

	/** Server: the shot at the end of the aim; the ball hits by the chance of the distance, else passes beside */
	void ShootNow();

	/** The musket in its hand on this machine, once it is armed */
	void ApplyArmed();

	UFUNCTION()
	void OnRep_Armed();

	/** Every machine: the report and the flame of a shot */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMusketShot(FVector_NetQuantize Muzzle, FVector_NetQuantizeNormal Direction);

	/** Muzzle of its musket (in front of it without one) */
	FVector GetMuzzle() const;

	/** Nothing solid between its musket and the target's chest */
	bool HasLineOfShot(const APawn* Victim) const;

	/** Its target is where it is now (spotted, a blow, an alarm): seen there at this moment (server, T-262) */
	void MarkSeen(const AActor* Foe);

	/** Keeps the target while it perceives it: seen (bSeesTarget), heard (its noises), or within the grace after it
	 *  went out of sight; updates where it was last; false once lost (server, T-262) */
	bool TrackTarget(APawn* Victim, double Now);

	/** True when a place is worth looking at: anywhere, or within LoseRadius of the post of a guardian or an ambusher */
	bool IsInReachOfPost(const FVector& Location) const;

	/** Goes to the lead (running after a foe it hunted, walking to a noise) and looks around there (server, T-262) */
	void Investigate(double Now);

	/** It came out with a musket (replicated: the musket in the hand) */
	UPROPERTY(ReplicatedUsing=OnRep_Armed)
	bool bArmed = false;

	/** Balls left (server) */
	int32 ShotsLeft = 0;

	/** World time the aimed shot goes off, 0 when no aim is under way (server) */
	double PendingShotTime = 0.0;

	/** World time its musket is loaded again (server) */
	double ReloadedTime = 0.0;

	/** Musket in its hand, and the flame of its shots */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MusketMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MusketFlash;

	FTimerHandle FlashTimer;

	/** WarriorRole of this warrior */
	ETikiWarriorRole WarriorRole = ETikiWarriorRole::Night;

	/** Foe hunted: a player, a fort soldier, the giant crab */
	TWeakObjectPtr<APawn> Target;

	/** Leap out of the bush, applied on the first think */
	FVector LeapDirection = FVector::ZeroVector;

	/** World time of the last blow start */
	double LastAttackTime = -1.0e6;

	/** World time the blow lands, 0 when no blow is pending */
	double PendingStrikeTime = 0.0;

	/** World time the warrior last had a target, for the burrow of an idle defender */
	double LastTargetTime = 0.0;

	/** Wander point of an idle warrior */
	FVector IdleGoal = FVector::ZeroVector;

	/** Post of a guardian (where it came out of its bush) */
	FVector Post = FVector::ZeroVector;

	/** Wander radius around the post, 0 when the warrior has no post */
	float PostRadius = 0.0f;

	/** The warrior is going back into the ground */
	bool bBurrowing = false;

	/** World time of its last growl at a player it spotted */
	double LastRoarTime = -1.0e6;

	/** World time of its last taunt */
	double LastTauntTime = -1.0e6;

	/** It has a target: it stands on guard (replicated for the clip) */
	UPROPERTY(Replicated)
	bool bInCombat = false;

	/** Where and when it last saw (or heard) its target (server, T-262) */
	FVector TargetSeenAt = FVector::ZeroVector;
	double TargetSeenTime = -1.0e6;

	/** It sees its target at this think (server, T-262) */
	bool bSeesTarget = false;

	/** What it looks for without a target: a foe it lost, a noise it heard (server, T-262) */
	FStealthLead Lead;

	/** World time it turns its head again while it searches (server) */
	double NextSearchTurnTime = 0.0;
};
