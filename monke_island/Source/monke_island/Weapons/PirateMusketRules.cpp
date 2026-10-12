// monke_island

#include "Weapons/PirateMusketRules.h"
#include "Weapons/PirateWeaponClips.inl"

UPirateMusketRules::UPirateMusketRules()
{
	// the old musket's shot (DA_MusketRules; scripts/character/setup_pirate_weapons.py copies the tuned values over)
	Damage = 10000.0f;
	RangeCm = 8000.0f;
	HipSpreadDegrees = 4.0f;
	AimSpreadDegrees = 0.6f;
	KnockbackSpeed = 250.0f;
	PhysicsImpulse = 40000.0f;
	AimFieldOfView = 65.0f;
	AmmoKind = EAmmoKind::MusketBall;
	// the clip starts at Trigger, the ball leaves at Shot (bean-musket-anim: frames 5 and 8 at 24 fps)
	FireEvent = TEXT("Shot");

	// framed on the view like the pistol: grip low and nearly centred, barrel level; the stock rests behind the camera
	bAlignFirstPersonToView = true;
	FirstPersonAlignBone = TEXT("root");
	GripSocket = TEXT("socket_grip_r");
	MuzzleSocket = TEXT("socket_muzzle");
	HipGripLocation = FVector(28.0f, 7.0f, -24.0f);
	HipRotation = FRotator::ZeroRotator;
	AimMuzzleLocation = FVector(72.0f, 0.0f, -6.0f);
	AimRotation = FRotator::ZeroRotator;

	FlashScale = FVector(0.35f, 0.14f, 0.14f);
	FlashSeconds = 0.08f;
	FlashLightIntensity = 60000.0f;
	FlashLightRadius = 600.0f;
	ViewKickPitch = 2.5f;
	ViewKickYaw = 0.6f;

	PirateWeaponClips::AddMusketClips(Clips);
}
