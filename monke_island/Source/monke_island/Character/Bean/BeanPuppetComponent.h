// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "BeanPuppetComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One source bone driving one bean bone */
USTRUCT(BlueprintType)
struct FBeanBoneMapping
{
	GENERATED_BODY()

	/** Bone of the animated source mesh (mannequin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	FName SourceBone;

	/** Bone of the bean skeleton that copies it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	FName TargetBone;

	/** Limb bone: aim the bean bone along the source bone direction (towards SourceChild / TargetChild) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	FName SourceChild;

	/** Child of TargetBone giving its rest direction (with SourceChild) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	FName TargetChild;

	/** Extra outward swing applied to the aimed direction, in degrees (+ = away from the body), for short bean arms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	float SpreadDegrees = 0.f;

	/** Fraction of the source rotation change copied (a bean is one stiff body: the trunk barely bends) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean", meta=(ClampMin="0", ClampMax="1"))
	float RotationScale = 1.f;
};

/**
 *  Bean "skin" for a character animated with another skeleton (BP_Islander and its mannequin animations).
 *  Each frame it copies the pose of the owner's animated mesh onto the bean skeleton: rotation deltas from the
 *  reference pose for the trunk and head, aimed directions for the limbs, scaled pelvis bob. The source mesh is
 *  hidden but keeps animating (and ragdolling), so every existing movement feature keeps working.
 *  It also spawns the outfit (leader pose on this component) and the hat (attached to HatBone).
 *  Stopgap until the bean has its own AnimBP and physical animation (T-026, T-027).
 */
UCLASS(ClassGroup=(Bean), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UBeanPuppetComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:

	UBeanPuppetComponent(const FObjectInitializer& ObjectInitializer);

	/** Outfit drawn over the body, sharing its skeleton (skin slot "Outfit") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Skin")
	TObjectPtr<USkeletalMesh> OutfitMesh;

	/** Hat attached to HatBone, pivot at the head contact point (skin slot "Hat") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Skin")
	TObjectPtr<UStaticMesh> HatMesh;

	/** Bean bone carrying the hat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Skin")
	FName HatBone = TEXT("hat");

	/** Source bone to bean bone table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Pose")
	TArray<FBeanBoneMapping> BoneMappings;

	/** Bean bone receiving the scaled translation of the source pelvis */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Pose")
	FName PelvisBone = TEXT("pelvis");

	/** Scale of the source pelvis translation; 0 = bean pelvis height / source pelvis height */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Pose", meta=(ClampMin="0"))
	float PelvisTranslationScale = 0.f;

	/** Hide the source mesh (it keeps animating) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Pose")
	bool bHideSourceMesh = true;

	/** Scale of the bean while crouched (Fall Guys squash) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Stance")
	FVector CrouchScale = FVector(1.12, 1.12, 0.72);

	/** Height the bean is raised while swimming so its head stays above the water (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Stance")
	float SwimLift = 60.0f;

	/** Eyes of the bean in mesh space (+Y forward): the owner's first person camera moves here, just in front of the visor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Camera")
	FVector FirstPersonEyeLocation = FVector(0.0, 27.0, 82.0);

	/** Hide the hat from its owner in first person (the brim would cover the view) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Camera")
	bool bHideHatInFirstPerson = true;

	/** Hide the whole body from its owner in first person, shadow kept (cahier §10). Off by owner choice (2026-10-03):
	 *  looking down shows the costume; the arms fold away instead (bHideArmsForOwnerInFirstPerson) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Camera")
	bool bHideBodyInFirstPerson = false;

	/** First person with the body visible: fold the body's arms away on the owner's machine only (the first person
	 *  arms replace them; the other players still see them) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Camera")
	bool bHideArmsForOwnerInFirstPerson = true;

	/** The hat falls off when the bean is knocked out (ragdoll) or takes damage (punch, fall) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat")
	bool bHatFallsOff = true;

	/** Upward kick given to a falling hat (cm/s), added to the body velocity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0"))
	float HatDropKick = 280.0f;

	/** The bean picks its hat back up when its feet come this close (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0"))
	float HatPickupRadius = 90.0f;

	/** Delay after the fall before the hat can be picked up again (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0", Units="s"))
	float HatPickupDelay = 1.0f;

	/** Drops the hat now (every machine plays it locally, the hat is cosmetic) */
	UFUNCTION(BlueprintCallable, Category="Bean|Hat")
	void DropHat(FVector Velocity);

	/** True while the hat lies on the ground */
	UFUNCTION(BlueprintPure, Category="Bean|Hat")
	bool IsHatDropped() const { return bHatDropped; }

	/** How much the hat stays level instead of following the head tilt (0 follows, 1 always level) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Skin", meta=(ClampMin="0", ClampMax="1"))
	float HatLevelBlend = 0.6f;

	/** Speed of the stance and arm blends (1/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Stance", meta=(ClampMin="0"))
	float BlendSpeed = 16.0f;

	/** Right upper / lower arm directions while winding up a punch (mesh space: +Y forward, +Z up, -X right) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Arms")
	FVector PunchWindupUpperArm = FVector(-0.5, -0.5, -0.1);

	/** See PunchWindupUpperArm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Arms")
	FVector PunchWindupLowerArm = FVector(-0.1, 0.6, 0.6);

	/** Right arm directions when the punch lands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Arms")
	FVector PunchStrikeUpperArm = FVector(-0.5, 0.86, 0.12);

	/** See PunchStrikeUpperArm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Arms")
	FVector PunchStrikeLowerArm = FVector(-0.2, 1.0, 0.08);

	/** Right arm directions in guard, boxer style beside the visor (mirrored for the left arm); short bean arms
	 *  crossed in front would sink into the coat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Arms")
	FVector GuardUpperArm = FVector(-0.85, 0.45, 0.05);

	/** See GuardUpperArm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Arms")
	FVector GuardLowerArm = FVector(-0.25, 0.35, 0.95);

	/** Returns the outfit component, if any */
	UFUNCTION(BlueprintPure, Category="Bean|Skin")
	USkeletalMeshComponent* GetOutfitComponent() const { return OutfitComponent; }

	/** Returns the hat component, if any */
	UFUNCTION(BlueprintPure, Category="Bean|Skin")
	UStaticMeshComponent* GetHatComponent() const { return HatComponent; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	/** Resolves bones, caches reference poses and hooks the tick after the source animation */
	bool InitPuppet();

	/** Spawns the outfit and hat components */
	void SpawnSkinParts();

	/** Copies the current source pose onto the bean skeleton */
	void CopyPose();

	/** Blends the stance (crouch squash, swim lift) and the punch / guard arm overrides */
	void UpdateStance(float DeltaTime);

	/** Puts the hat back on the head */
	void PickUpHat();

	/** Hat drop on damage (bound to the owner's health) */
	UFUNCTION()
	void HandleOwnerHealthChanged(float NewHealth, float Delta);

	/** Sets the arm overrides of one side for this frame */
	void SetArmOverride(int32 UpperIndex, int32 LowerIndex, const FVector& Upper, const FVector& Lower, float Alpha, float MirrorX);

	struct FResolvedMapping
	{
		float RotationScale = 1.f;
		int32 Source = INDEX_NONE;
		int32 SourceChild = INDEX_NONE;
		int32 TargetChild = INDEX_NONE;
		float SpreadRadians = 0.f;
		float Side = 0.f;
	};

	/** Animated mesh of the owning character */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SourceMesh;

	/** Outfit following this component's pose */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> OutfitComponent;

	/** Hat attached to HatBone */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HatComponent;

	/** Mapping per bean bone index (Source == INDEX_NONE: follows its parent) */
	TArray<FResolvedMapping> Resolved;

	/** Combat state of the owner (punch / guard), if any */
	UPROPERTY(Transient)
	TObjectPtr<class UIslanderCombatComponent> OwnerCombat;

	/** Per bean bone: aimed direction override (mesh space) and its weight this frame */
	TArray<FVector> OverrideDir;
	TArray<float> OverrideAlpha;

	int32 HatIndex = INDEX_NONE;
	bool bHatDropped = false;
	bool bWasRagdoll = false;
	double HatDropTime = 0.0;
	FTransform HatRelativeTransform = FTransform::Identity;
	float GrabAlpha = 0.f;
	bool bBodyHiddenForOwner = false;
	int32 UpperArmL = INDEX_NONE;
	int32 LowerArmL = INDEX_NONE;
	int32 UpperArmR = INDEX_NONE;
	int32 LowerArmR = INDEX_NONE;
	int32 HandL = INDEX_NONE;
	int32 HandR = INDEX_NONE;

	/** Smoothed blend weights */
	float WindupAlpha = 0.f;
	float StrikeAlpha = 0.f;
	float GuardAlpha = 0.f;
	float CrouchAlpha = 0.f;
	float SwimAlpha = 0.f;

	/** Placement of this component when standing on dry land */
	FVector BaseRelativeLocation = FVector::ZeroVector;
	FVector BaseRelativeScale = FVector::OneVector;

	TArray<FTransform> SourceRefCS;
	TArray<FTransform> TargetRefCS;
	TArray<FTransform> TargetCS;
	int32 PelvisIndex = INDEX_NONE;
	float PelvisScale = 1.f;
	bool bReady = false;
};
