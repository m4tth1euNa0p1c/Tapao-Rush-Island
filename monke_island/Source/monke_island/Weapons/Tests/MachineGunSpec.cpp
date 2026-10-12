// monke_island

#include "Misc/AutomationTest.h"
#include "Camera/CameraShakeBase.h"
#include "Creatures/TikiRules.h"
#include "Player/IslanderMovementComponent.h"
#include "Weapons/MachineGunMath.h"
#include "Weapons/MachineGunRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** The server's gun without a world (AMachineGunWeapon::ServerTickGun reduced to its rules): crank, round clock,
	 *  belt, heat, overheat */
	struct FGunSim
	{
		const UMachineGunRules* Rules = nullptr;
		float Crank = 0.0f;
		float Clock = 0.0f;
		float Heat = 0.0f;
		float Time = 0.0f;
		float LastRound = -100.0f;
		float OverheatTime = -1.0f;
		/** Time, heat and rounds fired when the last overheat ended (before the gun fired again) */
		float RecoverTime = -1.0f;
		float RecoverHeat = -1.0f;
		int32 RoundsAtOverheat = 0;
		int32 RoundsAtRecover = 0;
		int32 Belt = 100;
		int32 Rounds = 0;
		bool bFiring = false;
		bool bOverheated = false;

		explicit FGunSim(const UMachineGunRules* InRules, int32 InBelt = 100)
			: Rules(InRules), Belt(InBelt)
		{
		}

		float MaxRate() const
		{
			return MachineGunMath::RoundsPerSecond(1.0f, Rules->MinCadenceRpm, Rules->MaxCadenceRpm);
		}

		/** One frame of DeltaSeconds, the trigger held or not (DeltaSeconds 0: the press itself) */
		void Step(float DeltaSeconds, bool bHeld)
		{
			Time += DeltaSeconds;
			Heat = MachineGunMath::Cool(Heat, Time - LastRound, DeltaSeconds, Rules->CoolingDelaySeconds, Rules->CoolingPerSecond);
			if (bOverheated && MachineGunMath::HasRecovered(Heat, Rules->OverheatRecoverHeat))
			{
				bOverheated = false;
				RecoverTime = Time;
				RecoverHeat = Heat;
				RoundsAtRecover = Rounds;
			}
			const bool bCanFire = bHeld && !bOverheated && Belt > 0;
			if (bCanFire && !bFiring)
			{
				bFiring = true;
				Clock = MachineGunMath::BurstStartClock(Time - LastRound, MaxRate());
			}
			else if (!bCanFire)
			{
				bFiring = false;
			}
			Crank = MachineGunMath::StepCrank(Crank, bFiring, DeltaSeconds, Rules->SpinUpSeconds, Rules->SpinDownSeconds);
			if (!bFiring)
			{
				return;
			}
			const float Rate = MachineGunMath::RoundsPerSecond(Crank, Rules->MinCadenceRpm, Rules->MaxCadenceRpm);
			const int32 Count = MachineGunMath::AdvanceClock(Clock, DeltaSeconds, Rate, 3);
			for (int32 Index = 0; Index < Count && bFiring; ++Index)
			{
				--Belt;
				++Rounds;
				LastRound = Time;
				Heat = MachineGunMath::HeatAfterRound(Heat, Rules->HeatPerRound);
				if (MachineGunMath::IsOverheating(Heat, Rules->OverheatHeat))
				{
					bOverheated = true;
					bFiring = false;
					OverheatTime = Time;
					RoundsAtOverheat = Rounds;
				}
				else if (Belt <= 0)
				{
					bFiring = false;
				}
			}
		}

		/** Seconds of frames at Fps, the trigger held or not */
		void Run(float Seconds, bool bHeld, float Fps = 60.0f)
		{
			const int32 Frames = FMath::RoundToInt(Seconds * Fps);
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				Step(1.0f / Fps, bHeld);
			}
		}

		/** Press, then hold for Seconds */
		void Burst(float Seconds, float Fps = 60.0f)
		{
			Step(0.0f, true);
			Run(Seconds, true, Fps);
		}
	};
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.MachineGun
BEGIN_DEFINE_SPEC(FMachineGunSpec, "monke_island.Weapons.MachineGun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FMachineGunSpec)

void FMachineGunSpec::Define()
{
	Describe("Cadence", [this]()
	{
		It("turns from 450 to 600 rounds a minute as the crank comes up to speed in 0.4 s", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			TestEqual(TEXT("first turns"), MachineGunMath::RoundsPerSecond(0.0f, Rules->MinCadenceRpm, Rules->MaxCadenceRpm), 7.5f, 0.001f);
			TestEqual(TEXT("full speed"), MachineGunMath::RoundsPerSecond(1.0f, Rules->MinCadenceRpm, Rules->MaxCadenceRpm), 10.0f, 0.001f);
			float Crank = 0.0f;
			Crank = MachineGunMath::StepCrank(Crank, true, 0.2f, Rules->SpinUpSeconds, Rules->SpinDownSeconds);
			TestEqual(TEXT("half way at 0.2 s"), Crank, 0.5f, 0.001f);
			Crank = MachineGunMath::StepCrank(Crank, true, 0.25f, Rules->SpinUpSeconds, Rules->SpinDownSeconds);
			TestEqual(TEXT("full speed at 0.4 s, never beyond"), Crank, 1.0f);
			Crank = MachineGunMath::StepCrank(Crank, false, 1.0f, Rules->SpinUpSeconds, Rules->SpinDownSeconds);
			TestEqual(TEXT("stopped once let go"), Crank, 0.0f);
		});

		It("fires the first round at the press, then about ten a second", [this]()
		{
			FGunSim Gun(GetDefault<UMachineGunRules>());
			Gun.Step(0.0f, true);
			TestEqual(TEXT("first round at once"), Gun.Rounds, 1);
			Gun.Run(2.0f, true);
			// 1 + 0.4 s spinning up (~8.75/s) + 1.6 s at 10/s
			TestTrue(TEXT("about 20 rounds in 2 s"), Gun.Rounds >= 19 && Gun.Rounds <= 22);
		});

		It("never fires faster than the crank, however fast the trigger is tapped", [this]()
		{
			FGunSim Gun(GetDefault<UMachineGunRules>(), 1000);
			for (int32 Tap = 0; Tap < 40; ++Tap)
			{
				// a press every 0.05 s, released at once
				Gun.Step(0.0f, true);
				Gun.Run(0.05f, false);
			}
			TestTrue(TEXT("at most 10 a second over 2 s"), Gun.Rounds <= 21);
			TestTrue(TEXT("but the taps do fire"), Gun.Rounds >= 15);
		});

		It("keeps its cadence whatever the frame rate, and a long hitch never fires a burst at once", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			FGunSim Slow(Rules, 1000);
			FGunSim Fast(Rules, 1000);
			Slow.Burst(2.0f, 30.0f);
			Fast.Burst(2.0f, 144.0f);
			TestTrue(TEXT("30 fps and 144 fps fire alike"), FMath::Abs(Slow.Rounds - Fast.Rounds) <= 1);
			float Clock = 0.0f;
			TestEqual(TEXT("a 1 s hitch fires 3 rounds at most"), MachineGunMath::AdvanceClock(Clock, 1.0f, 10.0f, 3), 3);
			TestTrue(TEXT("and owes nothing"), Clock <= 1.0f);
		});

		It("turns the crank clip once per four rounds at full speed", [this]()
		{
			TestEqual(TEXT("0.4 s clip"), MachineGunMath::CrankClipRate(0.4f, 10.0f, 4), 1.0f, 0.001f);
			TestEqual(TEXT("0.5 s clip"), MachineGunMath::CrankClipRate(0.5f, 10.0f, 4), 1.25f, 0.001f);
			TestEqual(TEXT("reload stretched to 5 s"), MachineGunMath::StretchRate(4.0f, 5.0f), 0.8f, 0.001f);
			TestEqual(TEXT("no stretch"), MachineGunMath::StretchRate(4.0f, 0.0f), 1.0f);
		});
	});

	Describe("Heat", [this]()
	{
		It("overheats after about 75 rounds held down (7 to 8.5 s), before the belt runs dry", [this]()
		{
			FGunSim Gun(GetDefault<UMachineGunRules>());
			Gun.Step(0.0f, true);
			for (int32 Second = 0; Second < 15 && !Gun.bOverheated && Gun.Belt > 0; ++Second)
			{
				Gun.Run(1.0f, true);
			}
			TestTrue(TEXT("overheated"), Gun.bOverheated);
			TestTrue(TEXT("70 to 80 rounds"), Gun.Rounds >= 70 && Gun.Rounds <= 80);
			TestTrue(TEXT("7 to 8.5 s"), Gun.OverheatTime >= 7.0f && Gun.OverheatTime <= 8.5f);
			TestTrue(TEXT("rounds left on the belt"), Gun.Belt > 0);
		});

		It("empties a belt in 2 s bursts with 1 s pauses without overheating", [this]()
		{
			FGunSim Gun(GetDefault<UMachineGunRules>());
			for (int32 Cycle = 0; Cycle < 20 && Gun.Belt > 0 && !Gun.bOverheated; ++Cycle)
			{
				Gun.Burst(2.0f);
				Gun.Run(1.0f, false);
			}
			TestFalse(TEXT("never overheated"), Gun.bOverheated);
			TestEqual(TEXT("belt empty"), Gun.Belt, 0);
		});

		It("does not cool during a burst, cools after it, and fires again once cooled to the recover heat", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			TestEqual(TEXT("between two rounds"), MachineGunMath::Cool(0.5f, 0.1f, 0.016f, Rules->CoolingDelaySeconds, Rules->CoolingPerSecond), 0.5f);
			TestEqual(TEXT("a second after the delay"), MachineGunMath::Cool(0.5f, 2.0f, 1.0f, Rules->CoolingDelaySeconds, Rules->CoolingPerSecond),
				0.5f - Rules->CoolingPerSecond, 0.001f);
			TestEqual(TEXT("only past the delay"), MachineGunMath::Cool(0.5f, Rules->CoolingDelaySeconds + 0.1f, 1.0f, Rules->CoolingDelaySeconds,
				Rules->CoolingPerSecond), 0.5f - 0.1f * Rules->CoolingPerSecond, 0.001f);

			// the trigger held all along: the gun overheats, waits, then fires again by itself once cooled
			FGunSim Gun(Rules);
			Gun.Step(0.0f, true);
			for (int32 Second = 0; Second < 20 && Gun.RecoverTime < 0.0f; ++Second)
			{
				Gun.Run(1.0f, true);
			}
			TestTrue(TEXT("overheated"), Gun.OverheatTime > 0.0f);
			TestTrue(TEXT("then cooled down"), Gun.RecoverTime > Gun.OverheatTime);
			const float Wait = Gun.RecoverTime - Gun.OverheatTime;
			TestTrue(TEXT("2.5 to 5 s to cool down"), Wait >= 2.5f && Wait <= 5.0f);
			TestEqual(TEXT("no round while overheated, trigger held"), Gun.RoundsAtRecover, Gun.RoundsAtOverheat);
			TestTrue(TEXT("recovered at the recover heat"), Gun.RecoverHeat <= Rules->OverheatRecoverHeat + 0.005f);
			TestTrue(TEXT("and fires again"), Gun.Rounds > Gun.RoundsAtRecover);
			TestFalse(TEXT("recovers at the recover heat, not above"), MachineGunMath::HasRecovered(Rules->OverheatRecoverHeat + 0.01f, Rules->OverheatRecoverHeat));
		});

		It("glows only past the glow start, fully at full heat", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			TestEqual(TEXT("cold"), MachineGunMath::GlowAlpha(Rules->GlowStartHeat - 0.1f, Rules->GlowStartHeat), 0.0f);
			TestEqual(TEXT("white hot"), MachineGunMath::GlowAlpha(1.0f, Rules->GlowStartHeat), 1.0f, 0.001f);
			const float Half = MachineGunMath::GlowAlpha((Rules->GlowStartHeat + 1.0f) * 0.5f, Rules->GlowStartHeat);
			TestTrue(TEXT("in between"), Half > 0.2f && Half < 0.8f);
		});

		It("travels in a byte, at 1/255", [this]()
		{
			for (const float Heat : { 0.0f, 0.137f, 0.5f, 0.731f, 1.0f })
			{
				TestEqual(*FString::Printf(TEXT("heat %.3f"), Heat), MachineGunMath::UnpackHeat(MachineGunMath::PackHeat(Heat)), Heat, 0.0021f);
			}
			TestEqual(TEXT("clamped high"), MachineGunMath::PackHeat(2.0f), static_cast<uint8>(255));
			TestEqual(TEXT("clamped low"), MachineGunMath::PackHeat(-1.0f), static_cast<uint8>(0));
		});
	});

	Describe("Belt", [this]()
	{
		It("starts with a full belt and the rest in the boxes, never more than a player carries", [this]()
		{
			const MachineGunMath::FBelt Default = MachineGunMath::StartingBelt(300, 100, 200);
			TestEqual(TEXT("belt"), Default.Belt, 100);
			TestEqual(TEXT("reserve"), Default.Reserve, 100);
			const MachineGunMath::FBelt Few = MachineGunMath::StartingBelt(300, 100, 50);
			TestEqual(TEXT("few: all on the belt"), Few.Belt, 50);
			TestEqual(TEXT("few: none in reserve"), Few.Reserve, 0);
			TestEqual(TEXT("capped"), MachineGunMath::StartingBelt(300, 100, 999).Total(), 300);
		});

		It("feeds a new belt from the reserve, keeping the rounds left on the old one", [this]()
		{
			MachineGunMath::FBelt Partial{ 30, 100 };
			TestTrue(TEXT("loads"), MachineGunMath::LoadBelt(Partial, 100));
			TestEqual(TEXT("full belt"), Partial.Belt, 100);
			TestEqual(TEXT("reserve less 70"), Partial.Reserve, 30);
			MachineGunMath::FBelt Short{ 30, 20 };
			MachineGunMath::LoadBelt(Short, 100);
			TestEqual(TEXT("what the reserve holds"), Short.Belt, 50);
			TestEqual(TEXT("reserve empty"), Short.Reserve, 0);
			MachineGunMath::FBelt Full{ 100, 50 };
			TestFalse(TEXT("full: no reload"), MachineGunMath::CanReload(Full, 100));
			MachineGunMath::FBelt Dry{ 0, 0 };
			TestFalse(TEXT("nothing to load"), MachineGunMath::LoadBelt(Dry, 100));
		});
	});

	Describe("Spread", [this]()
	{
		It("blooms during a burst up to a cap, wider on the move, and settles after", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			float Bloom = 0.0f;
			for (int32 Round = 0; Round < 10; ++Round)
			{
				Bloom = MachineGunMath::BloomAfterRound(Bloom, Rules->BloomPerRound, Rules->MaxBloomDegrees);
			}
			TestEqual(TEXT("10 rounds"), Bloom, FMath::Min(10.0f * Rules->BloomPerRound, Rules->MaxBloomDegrees), 0.001f);
			for (int32 Round = 0; Round < 100; ++Round)
			{
				Bloom = MachineGunMath::BloomAfterRound(Bloom, Rules->BloomPerRound, Rules->MaxBloomDegrees);
			}
			TestEqual(TEXT("capped"), Bloom, Rules->MaxBloomDegrees, 0.001f);
			const float Steady = MachineGunMath::Spread(Rules->AimSpreadDegrees, Bloom, false, Rules->MovingBloomScale);
			const float Moving = MachineGunMath::Spread(Rules->HipSpreadDegrees, Bloom, true, Rules->MovingBloomScale);
			TestTrue(TEXT("the burst opens the cone"), Steady > Rules->AimSpreadDegrees);
			TestTrue(TEXT("wider on the move"), Moving > Steady);
			TestEqual(TEXT("settled after a second"), MachineGunMath::RecoverBloom(Bloom, 1.0f, Rules->BloomRecoveryPerSecond), 0.0f);
		});
	});

	Describe("Gestures", [this]()
	{
		It("fires through the crank gestures only, reloads over a burst or an inspection", [this]()
		{
			for (const EFirearmGesture Gesture : { EFirearmGesture::None, EFirearmGesture::FireStart, EFirearmGesture::FireLoop, EFirearmGesture::FireEnd })
			{
				TestTrue(*FString::Printf(TEXT("fires through %d"), static_cast<int32>(Gesture)), MachineGunMath::AllowsFire(Gesture));
			}
			for (const EFirearmGesture Gesture : { EFirearmGesture::Equip, EFirearmGesture::Reload, EFirearmGesture::Overheat, EFirearmGesture::Inspect,
				EFirearmGesture::Discard })
			{
				TestFalse(*FString::Printf(TEXT("lets go during %d"), static_cast<int32>(Gesture)), MachineGunMath::AllowsFire(Gesture));
			}
			TestTrue(TEXT("reload over a burst"), MachineGunMath::AllowsReload(EFirearmGesture::FireLoop));
			TestTrue(TEXT("reload over an inspection"), MachineGunMath::AllowsReload(EFirearmGesture::Inspect));
			TestFalse(TEXT("not over an overheat"), MachineGunMath::AllowsReload(EFirearmGesture::Overheat));
			TestFalse(TEXT("not over a reload"), MachineGunMath::AllowsReload(EFirearmGesture::Reload));
		});
	});

	Describe("Rules", [this]()
	{
		It("is a heavy belt-fed gun: 100 rounds, 450-600 a minute, a 5 s reload, its own ammunition, a tracer every 3 rounds", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			TestEqual(TEXT("belt"), Rules->BeltCapacity, 100);
			TestEqual(TEXT("min cadence"), Rules->MinCadenceRpm, 450.0f);
			TestEqual(TEXT("max cadence"), Rules->MaxCadenceRpm, 600.0f);
			TestEqual(TEXT("spin up"), Rules->SpinUpSeconds, 0.4f);
			TestEqual(TEXT("reload"), Rules->ReloadSeconds, 5.0f);
			TestEqual(TEXT("ammunition"), Rules->AmmoKind, EAmmoKind::MachineGunBelt);
			TestTrue(TEXT("carries more than a belt"), Rules->MaxRounds > Rules->BeltCapacity && Rules->StartRounds >= Rules->BeltCapacity);
			TestTrue(TEXT("tracers"), Rules->Tracer.bEnabled);
			TestEqual(TEXT("a tracer every 3 rounds"), Rules->Tracer.EveryNth, 3);
			TestFalse(TEXT("from the hip"), Rules->bAllowAiming);
			TestTrue(TEXT("heavy: slower walk and run"), Rules->WalkSpeedFactor < 1.0f && Rules->SprintSpeedFactor < 1.0f);
			TestTrue(TEXT("its own recoil, not the base kick"), Rules->ViewKickPitch == 0.0f && Rules->RecoilPitchPerRound > 0.0f);
			TestNotNull(TEXT("camera shake"), Rules->FireCameraShake.Get());
		});

		It("has the crank, reload and overheat gestures of the contract with T-272", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			const FFirearmClip* Loop = Rules->FindClip(EFirearmGesture::FireLoop);
			TestTrue(TEXT("FireLoop loops"), Loop && Loop->bLoop);
			TestTrue(TEXT("four rounds per turn"), Loop && Loop->CountEvents(TEXT("Fire")) == Rules->RoundsPerCrankTurn);
			const FFirearmClip* Start = Rules->FindClip(EFirearmGesture::FireStart);
			const FFirearmClip* End = Rules->FindClip(EFirearmGesture::FireEnd);
			TestTrue(TEXT("FireStart and FireEnd play once"), Start && !Start->bLoop && End && !End->bLoop);
			const FFirearmClip* Reload = Rules->FindClip(EFirearmGesture::Reload);
			TestTrue(TEXT("the belt goes in during the reload"), Reload && Reload->FindEventTime(Rules->ReloadLoadedEvent) > 0.0f);
			const FFirearmClip* Overheat = Rules->FindClip(EFirearmGesture::Overheat);
			TestTrue(TEXT("the overheat vents"), Overheat && Overheat->CountEvents(Rules->SteamVentEvent) == 1);
			for (const EFirearmGesture Gesture : { EFirearmGesture::Idle, EFirearmGesture::Walk, EFirearmGesture::Sprint, EFirearmGesture::Equip,
				EFirearmGesture::Inspect, EFirearmGesture::Discard })
			{
				TestNotNull(*FString::Printf(TEXT("gesture %d"), static_cast<int32>(Gesture)), Rules->FindClip(Gesture));
			}
		});

		It("kills a villager in two rounds, a warrior or a soldier in three", [this]()
		{
			const UMachineGunRules* Rules = GetDefault<UMachineGunRules>();
			const UTikiRules* Tiki = GetDefault<UTikiRules>();
			TestTrue(TEXT("one round never kills a villager"), Rules->Damage < Tiki->VillagerHealth);
			TestTrue(TEXT("two rounds kill a villager"), 2.0f * Rules->Damage >= Tiki->VillagerHealth);
			TestTrue(TEXT("three rounds kill a warrior"), 3.0f * Rules->Damage >= Tiki->WarriorHealth);
			TestTrue(TEXT("three rounds kill a soldier (100)"), 3.0f * Rules->Damage >= 100.0f);
		});
	});

	Describe("Carry", [this]()
	{
		It("slows its carrier while in hand, walk and run apart, and gives the speed back", [this]()
		{
			UIslanderMovementComponent* Move = NewObject<UIslanderMovementComponent>();
			Move->AddToRoot();
			TestEqual(TEXT("free hands"), Move->GetHeldItemSpeedFactor(false), 1.0f);
			Move->SetHeldItemSpeedFactors(0.72f, 0.68f);
			TestEqual(TEXT("walk"), Move->GetHeldItemSpeedFactor(false), 0.72f);
			TestEqual(TEXT("run"), Move->GetHeldItemSpeedFactor(true), 0.68f);
			Move->SetHeldItemSpeedFactors(1.0f, 1.0f);
			TestEqual(TEXT("given back"), Move->GetHeldItemSpeedFactor(true), 1.0f);
			Move->RemoveFromRoot();
		});
	});
}

#endif
