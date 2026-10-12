// monke_island

#include "Weapons/GrenadeRules.h"
#include "Weapons/PirateWeaponClips.inl"

UGrenadeRules::UGrenadeRules()
{
	// no shot of its own: the base Damage / spread are not used, the blast rules above are
	Damage = 0.0f;
	HipSpreadDegrees = 0.0f;
	AimSpreadDegrees = 0.0f;
	AimFieldOfView = 80.0f;
	AmmoKind = EAmmoKind::Grenade;

	// no muzzle: nothing flashes at the throw, the view does not kick
	FlashLightIntensity = 0.0f;
	FlashSeconds = 0.01f;
	ViewKickPitch = 0.0f;
	ViewKickYaw = 0.0f;

	// the grenade of the Idle clip framed on the view like the pistol
	bAlignFirstPersonToView = true;
	FirstPersonAlignBone = TEXT("root");
	GripSocket = TEXT("socket_grip_r");
	MuzzleSocket = TEXT("socket_fuse");
	HipGripLocation = FVector(30.0f, 14.0f, -16.0f);
	AimMuzzleLocation = FVector(40.0f, 10.0f, -8.0f);

	PirateWeaponClips::AddGrenadeClips(Clips);
}
