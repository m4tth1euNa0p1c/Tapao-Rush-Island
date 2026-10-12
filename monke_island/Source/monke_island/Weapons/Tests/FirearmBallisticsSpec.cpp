// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiLimbs.h"
#include "Creatures/TikiRules.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/PistolRules.h"
#include "Weapons/ShotgunRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** Damage of one pellet of the default shotgun at a distance (cm) */
	float PelletAt(const UShotgunRules* Shotgun, float Distance)
	{
		return Shotgun->Damage * FirearmDamage::ScaleAtDistance(Shotgun->DamageFalloff, Distance);
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Firearm.Ballistics
BEGIN_DEFINE_SPEC(FFirearmBallisticsSpec, "monke_island.Firearm.Ballistics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFirearmBallisticsSpec)

void FFirearmBallisticsSpec::Define()
{
	Describe("Falloff", [this]()
	{
		It("follows the curve: flat before its first point and after its last, linear between", [this]()
		{
			const TArray<FVector2D> Curve = { FVector2D(0.0, 1.0), FVector2D(500.0, 1.0), FVector2D(1000.0, 0.35), FVector2D(2500.0, 0.08) };
			TestEqual(TEXT("point blank"), FirearmDamage::ScaleAtDistance(Curve, 0.0f), 1.0f);
			TestEqual(TEXT("5 m"), FirearmDamage::ScaleAtDistance(Curve, 500.0f), 1.0f);
			TestEqual(TEXT("7.5 m, half way"), FirearmDamage::ScaleAtDistance(Curve, 750.0f), 0.675f, 0.001f);
			TestEqual(TEXT("10 m"), FirearmDamage::ScaleAtDistance(Curve, 1000.0f), 0.35f, 0.001f);
			TestEqual(TEXT("past the last point"), FirearmDamage::ScaleAtDistance(Curve, 4000.0f), 0.08f, 0.001f);
			TestEqual(TEXT("empty curve keeps the damage"), FirearmDamage::ScaleAtDistance({}, 900.0f), 1.0f);
		});
	});

	Describe("Shotgun", [this]()
	{
		It("kills any tiki at 4 m with its pellets on the body, never at 10 m", [this]()
		{
			const UShotgunRules* Shotgun = GetDefault<UShotgunRules>();
			const UTikiRules* Tiki = GetDefault<UTikiRules>();
			const float Toughest = FMath::Max(Tiki->VillagerHealth, Tiki->WarriorHealth);
			const float Weakest = FMath::Min(Tiki->VillagerHealth, Tiki->WarriorHealth);
			// at 4 m the cone is about 17 cm wide from the hip: all the pellets on the body; even 5 of 8 kill a warrior
			TestTrue(TEXT("all pellets at 4 m kill the toughest"), PelletAt(Shotgun, 400.0f) * Shotgun->PelletCount >= Toughest);
			TestTrue(TEXT("5 pellets at 4 m kill the toughest"), PelletAt(Shotgun, 400.0f) * 5.0f >= Toughest);
			TestTrue(TEXT("4 m is a close (shredding) shot"), 400.0f <= Shotgun->CloseBurstDistanceCm);
			TestTrue(TEXT("shreds 2 to 3 limbs"), Shotgun->CloseShredLimbsMin == 2 && Shotgun->CloseShredLimbsMax == 3);
			// at 10 m: even every pellet on the body does not kill the weakest
			TestTrue(TEXT("all pellets at 10 m do not kill the weakest"), PelletAt(Shotgun, 1000.0f) * Shotgun->PelletCount < Weakest);
			TestTrue(TEXT("10 m is not close"), 1000.0f > Shotgun->CloseBurstDistanceCm);
			TestTrue(TEXT("damage keeps falling beyond"), PelletAt(Shotgun, 2000.0f) < PelletAt(Shotgun, 1000.0f) * 0.5f);
		});

		It("tears a limb struck by two pellets at 10 m, not by one, and any limb struck at 4 m", [this]()
		{
			const UShotgunRules* Shotgun = GetDefault<UShotgunRules>();
			const UTikiRules* Tiki = GetDefault<UTikiRules>();
			const float OnLimbAt10 = PelletAt(Shotgun, 1000.0f) * Shotgun->LimbDamageScale;
			const float OnLimbAt4 = PelletAt(Shotgun, 400.0f) * Shotgun->LimbDamageScale;
			for (const FTikiLimb& Limb : Tiki->Limbs)
			{
				if (Limb.Loss == ETikiLimbLoss::Leg)
				{
					// thicker: three pellets at 10 m
					TArray<float> Damage;
					TikiLimbs::AddDamage(Damage, 0, OnLimbAt10, Limb.Health);
					TikiLimbs::AddDamage(Damage, 0, OnLimbAt10, Limb.Health);
					TestTrue(*FString::Printf(TEXT("%s: three pellets at 10 m"), *Limb.Part.ToString()), TikiLimbs::AddDamage(Damage, 0, OnLimbAt10, Limb.Health));
				}
				else
				{
					TArray<float> Damage;
					TestFalse(*FString::Printf(TEXT("%s: one pellet at 10 m"), *Limb.Part.ToString()), TikiLimbs::AddDamage(Damage, 0, OnLimbAt10, Limb.Health));
					TestTrue(*FString::Printf(TEXT("%s: two pellets at 10 m"), *Limb.Part.ToString()), TikiLimbs::AddDamage(Damage, 0, OnLimbAt10, Limb.Health));
				}
				TArray<float> Close;
				TestTrue(*FString::Printf(TEXT("%s: one pellet at 4 m"), *Limb.Part.ToString()), TikiLimbs::AddDamage(Close, 0, OnLimbAt4, Limb.Health));
			}
		});
	});

	Describe("Pistol", [this]()
	{
		It("takes three balls on the body of a warrior, one on a limb tears it", [this]()
		{
			const UPistolRules* Pistol = GetDefault<UPistolRules>();
			const UTikiRules* Tiki = GetDefault<UTikiRules>();
			TestEqual(TEXT("about 35"), Pistol->Damage, 35.0f);
			TestEqual(TEXT("three balls for a warrior"), FMath::CeilToInt(Tiki->WarriorHealth / Pistol->Damage), 3);
			for (const FTikiLimb& Limb : Tiki->Limbs)
			{
				TArray<float> Damage;
				TestTrue(*FString::Printf(TEXT("one ball tears %s"), *Limb.Part.ToString()),
					TikiLimbs::AddDamage(Damage, 0, Pistol->Damage * Pistol->LimbDamageScale, Limb.Health));
			}
		});
	});

	Describe("Grouping", [this]()
	{
		It("adds the pellets of one bone up, keeps the bones apart, and finds the strongest", [this]()
		{
			TArray<FFirearmBoneHit> Groups;
			FirearmDamage::AddPellet(Groups, TEXT("upperarm_l"), 10.0f, FVector(1.0, 0.0, 0.0));
			FirearmDamage::AddPellet(Groups, TEXT("spine_02"), 16.0f, FVector(2.0, 0.0, 0.0));
			FirearmDamage::AddPellet(Groups, TEXT("upperarm_l"), 10.0f, FVector(3.0, 0.0, 0.0));
			FirearmDamage::AddPellet(Groups, NAME_None, 5.0f, FVector::ZeroVector);
			TestEqual(TEXT("three groups"), Groups.Num(), 3);
			TestEqual(TEXT("arm: two pellets"), Groups[0].Pellets, 2);
			TestEqual(TEXT("arm: 20 damage"), Groups[0].Damage, 20.0f);
			TestEqual(TEXT("arm: first impact kept"), Groups[0].Impact, FVector(1.0, 0.0, 0.0));
			TestEqual(TEXT("strongest is the arm"), FirearmDamage::StrongestGroup(Groups), 0);
			TestEqual(TEXT("none in an empty list"), FirearmDamage::StrongestGroup({}), static_cast<int32>(INDEX_NONE));
		});

		It("gives a projectile the bone it passes nearest to, none beyond the radius", [this]()
		{
			// a torso bone along Z at x = 0, an arm bone along Z at y = 25
			const TArray<FVector> Joints = { FVector(0.0, 0.0, 120.0), FVector(0.0, 25.0, 110.0) };
			const TArray<FVector> Parents = { FVector(0.0, 0.0, 80.0), FVector(0.0, 25.0, 140.0) };
			const FVector Start(-500.0, 0.0, 100.0);
			TestEqual(TEXT("through the torso"), FirearmDamage::NearestBone(Joints, Parents, Start, FVector(500.0, 0.0, 100.0), 18.0f), 0);
			TestEqual(TEXT("through the arm"), FirearmDamage::NearestBone(Joints, Parents, FVector(-500.0, 24.0, 120.0), FVector(500.0, 24.0, 120.0), 18.0f), 1);
			TestEqual(TEXT("beside the body"), FirearmDamage::NearestBone(Joints, Parents, FVector(-500.0, 80.0, 120.0), FVector(500.0, 80.0, 120.0), 18.0f),
				static_cast<int32>(INDEX_NONE));
		});
	});
}

#endif
