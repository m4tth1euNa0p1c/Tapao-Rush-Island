// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/BoltRifleMath.h"
#include "Weapons/BoltRifleRules.h"
#include "Weapons/BoltRifleWeapon.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** The server's rifle without a world (ABoltRifleWeapon reduced to its rules): presses on fire, the shot clip from
	 *  its Fire event, the bolt cycle and the reload with their events, at the rules' clip lengths */
	struct FRifleSim
	{
		const UBoltRifleRules* Rules = nullptr;
		BoltRifleMath::FState State;
		float Time = 0.0f;
		EFirearmGesture Playing = EFirearmGesture::None;
		float GestureStart = 0.0f;
		float GestureEnd = 0.0f;
		bool bBoltClosed = false;
		bool bClipIn = false;
		TArray<float> ShotTimes;

		explicit FRifleSim(const UBoltRifleRules* InRules)
			: Rules(InRules)
		{
			State = BoltRifleMath::StartingRifle(Rules->MaxRounds, Rules->Capacity, Rules->StartLoadedRounds);
		}

		float EventTime(EFirearmGesture Gesture, FName Event) const
		{
			const FFirearmClip* Clip = Rules->FindClip(Gesture);
			return Clip ? Clip->FindEventTime(Event) : -1.0f;
		}

		void Start(EFirearmGesture Gesture, float ClipStart = 0.0f)
		{
			const FFirearmClip* Clip = Rules->FindClip(Gesture);
			Playing = Gesture;
			GestureStart = Time - ClipStart;
			GestureEnd = Time + (Clip ? Clip->Duration - ClipStart : 0.0f);
			bBoltClosed = false;
			bClipIn = false;
		}

		bool Busy() const
		{
			return Playing != EFirearmGesture::None && Time < GestureEnd;
		}

		void Press()
		{
			const bool bCut = Busy() && BoltRifleMath::CanCutForShot(Playing, State);
			switch (BoltRifleMath::OnTrigger(State, Busy() && !bCut, Rules->bAutoReloadOnEmptyFire))
			{
			case BoltRifleMath::ETrigger::Fire:
				BoltRifleMath::Fire(State);
				ShotTimes.Add(Time);
				Start(EFirearmGesture::Fire, EventTime(EFirearmGesture::Fire, Rules->FireEvent));
				break;
			case BoltRifleMath::ETrigger::CycleBolt:
				Start(EFirearmGesture::BoltCycle);
				break;
			case BoltRifleMath::ETrigger::Reload:
				Start(EFirearmGesture::Reload);
				break;
			default:
				break;
			}
		}

		/** Events and ends of the gesture up to Time */
		void Advance(float DeltaSeconds)
		{
			Time += DeltaSeconds;
			const float ClipTime = Time - GestureStart;
			if (Playing == EFirearmGesture::Reload && !bClipIn && ClipTime >= EventTime(EFirearmGesture::Reload, Rules->ReloadLoadedEvent))
			{
				BoltRifleMath::LoadClip(State, Rules->Capacity);
				bClipIn = true;
			}
			if ((Playing == EFirearmGesture::BoltCycle || Playing == EFirearmGesture::Reload) && !bBoltClosed
				&& ClipTime >= EventTime(Playing, Rules->BoltCloseEvent))
			{
				BoltRifleMath::CloseBolt(State);
				bBoltClosed = true;
			}
			if (Playing != EFirearmGesture::None && Time >= GestureEnd)
			{
				const EFirearmGesture Ended = Playing;
				Playing = EFirearmGesture::None;
				if (Ended == EFirearmGesture::Fire)
				{
					const EFirearmGesture Next = BoltRifleMath::AfterShot(State, Rules->bAutoCycleBolt, Rules->bAutoReloadAfterLastShot);
					if (Next != EFirearmGesture::None)
					{
						Start(Next);
					}
				}
			}
		}
	};
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.BoltRifle
BEGIN_DEFINE_SPEC(FBoltRifleSpec, "monke_island.Weapons.BoltRifle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UBoltRifleRules* Rules = nullptr;
END_DEFINE_SPEC(FBoltRifleSpec)

void FBoltRifleSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UBoltRifleRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Rules", [this]()
	{
		It("holds five rifle cartridges, hits hard and far, and keeps its scope", [this]()
		{
			TestEqual(TEXT("five rounds"), Rules->Capacity, 5);
			TestEqual(TEXT("rifle cartridges"), Rules->AmmoKind, EAmmoKind::RifleCartridge);
			TestTrue(TEXT("a soldier (100) in one shot"), Rules->Damage >= 100.0f);
			TestTrue(TEXT("far"), Rules->RangeCm >= 20000.0f);
			TestTrue(TEXT("the scope is the exception to the hip-only rule"), Rules->bAllowAiming);
			TestTrue(TEXT("a narrow view through the scope"), Rules->AimFieldOfView < 30.0f);
			TestEqual(TEXT("the eyepiece on the view axis"), Rules->AimSightSocket, FName(TEXT("socket_scope_eye")));
			TestTrue(TEXT("the weapon hides behind the scope"), Rules->bHideFirstPersonWhenAimed);
			TestTrue(TEXT("steady through the scope: tighter than without"), Rules->AimSpreadDegrees < Rules->NoScopeSpreadDegrees);
			TestTrue(TEXT("and much tighter than on the move"), Rules->NoScopeSpreadDegrees < Rules->HipSpreadDegrees);
		});

		It("has the gestures of the bolt and the stripper clip with their events in order", [this]()
		{
			const FFirearmClip* Cycle = Rules->FindClip(EFirearmGesture::BoltCycle);
			if (!TestNotNull(TEXT("BoltCycle"), Cycle))
			{
				return;
			}
			const float Open = Cycle->FindEventTime(TEXT("BoltOpen"));
			const float Eject = Cycle->FindEventTime(Rules->CaseEjectEvent);
			const float Close = Cycle->FindEventTime(Rules->BoltCloseEvent);
			TestTrue(TEXT("bolt opens, case out, bolt closes"), Open >= 0.0f && Open < Eject && Eject < Close && Close <= Cycle->Duration);
			const FFirearmClip* Reload = Rules->FindClip(EFirearmGesture::Reload);
			if (!TestNotNull(TEXT("Reload"), Reload))
			{
				return;
			}
			const float In = Reload->FindEventTime(Rules->ReloadLoadedEvent);
			TestTrue(TEXT("the clip goes in before the bolt closes"), In >= 0.0f && In < Reload->FindEventTime(Rules->BoltCloseEvent));
			for (const EFirearmGesture Gesture : { EFirearmGesture::Fire, EFirearmGesture::AimFire, EFirearmGesture::AimIn, EFirearmGesture::AimIdle,
				EFirearmGesture::AimOut })
			{
				TestNotNull(TEXT("shot and scope gestures"), Rules->FindClip(Gesture));
			}
			TestTrue(TEXT("AimIdle loops"), Rules->FindClip(EFirearmGesture::AimIdle)->bLoop);
		});
	});

	Describe("Bolt", [this]()
	{
		It("starts with a round chambered and the rest in the pouch", [this]()
		{
			const BoltRifleMath::FState Start = BoltRifleMath::StartingRifle(25, 5, 5);
			TestEqual(TEXT("five in the rifle"), Start.Loaded, 5);
			TestTrue(TEXT("chambered"), Start.bChambered);
			TestEqual(TEXT("twenty in the pouch"), Start.Reserve, 20);
			const BoltRifleMath::FState Few = BoltRifleMath::StartingRifle(3, 5, 5);
			TestEqual(TEXT("fewer rounds than the rifle holds"), Few.Loaded, 3);
			TestEqual(TEXT("pouch empty"), Few.Reserve, 0);
			TestFalse(TEXT("an empty rifle is not chambered"), BoltRifleMath::StartingRifle(0, 5, 5).bChambered);
		});

		It("never fires before the bolt closed on a round", [this]()
		{
			BoltRifleMath::FState State{ 5, true, 20 };
			TestEqual(TEXT("chambered: fire"), BoltRifleMath::OnTrigger(State, false, true), BoltRifleMath::ETrigger::Fire);
			BoltRifleMath::Fire(State);
			TestEqual(TEXT("four left"), State.Loaded, 4);
			TestFalse(TEXT("the case is in the chamber"), State.bChambered);
			TestEqual(TEXT("a press works the bolt"), BoltRifleMath::OnTrigger(State, false, true), BoltRifleMath::ETrigger::CycleBolt);
			TestFalse(TEXT("BoltCycle not cut before BoltClose"), BoltRifleMath::CanCutForShot(EFirearmGesture::BoltCycle, State));
			TestEqual(TEXT("busy: nothing"), BoltRifleMath::OnTrigger(State, true, true), BoltRifleMath::ETrigger::None);
			TestTrue(TEXT("BoltClose chambers"), BoltRifleMath::CloseBolt(State));
			TestTrue(TEXT("after BoltClose the end of the clip gives way to the shot"), BoltRifleMath::CanCutForShot(EFirearmGesture::BoltCycle, State));
			TestFalse(TEXT("but not the draw"), BoltRifleMath::CanCutForShot(EFirearmGesture::Equip, State));
			TestEqual(TEXT("fire again"), BoltRifleMath::OnTrigger(State, false, true), BoltRifleMath::ETrigger::Fire);
		});

		It("works the bolt after each shot and reloads after the fifth", [this]()
		{
			BoltRifleMath::FState State = BoltRifleMath::StartingRifle(Rules->MaxRounds, Rules->Capacity, Rules->StartLoadedRounds);
			for (int32 Shot = 1; Shot <= Rules->Capacity; ++Shot)
			{
				TestEqual(TEXT("chambered before each shot"), BoltRifleMath::OnTrigger(State, false, true), BoltRifleMath::ETrigger::Fire);
				BoltRifleMath::Fire(State);
				const EFirearmGesture Next = BoltRifleMath::AfterShot(State, true, true);
				TestEqual(TEXT("bolt after shots 1-4, stripper clip after the fifth"), Next,
					Shot < Rules->Capacity ? EFirearmGesture::BoltCycle : EFirearmGesture::Reload);
				if (Next == EFirearmGesture::BoltCycle)
				{
					BoltRifleMath::CloseBolt(State);
				}
			}
			TestEqual(TEXT("empty"), State.Loaded, 0);
			TestTrue(TEXT("a clip to load"), BoltRifleMath::CanReload(State, Rules->Capacity));
			TestTrue(TEXT("clip in"), BoltRifleMath::LoadClip(State, Rules->Capacity));
			TestFalse(TEXT("not chambered until BoltClose"), State.bChambered);
			BoltRifleMath::CloseBolt(State);
			TestEqual(TEXT("five again"), State.Loaded, 5);
			TestEqual(TEXT("fifteen left in the pouch"), State.Reserve, 15);
			TestTrue(TEXT("ready"), State.bChambered);
		});

		It("tops up a partly empty rifle from what the pouch holds", [this]()
		{
			BoltRifleMath::FState State{ 2, true, 2 };
			TestEqual(TEXT("two to load"), BoltRifleMath::RoundsToLoad(State, 5), 2);
			BoltRifleMath::LoadClip(State, 5);
			TestEqual(TEXT("four in"), State.Loaded, 4);
			TestEqual(TEXT("pouch empty"), State.Reserve, 0);
			TestFalse(TEXT("nothing more to load"), BoltRifleMath::CanReload(State, 5));
			const BoltRifleMath::FState Full{ 5, true, 10 };
			TestFalse(TEXT("full: no reload"), BoltRifleMath::CanReload(Full, 5));
		});

		It("clicks empty without rounds, reloads on the press with rounds in the pouch", [this]()
		{
			const BoltRifleMath::FState Empty{ 0, false, 0 };
			TestEqual(TEXT("nothing at all"), BoltRifleMath::OnTrigger(Empty, false, true), BoltRifleMath::ETrigger::DryFire);
			const BoltRifleMath::FState Pouch{ 0, false, 5 };
			TestEqual(TEXT("reload on the press"), BoltRifleMath::OnTrigger(Pouch, false, true), BoltRifleMath::ETrigger::Reload);
			TestEqual(TEXT("not when the rules say so"), BoltRifleMath::OnTrigger(Pouch, false, false), BoltRifleMath::ETrigger::DryFire);
			TestEqual(TEXT("last shot, pouch empty: nothing follows"), BoltRifleMath::AfterShot(Empty, true, true), EFirearmGesture::None);
			const BoltRifleMath::FState Left{ 3, false, 0 };
			TestEqual(TEXT("bolt by hand when the rules say so"), BoltRifleMath::AfterShot(Left, false, true), EFirearmGesture::None);
		});

		It("leaves at least the end of the shot and the bolt up to BoltClose between two shots, however fast the presses", [this]()
		{
			FRifleSim Sim(Rules);
			// a press every 50 ms for 12 s at 60 frames a second
			const float Step = 1.0f / 60.0f;
			float NextPress = 0.0f;
			while (Sim.Time < 12.0f)
			{
				if (Sim.Time >= NextPress)
				{
					Sim.Press();
					NextPress += 0.05f;
				}
				Sim.Advance(Step);
			}
			const FFirearmClip* Fire = Rules->FindClip(EFirearmGesture::Fire);
			const float FireTail = Fire->Duration - Fire->FindEventTime(Rules->FireEvent);
			const float BoltClose = Rules->FindClip(EFirearmGesture::BoltCycle)->FindEventTime(Rules->BoltCloseEvent);
			const float Shortest = FireTail + BoltClose;
			TestTrue(TEXT("more than one clip fired"), Sim.ShotTimes.Num() > Rules->Capacity);
			for (int32 Index = 1; Index < Sim.ShotTimes.Num(); ++Index)
			{
				TestTrue(TEXT("no shot before BoltClose"), Sim.ShotTimes[Index] - Sim.ShotTimes[Index - 1] >= Shortest - Step - UE_KINDA_SMALL_NUMBER);
			}
			TestEqual(TEXT("the rounds fired come out of the pouch"), Sim.State.Total(), Rules->MaxRounds - Sim.ShotTimes.Num());
		});
	});

	Describe("Scope", [this]()
	{
		It("is tightest through the scope standing still, wide on the move whatever the scope", [this]()
		{
			TestEqual(TEXT("scoped, steady"), BoltRifleMath::Spread(true, false, 0.05f, 1.5f, 4.0f), 0.05f);
			TestEqual(TEXT("no scope, steady"), BoltRifleMath::Spread(false, false, 0.05f, 1.5f, 4.0f), 1.5f);
			TestEqual(TEXT("scoped, moving"), BoltRifleMath::Spread(true, true, 0.05f, 1.5f, 4.0f), 4.0f);
			TestEqual(TEXT("no scope, moving"), BoltRifleMath::Spread(false, true, 0.05f, 1.5f, 4.0f), 4.0f);
		});

		It("lowers the scope during the reload and the bolt, never while carried", [this]()
		{
			const ABoltRifleWeapon* Rifle = GetDefault<ABoltRifleWeapon>();
			TestTrue(TEXT("reload"), Rifle->LowersAim(EFirearmGesture::Reload));
			TestTrue(TEXT("bolt (bBoltCycleLowersScope by default)"), Rifle->LowersAim(EFirearmGesture::BoltCycle));
			TestTrue(TEXT("inspection"), Rifle->LowersAim(EFirearmGesture::Inspect));
			TestFalse(TEXT("the shot through the scope"), Rifle->LowersAim(EFirearmGesture::AimFire));
			TestFalse(TEXT("carrying"), Rifle->LowersAim(EFirearmGesture::None));
		});

		It("stretches its reload and its bolt only when asked to", [this]()
		{
			TestEqual(TEXT("own length"), BoltRifleMath::StretchRate(3.5f, 0.0f), 1.0f);
			TestEqual(TEXT("twice as fast"), BoltRifleMath::StretchRate(3.5f, 1.75f), 2.0f);
		});
	});
}

#endif
