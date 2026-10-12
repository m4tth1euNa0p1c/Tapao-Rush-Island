// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Creatures/GiantCrabTypes.h"
#include "GiantCrabFeedback.generated.h"

class AGiantCrab;
class UAnimSequenceBase;
class UCameraShakeBase;
class UGiantCrabBodyComponent;
class UGiantCrabRules;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USkeletalMeshComponent;

/**
 *  What the giant crab sounds and feels like on every machine that plays sound (T-261, owner 2026-10-09: "ajoute les
 *  bruits de pas des pattes du crabe géant", "rends le crabe ultra terrifiant"); nothing on a dedicated server, nothing
 *  replicated: each machine plays what it already sees.
 *  - Steps: the AN_Step_<bone> notifies of the clip being played (T-260), else the tip of each leg touching the sand
 *    (the height of its hit body over its lowest point, no trace), worked out only while a local camera is near, at
 *    most MaxStepsPerSecond; the hind legs heavier, with a small shake.
 *  - Moments of the actions (slam, stomp, belly slam, roar and its wave, charge, death: AGiantCrab::GetActionMoments)
 *    and the cue notifies of the clips (claw snaps, body falls): sound, camera shake of the local players around, sand.
 *  - Voice: a hiss now and then while it fights, a growl while it waits.
 *  - Frenzy: red eyes (material parameters, red lights at the eyes).
 */
UCLASS(ClassGroup=(Creatures))
class MONKE_ISLAND_API UGiantCrabFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UGiantCrabFeedbackComponent();

	/** Gets ready for the rules on the mesh (every machine but a dedicated server) */
	void Setup(UGiantCrabRules* InRules, USkeletalMeshComponent* InMesh, UGiantCrabBodyComponent* InBody);

	/** A new action of the crab began Age seconds ago on the server (0: now): its moments play from there (sorted by
	 *  time), those long past are dropped (T-305) */
	void OnActionStarted(const TArray<FGiantCrabMoment>& Moments, float Age = 0.0f);

	/** A clip starts at StartTime (or keeps playing when it is the same and !bRestart): the clock of its notifies
	 *  follows it, the notifies before StartTime are not played */
	void OnClipPlayed(UAnimSequenceBase* InClip, bool bLoop, float Rate, bool bRestart, float StartTime = 0.0f);

	/** The play rate of the clip changes (the walk follows the speed of the body) */
	void OnClipRate(float Rate);

	/** In a frenzy or not: the eyes */
	void SetFrenzy(bool bFrenzy);

	/** The crab died on this machine: its eyes go out, then cloud over DeadEyesDelay seconds later */
	void OnDeath(float DeadEyesDelay);

	/** Plays a cue at a place: its sound, the shake of the local cameras around, sand */
	void PlayCue(EGiantCrabCue Cue, const FVector& Location, float VolumeScale = 1.0f);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** One foot watched for its steps */
	struct FFoot
	{
		FName Bone;
		int32 BoneIndex = INDEX_NONE;
		bool bHeavy = false;
		/** Armed (lifted) */
		bool bLifted = false;
		/** Lowest height seen lately (cm above the mesh), rising slowly */
		float Ground = TNumericLimits<float>::Max();
		/** Tip from the bone (bone space, cm) */
		FVector TipOffset = FVector::ZeroVector;
	};

	/** A notify of the clip being played: a step of a foot, or a cue */
	struct FClipEvent
	{
		float Time = 0.0f;
		int32 Foot = INDEX_NONE;
		EGiantCrabCue Cue = EGiantCrabCue::None;
	};

	/** The steps by the height of the tips */
	void UpdateFeet(float DeltaTime);

	/** The clock of the clip and its notifies */
	void UpdateClipClock(float DeltaTime);

	/** A step of a foot at its tip */
	void PlayStep(int32 FootIndex, const FVector& Tip);

	/** Tip of a foot in the world (its hit body, else its bone) */
	FVector GetFootTip(const FFoot& Foot) const;

	/** Hiss or growl now and then */
	void UpdateVoice(float DeltaTime);

	/** Dynamic materials of the eyes, made the first time they are needed */
	const TArray<TObjectPtr<UMaterialInstanceDynamic>>& GetEyeMaterials();

	/** Removes the red lights of the eyes */
	void RemoveEyeLights();

	/** Shakes the cameras of the local players around Location */
	void ShakeLocalCameras(TSubclassOf<UCameraShakeBase> Shake, const FVector& Location, float Scale, float Inner, float Outer) const;

	/** The local camera nearest Location, and its distance (false without a local player) */
	bool NearestLocalCamera(const FVector& Location, float& OutDistance) const;

	/** The crab */
	UPROPERTY(Transient)
	TObjectPtr<AGiantCrab> Crab;

	UPROPERTY(Transient)
	TObjectPtr<UGiantCrabRules> Rules;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UGiantCrabBodyComponent> Body;

	/** Eye materials in a frenzy, and the ones they replaced (by slot) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FrenzyMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> CalmMaterials;

	TArray<int32> EyeSlots;

	/** Red lights at the eyes in a frenzy */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> EyeLights;

	bool bFrenzyShown = false;

	/** World time the eyes of the dead crab cloud over, negative when nothing waits */
	double DeadEyesTime = -1.0;

	TArray<FFoot> Feet;

	/** Clip being played and its clock */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Clip;

	float ClipTime = 0.0f;
	float ClipRate = 1.0f;
	float ClipLength = 0.0f;
	bool bClipLoop = false;
	bool bClipStarted = false;
	TArray<FClipEvent> ClipEvents;
	TArray<float> ClipEventTimes;
	bool bClipHasSteps = false;

	/** Moments of the action under way, how many played, time since it began */
	TArray<FGiantCrabMoment> Moments;
	int32 MomentsPlayed = 0;
	float ActionTime = 0.0f;

	/** A local camera is near enough to hear the steps (checked a few times a second) */
	bool bAudible = false;
	float AudibleCheck = 0.0f;

	/** World times: next step allowed, next step shake, next hiss or growl */
	double NextStepTime = 0.0;
	double NextStepShake = 0.0;
	double NextVoice = 0.0;
	bool bWasEngaged = false;

	TArray<int32> Crossed;
};
