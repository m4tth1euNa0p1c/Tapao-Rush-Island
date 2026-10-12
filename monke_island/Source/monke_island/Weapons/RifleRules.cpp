// monke_island

#include "Weapons/RifleRules.h"
#include "Weapons/PirateWeaponClips.inl"

URifleRules::URifleRules()
{
	// one ball of 90: a tiki warrior (80) in one shot on the body, a soldier too; precise far away, slow to reload
	Damage = 90.0f;
	RangeCm = 16000.0f;
	FalloffStartCm = 0.0f;
	FalloffMinScale = 1.0f;
	DamageFalloff.Reset();
	LimbDamageScale = 1.0f;
	HipSpreadDegrees = 2.5f;
	AimSpreadDegrees = 0.2f;
	KnockbackSpeed = 300.0f;
	PhysicsImpulse = 45000.0f;
	AimFieldOfView = 50.0f;
	AmmoKind = EAmmoKind::RifleCartridge;

	// cartridges in the pouch; the patched ball takes twice the musket's time to ram down the rifled barrel
	MaxBalls = 12;
	ReloadSeconds = 3.0f;

	FlashScale = FVector(0.3f, 0.12f, 0.12f);
	ViewKickPitch = 2.2f;
	ViewKickYaw = 0.4f;

	// its own gestures (bean-rifle-anim: the musket's 24 at 24 fps)
	Clips.Reset();
	PirateWeaponClips::AddRifleClips(Clips);
}
