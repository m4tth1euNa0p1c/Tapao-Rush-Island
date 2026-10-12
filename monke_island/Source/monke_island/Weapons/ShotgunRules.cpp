// monke_island

#include "Weapons/ShotgunRules.h"
#include "Weapons/PirateWeaponClips.inl"

UShotgunRules::UShotgunRules()
{
	// T-167, 8 pellets of 16: up to 5 m all of them on the body, 128 > any tiki (shredded); at 10 m 35 % each and
	// half of them on the body, 22 to 45 (never lethal alone), a limb struck by two pellets torn off (x2.5 on limbs:
	// 2 x 14 > 25); beyond, a few scratches
	Damage = 16.0f;
	RangeCm = 2500.0f;
	FalloffStartCm = 600.0f;
	FalloffMinScale = 0.2f;
	DamageFalloff = { FVector2D(0.0, 1.0), FVector2D(500.0, 1.0), FVector2D(1000.0, 0.35), FVector2D(2500.0, 0.08) };
	LimbDamageScale = 2.5f;
	HipSpreadDegrees = 2.5f;
	AimSpreadDegrees = 1.8f;
	KnockbackSpeed = 350.0f;
	PhysicsImpulse = 12000.0f;
	AimFieldOfView = 75.0f;
	AmmoKind = EAmmoKind::Shell;
	FlashScale = FVector(0.4f, 0.2f, 0.2f);
	ViewKickPitch = 3.5f;
	ViewKickYaw = 0.8f;

	// CAM-FP-* of the manifest: (0, 0.06, 0.80) m, 18 mm lens (90 deg); scripts/character/setup_pirate_weapons.py rewrites it
	FirstPersonViewLocation = FVector(0.0, -6.0, 80.0);
	FirstPersonViewRotation = FRotator(-24.267, 84.611, 0.0);
	FirstPersonFieldOfView = 90.0f;

	PirateWeaponClips::AddShotgunClips(Clips);
}
