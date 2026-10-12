// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/GrenadeMath.h"
#include "Weapons/GrenadeRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Grenade
BEGIN_DEFINE_SPEC(FGrenadeSpec, "monke_island.Weapons.Grenade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UGrenadeRules* Rules = nullptr;
END_DEFINE_SPEC(FGrenadeSpec)

void FGrenadeSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UGrenadeRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Rules", [this]()
	{
		It("carries six grenades at most, a bag of three at the start, a 3 s fuse, a 4 m blast of 90 at the centre", [this]()
		{
			TestEqual(TEXT("max"), Rules->MaxGrenades, 6);
			TestEqual(TEXT("start"), Rules->StartGrenades, 3);
			TestEqual(TEXT("fuse"), Rules->FuseSeconds, 3.0f);
			TestEqual(TEXT("radius"), Rules->BlastRadiusCm, 400.0f);
			TestEqual(TEXT("centre damage"), Rules->CenterDamage, 90.0f);
			TestTrue(TEXT("long throw faster than the short one"), Rules->LongThrowSpeed > Rules->ShortThrowSpeed);
			TestEqual(TEXT("grenades"), Rules->AmmoKind, EAmmoKind::Grenade);
			TestTrue(TEXT("players knocked out"), Rules->bKnockoutPlayers);
		});

		It("has the 13 gestures of the pack with the fuse lit before the throw and the next grenade at HammerCocked", [this]()
		{
			TestTrue(TEXT("gestures"), Rules->Clips.Num() >= 13);
			for (const EFirearmGesture Gesture : { EFirearmGesture::Equip, EFirearmGesture::Idle, EFirearmGesture::Walk, EFirearmGesture::Sprint,
				EFirearmGesture::AimIn, EFirearmGesture::AimIdle, EFirearmGesture::AimOut, EFirearmGesture::Fire, EFirearmGesture::AimFire,
				EFirearmGesture::Reload, EFirearmGesture::DryFire, EFirearmGesture::Inspect, EFirearmGesture::Holster })
			{
				TestNotNull(*FString::Printf(TEXT("clip %d"), static_cast<int32>(Gesture)), Rules->FindClip(Gesture));
			}
			for (const EFirearmGesture Gesture : { EFirearmGesture::Fire, EFirearmGesture::AimFire })
			{
				const FFirearmClip* Throw = Rules->FindClip(Gesture);
				if (!TestNotNull(TEXT("throw clip"), Throw))
				{
					continue;
				}
				const float Light = Throw->FindEventTime(Rules->LightEvent);
				const float Fire = Throw->FindEventTime(Rules->FireEvent);
				TestTrue(TEXT("lit"), Light >= 0.0f);
				TestTrue(TEXT("thrown after the light"), Fire > Light);
				TestTrue(TEXT("the wind-up plays before the throw"), Fire > 0.0f);
			}
			const FFirearmClip* Reload = Rules->FindClip(EFirearmGesture::Reload);
			TestTrue(TEXT("next grenade in hand at HammerCocked"), Reload && Reload->FindEventTime(Rules->ReloadLoadedEvent) >= 0.0f);
		});
	});

	Describe("Bag", [this]()
	{
		It("starts with one in hand and the rest in the bag, throws, takes the next, clicks when empty", [this]()
		{
			GrenadeMath::FGrenadeState State = GrenadeMath::Starting(Rules->MaxGrenades, Rules->StartGrenades, Rules->bStartInHand);
			TestTrue(TEXT("in hand"), State.bInHand);
			TestEqual(TEXT("bag"), State.Bag, 2);
			TestEqual(TEXT("total"), State.Total(), 3);
			TestEqual(TEXT("throw"), GrenadeMath::OnTrigger(State, false, true), GrenadeMath::ETrigger::Throw);
			TestEqual(TEXT("busy"), GrenadeMath::OnTrigger(State, true, true), GrenadeMath::ETrigger::None);
			GrenadeMath::Throw(State);
			TestFalse(TEXT("hand empty"), State.bInHand);
			TestEqual(TEXT("empty hand: take the next"), GrenadeMath::OnTrigger(State, false, true), GrenadeMath::ETrigger::TakeNext);
			TestEqual(TEXT("R takes the next"), GrenadeMath::OnReloadKey(State, false), GrenadeMath::ETrigger::TakeNext);
			TestTrue(TEXT("taken"), GrenadeMath::TakeNext(State));
			TestFalse(TEXT("hand full: nothing more taken"), GrenadeMath::TakeNext(State));
			TestEqual(TEXT("one left in the bag"), State.Bag, 1);
			GrenadeMath::Throw(State);
			GrenadeMath::TakeNext(State);
			GrenadeMath::Throw(State);
			TestEqual(TEXT("all thrown"), State.Total(), 0);
			TestEqual(TEXT("empty: click"), GrenadeMath::OnTrigger(State, false, true), GrenadeMath::ETrigger::DryFire);
			TestEqual(TEXT("R: nothing"), GrenadeMath::OnReloadKey(State, false), GrenadeMath::ETrigger::None);
		});

		It("never starts with more than the bag holds, nor a grenade in hand when asked not to", [this]()
		{
			const GrenadeMath::FGrenadeState Many = GrenadeMath::Starting(6, 20, true);
			TestEqual(TEXT("capped"), Many.Total(), 6);
			const GrenadeMath::FGrenadeState Bagged = GrenadeMath::Starting(6, 3, false);
			TestFalse(TEXT("hand empty"), Bagged.bInHand);
			TestEqual(TEXT("three in the bag"), Bagged.Bag, 3);
			const GrenadeMath::FGrenadeState None = GrenadeMath::Starting(6, 0, true);
			TestFalse(TEXT("nothing in hand"), None.bInHand);
		});
	});

	Describe("Blast", [this]()
	{
		It("deals the centre damage up to the full radius, then less and less to nothing at the blast radius", [this]()
		{
			const float Centre = Rules->CenterDamage;
			const float Full = Rules->FullDamageRadiusCm;
			const float Radius = Rules->BlastRadiusCm;
			TestEqual(TEXT("at the centre"), GrenadeMath::DamageAtDistance(Centre, 0.0f, Full, Radius), Centre);
			TestEqual(TEXT("at the full radius"), GrenadeMath::DamageAtDistance(Centre, Full, Full, Radius), Centre);
			TestEqual(TEXT("half way"), GrenadeMath::DamageAtDistance(Centre, (Full + Radius) * 0.5f, Full, Radius), Centre * 0.5f, 0.01f);
			TestEqual(TEXT("at the radius"), GrenadeMath::DamageAtDistance(Centre, Radius, Full, Radius), 0.0f);
			TestEqual(TEXT("beyond"), GrenadeMath::DamageAtDistance(Centre, Radius + 1.0f, Full, Radius), 0.0f);
			TestTrue(TEXT("lethal to a tiki warrior (80) within 1.5 m"), GrenadeMath::DamageAtDistance(Centre, 150.0f, Full, Radius) >= 80.0f);
			TestTrue(TEXT("a villager (60) survives beyond 3 m"), GrenadeMath::DamageAtDistance(Centre, 310.0f, Full, Radius) < 60.0f);
		});

		It("knocks a player away from the blast and a little up", [this]()
		{
			const FVector Velocity = GrenadeMath::KnockoutVelocity(FVector(300.0, 0.0, 50.0), Rules->KnockoutSpeed);
			TestEqual(TEXT("away"), static_cast<float>(Velocity.X), Rules->KnockoutSpeed, 0.01f);
			TestEqual(TEXT("up"), static_cast<float>(Velocity.Z), Rules->KnockoutSpeed * 0.5f, 0.01f);
			TestTrue(TEXT("sideways kept flat"), FMath::IsNearlyZero(Velocity.Y));
		});
	});

	Describe("Throw", [this]()
	{
		It("lobs above the aim at the asked speed, short from the hip and long when aimed", [this]()
		{
			const FVector Short = GrenadeMath::ThrowVelocity(FVector::ForwardVector, Rules->ShortThrowSpeed, Rules->ThrowPitchDegrees);
			const FVector Long = GrenadeMath::ThrowVelocity(FVector::ForwardVector, Rules->LongThrowSpeed, Rules->ThrowPitchDegrees);
			TestEqual(TEXT("short speed"), static_cast<float>(Short.Size()), Rules->ShortThrowSpeed, 0.01f);
			TestEqual(TEXT("long speed"), static_cast<float>(Long.Size()), Rules->LongThrowSpeed, 0.01f);
			TestTrue(TEXT("lifted"), Short.Z > 0.0 && Long.Z > 0.0);
			TestEqual(TEXT("lift angle"), FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Short.Z), static_cast<float>(Short.X))), Rules->ThrowPitchDegrees, 0.01f);
			const FVector Down = GrenadeMath::ThrowVelocity(FVector(0.0, 0.0, -1.0), 100.0f, 10.0f);
			TestTrue(TEXT("a straight down aim stays down"), Down.Z < 0.0);
			TestEqual(TEXT("no lift at the feet"), static_cast<float>(GrenadeMath::ThrowVelocity(FVector::ForwardVector, 150.0f, 0.0f).Z), 0.0f, 0.01f);
		});

		It("burns the fuse from the light: a grenade held two seconds flies one, never under the minimum", [this]()
		{
			TestEqual(TEXT("thrown at once"), GrenadeMath::FuseLeft(3.0f, 0.0f, 0.3f), 3.0f);
			TestEqual(TEXT("held two seconds"), GrenadeMath::FuseLeft(3.0f, 2.0f, 0.3f), 1.0f);
			TestEqual(TEXT("held too long"), GrenadeMath::FuseLeft(3.0f, 5.0f, 0.3f), 0.3f);
			TestEqual(TEXT("negative time counts as none"), GrenadeMath::FuseLeft(3.0f, -1.0f, 0.3f), 3.0f);
		});
	});
}

#endif
