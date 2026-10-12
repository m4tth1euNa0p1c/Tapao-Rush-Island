// monke_island

#include "Weapons/PistolRules.h"
#include "Weapons/PirateWeaponClips.inl"

UPistolRules::UPistolRules()
{
	// T-167, one ball of 35: two for a villager, three for a tiki warrior (60 / 80 health) on the body; on a limb
	// it passes its health (25 / 30) and tears it off, the head torn off kills
	Damage = 35.0f;
	LimbDamageScale = 1.0f;
	RangeCm = 3000.0f;
	HipSpreadDegrees = 3.0f;
	AimSpreadDegrees = 0.8f;
	KnockbackSpeed = 180.0f;
	AimFieldOfView = 65.0f;
	AmmoKind = EAmmoKind::PistolCharge;

	// CAM-FP-* of the manifest: (0, 0.25, 0.83) m looking at (0.10, -1.0, 0.28), 22 mm lens (Blender -Y forward,
	// converted to the bean's mesh space: +Y forward, -X right); scripts/character/setup_pirate_weapons.py rewrites it
	FirstPersonViewLocation = FVector(0.0, -25.0, 83.0);
	FirstPersonViewRotation = FRotator(-23.682, 85.426, 0.0);
	FirstPersonFieldOfView = 78.58f;

	PirateWeaponClips::AddPistolClips(Clips);
}

UDoublePistolRules::UDoublePistolRules()
{
	// two charges loaded at once, two shots before the reload; a heavier piece kicks a little more
	Barrels = 2;
	MaxCharges = 16;
	ViewKickPitch = 1.8f;
	FlashScale = FVector(0.28f, 0.13f, 0.13f);

	// its own gestures (bean-pirate-pistol-double): the pistol's table until the pack is built
	Clips.Reset();
	PirateWeaponClips::AddDoublePistolClips(Clips);
}
