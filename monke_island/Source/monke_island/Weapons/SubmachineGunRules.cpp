// monke_island

#include "Weapons/SubmachineGunRules.h"

USubmachineGunRules::USubmachineGunRules()
{
	AmmoKind = EAmmoKind::SmgCartridge;

	// a pistol cartridge: a villager (60) in three, a warrior (80) in four, a soldier (100) in five; full damage to 15 m
	Damage = 22.0f;
	RangeCm = 6000.0f;
	DamageFalloff = { FVector2D(0.0, 1.0), FVector2D(1500.0, 1.0), FVector2D(6000.0, 0.4) };
	LimbDamageScale = 1.2f;
	KnockbackSpeed = 40.0f;
	PhysicsImpulse = 5000.0f;

	// from the hip only: tight standing, wide on the move, and the burst opens it (bloom)
	HipSpreadDegrees = 3.5f;
	AimSpreadDegrees = 1.2f;
	SteadySpeedCmps = 60.0f;
	BloomPerRound = 0.18f;
	MaxBloomDegrees = 3.5f;
	BloomRecoveryPerSecond = 7.0f;
	MovingBloomScale = 1.6f;

	// the drum: 50 rounds, three drums carried at spawn
	BeltCapacity = 50;
	MaxRounds = 250;
	StartRounds = 150;
	bAutoReloadOnEmptyFire = true;
	bAutoReloadWhenBeltRunsOut = true;
	ReloadSeconds = 0.0f;
	ReloadLoadedEvent = TEXT("MagIn");

	// about 11 rounds a second from the first one: no crank to spin up; the FireLoop clip of the pack is one second of
	// eleven rounds (played at rate 1 at this cadence)
	MinCadenceRpm = 660.0f;
	MaxCadenceRpm = 660.0f;
	SpinUpSeconds = 0.0f;
	SpinDownSeconds = 0.0f;
	RoundsPerCrankTurn = 11;
	bCanFireWhileSprinting = false;

	// no heat: no overheat, no glow, no steam
	HeatPerRound = 0.0f;
	HeatParameterName = NAME_None;
	HeatMaterialSlots.Reset();
	HeatLightIntensity = 0.0f;
	JacketSocket = NAME_None;
	SteamCapSocket = NAME_None;

	// the view climbs with every round and wanders; a light jolt of the camera
	RecoilPitchPerRound = 0.32f;
	RecoilYawPerRound = 0.14f;
	MovingRecoilScale = 1.4f;
	RecoilApplySpeed = 26.0f;
	FireShakeScale = 0.45f;
	MovingShakeScale = 1.3f;

	// held in the hands: no slowdown, the light gun framed like the other hand-held firearms until the pack's framing
	WalkSpeedFactor = 1.0f;
	SprintSpeedFactor = 1.0f;
	HipGripLocation = FVector(30.0f, 13.0f, -15.0f);

	// no belt bones; the empty drum is let fall at MagDrop of ReloadEmpty (DroppedBoxMesh: the pack's drum, set by
	// new_weapons_ue.py); the reload of a drum not yet empty keeps it (Reload: MagOut, MagIn, Charge)
	LoadedBeltBones.Reset();
	DroppedBoxEvent = TEXT("MagDrop");
	DroppedBoxSocket = TEXT("socket_drum");
	DroppedBoxSeconds = 6.0f;

	// one case per round out of the port, no links
	LinkMesh = nullptr;
	EjectSocket = TEXT("socket_eject");
	EjectDirectionSocket = TEXT("socket_eject_dir");
	CasingSpeedCmps = 220.0f;
	CasingSpreadDegrees = 20.0f;
	MaxCasings = 16;
	MaxLinks = 0;

	// a small flash flickering at the cadence; the recoil is the gun's own per round
	MuzzleSocket = TEXT("socket_muzzle");
	FlashScale = FVector(0.18f, 0.09f, 0.09f);
	FlashSeconds = 0.04f;
	FlashLightIntensity = 30000.0f;
	FlashLightRadius = 500.0f;
	ViewKickPitch = 0.0f;
	ViewKickYaw = 0.0f;

	// one tracer every four rounds
	Tracer.bEnabled = true;
	Tracer.EveryNth = 4;
	Tracer.LengthCm = 90.0f;
	Tracer.WidthCm = 1.8f;

	// the pack of T-334 (art/export/Weapons/Mitraillette/pack.json): bones root, socket_eject, socket_eject_dir, socket_drum
	FirstPersonAlignBone = TEXT("root");
	GripSocket = TEXT("socket_grip_r");

	// the gestures of that pack (30 fps; scripts/editor/new_weapons_ue.py writes the built manifest's over them): FireLoop
	// one second of eleven rounds, the reload of a drum (MagOut, MagIn, Charge) and of an empty one (MagDrop: let fall)
	Clips.Reset();
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Equip, 36, 30, false, { { TEXT("DrawWeapon"), 4 }, { TEXT("Ready"), 30 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Holster, 36, 30, false, { { TEXT("StowWeapon"), 30 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Idle, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Walk, 120, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Sprint, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Inspect, 150, 30, false, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::FireLoop, 30, 30, true, { { TEXT("Fire"), 0 }, { TEXT("Fire"), 3 }, { TEXT("Fire"), 5 },
		{ TEXT("Fire"), 8 }, { TEXT("Fire"), 11 }, { TEXT("Fire"), 14 }, { TEXT("Fire"), 16 }, { TEXT("Fire"), 19 }, { TEXT("Fire"), 22 },
		{ TEXT("Fire"), 25 }, { TEXT("Fire"), 27 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Reload, 84, 30, false, { { TEXT("MagOut"), 20 }, { TEXT("MagIn"), 50 },
		{ TEXT("Charge"), 68 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::ReloadEmpty, 96, 30, false, { { TEXT("MagOut"), 16 }, { TEXT("MagDrop"), 22 },
		{ TEXT("MagIn"), 54 }, { TEXT("Charge"), 76 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::DryFire, 12, 30, false, { { TEXT("DryClick"), 3 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Discard, 40, 30, false, { { TEXT("Empty"), 6 }, { TEXT("Release"), 24 } }));
}
