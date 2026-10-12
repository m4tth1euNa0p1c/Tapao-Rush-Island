// monke_island

#include "Weapons/BoltRifleRules.h"

UBoltRifleRules::UBoltRifleRules()
{
	AmmoKind = EAmmoKind::RifleCartridge;

	// a heavy rifle round: a tiki warrior (80) or a French soldier (100) in one shot on the body; full damage to 300 m
	Damage = 150.0f;
	RangeCm = 30000.0f;
	FalloffStartCm = 0.0f;
	FalloffMinScale = 1.0f;
	LimbDamageScale = 1.2f;
	BoneHitRadiusCm = 12.0f;
	KnockbackSpeed = 350.0f;
	PhysicsImpulse = 60000.0f;

	// steady through the scope: a pin; steady without it: a little; on the move, scope or not: wide (BoltRifleMath::Spread)
	AimSpreadDegrees = 0.05f;
	NoScopeSpreadDegrees = 1.5f;
	HipSpreadDegrees = 4.0f;
	SteadySpeedCmps = 60.0f;

	// the scope (owner 2026-10-11): the exception to the hip-only rule of the firearms (T-254); about a 4x scope from the
	// 90 deg view, its eyepiece on the view axis 14 cm before the eye (beyond the near plane), then the weapon hides
	bAllowAiming = true;
	AimFieldOfView = 22.0f;
	AimFramingSpeed = 9.0f;
	AimSightSocket = TEXT("socket_scope_eye");
	AimMuzzleLocation = FVector(14.0f, 0.0f, 0.0f);
	AimRotation = FRotator::ZeroRotator;
	bHideFirstPersonWhenAimed = true;
	AimHideAlpha = 0.92f;

	MuzzleSocket = TEXT("socket_muzzle");
	FlashScale = FVector(0.3f, 0.12f, 0.12f);
	FlashSeconds = 0.06f;
	FlashLightIntensity = 60000.0f;
	FlashLightRadius = 650.0f;
	ViewKickPitch = 4.5f;
	ViewKickYaw = 0.9f;

	// one fast tracer per round
	Tracer.bEnabled = true;
	Tracer.EveryNth = 1;
	Tracer.SpeedCmps = 32000.0f;
	Tracer.LengthCm = 260.0f;
	Tracer.WidthCm = 3.0f;
	Tracer.MaxDistanceCm = 30000.0f;

	// the pack of T-334 (art/export/Weapons/BoltRifle/pack.json): bones root, socket_scope_eye, socket_eject, socket_clip
	FirstPersonAlignBone = TEXT("root");
	GripSocket = TEXT("socket_grip_r");

	// the gestures of that pack (30 fps; scripts/editor/new_weapons_ue.py writes the built manifest's over them): the shot
	// from the hip and through the scope, the bolt (BoltOpen, CaseEject, BoltClose), the stripper clip (BoltOpen, ClipIn,
	// RoundsIn: the five rounds count, ClipEject, BoltClose)
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Equip, 45, 30, false, { { TEXT("DrawWeapon"), 6 }, { TEXT("Ready"), 38 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Holster, 36, 30, false, { { TEXT("StowWeapon"), 30 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Idle, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Walk, 120, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Sprint, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::AimIn, 15, 30, false, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::AimIdle, 90, 30, true, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::AimOut, 12, 30, false, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Fire, 24, 30, false, { { TEXT("Fire"), 2 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::AimFire, 24, 30, false, { { TEXT("Fire"), 2 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::BoltCycle, 27, 30, false, { { TEXT("BoltOpen"), 6 }, { TEXT("CaseEject"), 11 },
		{ TEXT("BoltClose"), 19 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Reload, 78, 30, false, { { TEXT("BoltOpen"), 10 }, { TEXT("ClipIn"), 30 },
		{ TEXT("RoundsIn"), 42 }, { TEXT("ClipEject"), 52 }, { TEXT("BoltClose"), 62 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::DryFire, 12, 30, false, { { TEXT("DryClick"), 3 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Inspect, 150, 30, false, {}));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::Discard, 40, 30, false, { { TEXT("Empty"), 8 }, { TEXT("Release"), 24 } }));
}
