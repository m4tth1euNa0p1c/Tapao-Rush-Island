// monke_island

#include "Weapons/CutlassRules.h"
#include "Weapons/CutlassCameraShake.h"

namespace
{
	FCutlassKey MakeCutlassKey(float Time, const FVector& Location, const FRotator& Rotation, float ScabbardAlpha)
	{
		FCutlassKey Key;
		Key.Time = Time;
		Key.GripLocation = Location;
		Key.GripRotation = Rotation;
		Key.ScabbardAlpha = ScabbardAlpha;
		return Key;
	}

	FCutlassViewKey MakeViewKey(float Time, const FVector& Location, const FRotator& Rotation)
	{
		FCutlassViewKey Key;
		Key.Time = Time;
		Key.Location = Location;
		Key.Rotation = Rotation;
		return Key;
	}

	/** The guard key of a first person track at Time */
	FCutlassViewKey ViewKeyAt(const FCutlassViewKey& Key, float Time)
	{
		FCutlassViewKey Out = Key;
		Out.Time = Time;
		return Out;
	}
}

UCutlassRules::UCutlassRules()
{
	// values of the maquette bean-cutlass (gameplay block)
	SlashRight.Damage = 35.0f;
	SlashLeft.Damage = 35.0f;
	Thrust.Damage = 45.0f;
	Thrust.bCut = false;
	Thrust.ArcDegrees = 40.0f;
	Finisher.Damage = 70.0f;
	Finisher.ArcDegrees = 60.0f;

	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	float SlashRightHitTime = 0.0f;
	float SlashLeftHitTime = 0.0f;
	float FinisherHitTime = 0.0f;
	float ThrustHitTime = 0.0f;
	TArray<FCutlassKey> SlashRightKeys;
	TArray<FCutlassKey> SlashLeftKeys;
	TArray<FCutlassKey> FinisherKeys;
	TArray<FCutlassKey> ThrustKeys;
#include "Weapons/CutlassPoses.inl"
	SlashRight.HitTime = SlashRightHitTime;
	SlashRight.Keys = MoveTemp(SlashRightKeys);
	SlashLeft.HitTime = SlashLeftHitTime;
	SlashLeft.Keys = MoveTemp(SlashLeftKeys);
	Finisher.HitTime = FinisherHitTime;
	Finisher.Keys = MoveTemp(FinisherKeys);
	Thrust.HitTime = ThrustHitTime;
	Thrust.Keys = MoveTemp(ThrustKeys);

	// T-152: active frames around each hit, where the blade really crosses the front of the bean (CutlassPoses.inl),
	// and a short step forward on the two cuts
	SlashRight.ActiveStartTime = 0.17f;
	SlashRight.ActiveEndTime = 0.32f;
	SlashRight.LungeDistanceCm = 30.0f;
	SlashLeft.ActiveStartTime = 0.14f;
	SlashLeft.ActiveEndTime = 0.29f;
	SlashLeft.LungeDistanceCm = 30.0f;
	Thrust.ActiveStartTime = 0.16f;
	Thrust.ActiveEndTime = 0.28f;
	Finisher.ActiveStartTime = 0.49f;
	Finisher.ActiveEndTime = 0.62f;

	HitCameraShake = UCutlassHitCameraShake::StaticClass();
	BlockCameraShake = UCutlassHitCameraShake::StaticClass();

	// parry: the guard grip, blade laid across in front of the face
	ParryKey = GuardKey;
	ParryKey.GripLocation += FVector(-4.0, 6.0, 18.0);
	ParryKey.GripRotation = FRotator(15.0, 180.0, 90.0);

	// T-239, first person tracks (camera space: X forward, Y right, Z up; blade along +X, edge toward -Z at roll 0).
	// Same time axis as the mesh space tracks: one eased segment across each hit, so the blade is fastest and crosses
	// the centre of the view at the hit time (checked by the cutlass spec against the near clip plane).
	FirstPersonGuardKey = MakeViewKey(0.0f, FVector(30.0, 17.0, -15.0), FRotator(24.0, -6.0, 35.0));
	FirstPersonParryKey = MakeViewKey(0.0f, FVector(27.0, 13.0, -3.0), FRotator(38.0, -76.0, -30.0));
	const FCutlassViewKey& Guard = FirstPersonGuardKey;

	// right cut: wound up at the right edge of the view, swept across to the left edge (edge leading), back to the guard
	SlashRight.ViewKeys = {
		ViewKeyAt(Guard, 0.0f),
		MakeViewKey(0.15f, FVector(22.0, 22.0, -8.0), FRotator(30.0, 72.0, 90.0)),
		MakeViewKey(0.33f, FVector(30.0, -16.0, -10.0), FRotator(-8.0, -72.0, 90.0)),
		MakeViewKey(0.42f, FVector(31.0, 8.0, -14.0), FRotator(12.0, -30.0, 60.0)),
		ViewKeyAt(Guard, 0.54f),
	};
	// backhand cut: the hand crosses to the left edge, the blade sweeps back to the right edge
	SlashLeft.ViewKeys = {
		ViewKeyAt(Guard, 0.0f),
		MakeViewKey(0.11f, FVector(22.0, -14.0, -4.0), FRotator(28.0, -74.0, -90.0)),
		MakeViewKey(0.29f, FVector(30.0, 22.0, -12.0), FRotator(-6.0, 70.0, -90.0)),
		MakeViewKey(0.39f, FVector(31.0, 20.0, -14.0), FRotator(14.0, 14.0, 0.0)),
		ViewKeyAt(Guard, 0.51f),
	};
	// thrust: pulled back at the hip, the point driven to the centre of the view, back to the guard
	Thrust.ViewKeys = {
		ViewKeyAt(Guard, 0.0f),
		MakeViewKey(0.12f, FVector(23.0, 16.0, -11.0), FRotator(8.0, -12.0, 0.0)),
		MakeViewKey(0.24f, FVector(50.0, 5.0, -5.0), FRotator(3.0, -3.0, 0.0)),
		ViewKeyAt(Guard, 0.44f),
	};
	// long blow: the track starts raised above the view (the charge of the held click brings the blade there, see
	// ACutlassWeapon::ComputeViewPose), creeps up during the hold, then the blade falls through the whole view, stays low
	// and comes back; the hit of the gameplay (0.57 s) is the middle of the fall
	Finisher.ViewKeys = {
		MakeViewKey(0.0f, FVector(22.0, 10.0, 13.0), FRotator(72.0, 12.0, 0.0)),
		MakeViewKey(0.483f, FVector(20.0, 10.0, 15.0), FRotator(78.0, 14.0, -4.0)),
		MakeViewKey(0.66f, FVector(30.0, 8.0, -17.0), FRotator(-58.0, -4.0, 0.0)),
		MakeViewKey(0.82f, FVector(29.0, 11.0, -16.0), FRotator(-44.0, -8.0, 20.0)),
		ViewKeyAt(Guard, 1.06f),
	};
	// draw: up from the left hip with a short flourish; sheathe: the same way down
	FirstPersonDrawKeys = {
		MakeViewKey(0.0f, FVector(16.0, -14.0, -34.0), FRotator(40.0, -30.0, 0.0)),
		MakeViewKey(0.18f, FVector(20.0, -8.0, -22.0), FRotator(55.0, -35.0, 0.0)),
		MakeViewKey(0.42f, FVector(27.0, 4.0, -2.0), FRotator(50.0, -20.0, 40.0)),
		ViewKeyAt(Guard, 0.58f),
	};
	FirstPersonSheatheKeys = {
		ViewKeyAt(Guard, 0.0f),
		MakeViewKey(0.20f, FVector(26.0, 2.0, -4.0), FRotator(48.0, -24.0, 20.0)),
		MakeViewKey(0.44f, FVector(19.0, -9.0, -24.0), FRotator(50.0, -34.0, 0.0)),
		MakeViewKey(0.64f, FVector(16.0, -14.0, -34.0), FRotator(40.0, -30.0, 0.0)),
	};
}

const FCutlassAttack* UCutlassRules::GetAttack(ECutlassMove Move) const
{
	switch (Move)
	{
	case ECutlassMove::SlashRight: return &SlashRight;
	case ECutlassMove::SlashLeft: return &SlashLeft;
	case ECutlassMove::Thrust: return &Thrust;
	case ECutlassMove::Finisher: return &Finisher;
	default: return nullptr;
	}
}

const TArray<FCutlassKey>* UCutlassRules::GetKeys(ECutlassMove Move) const
{
	switch (Move)
	{
	case ECutlassMove::Draw: return &DrawKeys;
	case ECutlassMove::Sheathe: return &SheatheKeys;
	default:
		{
			const FCutlassAttack* Attack = GetAttack(Move);
			return Attack ? &Attack->Keys : nullptr;
		}
	}
}

const TArray<FCutlassViewKey>* UCutlassRules::GetViewKeys(ECutlassMove Move) const
{
	switch (Move)
	{
	case ECutlassMove::Draw: return &FirstPersonDrawKeys;
	case ECutlassMove::Sheathe: return &FirstPersonSheatheKeys;
	default:
		{
			const FCutlassAttack* Attack = GetAttack(Move);
			return Attack ? &Attack->ViewKeys : nullptr;
		}
	}
}

float UCutlassRules::GetMoveDuration(ECutlassMove Move) const
{
	switch (Move)
	{
	case ECutlassMove::Draw: return DrawSeconds;
	case ECutlassMove::Sheathe: return SheatheSeconds;
	default:
		{
			const TArray<FCutlassKey>* Keys = GetKeys(Move);
			return Keys && Keys->Num() > 0 ? Keys->Last().Time : 0.0f;
		}
	}
}
