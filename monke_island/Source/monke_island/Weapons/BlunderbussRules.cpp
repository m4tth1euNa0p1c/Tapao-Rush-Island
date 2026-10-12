// monke_island

#include "Weapons/BlunderbussRules.h"
#include "Weapons/PirateWeaponClips.inl"

UBlunderbussRules::UBlunderbussRules()
{
	// 10 pellets of 14: up to 4 m all of them on the body, 140 > any tiki (shredded); at 8 m 30 % each and half of
	// them on the body, about 21 (never lethal alone); beyond 12 m a few scratches. The cone is twice the shotgun's.
	Damage = 14.0f;
	RangeCm = 1800.0f;
	FalloffStartCm = 400.0f;
	FalloffMinScale = 0.05f;
	DamageFalloff = { FVector2D(0.0, 1.0), FVector2D(400.0, 1.0), FVector2D(800.0, 0.3), FVector2D(1800.0, 0.05) };
	LimbDamageScale = 2.5f;
	HipSpreadDegrees = 6.0f;
	AimSpreadDegrees = 4.5f;
	KnockbackSpeed = 400.0f;
	PhysicsImpulse = 10000.0f;
	AimFieldOfView = 70.0f;
	AmmoKind = EAmmoKind::Shot;

	// charges of shot in the pouch, a heavier reload than the musket's
	MaxBalls = 10;
	ReloadSeconds = 2.5f;

	// a short wide barrel: a bigger flame, more kick
	FlashScale = FVector(0.45f, 0.22f, 0.22f);
	FlashSeconds = 0.09f;
	FlashLightIntensity = 70000.0f;
	ViewKickPitch = 4.0f;
	ViewKickYaw = 1.0f;

	// its own gestures (bean-blunderbuss-anim: the musket's 24 at 24 fps)
	Clips.Reset();
	PirateWeaponClips::AddBlunderbussClips(Clips);
}
