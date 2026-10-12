// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "BeanPuppetComponent.generated.h"

class UMaterialInterface;

class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class ABeanDroppedHat;
class APawn;

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

	/** Body lean into the acceleration, as a fraction of the balance angle atan(a / g) a real body takes (0 = upright) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Motion", meta=(ClampMin="0", ClampMax="1"))
	float LeanFactor = 0.45f;

	/** Largest lean of the body (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Motion", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float MaxLeanDegrees = 14.0f;

	/** Spring of the lean (1/s^2): how fast the body follows the acceleration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Motion", meta=(ClampMin="1"))
	float LeanStiffness = 90.0f;

	/** Damping of the lean (1/s): under 2 * sqrt(LeanStiffness) the bean wobbles a little when it stops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Motion", meta=(ClampMin="0"))
	float LeanDamping = 13.0f;

	/** Time over which the measured acceleration is smoothed (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Motion", meta=(ClampMin="0.01", Units="s"))
	float AccelerationSmoothing = 0.08f;

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

	/** Slots of the body hidden from its owner in first person: the hair and the moustache ride the head bone and
	 *  came in front of the camera when the head moved (lifting an object, owner report 2026-10-07); on the owner's
	 *  machine only they take FirstPersonHiddenMaterial, the other players still see them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Camera")
	TArray<FName> FirstPersonHiddenSlots = { TEXT("M_Bean_Hair"), TEXT("M_Bean_Moustache") };

	/** Fully see-through material for those slots (masked, opacity 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Camera")
	TSoftObjectPtr<UMaterialInterface> FirstPersonHiddenMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Materials/M_Bean_Hidden.M_Bean_Hidden")));

	/** The hat falls off when the bean is knocked out (ragdoll) or takes damage (punch, fall) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat")
	bool bHatFallsOff = true;

	/** Upward kick given to a falling hat (cm/s), added to the body velocity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0"))
	float HatDropKick = 280.0f;

	/** Maximum distance from the feet for the explicit hat interaction (F), in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0"))
	float HatPickupRadius = 90.0f;

	/** Delay after the fall before the hat can be picked up again (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0", Units="s"))
	float HatPickupDelay = 1.0f;

	/** Legacy serialized setting, ignored: a dropped hat only returns through interaction */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use F to pick up the hat; automatic return is disabled."))
	float HatReturnDelay = 5.0f;

	/** Legacy serialized setting, ignored: distance never respawns the hat */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Dropped hats stay in the world until picked up."))
	float HatLostDistance = 3000.0f;

	/** Duration of the pop when the hat comes back on the head (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bean|Hat", meta=(ClampMin="0", Units="s"))
	float HatPopTime = 0.25f;

	/** Drops one replicated physical hat (server only) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bean|Hat")
	void DropHat(FVector Velocity);

	/** Equips this component's dropped hat after the server validates an explicit interaction */
	bool TryPickUpHat(ABeanDroppedHat* Hat, APawn* User);

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

	/** Drives Bones from Source, a hidden component playing an animation of the same skeleton (an item's carry,
	 *  shot or reload), in local space over the copied pose: the other bones (legs) keep the walk.
	 *  Call every frame; Alpha 0 or a null Source gives the bones back. */
	void SetBoneOverrideSource(USkeletalMeshComponent* Source, const TArray<FName>& Bones, float Alpha);

	/** Takes the base pose from Source, a hidden component of the same skeleton playing the bean's own animations
	 *  (UBeanAnimDriverComponent): every bone's local rotation and the pelvis position, blended by Alpha, in place
	 *  of the mannequin retarget. Punch / guard arms, the bone override source (musket), the first person arm
	 *  folding, the lean and the hat still apply on top. Null Source or Alpha 0 = mannequin retarget. */
	void SetBasePoseSource(USkeletalMeshComponent* Source, float Alpha);

	/** Water stance of this frame, given by the bean's own animations (UBeanAnimDriverComponent, T-017): the bean floats
	 *  with its root RootDepthCm under the water surface (waves included), at most MaxLiftCm above its dry placement,
	 *  blended by Alpha. Call every frame; without a call this frame or the last, the bean is raised by SwimLift while
	 *  swimming. */
	void SetWaterStance(float Alpha, float RootDepthCm, float MaxLiftCm);

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

	/** Resolves bones, caches reference poses and hooks the tick after the source animation */
	bool InitPuppet();

	/** Spawns the outfit and hat components */
	void SpawnSkinParts();

	/** Copies the current source pose onto the bean skeleton */
	void CopyPose();

	/** Blends the stance (crouch squash, swim lift) and the punch / guard arm overrides */
	void UpdateStance(float DeltaTime);

	/** Shows the worn hat only while equipped, including for late joining clients */
	UFUNCTION()
	void OnRep_HatDropped();

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

	/** Applies the bone override source (see SetBoneOverrideSource) on BoneSpaceTransforms and TargetCS */
	void ApplyBoneOverrideSource();

	/** Applies the base pose source (see SetBasePoseSource) on BoneSpaceTransforms and TargetCS */
	void ApplyBasePoseSource();

	/** Rebuilds TargetCS from BoneSpaceTransforms (parents come first in the reference skeleton) */
	void RebuildComponentSpace();

	/** Animated component giving the base pose (see SetBasePoseSource) */
	TWeakObjectPtr<USkeletalMeshComponent> BasePoseSource;

	/** Weight of the base pose source */
	float BasePoseAlpha = 0.f;

	/** Water stance of the bean's animations (see SetWaterStance) and the frame it was given on (0: never) */
	float WaterStanceAlpha = 0.f;
	float WaterStanceRootDepth = 0.f;
	float WaterStanceMaxLift = 0.f;
	uint64 WaterStanceFrame = 0;

	/** Lift of the water stance this frame (cm, in the parent's space): the root floats WaterStanceRootDepth under the
	 *  water surface */
	float WaterStanceLift() const;

	/** Animated component driving some bones (see SetBoneOverrideSource) */
	TWeakObjectPtr<USkeletalMeshComponent> BoneOverrideSource;

	/** Bean bone indices driven by BoneOverrideSource */
	TArray<int32> BoneOverrideIndices;

	/** Names BoneOverrideIndices were resolved from */
	TArray<FName> BoneOverrideNames;

	/** Weight of BoneOverrideSource */
	float BoneOverrideAlpha = 0.f;

	/** Per bean bone: aimed direction override (mesh space) and its weight this frame */
	TArray<FVector> OverrideDir;
	TArray<float> OverrideAlpha;

	int32 HatIndex = INDEX_NONE;

	/** Equipment state is authoritative even if the dropped actor becomes irrelevant or is destroyed */
	UPROPERTY(ReplicatedUsing=OnRep_HatDropped)
	bool bHatDropped = false;

	/** The only hat this component may equip, owned and checked by the server */
	UPROPERTY(Transient)
	TObjectPtr<ABeanDroppedHat> DroppedHat;

	bool bWasRagdoll = false;
	float HatPopAlpha = 1.f;
	FTransform HatRelativeTransform = FTransform::Identity;
	float GrabAlpha = 0.f;
	bool bBodyHiddenForOwner = false;

	/** The hair and moustache slots are see-through on this machine (owner in first person) */
	bool bHeadSlotsHidden = false;

	/** Materials those slots had before (nullptr: the mesh's own), put back when the view changes */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMaterialInterface>> HeadSlotMaterials;

	/** Swaps the hair and moustache slots for the see-through material, or puts their materials back */
	void SetHeadSlotsHidden(bool bHide);
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

	/** Body lean (world, a tilt vector: direction and angle in radians), its speed, and the velocity and
	 *  acceleration measured last frame */
	FVector2D Lean = FVector2D::ZeroVector;
	FVector2D LeanSpeed = FVector2D::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	FVector2D SmoothedAcceleration = FVector2D::ZeroVector;
	bool bHasLastVelocity = false;

	/** Leans the body into the acceleration and the turns (damped spring) */
	void UpdateLean(float DeltaTime);

	/** Placement of this component when standing on dry land */
	FQuat BaseRelativeRotation = FQuat::Identity;
	FVector BaseRelativeLocation = FVector::ZeroVector;
	FVector BaseRelativeScale = FVector::OneVector;

	TArray<FTransform> SourceRefCS;
	TArray<FTransform> TargetRefCS;
	TArray<FTransform> TargetCS;
	int32 PelvisIndex = INDEX_NONE;
	float PelvisScale = 1.f;
	bool bReady = false;
};
