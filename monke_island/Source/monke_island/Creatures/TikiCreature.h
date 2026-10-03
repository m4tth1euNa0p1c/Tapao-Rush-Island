// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TikiCreature.generated.h"

class ATikiDirector;
class UAnimSequenceBase;
class UIslanderHealthComponent;
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
	Emerge
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
};

/**
 *  Base of the tiki creatures (villagers and warriors): a character moved by its own server logic (no NavMesh on
 *  the 1:1 atoll), health from UIslanderHealthComponent, clips played in single node mode from the replicated
 *  movement and one-shot actions. Server authoritative; clients only pick the clips.
 */
UCLASS(Abstract)
class MONKE_ISLAND_API ATikiCreature : public ACharacter
{
	GENERATED_BODY()

public:

	ATikiCreature();

	/** Rules shared by the creatures (DA_TikiRules); the director sets them on spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TObjectPtr<UTikiRules> Rules;

	/** Clips of this creature */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tiki")
	FTikiAnimSet Anims;

	/** Returns the health component */
	UFUNCTION(BlueprintPure, Category="Tiki")
	UIslanderHealthComponent* GetHealth() const { return Health; }

	/** True once dead */
	UFUNCTION(BlueprintPure, Category="Tiki")
	bool IsDead() const;

	/** Director that spawned this creature (server) */
	void SetDirector(ATikiDirector* InDirector);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

	/** Server decisions, every tick while alive */
	virtual void ServerThink(float DeltaSeconds) {}

	/** Server: the creature took damage from Causer (a player pawn, usually) */
	virtual void OnDamaged(AActor* Causer, float Damage) {}

	/** Health of this creature from the rules */
	virtual float GetRulesHealth() const { return 100.0f; }

	/** Server: walks or runs toward a point; returns the horizontal distance left (cm) */
	float MoveToward(const FVector& Target, float Speed);

	/** Server: stops and turns toward a point */
	void FaceToward(const FVector& Target);

	/** Server: plays a one-shot clip on every machine */
	void PlayAction(ETikiAction Action);

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

	/** Item held in the hand (weapon, basket), follows the body pose */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> HeldItem;

	/** Routes engine damage to OnDamaged (server) */
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** Death on every machine */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** Plays the action clip on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAction(ETikiAction Action);

	/** Starts a one-shot clip locally */
	void PlayLocalAction(ETikiAction Action);

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
};
