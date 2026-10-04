// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BeanAnimLibrary.generated.h"

class UAnimSequence;

/**
 *  Clips of the Bean animation library (maquette bean-anim-library of the second PC: 30 clips on SK_Bean_Body,
 *  24 fps, no root motion) and how UBeanAnimDriverComponent plays them (T-078). Filled by
 *  scripts/character/import_bean_library.py.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBeanAnimLibrary : public UDataAsset
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ locomotion

	/** Clip Idle of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locomotion")
	TObjectPtr<UAnimSequence> Idle;

	/** Clip Walk of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locomotion")
	TObjectPtr<UAnimSequence> Walk;

	/** Clip Run of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locomotion")
	TObjectPtr<UAnimSequence> Run;

	/** Clip JumpStart of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locomotion")
	TObjectPtr<UAnimSequence> JumpStart;

	/** Clip JumpLoop of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locomotion")
	TObjectPtr<UAnimSequence> JumpLoop;

	/** Clip JumpLand of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locomotion")
	TObjectPtr<UAnimSequence> JumpLand;

	/** Clip CarryIdle of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry")
	TObjectPtr<UAnimSequence> CarryIdle;

	/** Clip CarryWalk of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry")
	TObjectPtr<UAnimSequence> CarryWalk;

	/** Clip BoatSit of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat")
	TObjectPtr<UAnimSequence> BoatSit;

	/** Clip BoatRow of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat")
	TObjectPtr<UAnimSequence> BoatRow;

	// ------------------------------------------------------------------ one shots

	/** Clip Death of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> Death;

	/** Clip HitReact of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> HitReact;

	/** Clip Pickup of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> Pickup;

	/** Clip Interact of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> Interact;

	/** Clip EatCoconut of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> EatCoconut;

	/** Clip Wave of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> Wave;

	/** Clip Celebrate of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TObjectPtr<UAnimSequence> Celebrate;

	// ------------------------------------------------------------------ weapons (upper body)

	/** Clip SabreIdle of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreIdle;

	/** Clip SabreDraw of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreDraw;

	/** Clip SabreSlashRight of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreSlashRight;

	/** Clip SabreSlashLeft of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreSlashLeft;

	/** Clip SabreThrust of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreThrust;

	/** Clip SabreFinisher of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreFinisher;

	/** Clip SabreBlock of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreBlock;

	/** Clip SabreSheathe of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sabre")
	TObjectPtr<UAnimSequence> SabreSheathe;

	/** Clip MusketHold of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UAnimSequence> MusketHold;

	/** Clip MusketAim of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UAnimSequence> MusketAim;

	/** Clip MusketReload of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UAnimSequence> MusketReload;

	/** Clip MusketFire of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UAnimSequence> MusketFire;

	/** Clip MusketLower of the library (A_Bean_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Musket")
	TObjectPtr<UAnimSequence> MusketLower;

	/** Clip TorchEquip of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchEquip;

	/** Clip TorchIgnite of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchIgnite;

	/** Clip TorchIdle of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchIdle;

	/** Clip TorchRaise of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchRaise;

	/** Clip TorchLight of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchLight;

	/** Clip TorchSwing of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchSwing;

	/** Clip TorchUnequip of the library (A_Bean_Torch_*, maquette bean-torch-anim) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Torch")
	TObjectPtr<UAnimSequence> TorchUnequip;

	/** Clip TrumpetEquip of the library (A_Bean_Trumpet_*, maquette bean-trumpet-anim, emote V) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UAnimSequence> TrumpetEquip;

	/** Clip TrumpetFanfare of the library (A_Bean_Trumpet_*, maquette bean-trumpet-anim, emote V) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UAnimSequence> TrumpetFanfare;

	/** Clip TrumpetPlayLoop of the library (A_Bean_Trumpet_*, maquette bean-trumpet-anim, emote V) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UAnimSequence> TrumpetPlayLoop;

	/** Clip TrumpetUnequip of the library (A_Bean_Trumpet_*, maquette bean-trumpet-anim, emote V) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UAnimSequence> TrumpetUnequip;

	// ------------------------------------------------------------------ tuning

	/** Ground speed at which the walk clip is fully played (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="1"))
	float WalkSpeed = 150.0f;

	/** Ground speed at which the run clip is fully played (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="1"))
	float RunSpeed = 380.0f;

	/** Speeds at which the walk / run / carry clips do not slide (maquette: 49, 112, 45 cm/s): the play rate is the
	 *  speed over these, so the feet keep pace */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="1"))
	float WalkNoSlideSpeed = 49.0f;

	/** See WalkNoSlideSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="1"))
	float RunNoSlideSpeed = 112.0f;

	/** See WalkNoSlideSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="1"))
	float CarryNoSlideSpeed = 45.0f;

	/** Limits of the locomotion play rate (a bean's legs only go so fast) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0.1"))
	float MinPlayRate = 0.6f;

	/** See MinPlayRate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0.1"))
	float MaxPlayRate = 3.2f;

	/** Time a change of state of the whole body takes to blend (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0.01", Units="s"))
	float StateBlendTime = 0.2f;

	/** Blend in / out of a one shot (pickup, hit, emote) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0.01", Units="s"))
	float OneShotBlendIn = 0.15f;

	/** See OneShotBlendIn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0.01", Units="s"))
	float OneShotBlendOut = 0.2f;

	/** First bone of the upper body layer (weapons, eating): it and its children follow the upper clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning")
	FName UpperBodyRootBone = TEXT("spine_01");

	/** The boat is rowed when it moves faster than this (cm/s): the row clip, else seated idle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0"))
	float BoatRowSpeed = 25.0f;

	/** Oar angles of A_Bean_Boat_Row, one per frame at 24 fps (X theta: fore / aft sweep, Y phi: tilt, deg), from
	 *  the maquette manifest (boat.oar_curve_per_frame): the boat's oars follow the rower's hands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat")
	TArray<FVector2D> BoatRowOarCurve = { FVector2D(6.00f, -20.00f), FVector2D(5.86f, -20.00f), FVector2D(5.46f, -20.00f), FVector2D(4.79f, -20.00f), FVector2D(3.89f, -20.00f), FVector2D(2.77f, -20.00f), FVector2D(1.47f, -20.00f), FVector2D(0.02f, -20.00f), FVector2D(-1.55f, -20.00f), FVector2D(-3.17f, -20.00f), FVector2D(-4.83f, -20.00f), FVector2D(-6.45f, -20.00f), FVector2D(-8.02f, -20.00f), FVector2D(-9.47f, -20.00f), FVector2D(-10.77f, -20.00f), FVector2D(-11.89f, -20.00f), FVector2D(-12.79f, -20.00f), FVector2D(-13.46f, -20.00f), FVector2D(-13.86f, -20.00f), FVector2D(-14.00f, -12.00f), FVector2D(-13.86f, -12.00f), FVector2D(-13.46f, -12.00f), FVector2D(-12.79f, -12.00f), FVector2D(-11.89f, -12.00f), FVector2D(-10.77f, -12.00f), FVector2D(-9.47f, -12.00f), FVector2D(-8.02f, -12.00f), FVector2D(-6.45f, -12.00f), FVector2D(-4.83f, -12.00f), FVector2D(-3.17f, -12.00f), FVector2D(-1.55f, -12.00f), FVector2D(0.02f, -12.00f), FVector2D(1.47f, -12.00f), FVector2D(2.77f, -12.00f), FVector2D(3.89f, -12.00f), FVector2D(4.79f, -12.00f), FVector2D(5.46f, -12.00f), FVector2D(5.86f, -12.00f), FVector2D(6.00f, -12.00f) };

	/** Frame rate of BoatRowOarCurve */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat", meta=(ClampMin="1"))
	float BoatRowOarCurveRate = 24.0f;

	/** The rower's hands are pulled onto the oar handles (two bone IK): the boat is scaled in game (x0.75), not in
	 *  the maquette, so the clip alone misses the handles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat", meta=(ClampMin="0", ClampMax="1"))
	float RowHandIKAlpha = 1.0f;

	/** Grip in the mitten: distance from the hand bone along the forearm (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat", meta=(ClampMin="0"))
	float HandGripReachCm = 4.0f;

	/** Vertical speed above which a fall is a jump start (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0"))
	float JumpStartSpeed = 100.0f;

	/** Time in the air before the landing clip is played on touchdown (s): small hops land without it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tuning", meta=(ClampMin="0", Units="s"))
	float LandMinAirTime = 0.3f;
};
