// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "MusketRules.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/** One key of the first person musket animation: the musket relative to the camera and where each mitten holds it */
USTRUCT(BlueprintType)
struct FMusketFirstPersonKey
{
	GENERATED_BODY()

	/** Time of the key from the start of the action (s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket", meta=(ClampMin="0", Units="s"))
	float Time = 0.0f;

	/** Musket grip (socket grip_r) relative to the camera (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	FVector MusketLocation = FVector::ZeroVector;

	/** Musket rotation relative to the camera (the barrel points along +X) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	FRotator MusketRotation = FRotator::ZeroRotator;

	/** Musket socket (bone) the right mitten holds; None = RightHandOffset is a free point relative to the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	FName RightHandSocket = TEXT("socket_grip_r");

	/** Right mitten offset from its socket in the musket frame (cm), or its point relative to the camera without a socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	FVector RightHandOffset = FVector::ZeroVector;

	/** Musket socket (bone) the left mitten holds; None = LeftHandOffset is a free point relative to the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	FName LeftHandSocket = TEXT("socket_grip_l");

	/** See RightHandOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	FVector LeftHandOffset = FVector::ZeroVector;
};

/** A sound of the reload, at its time on the reload timeline (same time base as ReloadKeys) */
USTRUCT(BlueprintType)
struct FMusketSoundCue
{
	GENERATED_BODY()

	/** Time on the reload timeline (s, A_Musket_Reload time: stretched with the keys over ReloadSeconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket", meta=(ClampMin="0", Units="s"))
	float Time = 0.0f;

	/** Sound played */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket")
	TObjectPtr<USoundBase> Sound;

	/** Volume multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Musket", meta=(ClampMin="0"))
	float Volume = 1.0f;
};

/**
 *  Tunable rules of Bean's flintlock musket (DA_MusketRules): ammunition carried by each player, reload, shot,
 *  first person poses and the animations made on the second PC (maquettes bean-musket, bean-musket-anim).
 *  A muzzle loader: one ball in the barrel, a full reload (about 6 s) after each shot.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UMusketRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UMusketRules();

	/** Balls a player carries, the one in the barrel included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 MaxBalls = 23;

	/** The musket starts loaded (that ball counts in MaxBalls) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	bool bStartLoaded = true;

	/** Full reload time: half cock, prime, powder, ball, ramrod, cock (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload", meta=(ClampMin="0.1", Units="s"))
	float ReloadSeconds = 6.0f;

	/** Pressing fire with an empty barrel starts the reload */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadOnEmptyFire = true;

	/** Lock time: flash in the pan, then the shot leaves (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", Units="s"))
	float FireDelaySeconds = 0.12f;

	/** Time after the shot before the musket can be used again (recoil) (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", Units="s"))
	float RecoverSeconds = 0.6f;

	/** Damage of a hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float Damage = 10000.0f;

	/** Reach of the shot (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="100", Units="cm"))
	float RangeCm = 8000.0f;

	/** Spread half angle from the hip (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float HipSpreadDegrees = 4.0f;

	/** Spread half angle when aiming (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float AimSpreadDegrees = 0.6f;

	/** Speed given to a character hit (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float KnockbackSpeed = 250.0f;

	/** Impulse given to a physics body hit (kg cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float PhysicsImpulse = 40000.0f;

	/** Players can hurt each other */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	bool bFriendlyFire = false;

	/** Trace channel of the shot (Camera: the Pawn and CharacterMesh profiles ignore Visibility, the ball went through bodies) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Camera;

	/** Field of view while aiming in first person (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim", meta=(ClampMin="20", ClampMax="120", Units="deg"))
	float AimFieldOfView = 65.0f;

	/** Musket mesh (SK_Bean_Musket: hammer, frizzen, trigger and ramrod bones) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	TObjectPtr<USkeletalMesh> MusketMesh;

	/** Bone of the bean the musket grip follows in third person */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	FName HandBone = TEXT("hand_r");

	/** Musket grip relative to HandBone in third person */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	FTransform HandOffset;

	/** Bean body animation of the musket carried at rest (A_Bean_Musket_Hold, loops), on the upper body in third person */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TObjectPtr<UAnimSequence> BodyHoldAnimation;

	/** Bean bones the musket animations drive in third person (the legs keep walking) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TArray<FName> UpperBodyBones = { TEXT("spine_01"), TEXT("spine_02"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"),
		TEXT("hand_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") };

	/** Bean body animation of the reload (A_Bean_Musket_Reload), played in third person */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TObjectPtr<UAnimSequence> BodyReloadAnimation;

	/** Bean body animation of the shot (A_Bean_Musket_Fire) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TObjectPtr<UAnimSequence> BodyFireAnimation;

	/** Musket bones during the reload (A_Musket_Reload: hammer, frizzen, ramrod) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TObjectPtr<UAnimSequence> MusketReloadAnimation;

	/** Time of the trigger pull in the shot animations: they start there when fire is pressed (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="0", Units="s"))
	float FireAnimationTriggerTime = 0.4f;

	/** Musket bones during the shot (A_Musket_Fire: trigger, hammer) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TObjectPtr<UAnimSequence> MusketFireAnimation;

	/** Third person: the musket (socket grip_r) relative to HandBone at each frame of BodyHoldAnimation (scripts/props/musket_tp_tracks.py) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TArray<FTransform> HoldMusketTrack;

	/** Third person: same for BodyReloadAnimation (the right hand lets go of the musket, the left one holds the barrel) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TArray<FTransform> ReloadMusketTrack;

	/** Third person: same for BodyFireAnimation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TArray<FTransform> FireMusketTrack;

	/** Frames per second of the musket tracks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="1"))
	float MusketTrackFrameRate = 24.0f;

	/** First person: musket held at rest */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FMusketFirstPersonKey HoldPose;

	/** First person: musket shouldered (right click) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FMusketFirstPersonKey AimPose;

	/** First person: reload keys, sorted by time, stretched over ReloadSeconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	TArray<FMusketFirstPersonKey> ReloadKeys;

	/** First person: right shoulder relative to the camera (cm); the arm aims from it at the mitten's target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector RightShoulder = FVector(-5.0f, 18.0f, -20.0f);

	/** First person: left shoulder relative to the camera (cm, on the left: negative Y) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector LeftShoulder = FVector(-5.0f, -18.0f, -22.0f);

	/** Centre of the mitten in the first person arm mesh (SM_Bean_FPArm, cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector ArmHandPoint = FVector(36.0f, 0.0f, -1.5f);

	/** First person: an arm stretches up to this factor to reach the musket (rubber bean arms), then slides along */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="1", ClampMax="3"))
	float MaxArmStretch = 1.6f;

	/** First person: speed at which a mitten moves to its next hold (1/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0"))
	float HandBlendSpeed = 18.0f;

	/** First person: kick of the musket at the shot (cm back, deg up) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector2D RecoilKick = FVector2D(9.0f, 8.0f);

	/** Speed of the first person pose blends (1/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0"))
	float PoseBlendSpeed = 12.0f;

	/** First person: the view jumps up at the shot (deg), then the aim settles back by itself */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0", Units="deg"))
	float ViewKickPitch = 2.5f;

	/** First person: random sideways view kick at the shot (deg, either side) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0", Units="deg"))
	float ViewKickYaw = 0.6f;

	/** First person: camera shake roll at the shot (deg), fades with the recoil */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0", Units="deg"))
	float ViewShakeRoll = 3.0f;

	/** Sound of the shot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<USoundBase> FireSound;

	/** Sound of the flash in the pan, at the trigger pull (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<USoundBase> LockSound;

	/** Reload sounds (hammer, frizzen, powder, cartridge, ball, ramrod), sorted by time; heard around the carrier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TArray<FMusketSoundCue> ReloadSoundCues;

	/** Attenuation of the reload sounds (none = heard everywhere) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<USoundAttenuation> ReloadSoundAttenuation;

	/** Muzzle flame mesh (pointing along +X, about 1 m long at scale 1) and its material (unlit, additive) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UStaticMesh> FlashMesh;

	/** See FlashMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UMaterialInterface> FlashMaterial;

	/** Size of the flame (scale of FlashMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FVector FlashScale = FVector(0.35f, 0.14f, 0.14f);

	/** Time the flame and the light last (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0.01", Units="s"))
	float FlashSeconds = 0.08f;

	/** Flash light at the muzzle (unitless intensity, 0 = no light) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0"))
	float FlashLightIntensity = 60000.0f;

	/** See FlashLightIntensity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FLinearColor FlashLightColor = FLinearColor(1.0f, 0.62f, 0.3f);

	/** See FlashLightIntensity (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", Units="cm"))
	float FlashLightRadius = 600.0f;

	/** Black powder smoke: puff mesh (a sphere) and its translucent material (scalar parameter SmokeOpacityParameter) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UStaticMesh> SmokeMesh;

	/** See SmokeMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UMaterialInterface> SmokeMaterial;

	/** See SmokeMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FName SmokeOpacityParameter = TEXT("Opacity");

	/** Puffs of a shot (0 = no smoke); a few from the muzzle, one from the pan */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", ClampMax="16"))
	int32 SmokePuffs = 7;

	/** Life of the smoke (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0.1", Units="s"))
	float SmokeSeconds = 2.5f;

	/** Puff diameter at the start and at the end (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="1", Units="cm"))
	FVector2D SmokeSize = FVector2D(12.0f, 90.0f);

	/** Speed of the puffs leaving the muzzle (cm/s), slowed by the air */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0"))
	float SmokeSpeed = 450.0f;

	/** Rise of the smoke (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	float SmokeRise = 18.0f;

	/** Opacity of a fresh puff */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", ClampMax="1"))
	float SmokeOpacity = 0.55f;
};
