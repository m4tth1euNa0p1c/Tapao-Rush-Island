// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/FirearmMath.h"
#include "Weapons/PirateMusketWeapon.h"
#include "Weapons/PistolWeapon.h"
#include "Weapons/ShotgunWeapon.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Stance
BEGIN_DEFINE_SPEC(FFirearmStanceSpec, "monke_island.Weapons.Stance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFirearmStanceSpec)

void FFirearmStanceSpec::Define()
{
	It("shoots tight when steady or aiming, wide on the move (no aiming, owner 2026-10-09)", [this]()
	{
		TestEqual(TEXT("steady"), FirearmMath::SpreadForStance(false, false, 0.6f, 4.0f), 0.6f);
		TestEqual(TEXT("moving"), FirearmMath::SpreadForStance(false, true, 0.6f, 4.0f), 4.0f);
		TestEqual(TEXT("aiming on the move, when the rules allow it"), FirearmMath::SpreadForStance(true, true, 0.6f, 4.0f), 0.6f);
		TestEqual(TEXT("aiming steady"), FirearmMath::SpreadForStance(true, false, 0.6f, 4.0f), 0.6f);
	});

	Describe("Aim of the carrier and of the server (network review T-268 R-38)", [this]()
	{
		It("aims only when wanted, allowed by the rules, usable and free of a gesture that lowers it", [this]()
		{
			TestTrue(TEXT("click held"), FirearmMath::IsAimed(true, true, true, true));
			TestFalse(TEXT("click let go"), FirearmMath::IsAimed(false, true, true, true));
			TestFalse(TEXT("rules without aiming (Counter-Strike)"), FirearmMath::IsAimed(true, false, true, true));
			TestFalse(TEXT("swimming, carrying, knocked out"), FirearmMath::IsAimed(true, true, false, true));
			TestFalse(TEXT("reloading"), FirearmMath::IsAimed(true, true, true, false));
		});

		It("lowers the aim during a reload on both machines and raises it again on both while the click is held", [this]()
		{
			const APistolWeapon* Pistol = GetDefault<APistolWeapon>();
			// the same wish and the same replicated gesture give the same aim on the server and on the carrier
			const bool bWanted = true;
			for (const EFirearmGesture Gesture : { EFirearmGesture::Reload, EFirearmGesture::None })
			{
				const bool bServer = FirearmMath::IsAimed(bWanted, true, true, !Pistol->LowersAim(Gesture));
				const bool bCarrier = FirearmMath::IsAimed(bWanted, true, true, !Pistol->LowersAim(Gesture));
				TestEqual(TEXT("server and carrier agree"), bServer, bCarrier);
				TestEqual(TEXT("lowered only while the reload plays"), bServer, Gesture == EFirearmGesture::None);
			}
		});

		It("knows which gestures lower the aim", [this]()
		{
			const APistolWeapon* Pistol = GetDefault<APistolWeapon>();
			const AShotgunWeapon* Shotgun = GetDefault<AShotgunWeapon>();
			TestTrue(TEXT("pistol reload"), Pistol->LowersAim(EFirearmGesture::Reload));
			TestFalse(TEXT("pistol cock"), Pistol->LowersAim(EFirearmGesture::Cock));
			TestFalse(TEXT("pistol shot"), Pistol->LowersAim(EFirearmGesture::AimFire));
			for (const EFirearmGesture Gesture : { EFirearmGesture::ReloadStart, EFirearmGesture::ReloadInsert, EFirearmGesture::ReloadEnd,
				EFirearmGesture::ReloadEmpty })
			{
				TestTrue(TEXT("every step of the shotgun reload"), Shotgun->LowersAim(Gesture));
			}
			TestFalse(TEXT("shotgun shot"), Shotgun->LowersAim(EFirearmGesture::Fire));
			for (const AFirearmWeapon* Weapon : { static_cast<const AFirearmWeapon*>(Pistol), static_cast<const AFirearmWeapon*>(Shotgun),
				static_cast<const AFirearmWeapon*>(GetDefault<APirateMusketWeapon>()) })
			{
				TestTrue(TEXT("inspection"), Weapon->LowersAim(EFirearmGesture::Inspect));
				TestTrue(TEXT("empty weapon thrown away"), Weapon->LowersAim(EFirearmGesture::Discard));
				TestFalse(TEXT("carrying"), Weapon->LowersAim(EFirearmGesture::None));
			}
		});
	});

	Describe("Carry loops that hold while the speed or the run hesitates (owner report 2026-10-11, T-330)", [this]()
	{
		It("smooths the speed whatever the frame rate", [this]()
		{
			float Smoothed = 0.0f;
			for (int32 Frame = 0; Frame < 6; ++Frame)
			{
				Smoothed = FirearmMath::SmoothSpeed(Smoothed, 600.0f, 1.0f / 60.0f, 0.12f);
			}
			const float Once = FirearmMath::SmoothSpeed(0.0f, 600.0f, 0.1f, 0.12f);
			TestTrue(TEXT("six frames at 60 Hz like one frame of 0.1 s"), FMath::IsNearlyEqual(Smoothed, Once, 1.0f));
			TestTrue(TEXT("not there yet"), Once > 200.0f && Once < 600.0f);
			TestEqual(TEXT("no smoothing"), FirearmMath::SmoothSpeed(10.0f, 600.0f, 0.016f, 0.0f), 600.0f);
		});

		It("starts walking above the threshold and stops only well under it", [this]()
		{
			TestFalse(TEXT("idle at 35 cm/s"), FirearmMath::IsMovingForLoop(35.0f, 40.0f, 0.6f, false));
			TestTrue(TEXT("walks from 41 cm/s"), FirearmMath::IsMovingForLoop(41.0f, 40.0f, 0.6f, false));
			TestTrue(TEXT("still walks at 30 cm/s"), FirearmMath::IsMovingForLoop(30.0f, 40.0f, 0.6f, true));
			TestFalse(TEXT("stops under 24 cm/s"), FirearmMath::IsMovingForLoop(20.0f, 40.0f, 0.6f, true));
		});

		It("switches only once the new loop was wanted long enough and the old one played long enough", [this]()
		{
			TestFalse(TEXT("a run hesitating for a frame"), FirearmMath::ShouldSwitchCarryLoop(10.0, 9.99, 5.0, 0.15f, 0.35f));
			TestFalse(TEXT("the loop just started"), FirearmMath::ShouldSwitchCarryLoop(10.0, 9.5, 9.9, 0.15f, 0.35f));
			TestTrue(TEXT("a real change"), FirearmMath::ShouldSwitchCarryLoop(10.0, 9.8, 9.0, 0.15f, 0.35f));
		});

		It("keeps the phase of the step from the walk to the run", [this]()
		{
			TestTrue(TEXT("half way stays half way"), FMath::IsNearlyEqual(FirearmMath::SamePhaseTime(0.5f, 1.0f, 0.8f), 0.4f));
			TestTrue(TEXT("a looped time"), FMath::IsNearlyEqual(FirearmMath::SamePhaseTime(2.25f, 1.0f, 2.0f), 0.5f));
			TestEqual(TEXT("no length"), FirearmMath::SamePhaseTime(0.5f, 0.0f, 0.8f), 0.0f);
		});
	});
}

#endif
