// monke_island

#include "Weapons/MusketRules.h"

namespace
{
	const FName GripR(TEXT("socket_grip_r"));
	const FName GripL(TEXT("socket_grip_l"));
	const FName Pan(TEXT("socket_pan"));
	const FName Barrel(TEXT("socket_barrel"));
	const FName Muzzle(TEXT("socket_muzzle"));
	const FName RamrodTip(TEXT("socket_ramrod_tip"));

	/** Musket placements relative to the camera (grip, rotation) */
	struct FPlacement
	{
		FVector Location;
		FRotator Rotation;
	};

	// ready, low on the right, barrel forward
	const FPlacement Ready{ FVector(30.0, 13.0, -20.0), FRotator(3.0, -4.0, 0.0) };
	// shouldered, the barrel under the eye
	const FPlacement Shouldered{ FVector(20.0, 0.0, -9.5), FRotator(0.0, 0.0, 0.0) };
	// across the chest, the lock turned up to the view (half cock, priming, cock)
	const FPlacement Lock{ FVector(30.0, 7.0, -26.0), FRotator(12.0, -22.0, -38.0) };
	// cast about: butt down, muzzle up in front of the face (powder, ball, ramrod)
	const FPlacement Upright{ FVector(25.0, -4.0, -60.0), FRotator(75.0, -8.0, 0.0) };

	// the right mitten goes to the cartridge box on the belt, low on the right (relative to the camera)
	const FVector Pouch(12.0, 24.0, -48.0);

	FMusketFirstPersonKey MakeKey(float Time, const FPlacement& Musket, FName RightSocket, const FVector& RightOffset,
		FName LeftSocket = GripL, const FVector& LeftOffset = FVector::ZeroVector)
	{
		FMusketFirstPersonKey Key;
		Key.Time = Time;
		Key.MusketLocation = Musket.Location;
		Key.MusketRotation = Musket.Rotation;
		Key.RightHandSocket = RightSocket;
		Key.RightHandOffset = RightOffset;
		Key.LeftHandSocket = LeftSocket;
		Key.LeftHandOffset = LeftOffset;
		return Key;
	}
}

UMusketRules::UMusketRules()
{
	// first person: the mittens hold musket sockets (SK_Bean_Musket), the arms are solved from the shoulders
	HoldPose = MakeKey(0.0f, Ready, GripR, FVector::ZeroVector);
	AimPose = MakeKey(0.0f, Shouldered, GripR, FVector::ZeroVector);

	// reload: phases of the maquette bean-musket-anim (timeline.json, A_Musket_Reload is 6.583 s); the musket
	// ramrod animation plays with it, the right mitten follows the ramrod tip while it is out
	const FVector Thumb(-2.0, 3.0, 3.0);
	const FVector PrimeOver(0.0, 2.0, 9.0);
	const FVector Left(0.0, 0.0, -3.0);
	ReloadKeys = {
		MakeKey(0.00f, Ready, GripR, FVector::ZeroVector),
		MakeKey(0.45f, Lock, Pan, Thumb),                                  // half cock
		MakeKey(1.00f, Lock, Pan, Thumb),
		MakeKey(1.20f, Lock, NAME_None, Pouch),                            // powder flask
		MakeKey(1.45f, Lock, Pan, PrimeOver),                              // priming
		MakeKey(1.90f, Lock, Pan, PrimeOver),
		MakeKey(2.15f, Lock, GripR, FVector::ZeroVector),                  // close the frizzen
		MakeKey(2.65f, Upright, NAME_None, Pouch, Barrel, Left),           // cast about, cartridge
		MakeKey(2.95f, Upright, NAME_None, Pouch, Barrel, Left),
		MakeKey(3.15f, Upright, Muzzle, FVector(8.0, 0.0, 0.0), Barrel, Left),   // powder
		MakeKey(3.50f, Upright, Muzzle, FVector(6.0, 0.0, 0.0), Barrel, Left),
		MakeKey(3.70f, Upright, NAME_None, Pouch, Barrel, Left),           // ball
		MakeKey(4.05f, Upright, NAME_None, Pouch, Barrel, Left),
		MakeKey(4.20f, Upright, Muzzle, FVector(4.0, 0.0, 0.0), Barrel, Left),
		MakeKey(4.50f, Upright, Muzzle, FVector(2.0, 0.0, 0.0), Barrel, Left),
		MakeKey(4.62f, Upright, RamrodTip, FVector(-5.0, 0.0, 0.0), Barrel, Left),    // draw the ramrod
		MakeKey(4.85f, Upright, RamrodTip, FVector(-18.0, 0.0, 0.0), Barrel, Left),   // ram (strokes from the animation)
		MakeKey(5.62f, Upright, RamrodTip, FVector(-18.0, 0.0, 0.0), Barrel, Left),
		MakeKey(5.75f, Upright, RamrodTip, FVector(-24.0, 0.0, 0.0), Barrel, Left),   // return it
		MakeKey(5.96f, Upright, RamrodTip, FVector(-24.0, 0.0, 0.0), Barrel, Left),
		MakeKey(6.15f, Lock, Pan, Thumb),                                  // cock
		MakeKey(6.35f, Lock, Pan, Thumb),
		MakeKey(6.583f, Ready, GripR, FVector::ZeroVector),
	};

	// third person: the musket relative to hand_r along the body animations (scripts/props/musket_tp_tracks.py)
#include "Weapons/MusketThirdPersonTracks.inl"

	// musket grip on hand_r in third person without a track (attach_tp.json, first frame of the tracks)
	HandOffset = FTransform(FRotator(-27.28, -113.87, -160.12), FVector(-0.48, -7.67, -0.03));
}
