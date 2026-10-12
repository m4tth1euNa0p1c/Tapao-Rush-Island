// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BeanFirstPersonArmsComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** Pose of one first person arm relative to the camera (X forward, Y right, Z up) */
USTRUCT(BlueprintType)
struct FBeanArmPose
{
	GENERATED_BODY()

	FBeanArmPose() = default;

	FBeanArmPose(const FVector& InLocation, const FRotator& InRotation, float InStretch = 1.0f)
		: Location(InLocation), Rotation(InRotation), Stretch(InStretch)
	{
	}

	/** Shoulder position relative to the camera (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	FVector Location = FVector::ZeroVector;

	/** Arm rotation relative to the camera (the arm mesh points along +X) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Length factor of the arm along +X (rubber arms reaching an item); 1 = mesh length */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bean", meta=(ClampMin="0.1"))
	float Stretch = 1.0f;
};

/**
 *  Content Warning style first person arms, seen by their owner only (cahier §10: local arms, body hidden).
 *  Two copies of SM_Bean_FPArm attached to the first person camera and animated procedurally from the
 *  player state: out of view at rest, jab on a punch, forearms up in guard, both reaching while holding an
 *  object, pumping while sprinting, a little sway when walking.
 */
UCLASS(ClassGroup=(Bean), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UBeanFirstPersonArmsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UBeanFirstPersonArmsComponent();

	/** Right arm mesh (pivot at the shoulder, pointing along +X); the left arm is mirrored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms")
	TObjectPtr<UStaticMesh> ArmMesh;

	/** Right arm out of view (left mirrored on Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Poses")
	FBeanArmPose RestPose = FBeanArmPose(FVector(-5.0, 24.0, -62.0), FRotator(40.0, -5.0, 0.0));

	/** Right arm pulled back before the punch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Poses")
	FBeanArmPose WindupPose = FBeanArmPose(FVector(-10.0, 22.0, -34.0), FRotator(30.0, -5.0, -10.0));

	/** Right arm extended at the impact, toward the centre of the view */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Poses")
	FBeanArmPose StrikePose = FBeanArmPose(FVector(10.0, 16.0, -25.0), FRotator(22.0, -14.0, 0.0));

	/** Right forearm raised in guard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Poses")
	FBeanArmPose GuardPose = FBeanArmPose(FVector(4.0, 18.0, -40.0), FRotator(50.0, -30.0, 20.0));

	/** Right arm reaching for the held object */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Poses")
	FBeanArmPose HoldPose = FBeanArmPose(FVector(0.0, 18.0, -34.0), FRotator(20.0, -10.0, 0.0));

	/** Right arm low in view while sprinting (pumps back and forth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Poses")
	FBeanArmPose SprintPose = FBeanArmPose(FVector(-8.0, 24.0, -42.0), FRotator(32.0, -5.0, 0.0));

	/** Arm pumping amplitude while sprinting (cm) and frequency (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Motion")
	float SprintPumpAmplitude = 9.0f;

	/** See SprintPumpAmplitude */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Motion")
	float SprintPumpFrequency = 2.6f;

	/** Speed of the pose blends (1/s); the strike snaps instantly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arms|Motion", meta=(ClampMin="0"))
	float BlendSpeed = 18.0f;

	/** Drives both arms from an equipped item (e.g. the musket): poses applied as given, every frame */
	void SetPoseOverride(const FBeanArmPose& Right, const FBeanArmPose& Left);

	/** Gives the arms back to the procedural poses */
	void ClearPoseOverride();

	/** True while an item drives the arms */
	bool HasPoseOverride() const { return bPoseOverride; }

	/** Hides both arms while an equipped item shows its own first person arms (the pirate pistol and shotgun packs) */
	void SetHiddenByItem(bool bInHidden) { bHiddenByItem = bInHidden; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;

private:

	/** Blends one arm toward its target pose */
	void UpdateArm(UStaticMeshComponent* Arm, const FBeanArmPose& Target, float Mirror, float Alpha, float DeltaTime);

	/** Right arm */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RightArm;

	/** Left arm (mirrored) */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LeftArm;

	/** Time accumulator for the sprint pumping */
	float PumpTime = 0.f;

	/** An item drives the arms */
	bool bPoseOverride = false;

	/** An item shows its own arms: these stay hidden */
	bool bHiddenByItem = false;

	/** Arm poses given by the item (the left one is mirrored like the procedural poses) */
	FBeanArmPose OverrideRight;
	FBeanArmPose OverrideLeft;
};
