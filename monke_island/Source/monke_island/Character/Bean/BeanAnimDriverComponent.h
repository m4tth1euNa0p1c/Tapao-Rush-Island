// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeanAnimDriverComponent.generated.h"

class UAnimSequence;
class UBeanAnimLibrary;
class UBeanFirstPersonArmsComponent;
class UBeanLibraryAnimInstance;
class UBeanPuppetComponent;
class USkeletalMeshComponent;
struct FBeanAnimSample;

/**
 *  Pulls the bean's hands onto world targets after the puppet has its pose (two bone IK on both arms): the rower's
 *  mittens on the oar handles. Created by UBeanAnimDriverComponent, ticks after the puppet.
 */
UCLASS(ClassGroup=(Bean))
class MONKE_ISLAND_API UBeanHandIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBeanHandIKComponent();

	/** Targets of the grips (world) and the weight of the pull; Alpha 0 releases the hands */
	void SetTargets(const FVector& Right, const FVector& Left, float InAlpha, float InGripReachCm);

	/** Puppet whose arms are bent */
	void SetPuppet(UBeanPuppetComponent* InPuppet);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	/** Bends one arm so its grip reaches Target (puppet component space) */
	void SolveArm(FName UpperBone, FName LowerBone, FName HandBone, const FVector& TargetCS, const FVector& Pole);

	TWeakObjectPtr<UBeanPuppetComponent> Puppet;
	FVector RightTarget = FVector::ZeroVector;
	FVector LeftTarget = FVector::ZeroVector;
	float Alpha = 0.0f;
	float GripReachCm = 4.0f;
};

/**
 *  Plays the Bean animation library on the player (T-078). A hidden SK_Bean_Body runs UBeanLibraryAnimInstance; this
 *  component decides its pose every frame from the character state and hands it to the puppet as base pose
 *  (UBeanPuppetComponent::SetBasePoseSource): locomotion by speed, jump, carry, boat, death, hit and pickup one
 *  shots, and the sabre on the upper body. The puppet keeps its procedural layers on top (musket override, punch,
 *  lean, hat). Ragdoll and swimming give the body back to the mannequin retarget (no clip for them).
 *  Cosmetic: every machine computes it from replicated state.
 */
UCLASS(ClassGroup=(Bean), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UBeanAnimDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBeanAnimDriverComponent();

	/** Clips and tuning (DA_BeanAnimLibrary) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Anim")
	TObjectPtr<UBeanAnimLibrary> Library;

	/** First person: the owner sees the body's own animated arms (and the sabre in its hand) instead of the
	 *  first person arm meshes, while the hands are free or hold the sabre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Anim")
	bool bBodyArmsInFirstPerson = true;

	/** True while the library drives the body */
	UFUNCTION(BlueprintPure, Category="Bean|Anim")
	bool IsDrivingBody() const { return bDriving; }

	/** True while the owner sees the body arms in first person (the sabre then stays in the body's hand) */
	UFUNCTION(BlueprintPure, Category="Bean|Anim")
	bool ShowsBodyArmsInFirstPerson() const { return bBodyArmsShown; }

	/** Plays a one shot of the whole body (pickup, emote...) on this machine */
	void PlayFullBody(UAnimSequence* Sequence);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Whole body states, blended by weight */
	enum class EBodyState : uint8 { Ground, Carry, Air, Land, Boat, Dead, Count };

	/** Creates the hidden animated mesh once the puppet is found */
	bool InitSource();

	/** Picks the body state of this frame */
	EBodyState PickState(float DeltaTime);

	/** Clips of a state at this frame, weights summing to 1 */
	void StateSamples(EBodyState State, TArray<FBeanAnimSample>& Out) const;

	/** Upper body clips (the sabre) and their target weight */
	float UpperSamples(TArray<FBeanAnimSample>& Out) const;

	/** First person arms: body arms or arm meshes */
	void UpdateFirstPersonArms();

	/** Hit react on damage */
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> AnimSource;

	UPROPERTY(Transient)
	TObjectPtr<UBeanLibraryAnimInstance> AnimInstance;

	/** Hands on the oars while rowing */
	UPROPERTY(Transient)
	TObjectPtr<UBeanHandIKComponent> HandIK;

	/** Rowing: the oars of the boat follow the clip, the hands go on their handles */
	void UpdateRowing(EBodyState State);

	TWeakObjectPtr<UBeanPuppetComponent> Puppet;
	TWeakObjectPtr<UBeanFirstPersonArmsComponent> FirstPersonArms;

	float StateWeights[static_cast<int32>(EBodyState::Count)] = {};
	float StateTimes[static_cast<int32>(EBodyState::Count)] = {};
	EBodyState LastState = EBodyState::Ground;

	/** Normalised phase shared by the walk and run cycles, so they blend in step */
	float GroundPhase = 0.0f;
	float IdleTime = 0.0f;
	float CarryTime = 0.0f;
	float Speed = 0.0f;
	float AirTime = 0.0f;
	bool bWasFalling = false;
	bool bWasHolding = false;
	float BoatSpeed = 0.0f;

	/** One shot of the whole body */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> FullBodyClip;
	float FullBodyTime = 0.0f;

	/** Smoothed weight of the upper body layer */
	float UpperWeight = 0.0f;

	/** Weight given to the puppet (0 in ragdoll and in the water) */
	float BodyAlpha = 0.0f;

	bool bDriving = false;
	bool bBodyArmsShown = false;
	bool bPuppetHidesArmsDefault = true;
	int32 InitAttempts = 0;
};
