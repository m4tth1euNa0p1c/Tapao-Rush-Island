// monke_island

#include "Weapons/CutlassRules.h"

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

	// parry: the guard grip, blade laid across in front of the face
	ParryKey = GuardKey;
	ParryKey.GripLocation += FVector(-4.0, 6.0, 18.0);
	ParryKey.GripRotation = FRotator(15.0, 180.0, 90.0);
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
