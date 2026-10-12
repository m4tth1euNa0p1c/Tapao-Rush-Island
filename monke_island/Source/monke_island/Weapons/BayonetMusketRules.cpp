// monke_island

#include "Weapons/BayonetMusketRules.h"
#include "Weapons/CutlassCameraShake.h"

UBayonetMusketRules::UBayonetMusketRules()
{
	// the musket's ball (docs/features/musket.md: 70 at 80 m); new_weapons_ue.py copies DA_PirateMusketRules' tuned shot
	Damage = 70.0f;

	// the thrust: a tiki villager (60) in two, a warrior (80) in two; a short step forward
	Stab.Damage = 55.0f;
	Stab.ReachCm = 150.0f;
	Stab.HalfAngleDegrees = 30.0f;
	Stab.MaxTargets = 1;
	Stab.StaminaCostFraction = 0.12f;
	Stab.LungeDistanceCm = 40.0f;
	Stab.LungeSeconds = 0.18f;
	Stab.KnockbackSpeed = 220.0f;
	Stab.KnockbackUpSpeed = 50.0f;
	Stab.PhysicsImpulse = 30000.0f;
	Stab.HitEvent = TEXT("StabHit");
	Stab.HitShakeScale = 1.0f;

	// the charge on the run: a warrior or a soldier (100) in one, a longer reach and lunge, twice the stamina
	Charge.Damage = 110.0f;
	Charge.ReachCm = 190.0f;
	Charge.HalfAngleDegrees = 25.0f;
	Charge.MaxTargets = 2;
	Charge.StaminaCostFraction = 0.25f;
	Charge.LungeDistanceCm = 160.0f;
	Charge.LungeSeconds = 0.3f;
	Charge.KnockbackSpeed = 520.0f;
	Charge.KnockbackUpSpeed = 90.0f;
	Charge.PhysicsImpulse = 60000.0f;
	Charge.HitEvent = TEXT("ChargeHit");
	Charge.HitShakeScale = 1.6f;

	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	HitCameraShake = UCutlassHitCameraShake::StaticClass();

	// the musket's two bayonet gestures after its own, those of the pack of T-334 (art/export/Weapons/BayonetMusket, 24 fps;
	// scripts/editor/new_weapons_ue.py writes the manifest's over them); the shot and the reload are the pirate musket's
	Clips.Add(FFirearmClip::Make(EFirearmGesture::BayonetStab, 15, 24, false, { { TEXT("StabStart"), 3 }, { TEXT("StabHit"), 7 } }));
	Clips.Add(FFirearmClip::Make(EFirearmGesture::BayonetCharge, 24, 24, false, { { TEXT("ChargeStart"), 2 }, { TEXT("ChargeHit"), 16 } }));
}
