// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "BandageRules.generated.h"

class UAnimSequence;
class USoundBase;
class UStaticMesh;

/** Why the server refuses to start a bandage (the carrier is told; nothing else happens) */
UENUM(BlueprintType)
enum class EBandageRefusal : uint8
{
	None,
	/** No DA_BandageRules on the item */
	NoRules,
	/** The bandage is not the item in hand */
	NotInHand,
	/** The item is in no inventory of its carrier (a stray actor, someone else's request) */
	NotCarried,
	/** Dead, knocked down, in the air or in the water */
	CarrierUnable,
	/** Running: the bandage needs both hands and a steady body */
	Sprinting,
	/** Health is already full */
	FullHealth,
	/** A bandage is already being applied */
	Busy
};

/** What cut a bandage being applied */
UENUM(BlueprintType)
enum class EBandageInterrupt : uint8
{
	None,
	/** The carrier took a blow */
	Hit,
	/** The carrier ran */
	Sprint,
	/** The carrier jumped or fell */
	Jump,
	/** The carrier went into deep water */
	Water,
	/** The bandage was put away or dropped */
	PutAway,
	/** The carrier died or went ragdoll */
	Down
};

/** One key of the first person arms during the application (clip time), both arms written as right arm poses (the left
 *  one is mirrored by UBeanFirstPersonArmsComponent, as its procedural poses) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FBandageArmKey
{
	GENERATED_BODY()

	/** Time of the key in the clip A_Bean_Bandage_Apply (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bandage", meta=(ClampMin="0", Units="s"))
	float TimeSeconds = 0.0f;

	/** Right arm (holds the roll) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bandage")
	FBeanArmPose Right;

	/** Left arm (the forearm being wrapped), mirrored by the arms component */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bandage")
	FBeanArmPose Left;
};

/**
 *  Healing bandages (T-340, owner request 2026-10-11, docs/features/bandages.md): a stack of linen rolls in the inventory;
 *  F or a left click applies one (A_Bean_Bandage_Apply, about 3 s), a blow, a sprint, a jump, deep water or putting it
 *  away cuts it; at the BandageDone moment of the clip the carrier's health goes back to its maximum and one roll of the
 *  stack is used at the end. Server authoritative. Every value of the feature (DA_BandageRules); the timings of the clip
 *  and the meshes of the art kit (art/export/Props/Bandage/manifest.json) are set by scripts/editor/bandages_ue.py.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBandageRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UBandageRules();

	// ------------------------------------------------------------------ application

	/** Length of an application (s), the clip stretched over it (manifest: clip seconds, 3.0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply", meta=(ClampMin="0.2", Units="s"))
	float ApplySeconds = 3.0f;

	/** Length of the clip A_Bean_Bandage_Apply as made (s): the first person keys are in its time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply", meta=(ClampMin="0.1", Units="s"))
	float ClipSeconds = 3.0f;

	/** Moment of the clip the bandage starts going round the forearm (event WrapStart, s of the clip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply", meta=(ClampMin="0", Units="s"))
	float WrapStartSeconds = 0.4f;

	/** Moment of the clip the bandage is tied (event BandageDone, s of the clip): the heal lands then */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply", meta=(ClampMin="0", Units="s"))
	float HealEventSeconds = 2.73f;

	/** The bandage brings health back to its maximum (owner 2026-10-11); off: HealAmount */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply")
	bool bHealToMax = true;

	/** Health a bandage gives back when bHealToMax is off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply", meta=(ClampMin="0", EditCondition="!bHealToMax"))
	float HealAmount = 50.0f;

	/** A bandage may be applied at full health (it is then wasted); off: refused */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply")
	bool bUsableAtFullHealth = false;

	/** The movement keys rest during an application (the carrier stands still); off: the carrier may walk, a sprint cuts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Apply")
	bool bHoldStillWhileApplying = false;

	// ------------------------------------------------------------------ what cuts it

	/** A blow cuts the bandage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt")
	bool bInterruptOnHit = true;

	/** Smallest blow that cuts it (health lost at once) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt", meta=(ClampMin="0", EditCondition="bInterruptOnHit"))
	float MinHitDamage = 0.5f;

	/** A sprint cuts the bandage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt")
	bool bInterruptOnSprint = true;

	/** A jump or a fall cuts the bandage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt")
	bool bInterruptOnJump = true;

	/** Deep water cuts the bandage (and refuses one): swimming, or water deeper than MaxWaterDepthCm at the feet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt")
	bool bInterruptInWater = true;

	/** Deepest water at the feet a bandage is applied in (cm; wading in the lagoon is fine) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt", meta=(ClampMin="0", Units="cm", EditCondition="bInterruptInWater"))
	float MaxWaterDepthCm = 40.0f;

	/** Fastest walk a bandage goes on at (cm/s, ground speed); 0: any walk, only the sprint cuts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interrupt", meta=(ClampMin="0", Units="cm/s"))
	float MaxMoveSpeedCmS = 0.0f;

	// ------------------------------------------------------------------ inventory and shop (read by scripts/editor/bandages_ue.py)

	/** Bandages one inventory cell holds (DA_Item_Bandage.MaxStack) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="999"))
	int32 StackSize = 5;

	/** Price of one bandage at the merchant of the galleon (crowns) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0"))
	int32 ShopPrice = 150;

	/** Bandages given at spawn on the test map L_Test_Flat (BP_TestGameMode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0", ClampMax="999"))
	int32 TestMapStartingCount = 3;

	// ------------------------------------------------------------------ look

	/** Roll held in the right hand (SM_Bandage_Roll: sockets grip, tail) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> RollMesh;

	/** Bandage wrapped around the left forearm, shown from WrapStart (SM_Bandage_Wrapped) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> WrappedMesh;

	/** Clip of the application on the bean (A_Bean_Bandage_Apply, SK_Bean_Body_Skeleton), played on the whole body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UAnimSequence> ApplyClip;

	/** Sound at the BandageDone moment, where the carrier stands (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<USoundBase> DoneSound;

	/** Bone of the hand holding the roll */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FName HeldBone = TEXT("hand_r");

	/** Socket of the roll that meets the hand */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FName GripSocket = TEXT("grip");

	/** The roll's grip relative to the hand bone (cm from an unscaled bone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FTransform RollInHand = FTransform::Identity;

	/** Bone the wrapped bandage sits on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FName WrappedBone = TEXT("lowerarm_l");

	/** The wrapped bandage relative to its bone (cm from an unscaled bone; the forearm axis is +X of the mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FTransform WrappedOnBone = FTransform::Identity;

	/** The wrapped bandage stays on the forearm this long after a bandage healed (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0", Units="s"))
	float WrappedLingerSeconds = 6.0f;

	// ------------------------------------------------------------------ first person

	/** Roll held in the first person view, relative to the first person camera (X forward, Y right, Z up) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person")
	FTransform FirstPersonRollHeld = FTransform(FRotator(-10.0, 0.0, 0.0), FVector(40.0, 18.0, -22.0));

	/** Right shoulder of the first person arms, relative to the camera */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person")
	FVector FirstPersonRightShoulder = FVector(-6.0, 18.0, -24.0);

	/** Point of the first person arm mesh that holds the roll */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person")
	FVector FirstPersonArmHandPoint = FVector(36.0, 0.0, -1.5);

	/** Longest stretch of the first person arm reaching the roll */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person", meta=(ClampMin="1"))
	float FirstPersonArmMaxStretch = 1.6f;

	/** Keys of both first person arms over the clip (manifest first_person.keys); empty: the procedural wrap below */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person")
	TArray<FBandageArmKey> FirstPersonKeys;

	/** Procedural wrap: the left forearm raised across the bottom of the view (a right arm pose, mirrored) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person")
	FBeanArmPose FirstPersonLeftForearm = FBeanArmPose(FVector(2.0, 22.0, -36.0), FRotator(24.0, -38.0, 0.0));

	/** Procedural wrap: centre of the circle the roll goes round (the left forearm, relative to the camera) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person")
	FVector FirstPersonWrapCenter = FVector(42.0, -4.0, -24.0);

	/** Procedural wrap: radius of the circle (cm) and turns between WrapStart and BandageDone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person", meta=(ClampMin="0", Units="cm"))
	float FirstPersonWrapRadiusCm = 6.0f;

	/** See FirstPersonWrapRadiusCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="First Person", meta=(ClampMin="0"))
	float FirstPersonWrapTurns = 3.0f;

	// ------------------------------------------------------------------ input and network

	/** Keys that apply a bandage (F and the left click): mapped over the interaction and the punch only while one can be
	 *  applied, so F keeps picking things up when the crosshair is on something to use */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TArray<FKey> UseKeys;

	/** Priority of their context (above the character's contexts and the inventory's) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 InputPriority = 6;

	/** The owner checks this often whether a bandage can be applied (presentation and keys; the server decides) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(ClampMin="0.02", Units="s"))
	float LocalCheckSeconds = 0.15f;

	/** Server: bandage requests one player may send at once, and given back each second (SEC-03) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1"))
	float RequestBurst = 4.0f;

	/** See RequestBurst */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1"))
	float RequestsPerSecond = 2.0f;

	// ------------------------------------------------------------------ texts

	/** Prompt while a bandage can be applied: {0} the key, {1} the bandages carried */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	FText PromptText;

	/** Line during the application (with its gauge) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	FText ApplyingText;

	/** Line once healed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	FText HealedText;

	/** Line in hand at full health */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	FText FullHealthText;

	/** Line in hand while the carrier cannot apply one (in the air, swimming, running) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	FText UnableText;

	/** Lines of a cut bandage, by cause */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text")
	TMap<EBandageInterrupt, FText> InterruptTexts;

	/** A notice of the owner (refusal, cut, healed) stays this long (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text", meta=(ClampMin="0", Units="s"))
	float NoticeSeconds = 2.5f;

	/** Name of the first valid use key */
	FText GetUseKeyText() const;

	/** Line of a cut bandage */
	FText GetInterruptText(EBandageInterrupt Interrupt) const;

	/** Line of a refusal (empty when there is nothing to say) */
	FText GetRefusalText(EBandageRefusal Refusal) const;

	/** Health a bandage gives back from Health (never above MaxHealth) */
	float ComputeHeal(float Health, float MaxHealth) const;

	/** Time of the clip at Elapsed seconds of an application (the clip stretched over ApplySeconds) */
	float ToClipTime(float Elapsed) const
	{
		return ApplySeconds > KINDA_SMALL_NUMBER ? Elapsed * ClipSeconds / ApplySeconds : 0.0f;
	}

	/** Seconds of an application at which a moment of the clip comes */
	float ToApplyTime(float ClipTime) const
	{
		return ClipSeconds > KINDA_SMALL_NUMBER ? FMath::Clamp(ClipTime * ApplySeconds / ClipSeconds, 0.0f, ApplySeconds) : 0.0f;
	}
};
