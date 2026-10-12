// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/MusketMath.h"
#include "Weapons/PirateMusketRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Musket
BEGIN_DEFINE_SPEC(FPirateMusketSpec, "monke_island.Musket.Pirate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UPirateMusketRules* Rules = nullptr;
END_DEFINE_SPEC(FPirateMusketSpec)

void FPirateMusketSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UPirateMusketRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	It("holds the 24 gestures of bean-musket-anim, plus the shot reused when aiming", [this]()
	{
		// 25 with the v0.3 pack; the v2 pack adds Discard (owner 2026-10-09)
		TestTrue(TEXT("clips"), Rules->Clips.Num() >= 25);
		for (const EFirearmGesture Gesture : { EFirearmGesture::Equip, EFirearmGesture::Idle, EFirearmGesture::Walk, EFirearmGesture::Sprint,
			EFirearmGesture::AimIn, EFirearmGesture::AimIdle, EFirearmGesture::AimOut, EFirearmGesture::Fire, EFirearmGesture::AimFire,
			EFirearmGesture::GroundReload, EFirearmGesture::Holster })
		{
			TestNotNull(*FString::Printf(TEXT("clip %d"), static_cast<int32>(Gesture)), Rules->FindClip(Gesture));
		}
	});

	It("stretches the 5.5 s ground reload over ReloadSeconds", [this]()
	{
		const FFirearmClip* Reload = Rules->FindClip(Rules->ReloadGesture);
		if (!TestNotNull(TEXT("reload clip"), Reload))
		{
			return;
		}
		// weapons v2 (2026-10-09): the pack's ground reload is 132 frames at 24 fps
		TestEqual(TEXT("clip length"), Reload->Duration, 5.5f, 0.001f);
		// T-212: the reload rebuilt here, the ramrod struck twice
		TestEqual(TEXT("12 phase events"), Reload->Events.Num(), 12);
		TestEqual(TEXT("default reload time"), Rules->ReloadSeconds, 1.5f);
		const float Rate = MusketMath::StretchRate(Reload->Duration, Rules->ReloadSeconds);
		TestEqual(TEXT("rate"), Rate, 5.5f / 1.5f, 0.001f);
		TestEqual(TEXT("real length"), Reload->Duration / Rate, Rules->ReloadSeconds, 0.001f);
		TestEqual(TEXT("bad lengths play at 1"), MusketMath::StretchRate(0.0f, 1.5f), 1.0f);
		TestEqual(TEXT("bad target plays at 1"), MusketMath::StretchRate(6.0f, 0.0f), 1.0f);
	});

	It("keeps the lock time between the trigger and the shot", [this]()
	{
		for (const EFirearmGesture Gesture : { EFirearmGesture::Fire, EFirearmGesture::AimFire })
		{
			const FFirearmClip* Shot = Rules->FindClip(Gesture);
			if (!TestNotNull(TEXT("shot clip"), Shot))
			{
				return;
			}
			const float Trigger = Shot->FindEventTime(Rules->TriggerEvent);
			const float Ball = Shot->FindEventTime(Rules->FireEvent);
			TestTrue(TEXT("trigger found"), Trigger >= 0.0f);
			TestEqual(TEXT("lock time 3 frames at 24 fps"), Ball - Trigger, 0.125f, 0.001f);
		}
	});

	It("loads one ball from the pouch at the end of a reload only", [this]()
	{
		MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(Rules->MaxBalls, Rules->bStartLoaded);
		TestEqual(TEXT("total"), Ammo.Total(), 15);
		TestTrue(TEXT("fires"), MusketMath::CanFire(Ammo, false, false));
		MusketMath::Fire(Ammo);
		TestFalse(TEXT("busy reload refused"), MusketMath::CanReload(Ammo, true, false));
		TestTrue(TEXT("reloads"), MusketMath::CanReload(Ammo, false, false));
		// a cut reload changes nothing: the ball only moves at its end
		TestEqual(TEXT("pouch kept"), Ammo.PouchBalls, 14);
		TestTrue(TEXT("completed"), MusketMath::CompleteReload(Ammo));
		TestTrue(TEXT("loaded"), Ammo.bLoaded);
		TestEqual(TEXT("pouch: one ball went into the barrel"), Ammo.PouchBalls, 13);
	});
}

#endif
