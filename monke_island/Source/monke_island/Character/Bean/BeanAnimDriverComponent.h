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
 *  lean, hat). In the water (T-017) the bean treads water or swims the breaststroke and the puppet floats it at the
 *  surface the clips are made for (UBeanPuppetComponent::SetWaterStance); without swim clips, and in ragdoll, the body
 *  goes back to the mannequin retarget.
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

	/** Plays a one shot of the whole body (pickup, emote...) on this machine, at PlayRate from StartTime (s of the clip;
	 *  the capture gestures of T-336 stretch their clip over the gesture and start a late one where the server is) */
	void PlayFullBody(UAnimSequence* Sequence, float PlayRate = 1.0f, float StartTime = 0.0f);

	/** Stops the one shot of the whole body if it is Sequence (a gesture cut short, T-336); nothing otherwise */
	void StopFullBody(const UAnimSequence* Sequence);

	/** An item in hand with this actor tag lets its carrier see the body's own arms in first person, as bare hands (the
	 *  capture items of T-336: the gestures of the body are seen) */
	static const FName BodyArmsInFirstPersonTag;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Whole body states, blended by weight */
	enum class EBodyState : uint8 { Ground, Carry, Air, Land, Boat, Dead, Swim, Count };

	/** Creates the hidden animated mesh once the puppet is found */
	bool InitSource();

	/** The library has a swim clip (A_Bean_Swim or A_Bean_TreadWater) */
	bool HasSwimClips() const;

	/** Water pose of this frame (BeanSwimAnim::IsInWaterPose) and the swimming speed */
	void UpdateWaterPose();

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

	/** In the water this frame (swimming, or deep water at the feet), and the speed through the water (cm/s) */
	bool bInWaterPose = false;
	float SwimSpeed = 0.0f;

	/** Clocks of the water clips: treading water in seconds, the stroke as a normalised phase */
	float TreadTime = 0.0f;
	float SwimPhase = 0.0f;

	/** Smoothed weight of the stroke against treading water */
	float StrokeAlpha = 0.0f;

	/** Smoothed weight of the float lift given to the puppet */
	float WaterAlpha = 0.0f;

	/** One shot of the whole body, its time in the clip and its play rate */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> FullBodyClip;
	float FullBodyTime = 0.0f;
	float FullBodyRate = 1.0f;

	/** Smoothed weight of the upper body layer */
	float UpperWeight = 0.0f;

	/** Weight given to the puppet (0 in ragdoll, and in the water when the library has no swim clip) */
	float BodyAlpha = 0.0f;

	bool bDriving = false;
	bool bBodyArmsShown = false;
	bool bPuppetHidesArmsDefault = true;
	int32 InitAttempts = 0;
};
