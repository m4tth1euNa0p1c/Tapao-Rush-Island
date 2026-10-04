// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TorchRules.generated.h"

class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UStaticMesh;

/** Moves of the torch, played on the upper body (animation library, clips A_Bean_Torch_*) */
UENUM(BlueprintType)
enum class ETorchMove : uint8
{
	None,
	/** Taken from the left hip */
	Equip,
	/** Lit with the flint and steel */
	Ignite,
	/** Blow with the burning head */
	Swing,
	/** Put out and hung back on the hip */
	Unequip
};

/**
 *  Bean's hand torch (maquettes bean-torch / bean-torch-anim of the second PC, T-099): slot 3, held in the left hand,
 *  lights the night, burns 300 s, goes out in the water, a blow of the burning head sets a mob on fire.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTorchRules : public UDataAsset
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ meshes

	/** Torch (SM_Bean_Torch: grip at the origin, sockets flame / light / heel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> TorchMesh;

	/** Flame (SM_Bean_Torch_Flame), at the flame socket while lit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UStaticMesh> FlameMesh;

	/** Material of the flame (animated emissive) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	TObjectPtr<UMaterialInterface> FlameMaterial;

	/** The torch in the left mitten, relative to GripBone (attach_torch.json, converted like the cutlass) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FTransform GripInHand = FTransform(FRotator(21.53, 85.51, -149.46), FVector(0.49, -7.31, -0.04));

	/** The torch hung on the left hip, relative to HipBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FTransform OnHip = FTransform(FRotator(-21.8, -180.0, 108.78), FVector(33.17, -18.0, 2.0));

	/** Bones of the hand and the hip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FName GripBone = TEXT("hand_l");

	/** See GripBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meshes")
	FName HipBone = TEXT("pelvis");

	// ------------------------------------------------------------------ light

	/** Light of the flame (cd) and its reach (cm); raised (right click held) it lights farther */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0"))
	float LightIntensity = 9.0f;

	/** See LightIntensity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0"))
	float RaisedLightIntensity = 11.0f;

	/** See LightIntensity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0"))
	float LightRadius = 950.0f;

	/** See LightIntensity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0"))
	float RaisedLightRadius = 1250.0f;

	/** Colour temperature of the flame (K) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="1000"))
	float LightTemperature = 1850.0f;

	/** Dynamic shadows of the flame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light")
	bool bLightCastsShadows = true;

	/** Flicker: intensity x (Base + SinAmplitude sin(2 pi SinHz t) + NoiseAmplitude noise(NoiseHz t)) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light")
	FVector4f Flicker = FVector4f(0.82f, 0.10f, 1.3f, 0.08f);

	/** Frequency of the noise of the flicker (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0"))
	float FlickerNoiseHz = 9.0f;

	/** Time the raise takes (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0.01", Units="s"))
	float RaiseBlendTime = 0.25f;

	// ------------------------------------------------------------------ fuel

	/** Burning time of a torch (s); out of fuel it cannot be lit again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fuel", meta=(ClampMin="0", Units="s"))
	float BurnTime = 300.0f;

	/** Under this much fuel the flame weakens (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fuel", meta=(ClampMin="0", Units="s"))
	float WarningTime = 30.0f;

	// ------------------------------------------------------------------ moves

	/** Durations of the moves as played (clips: 1.0, 2.0, 0.917, 1.0 s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(ClampMin="0.05", Units="s"))
	float EquipSeconds = 1.0f;

	/** See EquipSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(ClampMin="0.05", Units="s"))
	float IgniteSeconds = 2.0f;

	/** See EquipSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(ClampMin="0.05", Units="s"))
	float SwingSeconds = 0.917f;

	/** See EquipSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(ClampMin="0.05", Units="s"))
	float UnequipSeconds = 1.0f;

	/** Notifies of the clips: hand on the torch (equip), flame on (ignite), blow lands (swing), back on the hip (unequip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(Units="s"))
	float AttachToHandTime = 0.417f;

	/** See AttachToHandTime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(Units="s"))
	float FlameOnTime = 1.417f;

	/** See AttachToHandTime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(Units="s"))
	float SwingHitTime = 0.292f;

	/** See AttachToHandTime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moves", meta=(Units="s"))
	float AttachToHipTime = 0.667f;

	// ------------------------------------------------------------------ blow

	/** Blow of the torch: damage, burn of a lit head (per second, for BurnSeconds), stamina, reach and arc */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0"))
	float SwingDamage = 10.0f;

	/** See SwingDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0"))
	float BurnPerSecond = 4.0f;

	/** See SwingDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0", Units="s"))
	float BurnSeconds = 3.0f;

	/** See SwingDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0", ClampMax="1"))
	float StaminaCostFraction = 0.06f;

	/** See SwingDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0"))
	float ReachCm = 110.0f;

	/** See SwingDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blow", meta=(ClampMin="0", ClampMax="360"))
	float ArcDegrees = 100.0f;

	// ------------------------------------------------------------------ input

	/** Keys of the torch: left click a blow, right click held raises it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** See MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SwingAction;

	/** See MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> RaiseAction;

	/** Priority of the context (over punch and guard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 MappingPriority = 2;

	/** Duration of a move as played */
	float GetMoveDuration(ETorchMove Move) const
	{
		switch (Move)
		{
		case ETorchMove::Equip:		return EquipSeconds;
		case ETorchMove::Ignite:	return IgniteSeconds;
		case ETorchMove::Swing:		return SwingSeconds;
		case ETorchMove::Unequip:	return UnequipSeconds;
		default:					return 0.0f;
		}
	}
};
