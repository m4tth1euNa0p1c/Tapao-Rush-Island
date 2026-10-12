// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/AmmoTypes.h"
#include "Weapons/MachineGunMath.h"
#include "Weapons/MachineGunRules.h"
#include "Weapons/SubmachineGunRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** The server's submachine gun without a world (AMachineGunWeapon::ServerTickGun reduced to its rules, as the
	 *  machine gun's spec does): trigger, cadence, drum, heat, bloom */
	struct FSmgSim
	{
		const UMachineGunRules* Rules = nullptr;
		float Crank = 0.0f;
		float Clock = 0.0f;
		float Heat = 0.0f;
		float Bloom = 0.0f;
		float Time = 0.0f;
		float LastRound = -100.0f;
		int32 Drum = 0;
		int32 Rounds = 0;
		bool bFiring = false;
		bool bOverheated = false;
		/** Spread of the first and of the last round fired (deg, steady) */
		float FirstSpread = -1.0f;
		float LastSpread = -1.0f;

		explicit FSmgSim(const UMachineGunRules* InRules)
			: Rules(InRules), Drum(InRules->BeltCapacity)
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
			const bool bCanFire = bHeld && !bOverheated && Drum > 0;
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
				Bloom = MachineGunMath::RecoverBloom(Bloom, DeltaSeconds, Rules->BloomRecoveryPerSecond);
				return;
			}
			const float Rate = MachineGunMath::RoundsPerSecond(Crank, Rules->MinCadenceRpm, Rules->MaxCadenceRpm);
			const int32 Count = MachineGunMath::AdvanceClock(Clock, DeltaSeconds, Rate, 3);
			for (int32 Index = 0; Index < Count && bFiring; ++Index)
			{
				const float Spread = MachineGunMath::Spread(Rules->AimSpreadDegrees, Bloom, false, Rules->MovingBloomScale);
				FirstSpread = FirstSpread < 0.0f ? Spread : FirstSpread;
				LastSpread = Spread;
				--Drum;
				++Rounds;
				LastRound = Time;
				Heat = MachineGunMath::HeatAfterRound(Heat, Rules->HeatPerRound);
				Bloom = MachineGunMath::BloomAfterRound(Bloom, Rules->BloomPerRound, Rules->MaxBloomDegrees);
				if (MachineGunMath::IsOverheating(Heat, Rules->OverheatHeat))
				{
					bOverheated = true;
					bFiring = false;
				}
				else if (Drum <= 0)
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

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Mitraillette
BEGIN_DEFINE_SPEC(FMitrailletteSpec, "monke_island.Weapons.Mitraillette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	USubmachineGunRules* Rules = nullptr;
END_DEFINE_SPEC(FMitrailletteSpec)

void FMitrailletteSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<USubmachineGunRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Rules", [this]()
	{
		It("is a drum of 50 pistol cartridges at about 11 rounds a second, held in the hands", [this]()
		{
			TestEqual(TEXT("drum of 50"), Rules->BeltCapacity, 50);
			TestEqual(TEXT("its own ammunition"), Rules->AmmoKind, EAmmoKind::SmgCartridge);
			TestNotEqual(TEXT("not the machine gun's"), Rules->AmmoKind, EAmmoKind::MachineGunBelt);
			const float Rate = MachineGunMath::RoundsPerSecond(1.0f, Rules->MinCadenceRpm, Rules->MaxCadenceRpm);
			TestTrue(TEXT("about 11 rounds a second"), Rate > 10.5f && Rate < 11.5f);
			TestEqual(TEXT("no crank to spin up: the first rounds at the full cadence"),
				MachineGunMath::RoundsPerSecond(0.0f, Rules->MinCadenceRpm, Rules->MaxCadenceRpm), Rate);
			TestEqual(TEXT("no slowdown in hand"), Rules->WalkSpeedFactor, 1.0f);
			TestEqual(TEXT("no slowdown running"), Rules->SprintSpeedFactor, 1.0f);
			TestEqual(TEXT("no heat"), Rules->HeatPerRound, 0.0f);
			TestTrue(TEXT("the view climbs with each round"), Rules->RecoilPitchPerRound > 0.0f);
			TestTrue(TEXT("a lighter jolt than the machine gun's"), Rules->FireShakeScale < GetDefault<UMachineGunRules>()->FireShakeScale);
			TestTrue(TEXT("weaker rounds than the machine gun's"), Rules->Damage < GetDefault<UMachineGunRules>()->Damage);
		});

		It("loops FireLoop while it fires and feeds the drum at MagIn, an empty one let fall at MagDrop", [this]()
		{
			const FFirearmClip* Loop = Rules->FindClip(EFirearmGesture::FireLoop);
			if (!TestNotNull(TEXT("FireLoop"), Loop))
			{
				return;
			}
			TestTrue(TEXT("loops"), Loop->bLoop);
			TestEqual(TEXT("one loop of the clip shows RoundsPerCrankTurn rounds"), Loop->CountEvents(Rules->FireEvent), Rules->RoundsPerCrankTurn);
			const float LoopRate = MachineGunMath::CrankClipRate(Loop->Duration, MachineGunMath::RoundsPerSecond(1.0f, Rules->MinCadenceRpm,
				Rules->MaxCadenceRpm), Rules->RoundsPerCrankTurn);
			TestTrue(FString::Printf(TEXT("played near its own speed at the cadence (%.2f)"), LoopRate), LoopRate > 0.8f && LoopRate < 1.25f);
			const FFirearmClip* Reload = Rules->FindClip(EFirearmGesture::Reload);
			const FFirearmClip* Empty = Rules->FindClip(EFirearmGesture::ReloadEmpty);
			if (!TestNotNull(TEXT("Reload"), Reload) || !TestNotNull(TEXT("ReloadEmpty"), Empty))
			{
				return;
			}
			TestTrue(TEXT("a drum swapped by hand: the rounds at MagIn"), Reload->FindEventTime(Rules->ReloadLoadedEvent) > 0.0f);
			TestTrue(TEXT("and not let fall"), Reload->FindEventTime(Rules->DroppedBoxEvent) < 0.0f);
			const float Drop = Empty->FindEventTime(Rules->DroppedBoxEvent);
			const float In = Empty->FindEventTime(Rules->ReloadLoadedEvent);
			TestTrue(TEXT("an empty drum let fall, then the new one in"), Drop >= 0.0f && Drop < In);
			TestNull(TEXT("no crank start"), Rules->FindClip(EFirearmGesture::FireStart));
		});
	});

	Describe("Burst", [this]()
	{
		It("fires about 11 rounds a second while the trigger is held, whatever the frame rate", [this]()
		{
			FSmgSim At60(Rules);
			At60.Burst(2.0f, 60.0f);
			FSmgSim At30(Rules);
			At30.Burst(2.0f, 30.0f);
			FSmgSim At144(Rules);
			At144.Burst(2.0f, 144.0f);
			TestTrue(FString::Printf(TEXT("about 22 rounds in 2 s at 60 fps (%d)"), At60.Rounds), At60.Rounds >= 21 && At60.Rounds <= 23);
			TestTrue(TEXT("the same at 30 fps"), FMath::Abs(At30.Rounds - At60.Rounds) <= 1);
			TestTrue(TEXT("the same at 144 fps"), FMath::Abs(At144.Rounds - At60.Rounds) <= 1);
		});

		It("empties its drum in about 4.5 s held and never overheats", [this]()
		{
			FSmgSim Sim(Rules);
			Sim.Burst(8.0f);
			TestEqual(TEXT("the whole drum"), Sim.Rounds, Rules->BeltCapacity);
			TestEqual(TEXT("then dry"), Sim.Drum, 0);
			TestFalse(TEXT("no overheat"), Sim.bOverheated);
			TestTrue(TEXT("dry before 5 s"), Sim.LastRound < 5.0f);
		});

		It("never fires faster than its cadence when the trigger is tapped", [this]()
		{
			FSmgSim Sim(Rules);
			// a press and a release every frame at 144 fps for 2 s
			for (int32 Frame = 0; Frame < 288; ++Frame)
			{
				Sim.Step(1.0f / 144.0f, Frame % 2 == 0);
			}
			TestTrue(FString::Printf(TEXT("no more than the cadence (%d)"), Sim.Rounds), Sim.Rounds <= 23);
		});

		It("opens its spread during a burst and closes it once the trigger is let go", [this]()
		{
			FSmgSim Sim(Rules);
			Sim.Burst(1.5f);
			TestTrue(TEXT("the first round leaves with the spread of the stance"), FMath::IsNearlyEqual(Sim.FirstSpread, Rules->AimSpreadDegrees, 1e-3f));
			TestTrue(TEXT("the burst opens it"), Sim.LastSpread > Sim.FirstSpread + 1.0f);
			TestTrue(TEXT("up to its limit"), Sim.LastSpread <= Rules->AimSpreadDegrees + Rules->MaxBloomDegrees + 1e-3f);
			Sim.Run(1.0f, false);
			TestTrue(TEXT("closed again after a second"), Sim.Bloom <= 0.0f + 1e-3f);
			TestTrue(TEXT("wider on the move"), MachineGunMath::Spread(Rules->HipSpreadDegrees, 2.0f, true, Rules->MovingBloomScale)
				> MachineGunMath::Spread(Rules->AimSpreadDegrees, 2.0f, false, Rules->MovingBloomScale));
		});
	});

	Describe("Ammo", [this]()
	{
		It("starts with a full drum and two more, and a new drum takes what is left", [this]()
		{
			MachineGunMath::FBelt Belt = MachineGunMath::StartingBelt(Rules->MaxRounds, Rules->BeltCapacity, Rules->StartRounds);
			TestEqual(TEXT("full drum"), Belt.Belt, 50);
			TestEqual(TEXT("two more drums"), Belt.Reserve, 100);
			Belt.Belt = 7;
			TestTrue(TEXT("a reload is worth it"), MachineGunMath::CanReload(Belt, Rules->BeltCapacity));
			MachineGunMath::LoadBelt(Belt, Rules->BeltCapacity);
			TestEqual(TEXT("full again"), Belt.Belt, 50);
			TestEqual(TEXT("43 taken from the reserve"), Belt.Reserve, 57);
			TestEqual(TEXT("a box of 50 fits in a gun short of 50"), AmmoMath::Room(Rules->MaxRounds, 50, 150), 50);
		});
	});
}

#endif
