// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/MusketMath.h"
#include "Weapons/MusketRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons
BEGIN_DEFINE_SPEC(FMusketSpec, "monke_island.Weapons.Musket",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UMusketRules* Rules = nullptr;
END_DEFINE_SPEC(FMusketSpec)

void FMusketSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UMusketRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	It("gives each player 23 balls, one of them already in the barrel", [this]()
	{
		TestEqual(TEXT("default MaxBalls"), Rules->MaxBalls, 23);
		const MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(Rules->MaxBalls, Rules->bStartLoaded);
		TestTrue(TEXT("loaded"), Ammo.bLoaded);
		TestEqual(TEXT("pouch"), Ammo.PouchBalls, 22);
		TestEqual(TEXT("total"), Ammo.Total(), 23);
	});

	It("starts empty when bStartLoaded is off, and never below zero", [this]()
	{
		const MusketMath::FAmmo Empty = MusketMath::StartingAmmo(23, false);
		TestFalse(TEXT("not loaded"), Empty.bLoaded);
		TestEqual(TEXT("pouch"), Empty.PouchBalls, 23);
		const MusketMath::FAmmo None = MusketMath::StartingAmmo(0, true);
		TestFalse(TEXT("nothing to load"), None.bLoaded);
		TestEqual(TEXT("no ball"), None.PouchBalls, 0);
	});

	It("fires once, then needs a full reload that takes one ball from the pouch", [this]()
	{
		MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(23, true);
		TestTrue(TEXT("can fire"), MusketMath::CanFire(Ammo, false, false));
		TestFalse(TEXT("cannot reload a loaded musket"), MusketMath::CanReload(Ammo, false, false));
		MusketMath::Fire(Ammo);
		TestFalse(TEXT("empty barrel cannot fire"), MusketMath::CanFire(Ammo, false, false));
		TestFalse(TEXT("no reload during the recoil"), MusketMath::CanReload(Ammo, false, true));
		TestTrue(TEXT("can reload"), MusketMath::CanReload(Ammo, false, false));
		TestTrue(TEXT("reload done"), MusketMath::CompleteReload(Ammo));
		TestEqual(TEXT("pouch after one reload"), Ammo.PouchBalls, 21);
		TestEqual(TEXT("total after one shot"), Ammo.Total(), 22);
	});

	It("shoots exactly 23 times with 23 balls", [this]()
	{
		MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(23, true);
		int32 Shots = 0;
		for (int32 Guard = 0; Guard < 100; ++Guard)
		{
			if (MusketMath::CanFire(Ammo, false, false))
			{
				MusketMath::Fire(Ammo);
				++Shots;
			}
			else if (!MusketMath::CompleteReload(Ammo))
			{
				break;
			}
		}
		TestEqual(TEXT("shots"), Shots, 23);
		TestEqual(TEXT("nothing left"), Ammo.Total(), 0);
		TestFalse(TEXT("no reload without balls"), MusketMath::CanReload(Ammo, false, false));
	});

	It("cannot fire while reloading", [this]()
	{
		MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(23, true);
		TestFalse(TEXT("reloading"), MusketMath::CanFire(Ammo, true, false));
		TestFalse(TEXT("recovering"), MusketMath::CanFire(Ammo, false, true));
	});

	It("reloads in a few seconds and plays its first person keys in order", [this]()
	{
		// class default 6 s; the shipped DA_MusketRules is tuned shorter (1.5 s, owner request 2026-10-06): both stay in a playable range
		TestEqual(TEXT("default reload time"), Rules->ReloadSeconds, 6.0f, 0.01f);
		if (const UMusketRules* Shipped = LoadObject<UMusketRules>(nullptr, TEXT("/Game/MonkeIsland/Weapons/Musket/DA_MusketRules.DA_MusketRules")))
		{
			TestTrue(FString::Printf(TEXT("DA reload time %.2f s in [1, 6]"), Shipped->ReloadSeconds),
				Shipped->ReloadSeconds >= 1.0f && Shipped->ReloadSeconds <= 6.0f);
		}
		TestTrue(TEXT("reload keys"), Rules->ReloadKeys.Num() >= 10);
		for (int32 Index = 1; Index < Rules->ReloadKeys.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("key %d sorted"), Index), Rules->ReloadKeys[Index].Time >= Rules->ReloadKeys[Index - 1].Time);
		}
	});

	It("samples the key track: ends held, eased in between", [this]()
	{
		FMusketFirstPersonKey A;
		A.Time = 0.0f;
		A.MusketLocation = FVector(0.0, 0.0, 0.0);
		FMusketFirstPersonKey B;
		B.Time = 2.0f;
		B.MusketLocation = FVector(10.0, 0.0, 0.0);
		const TArray<FMusketFirstPersonKey> Keys = { A, B };
		const FMusketFirstPersonKey Fallback;

		TestEqual(TEXT("before"), MusketMath::SampleKeys(Keys, -1.0f, Fallback).MusketLocation.X, 0.0, 0.001);
		TestEqual(TEXT("middle"), MusketMath::SampleKeys(Keys, 1.0f, Fallback).MusketLocation.X, 5.0, 0.001);
		TestTrue(TEXT("eased start"), MusketMath::SampleKeys(Keys, 0.5f, Fallback).MusketLocation.X < 2.5);
		TestEqual(TEXT("after"), MusketMath::SampleKeys(Keys, 5.0f, Fallback).MusketLocation.X, 10.0, 0.001);
		TestEqual(TEXT("empty track"), MusketMath::SampleKeys({}, 1.0f, B).MusketLocation.X, 10.0, 0.001);
	});

	It("puts the first person mitten on its target and stretches the arm only so far", [this]()
	{
		const FVector Shoulder(-5.0, 18.0, -20.0);
		const FVector HandPoint(36.0, 0.0, -1.5);
		for (const FVector& Target : { FVector(30.0, 13.0, -20.0), FVector(45.0, -6.0, 5.0), FVector(10.0, 20.0, -50.0) })
		{
			const FBeanArmPose Arm = MusketMath::SolveArm(Shoulder, Target, HandPoint, 1.6f);
			const FVector Hand = Arm.Location + Arm.Rotation.RotateVector(FVector(HandPoint.X * Arm.Stretch, HandPoint.Y, HandPoint.Z));
			TestTrue(FString::Printf(TEXT("mitten on %s"), *Target.ToString()), Hand.Equals(Target, 0.01));
			TestTrue(TEXT("stretch in range"), Arm.Stretch >= 1.0f && Arm.Stretch <= 1.6f);
		}
		const FBeanArmPose Mirrored = MusketMath::MirrorArm(FBeanArmPose(FVector(1.0, -2.0, 3.0), FRotator(10.0, -20.0, 30.0), 1.2f));
		TestEqual(TEXT("mirrored Y"), Mirrored.Location.Y, 2.0, 0.001);
		TestEqual(TEXT("mirrored yaw"), Mirrored.Rotation.Yaw, 20.0, 0.001);
		TestEqual(TEXT("mirrored roll"), Mirrored.Rotation.Roll, -30.0, 0.001);
	});

	It("turns the musket the short way between keys", [this]()
	{
		FMusketFirstPersonKey A;
		A.MusketRotation = FRotator(0.0, 0.0, 170.0);
		FMusketFirstPersonKey B;
		B.MusketRotation = FRotator(0.0, 0.0, -170.0);
		const FRotator Middle = MusketMath::BlendKeys(A, B, 0.5f).MusketRotation;
		TestTrue(TEXT("through 180, not through 0"), FMath::Abs(FRotator::NormalizeAxis(Middle.Roll)) > 175.0f);
	});

	It("samples the third person musket tracks between frames", [this]()
	{
		const TArray<FTransform> Track = { FTransform(FVector(0.0, 0.0, 0.0)), FTransform(FVector(10.0, 0.0, 0.0)) };
		TestEqual(TEXT("half a frame"), MusketMath::SampleTrack(Track, 0.5f / 24.0f, 24.0f, FTransform::Identity).GetLocation().X, 5.0, 0.001);
		TestEqual(TEXT("held after the end"), MusketMath::SampleTrack(Track, 3.0f, 24.0f, FTransform::Identity).GetLocation().X, 10.0, 0.001);
		TestEqual(TEXT("fallback"), MusketMath::SampleTrack({}, 0.0f, 24.0f, FTransform(FVector(7.0, 0.0, 0.0))).GetLocation().X, 7.0, 0.001);
		TestTrue(TEXT("reload track baked"), Rules->ReloadMusketTrack.Num() > 100 && Rules->HoldMusketTrack.Num() > 0 && Rules->FireMusketTrack.Num() > 0);
		TestTrue(TEXT("hold track starts on the grip"), Rules->HoldMusketTrack[0].Equals(Rules->HandOffset, 0.05));
	});

	It("keeps the shot inside the spread cone", [this]()
	{
		FRandomStream Stream(1234);
		const FVector Aim(1.0, 0.0, 0.0);
		for (int32 Index = 0; Index < 200; ++Index)
		{
			const FVector Direction = MusketMath::SpreadDirection(Aim, 4.0f, Stream);
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Direction, Aim)));
			if (!TestTrue(TEXT("inside 4 degrees"), Angle <= 4.01f))
			{
				break;
			}
		}
		TestEqual(TEXT("no spread"), MusketMath::SpreadDirection(Aim, 0.0f, Stream).X, 1.0, 0.0001);
	});
}

#endif
