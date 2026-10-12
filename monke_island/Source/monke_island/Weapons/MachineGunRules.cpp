// monke_island

#include "Weapons/MachineGunRules.h"
#include "Weapons/MachineGunCameraShake.h"

UMachineGunRules::UMachineGunRules()
{
	AmmoKind = EAmmoKind::MachineGunBelt;

	// a rifle round per shot: a villager (60) in two, a warrior (80) or a soldier (100) in three; full damage to 40 m
	Damage = 34.0f;
	RangeCm = 12000.0f;
	DamageFalloff = { FVector2D(0.0, 1.0), FVector2D(4000.0, 1.0), FVector2D(12000.0, 0.55) };
	LimbDamageScale = 1.4f;
	KnockbackSpeed = 70.0f;
	PhysicsImpulse = 9000.0f;

	// from the hip only (no aiming): tight standing, wide on the move, and the bloom of the burst on top
	HipSpreadDegrees = 3.0f;
	AimSpreadDegrees = 0.9f;
	SteadySpeedCmps = 60.0f;

	// the pack of T-272: sockets are bones socket_* of the gun skeleton (socket_grip_r on the crank handle: the right
	// hand turns it); a heavy gun carried low at the hip until the framing of the pack replaces it
	MuzzleSocket = TEXT("socket_muzzle");
	HipGripLocation = FVector(38.0f, 16.0f, -30.0f);

	// the flash of each round, flickering at the cadence; the recoil is the gun's own (Recoil category), not the single
	// kick of the base
	FlashScale = FVector(0.32f, 0.15f, 0.15f);
	FlashSeconds = 0.045f;
	FlashLightIntensity = 55000.0f;
	FlashLightRadius = 750.0f;
	ViewKickPitch = 0.0f;
	ViewKickYaw = 0.0f;

	// one tracer every three rounds (lead 2026-10-10)
	Tracer.bEnabled = true;
	Tracer.EveryNth = 3;

	FireCameraShake = UMachineGunFireCameraShake::StaticClass();

	// the loaded belt of T-272: 17 pockets (belt_01..17) from the extraction point back into the box
	for (int32 Pocket = 1; Pocket <= 17; ++Pocket)
	{
		LoadedBeltBones.Add(FName(*FString::Printf(TEXT("belt_%02d"), Pocket)));
	}

	// the gestures of the pack (scripts/props/machinegun_pack/animation.py, 30 fps): one turn of the crank is FireLoop,
	// four rounds (450 a minute at play rate 1: the gun plays it faster at its own cadence); machinegun_setup_ue.py
	// writes the lengths and events of the imported clips over these
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Equip, 54, 30, false, { { TEXT("DrawWeapon"), 6 }, { TEXT("Ready"), 46 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Holster, 48, 30, false, { { TEXT("StowWeapon"), 42 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Idle, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Walk, 120, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Sprint, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Inspect, 180, 30, false, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::FireStart, 17, 30, false, { { TEXT("CrankTick"), 0 }, { TEXT("Fire"), 1 }, { TEXT("Fire"), 9 },
		{ TEXT("Fire"), 13 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::FireLoop, 16, 30, true, { { TEXT("CrankTick"), 0 }, { TEXT("Fire"), 0 }, { TEXT("Fire"), 4 },
		{ TEXT("Fire"), 8 }, { TEXT("Fire"), 12 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::FireEnd, 10, 30, false, { { TEXT("CrankTick"), 8 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Reload, 150, 30, false, { { TEXT("CoverOpen"), 16 }, { TEXT("BeltOut"), 34 }, { TEXT("BoxOut"), 64 },
		{ TEXT("BoxIn"), 92 }, { TEXT("BeltIn"), 116 }, { TEXT("CoverClose"), 126 }, { TEXT("Charge"), 136 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Overheat, 60, 30, false, { { TEXT("SteamVent"), 6 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::DryFire, 14, 30, false, { { TEXT("DryClick"), 4 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Discard, 40, 30, false, { { TEXT("Empty"), 4 }, { TEXT("Release"), 24 } }));
}
