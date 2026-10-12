// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Weapons/AmmoTypes.h"
#include "Weapons/FirearmTracer.h"
#include "FirearmRules.generated.h"

class UAnimSequence;
class UMaterialInterface;
class UNiagaraSystem;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/** A gesture of a pirate firearm pack (second PC: bean-pirate-pistol, bean-pirate-shotgun), first and third person */
UENUM(BlueprintType)
enum class EFirearmGesture : uint8
{
	None,
	Equip,
	Idle,
	Walk,
	Sprint,
	AimIn,
	AimIdle,
	AimOut,
	/** Shot from the hip (the shotgun's FirePump: shot then pump) */
	Fire,
	/** Shot while aiming (the shotgun's AimFirePump) */
	AimFire,
	/** Pistol: full reload (paper cartridge, ramrod, hammer cocked) */
	Reload,
	DryFire,
	Inspect,
	Holster,
	/** Pistol: cock the hammer */
	Cock,
	/** Shotgun: brings the loading port up */
	ReloadStart,
	/** Shotgun: one shell into the tube (repeated) */
	ReloadInsert,
	/** Shotgun: back to the ready pose */
	ReloadEnd,
	/** Shotgun: empty gun, three shells then the pump */
	ReloadEmpty,
	/** Musket (T-170, bean-musket-anim): the whole reload, butt on the ground (half cock to full cock) */
	GroundReload,
	/** Musket: phases of the reload, also clips of their own (optional transitions) */
	HalfCock,
	PrimePan,
	CloseFrizzen,
	Powder,
	BallAndPatch,
	DrawRamrod,
	TurnRamrod,
	RamrodStroke,
	ReturnRamrod,
	/** Musket: brings the musket to the shoulder */
	Present,
	/** Musket: kick after the shot */
	Recoil,
	/** Musket: carried at rest */
	Hold,
	/** Musket: back to the carry after the shot */
	ReturnToPort,
	/** Empty weapon thrown away (owner 2026-10-09): events Empty at the start, Release where it leaves the hand */
	Discard,
	/** Crank machine gun (T-273): the crank starts turning, the first rounds leave */
	FireStart,
	/** Crank machine gun: one turn of the crank at full speed (four rounds), looped while the gun fires */
	FireLoop,
	/** Crank machine gun: the crank stops */
	FireEnd,
	/** Crank machine gun: the barrel overheats, the steam vents (event SteamVent); no fire until it cooled */
	Overheat,
	/** Bolt rifle (T-335): the bolt is worked after a shot (events BoltOpen, CaseEject, BoltClose); no shot before BoltClose */
	BoltCycle,
	/** Bayonet musket (T-335): a thrust of the bayonet, the point strikes at the event StabHit */
	BayonetStab,
	/** Bayonet musket (T-335): the charge, a longer lunge on the run, the point strikes at the event ChargeHit */
	BayonetCharge
};

/** An event of a gesture, from the manifest of the pack (sound, flash, ammunition, reload logic) */
USTRUCT(BlueprintType)
struct FFirearmClipEvent
{
	GENERATED_BODY()

	FFirearmClipEvent() = default;

	FFirearmClipEvent(const TCHAR* InName, int32 Frame, int32 Fps = 30)
		: Name(InName), Time(static_cast<float>(Frame) / static_cast<float>(FMath::Max(Fps, 1)))
	{
	}

	/** Name of the event in the manifest (Fire, InsertSlug, EjectSpentShell, HammerCocked...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	FName Name;

	/** Time from the start of the clip (s, at play rate 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm", meta=(ClampMin="0", Units="s"))
	float Time = 0.0f;
};

/** An event of the generated table: name and frame */
struct FFirearmEventDef
{
	const TCHAR* Name;
	int32 Frame;
};

/**
 *  One gesture of the pack: the bean body clip and the weapon clip, in first and third person. The four clips of a
 *  gesture are played together, from the same time, with the weapon root on the bean root (the weapon motion is
 *  baked in the bean's space by the second PC: never attach the weapon to the hand a second time).
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FFirearmClip
{
	GENERATED_BODY()

	/** Gesture this clip plays */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	EFirearmGesture Gesture = EFirearmGesture::None;

	/** Bean body, third person (A_Bean_TP_<Weapon>_<Gesture>, on the project's bean skeleton) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	TObjectPtr<UAnimSequence> BeanThirdPerson;

	/** Weapon, third person (A_<Weapon>_TP_<Gesture>, on the weapon skeleton) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	TObjectPtr<UAnimSequence> WeaponThirdPerson;

	/** First person arms (A_Bean_FP_<Weapon>_<Gesture>, played on SK_Bean_<Weapon>_FP) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	TObjectPtr<UAnimSequence> BeanFirstPerson;

	/** Weapon, first person (A_<Weapon>_FP_<Gesture>) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	TObjectPtr<UAnimSequence> WeaponFirstPerson;

	/** Length of the gesture at play rate 1 (s, frames of the manifest / fps); the server logic runs on it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm", meta=(ClampMin="0.01", Units="s"))
	float Duration = 1.0f;

	/** Speed of the gesture (logic and animations together) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm", meta=(ClampMin="0.1", ClampMax="4"))
	float PlayRate = 1.0f;

	/** Loops (Idle, Walk, Sprint, AimIdle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	bool bLoop = false;

	/** Events of the manifest, sorted by time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	TArray<FFirearmClipEvent> Events;

	/** Real time the gesture lasts (s) */
	float GetPlayDuration() const { return Duration / FMath::Max(PlayRate, 0.1f); }

	/** Time of the Occurrence-th event called Name (s, clip time), or -1 */
	float FindEventTime(FName Name, int32 Occurrence = 0) const;

	/** Number of events called Name */
	int32 CountEvents(FName Name) const;

	/** Clip of the generated table (PirateWeaponClips.inl): length in frames at Fps, events in frames */
	static FFirearmClip Make(EFirearmGesture InGesture, int32 Frames, int32 Fps, bool bInLoop, std::initializer_list<FFirearmEventDef> InEvents);
};

/** A sound played at an event of the gestures (heard around the carrier) */
USTRUCT(BlueprintType)
struct FFirearmEventSound
{
	GENERATED_BODY()

	/** Event name (manifest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	FName Event;

	/** Sound played */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm")
	TObjectPtr<USoundBase> Sound;

	/** Volume multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firearm", meta=(ClampMin="0"))
	float Volume = 1.0f;
};

/**
 *  Tunable rules shared by the pirate firearms of the second PC (DA_PistolRules, DA_ShotgunRules): shot, damage,
 *  meshes, first person view of the pack and the gestures (clips and their events). The redesigned musket
 *  (UPirateMusketRules, T-170) uses them too; the old procedural musket keeps its own UMusketRules.
 */
UCLASS(Abstract, BlueprintType)
class MONKE_ISLAND_API UFirearmRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Ammunition this weapon's pouch takes from the pickups (AAmmoPickup) and the merchant (owner 2026-10-09) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	EAmmoKind AmmoKind = EAmmoKind::None;

	/** Fire on a weapon with nothing loaded and nothing left in the pouch throws it away (owner request 2026-10-09:
	 *  the Discard gesture, or DryFire then the throw when the pack has no Discard clip): it lands as a pickup, its
	 *  counters kept (empty), usable again once ammunition is picked up. Off (owner 2026-10-11, T-329: "le rechargement
	 *  normal pour toutes les armes"): the empty weapon stays in hand, clicks (DryFire) and reloads once it has rounds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	bool bDiscardWhenEmpty = false;

	/** Speed the thrown away weapon leaves the hand at, to the right and down (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0", Units="cm/s"))
	float DiscardTossSpeed = 350.0f;

	/** Angle of that throw below the horizontal (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0", ClampMax="89", Units="deg"))
	float DiscardTossPitchDegrees = 30.0f;

	/** Event of the Discard clip where the weapon leaves the hand (the end of the clip when missing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	FName DiscardReleaseEvent = TEXT("Release");

	/** Damage of one projectile (the pistol ball, one shotgun pellet) at full strength */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float Damage = 90.0f;

	/** Reach of the shot (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="100", Units="cm"))
	float RangeCm = 3000.0f;

	/** Damage starts to drop beyond this distance (cm); 0 = full damage over the whole range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", Units="cm"))
	float FalloffStartCm = 0.0f;

	/** Share of the damage left at RangeCm when FalloffStartCm is set */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="1"))
	float FalloffMinScale = 1.0f;

	/** Share of the damage left by distance (T-167): points (X distance cm, Y share) sorted by distance, linear
	 *  between them; empty = FalloffStartCm / FalloffMinScale above */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	TArray<FVector2D> DamageFalloff;

	/** Damage a limb of a tiki takes from a projectile, relative to its damage (T-167: thin limbs tear sooner; the
	 *  limb health is in DA_TikiRules Limbs) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float LimbDamageScale = 1.0f;

	/** A projectile struck the bone of a character it passes this near (cm, the bodies whose capsule takes the
	 *  trace); farther, it struck the body without a bone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="1", Units="cm"))
	float BoneHitRadiusCm = 18.0f;

	/** Spread half angle from the hip (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float HipSpreadDegrees = 3.0f;

	/** Spread half angle when aiming (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float AimSpreadDegrees = 0.8f;

	/** Speed given to a character hit by the whole shot (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float KnockbackSpeed = 180.0f;

	/** Impulse given to a physics body by one projectile (kg cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta=(ClampMin="0"))
	float PhysicsImpulse = 25000.0f;

	/** Players can hurt each other */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	bool bFriendlyFire = false;

	/** Trace channel of the shot (Camera, like the musket: the Pawn profiles ignore Visibility) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Camera;

	/** Event of the Fire / AimFire clips where the shot leaves: the shot clip starts there, so the shot is immediate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	FName FireEvent = TEXT("Fire");

	/** Field of view while aiming in first person (deg; a scope narrows it a lot: the bolt rifle, T-335) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim", meta=(ClampMin="5", ClampMax="120", Units="deg"))
	float AimFieldOfView = 70.0f;

	/** The right click aims down the sights. Off (owner 2026-10-09, "comme dans Counter-Strike"): the firearms fire from
	 *  the hip only, the crosshair is the sight, and the spread comes from the stance (SteadySpeedCmps) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim")
	bool bAllowAiming = false;

	/** Without aiming, a carrier slower than this (cm/s) shoots with AimSpreadDegrees, faster with HipSpreadDegrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim", meta=(ClampMin="0", Units="cm/s"))
	float SteadySpeedCmps = 60.0f;

	/** A scope (the bolt rifle, T-335): once the aim framing is at AimHideAlpha, the first person weapon and arms are
	 *  hidden and the view is the narrow AimFieldOfView alone (the scope's lens is not a window: no weapon in front of
	 *  the eye); IsScopeView tells a HUD it may draw the scope's ring */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim")
	bool bHideFirstPersonWhenAimed = false;

	/** Share of the aim framing (0 hip, 1 aimed) from which the scope view hides the first person weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim", meta=(ClampMin="0.1", ClampMax="1"))
	float AimHideAlpha = 0.9f;

	/** Weapon skeletal mesh (SK_Bean_PiratePistol / SK_Bean_PirateShotgun, its own skeleton), both views */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	/** First person arms of the pack (SK_Bean_<Weapon>_FP: FPArm sleeves on the project's bean skeleton) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	TObjectPtr<USkeletalMesh> FirstPersonArmsMesh;

	/** Camera of the FP scenes of the pack in the bean's mesh space (cm; from the manifest, converted to Unreal axes) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector FirstPersonViewLocation = FVector::ZeroVector;

	/** See FirstPersonViewLocation (the view looks along +X) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FRotator FirstPersonViewRotation = FRotator::ZeroRotator;

	/** Field of view of the FP scenes of the pack (deg, horizontal); 0 = keep the camera's */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0", ClampMax="120", Units="deg"))
	float FirstPersonFieldOfView = 0.0f;

	/**
	 *  Frames the weapon on the view instead of the pack's FP camera: the weapon of the Idle clip (first frame) is put
	 *  at HipGripLocation / HipRotation, that of the AimIdle clip with its muzzle at AimMuzzleLocation / AimRotation
	 *  (barrel along the view axis), and every gesture keeps its motion relative to that pose. Off: the pack's camera
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	bool bAlignFirstPersonToView = true;

	/** Rigid weapon bone used to read the weapon's orientation in a clip (its rest pose has the barrel along +X, up +Z) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FName FirstPersonAlignBone = TEXT("gun_root");

	/** Weapon bone held by the right hand (placed at HipGripLocation) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FName GripSocket = TEXT("socket_grip_r");

	/** Held (hip): the grip in the view (cm; X forward, Y right, Z up): low on the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector HipGripLocation = FVector(30.0f, 13.0f, -15.0f);

	/** Held (hip): orientation of the weapon in the view (zero: barrel straight ahead, level, no roll) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FRotator HipRotation = FRotator::ZeroRotator;

	/** Aimed: the muzzle (or the AimSightSocket) in the view (cm; Y = 0 keeps the barrel on the crosshair, Z < 0 lowers it
	 *  to clear the view) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FVector AimMuzzleLocation = FVector(45.0f, 0.0f, -4.0f);

	/** Aimed: the weapon bone put at AimMuzzleLocation instead of the muzzle (a scope's eyepiece, socket_scope_eye of the
	 *  bolt rifle, T-335: the scope's axis on the view axis); none, or a bone the weapon does not have = the muzzle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FName AimSightSocket;

	/** Aimed: orientation of the weapon in the view (zero: barrel along the view axis) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person")
	FRotator AimRotation = FRotator::ZeroRotator;

	/** Speed of the move between the hip and aimed framings (1/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|First Person", meta=(ClampMin="0.1"))
	float AimFramingSpeed = 12.0f;

	/** Bean bone the third person weapon follows: the weapon of the clip keeps its place relative to this bone of the
	 *  clip, on the puppet's real bone (the legs and the lean of the puppet are not those of the clip) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Third Person")
	FName AnchorBone = TEXT("hand_r");

	/** Bean bones the gestures drive in third person (the legs keep the bean's own walk) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Third Person")
	TArray<FName> UpperBodyBones = { TEXT("spine_01"), TEXT("spine_02"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"),
		TEXT("hand_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") };

	/** Weapon relative to AnchorBone when the carrier has no bean puppet (mannequin fallback, cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Third Person")
	FTransform FallbackHandOffset;

	/** Speed above which the carrier walks with the weapon (Walk clip, cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="0", Units="cm/s"))
	float WalkSpeedThreshold = 40.0f;

	/** The walk loop goes on until the speed falls under this fraction of WalkSpeedThreshold: a speed hovering at the
	 *  threshold no longer flips the idle and walk loops (T-330) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="0", ClampMax="1"))
	float WalkExitSpeedFactor = 0.6f;

	/** Time over which the carrier's speed is smoothed before the idle, walk or run loop is chosen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="0", Units="s"))
	float LoopSpeedSmoothingSeconds = 0.12f;

	/** A new carry loop (idle, walk, run) must be wanted this long before it plays: a run or a ground contact that
	 *  hesitates for a frame no longer restarts the arms and the weapon (owner report 2026-10-11, T-330) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="0", Units="s"))
	float LoopConfirmSeconds = 0.15f;

	/** Shortest time a carry loop plays before another carry loop takes its place */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation", meta=(ClampMin="0", Units="s"))
	float LoopMinSeconds = 0.35f;

	/** Gestures of the pack (default lengths and events: PirateWeaponClips.inl, generated from the manifest) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Animation")
	TArray<FFirearmClip> Clips;

	/** Weapon socket (bone) of the muzzle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FName MuzzleSocket = TEXT("socket_muzzle");

	/** Tracer rounds of the shots: the round and a glowing streak from the muzzle to where each projectile ends
	 *  (owner 2026-10-10); cosmetic, every machine draws its own */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tracer")
	FFirearmTracerLook Tracer;

	/** Sound of the shot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<USoundBase> FireSound;

	/** Sounds at the other events (hammer, pump, shell, ramrod...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TArray<FFirearmEventSound> EventSounds;

	/** Attenuation of the event sounds (none = heard everywhere) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<USoundAttenuation> EventSoundAttenuation;

	/** Muzzle flame mesh (pointing along +X) and its material (unlit, additive): the musket's M_MusketFlash fits */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UStaticMesh> FlashMesh;

	/** See FlashMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UMaterialInterface> FlashMaterial;

	/** Size of the flame (scale of FlashMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FVector FlashScale = FVector(0.25f, 0.12f, 0.12f);

	/** Time the flame and the light last (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0.01", Units="s"))
	float FlashSeconds = 0.07f;

	/** Flash light at the muzzle (unitless intensity, 0 = no light) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0"))
	float FlashLightIntensity = 40000.0f;

	/** See FlashLightIntensity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	FLinearColor FlashLightColor = FLinearColor(1.0f, 0.62f, 0.3f);

	/** See FlashLightIntensity (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", Units="cm"))
	float FlashLightRadius = 500.0f;

	/** Blast at the muzzle (T-167): Niagara systems spawned together at the shot along the barrel (flame, smoke,
	 *  sparks; NS_<Weapon>_Muzzle*); empty = the flame mesh only */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TArray<TObjectPtr<UNiagaraSystem>> MuzzleBlastSystems;

	/** Puff where a projectile strikes the ground or a wall (NS_Firearm_Impact), along the hit normal; none = nothing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects")
	TObjectPtr<UNiagaraSystem> ImpactSystem;

	/** No muzzle blast nor impact farther than this from the local camera (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", Units="cm"))
	float EffectsCullDistance = 8000.0f;

	/** First person: the view jumps up at the shot (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", Units="deg"))
	float ViewKickPitch = 1.5f;

	/** First person: random sideways view kick at the shot (deg, either side) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Effects", meta=(ClampMin="0", Units="deg"))
	float ViewKickYaw = 0.4f;

	/** Clip of a gesture, null when the pack has none */
	const FFirearmClip* FindClip(EFirearmGesture Gesture) const;
};
