// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BeanAnimLibrary.generated.h"

class UAnimSequence;

/**
 *  Clips of the Bean animation library (maquette bean-anim-library of the second PC: 30 clips on SK_Bean_Body,
 *  24 fps, no root motion) and how UBeanAnimDriverComponent plays them (T-078). Filled by
 *  scripts/character/import_bean_library.py; the swim clips (T-017, 30 fps) by scripts/character/bean_swim_import_ue.py.
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

	/** Breaststroke loop, head and hat out of the water (A_Bean_Swim, 1.2 s, scripts/character/bean_swim_clips.py, T-017) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim")
	TObjectPtr<UAnimSequence> Swim;

	/** Treading water in place: legs pedalling, hands sculling (A_Bean_TreadWater, 1.6 s, T-017) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim")
	TObjectPtr<UAnimSequence> TreadWater;

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

	// ------------------------------------------------------------------ capture (T-336, clips of T-337)

	/** Sets the folded cage down in front and opens it (A_Bean_Cage_Place, 1.5 s, event CagePlaced at 0.95 s): a one shot
	 *  of the whole body, played by ACageItem at the pace of DA_CaptureRules.PlaceGestureSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	TObjectPtr<UAnimSequence> CagePlace;

	/** Closes the handcuffs on the wrists of the target in front (A_Bean_Cuff, 1.2 s, event CuffsClosed at 0.7 s): a one
	 *  shot of the whole body, stretched by AHandcuffsItem over the hold of the key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	TObjectPtr<UAnimSequence> Cuff;

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

	// ------------------------------------------------------------------ swimming (T-017)

	/** Swimming speed at which the stroke clip plays at its own pace (cm/s, the movement's MaxSwimSpeed): the play rate is
	 *  the speed over it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="1"))
	float SwimStrokeSpeed = 120.0f;

	/** Limits of the stroke play rate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0.1"))
	float SwimMinPlayRate = 0.7f;

	/** See SwimMinPlayRate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0.1"))
	float SwimMaxPlayRate = 1.6f;

	/** Up to this swimming speed the bean only treads water (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float TreadMaxSpeed = 20.0f;

	/** From this swimming speed the stroke is fully played; treading water and the stroke cross fade in between (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float SwimFullSpeed = 70.0f;

	/** Time the whole body takes to blend into or out of the water clips, and the float lift to come or go (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0.01", Units="s"))
	float SwimBlendTime = 0.35f;

	/** Time the cross fade between treading water and the stroke takes to follow the speed (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0.01", Units="s"))
	float SwimStrokeBlendTime = 0.3f;

	/** Water depth at the bottom of the capsule from which the bean takes the water pose even when it is not swimming
	 *  (walking or falling, cm): a 1.05 m bean floats before the movement swims (SwimStartDepth is an adult's 120 cm),
	 *  and its capsule bobs at that depth between swimming and falling. Above SwimRootDepth so the floating bean clears
	 *  the bottom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float SwimPoseMinDepth = 75.0f;

	/** Depth margin below SwimPoseMinDepth before the bean leaves the water pose (cm): no flicker in the waves */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float SwimPoseExitHysteresis = 10.0f;

	/** Rising faster than this (a jump out of the water, 620 cm/s) leaves the water pose at once (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float SwimPoseMaxRiseSpeed = 300.0f;

	/** Depth of the bean root under the water surface while it floats (cm): the swim clips are authored for this water
	 *  line (water_line_cm of art/export/Characters/Bean/Anims/bean_swim_clips.json), eyes, moustache and hat above it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float SwimRootDepth = 60.0f;

	/** Highest the bean is raised above its dry placement to float at SwimRootDepth (cm): a diving capsule takes it down
	 *  beyond. The bean capsule (55 cm half height) swims about 120 cm deep at its feet: a 60 cm lift */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swim", meta=(ClampMin="0"))
	float SwimMaxLift = 75.0f;
};
