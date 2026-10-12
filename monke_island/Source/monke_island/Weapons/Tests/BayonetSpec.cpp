// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/BayonetMath.h"
#include "Weapons/BayonetMusketRules.h"
#include "Weapons/BayonetMusketWeapon.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Bayonet
BEGIN_DEFINE_SPEC(FBayonetSpec, "monke_island.Weapons.Bayonet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UBayonetMusketRules* Rules = nullptr;
END_DEFINE_SPEC(FBayonetSpec)

void FBayonetSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UBayonetMusketRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Rules", [this]()
	{
		It("keeps the musket's shot and reload and adds the thrust and the charge", [this]()
		{
			TestEqual(TEXT("musket balls"), Rules->AmmoKind, EAmmoKind::MusketBall);
			TestEqual(TEXT("the musket's reload"), Rules->ReloadGesture, EFirearmGesture::GroundReload);
			TestNotNull(TEXT("its clip"), Rules->FindClip(EFirearmGesture::GroundReload));
			TestNotNull(TEXT("the musket's shot"), Rules->FindClip(EFirearmGesture::Fire));
			TestFalse(TEXT("fired from the hip: the right click is the bayonet's"), Rules->bAllowAiming);
			TestTrue(TEXT("the charge hits harder"), Rules->Charge.Damage > Rules->Stab.Damage);
			TestTrue(TEXT("and further"), Rules->Charge.ReachCm > Rules->Stab.ReachCm);
			TestTrue(TEXT("with a longer lunge"), Rules->Charge.LungeDistanceCm > Rules->Stab.LungeDistanceCm);
			TestTrue(TEXT("for more stamina"), Rules->Charge.StaminaCostFraction > Rules->Stab.StaminaCostFraction);
			TestTrue(TEXT("strikes pawns"), Rules->HitObjectTypes.Contains(UEngineTypes::ConvertToObjectType(ECC_Pawn)));
		});

		It("strikes at the StabHit and ChargeHit events of its gestures", [this]()
		{
			for (const bool bCharge : { false, true })
			{
				const FBayonetThrust& Thrust = Rules->GetThrust(bCharge);
				const FFirearmClip* Clip = Rules->FindClip(bCharge ? EFirearmGesture::BayonetCharge : EFirearmGesture::BayonetStab);
				if (!TestNotNull(TEXT("the gesture"), Clip))
				{
					continue;
				}
				const float Hit = Clip->FindEventTime(Thrust.HitEvent);
				TestTrue(TEXT("the point strikes inside the clip"), Hit > 0.0f && Hit < Clip->Duration);
			}
			TestEqual(TEXT("thrust event"), Rules->Stab.HitEvent, FName(TEXT("StabHit")));
			TestEqual(TEXT("charge event"), Rules->Charge.HitEvent, FName(TEXT("ChargeHit")));
		});

		It("lowers the aim while it thrusts", [this]()
		{
			const ABayonetMusketWeapon* Musket = GetDefault<ABayonetMusketWeapon>();
			TestTrue(TEXT("thrust"), Musket->LowersAim(EFirearmGesture::BayonetStab));
			TestTrue(TEXT("charge"), Musket->LowersAim(EFirearmGesture::BayonetCharge));
			TestFalse(TEXT("carrying"), Musket->LowersAim(EFirearmGesture::None));
		});
	});

	Describe("Cone", [this]()
	{
		It("strikes in a short cone in front of the carrier", [this]()
		{
			const FVector Forward(1.0, 0.0, 0.0);
			const float Reach = Rules->Stab.ReachCm;
			const float Angle = Rules->Stab.HalfAngleDegrees;
			const float Height = Rules->HeightToleranceCm;
			TestTrue(TEXT("in front"), BayonetMath::ThrustScore(Forward, FVector(100.0, 0.0, 0.0), Reach, Angle, Height) >= 0.0f);
			TestTrue(TEXT("at the edge of the cone"), BayonetMath::ThrustScore(Forward,
				FVector(FMath::Cos(FMath::DegreesToRadians(Angle - 1.0f)), FMath::Sin(FMath::DegreesToRadians(Angle - 1.0f)), 0.0) * 100.0, Reach, Angle, Height) >= 0.0f);
			TestTrue(TEXT("beside: out"), BayonetMath::ThrustScore(Forward, FVector(0.0, 100.0, 0.0), Reach, Angle, Height) < 0.0f);
			TestTrue(TEXT("behind: out"), BayonetMath::ThrustScore(Forward, FVector(-100.0, 0.0, 0.0), Reach, Angle, Height) < 0.0f);
			TestTrue(TEXT("beyond the reach: out"), BayonetMath::ThrustScore(Forward, FVector(Reach + 10.0f, 0.0, 0.0), Reach, Angle, Height) < 0.0f);
			TestTrue(TEXT("far above: out"), BayonetMath::ThrustScore(Forward, FVector(100.0, 0.0, Height + 10.0f), Reach, Angle, Height) < 0.0f);
			TestTrue(TEXT("a little below (a crab): in"), BayonetMath::ThrustScore(Forward, FVector(100.0, 0.0, -60.0), Reach, Angle, Height) >= 0.0f);
			TestEqual(TEXT("against the carrier: struck"), BayonetMath::ThrustScore(Forward, FVector(0.5, 0.5, 0.0), Reach, Angle, Height), 0.0f);
		});

		It("strikes the target right in front of the point first", [this]()
		{
			const FVector Forward(0.0, 1.0, 0.0);
			const float OnAxis = BayonetMath::ThrustScore(Forward, FVector(0.0, 120.0, 0.0), 150.0f, 30.0f, 140.0f);
			const float OffAxis = BayonetMath::ThrustScore(Forward, FVector(50.0, 110.0, 0.0), 150.0f, 30.0f, 140.0f);
			const float Nearer = BayonetMath::ThrustScore(Forward, FVector(0.0, 60.0, 0.0), 150.0f, 30.0f, 140.0f);
			TestTrue(TEXT("both in the cone"), OnAxis >= 0.0f && OffAxis >= 0.0f);
			TestTrue(TEXT("on the axis before off it"), OnAxis < OffAxis);
			TestTrue(TEXT("the nearer first"), Nearer < OnAxis);
			TestTrue(TEXT("the charge reaches further than the thrust"), BayonetMath::ThrustScore(FVector(1.0, 0.0, 0.0), FVector(170.0, 0.0, 0.0),
				Rules->Charge.ReachCm, Rules->Charge.HalfAngleDegrees, Rules->HeightToleranceCm) >= 0.0f
				&& BayonetMath::ThrustScore(FVector(1.0, 0.0, 0.0), FVector(170.0, 0.0, 0.0), Rules->Stab.ReachCm, Rules->Stab.HalfAngleDegrees,
					Rules->HeightToleranceCm) < 0.0f);
		});
	});

	Describe("Charge", [this]()
	{
		It("charges on the run, thrusts otherwise", [this]()
		{
			TestTrue(TEXT("sprinting"), BayonetMath::IsCharge(true, 0.0f, Rules->ChargeMinSpeedCmps));
			TestFalse(TEXT("walking (350 cm/s)"), BayonetMath::IsCharge(false, 350.0f, Rules->ChargeMinSpeedCmps));
			TestTrue(TEXT("fast enough without the sprint key"), BayonetMath::IsCharge(false, Rules->ChargeMinSpeedCmps + 1.0f, Rules->ChargeMinSpeedCmps));
			TestFalse(TEXT("speed alone does not count when the rules ask for the sprint"), BayonetMath::IsCharge(false, 900.0f, 0.0f));
			TestTrue(TEXT("the server's slack takes a charge slowed a little since the press"),
				BayonetMath::IsCharge(false, Rules->ChargeMinSpeedCmps * 0.9f, Rules->ChargeMinSpeedCmps * Rules->ChargeSpeedTolerance));
		});
	});

	Describe("Stamina", [this]()
	{
		It("costs a share of the stamina and is refused below it", [this]()
		{
			const float Max = 100.0f;
			TestEqual(TEXT("thrust cost"), BayonetMath::StaminaCost(Max, Rules->Stab.StaminaCostFraction), Max * Rules->Stab.StaminaCostFraction);
			TestTrue(TEXT("the charge costs more"), BayonetMath::StaminaCost(Max, Rules->Charge.StaminaCostFraction)
				> BayonetMath::StaminaCost(Max, Rules->Stab.StaminaCostFraction));
			TestTrue(TEXT("enough for a thrust"), BayonetMath::HasStaminaFor(Max * Rules->Stab.StaminaCostFraction, Max, Rules->Stab.StaminaCostFraction));
			TestFalse(TEXT("not below it"), BayonetMath::HasStaminaFor(Max * Rules->Stab.StaminaCostFraction - 1.0f, Max, Rules->Stab.StaminaCostFraction));
			TestFalse(TEXT("a thrust's stamina is not a charge's"), BayonetMath::HasStaminaFor(Max * Rules->Stab.StaminaCostFraction, Max,
				Rules->Charge.StaminaCostFraction));
			// a full bar: a few thrusts in a row, then the stamina has to come back
			float Stamina = Max;
			int32 Thrusts = 0;
			while (BayonetMath::HasStaminaFor(Stamina, Max, Rules->Stab.StaminaCostFraction) && Thrusts < 100)
			{
				Stamina -= BayonetMath::StaminaCost(Max, Rules->Stab.StaminaCostFraction);
				++Thrusts;
			}
			TestTrue(FString::Printf(TEXT("a bar is a few thrusts, not endless (%d)"), Thrusts), Thrusts >= 4 && Thrusts <= 12);
		});
	});

	Describe("When", [this]()
	{
		It("thrusts carrying, inspecting or reloading (the reload cut), never over the draw, a shot or another thrust", [this]()
		{
			const EFirearmGesture Reload = Rules->ReloadGesture;
			TestTrue(TEXT("carrying"), BayonetMath::MayThrust(EFirearmGesture::None, Reload, true));
			TestTrue(TEXT("inspecting"), BayonetMath::MayThrust(EFirearmGesture::Inspect, Reload, true));
			TestTrue(TEXT("reloading, the reload cut"), BayonetMath::MayThrust(Reload, Reload, true));
			TestFalse(TEXT("reloading, when the rules keep the reload"), BayonetMath::MayThrust(Reload, Reload, false));
			for (const EFirearmGesture Gesture : { EFirearmGesture::Equip, EFirearmGesture::Fire, EFirearmGesture::AimFire, EFirearmGesture::BayonetStab,
				EFirearmGesture::BayonetCharge, EFirearmGesture::Discard })
			{
				TestFalse(TEXT("never over these"), BayonetMath::MayThrust(Gesture, Reload, true));
			}
		});

		It("lunges toward the point and stops short of an enemy ahead", [this]()
		{
			TestEqual(TEXT("no enemy: the whole lunge"), BayonetMath::LungeDistance(160.0f, -1.0f, 70.0f), 160.0f);
			TestEqual(TEXT("an enemy at 150 cm: 80 cm"), BayonetMath::LungeDistance(160.0f, 150.0f, 70.0f), 80.0f);
			TestEqual(TEXT("an enemy at the point: no lunge"), BayonetMath::LungeDistance(160.0f, 50.0f, 70.0f), 0.0f);
			TestEqual(TEXT("an enemy far ahead: the whole lunge"), BayonetMath::LungeDistance(40.0f, 500.0f, 70.0f), 40.0f);
		});
	});
}

#endif
