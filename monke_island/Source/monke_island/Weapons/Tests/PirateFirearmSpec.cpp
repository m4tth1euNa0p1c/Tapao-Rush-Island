// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiCombat.h"
#include "Weapons/FirearmMath.h"
#include "Weapons/PistolRules.h"
#include "Weapons/ShotgunRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.PirateFirearms
BEGIN_DEFINE_SPEC(FPirateFirearmSpec, "monke_island.Weapons.PirateFirearms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UPistolRules* Pistol = nullptr;
	UShotgunRules* Shotgun = nullptr;
END_DEFINE_SPEC(FPirateFirearmSpec)

void FPirateFirearmSpec::Define()
{
	BeforeEach([this]()
	{
		Pistol = NewObject<UPistolRules>();
		Pistol->AddToRoot();
		Shotgun = NewObject<UShotgunRules>();
		Shotgun->AddToRoot();
	});

	AfterEach([this]()
	{
		Pistol->RemoveFromRoot();
		Shotgun->RemoveFromRoot();
		Pistol = nullptr;
		Shotgun = nullptr;
	});

	Describe("Clips", [this]()
	{
		It("carries the pistol and shotgun gestures of the manifests with their logic events", [this]()
		{
			// 14 and 16 gestures of the first packs; the v2 packs add Discard (owner 2026-10-09)
			TestTrue(TEXT("pistol gestures"), Pistol->Clips.Num() >= 14);
			TestTrue(TEXT("shotgun gestures"), Shotgun->Clips.Num() >= 16);
			const FFirearmClip* Reload = Pistol->FindClip(EFirearmGesture::Reload);
			TestTrue(TEXT("pistol reload"), Reload != nullptr);
			if (Reload)
			{
				// weapons v2 (2026-10-09): the pack's reload is 135 frames at 30 fps, the hammer cocked at frame 120
				TestEqual(TEXT("reload 135 frames at 30 fps"), Reload->Duration, 4.5f, 1e-4f);
				TestEqual(TEXT("charge in at HammerCocked, frame 120"), Reload->FindEventTime(Pistol->ReloadLoadedEvent), 120.0f / 30.0f, 1e-4f);
			}
			const FFirearmClip* Cock = Pistol->FindClip(EFirearmGesture::Cock);
			TestTrue(TEXT("cock event"), Cock && Cock->FindEventTime(Pistol->CockEvent) > 0.0f);
			const FFirearmClip* Empty = Shotgun->FindClip(EFirearmGesture::ReloadEmpty);
			TestTrue(TEXT("ReloadEmpty shows three shells"), Empty && Empty->CountEvents(Shotgun->InsertEvent) == 3);
			const FFirearmClip* Insert = Shotgun->FindClip(EFirearmGesture::ReloadInsert);
			TestTrue(TEXT("one shell per ReloadInsert"), Insert && Insert->CountEvents(Shotgun->InsertEvent) == 1);
			const FFirearmClip* Shot = Shotgun->FindClip(EFirearmGesture::Fire);
			TestTrue(TEXT("FirePump: shot then shell out"), Shot && Shot->FindEventTime(Shotgun->FireEvent) < Shot->FindEventTime(Shotgun->EjectEvent));
			TestTrue(TEXT("Idle loops"), Pistol->FindClip(EFirearmGesture::Idle) && Pistol->FindClip(EFirearmGesture::Idle)->bLoop);
			TestNull(TEXT("no pump gesture for the pistol"), Pistol->FindClip(EFirearmGesture::ReloadInsert));
		});

		It("finds the n-th event of a name and -1 for a missing one", [this]()
		{
			const FFirearmClip* Empty = Shotgun->FindClip(EFirearmGesture::ReloadEmpty);
			if (!TestNotNull(TEXT("ReloadEmpty"), Empty))
			{
				return;
			}
			TestEqual(TEXT("first insert, frame 68"), Empty->FindEventTime(Shotgun->InsertEvent, 0), 68.0f / 30.0f, 1e-4f);
			TestEqual(TEXT("third insert, frame 178"), Empty->FindEventTime(Shotgun->InsertEvent, 2), 178.0f / 30.0f, 1e-4f);
			TestEqual(TEXT("no fourth"), Empty->FindEventTime(Shotgun->InsertEvent, 3), -1.0f);
		});

		It("takes its ammunition kind from the rules: charges for the pistol, shells for the shotgun", [this]()
		{
			TestEqual(TEXT("pistol"), Pistol->AmmoKind, EAmmoKind::PistolCharge);
			TestEqual(TEXT("shotgun"), Shotgun->AmmoKind, EAmmoKind::Shell);
		});
	});

	Describe("Spread", [this]()
	{
		It("throws PelletCount pellets, all inside the cone, not all on the axis", [this]()
		{
			FRandomStream Stream(1234);
			TArray<FVector> Directions;
			const FVector Aim(1.0, 1.0, 0.2);
			FirearmMath::SpreadDirections(Aim, Shotgun->PelletCount, Shotgun->HipSpreadDegrees, Stream, Directions);
			TestEqual(TEXT("pellets"), Directions.Num(), Shotgun->PelletCount);
			const FVector Axis = Aim.GetSafeNormal();
			float MaxAngle = 0.0f;
			for (const FVector& Direction : Directions)
			{
				TestTrue(TEXT("unit"), Direction.IsNormalized());
				MaxAngle = FMath::Max(MaxAngle, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Direction, Axis)), -1.0f, 1.0f))));
			}
			TestTrue(TEXT("inside the cone"), MaxAngle <= Shotgun->HipSpreadDegrees + 0.01f);
			TestTrue(TEXT("spread out"), MaxAngle > Shotgun->HipSpreadDegrees * 0.2f);
		});

		It("aims tighter than from the hip, and a zero cone is the aim itself", [this]()
		{
			TestTrue(TEXT("shotgun aim < hip"), Shotgun->AimSpreadDegrees < Shotgun->HipSpreadDegrees);
			TestTrue(TEXT("pistol aim < hip"), Pistol->AimSpreadDegrees < Pistol->HipSpreadDegrees);
			FRandomStream Stream(7);
			TArray<FVector> Directions;
			FirearmMath::SpreadDirections(FVector(0.0, 0.0, 3.0), 3, 0.0f, Stream, Directions);
			for (const FVector& Direction : Directions)
			{
				TestTrue(TEXT("on the axis"), Direction.Equals(FVector::UpVector, 1e-4));
			}
		});

		It("keeps full damage up to FalloffStart, then falls to FalloffMinScale at the range", [this]()
		{
			const float Near = FirearmMath::DamageAtDistance(Shotgun->Damage, Shotgun->FalloffStartCm * 0.5f, Shotgun->FalloffStartCm, Shotgun->RangeCm, Shotgun->FalloffMinScale);
			const float Far = FirearmMath::DamageAtDistance(Shotgun->Damage, Shotgun->RangeCm, Shotgun->FalloffStartCm, Shotgun->RangeCm, Shotgun->FalloffMinScale);
			TestEqual(TEXT("near"), Near, Shotgun->Damage);
			TestEqual(TEXT("far"), Far, Shotgun->Damage * Shotgun->FalloffMinScale, 1e-3f);
			TestEqual(TEXT("no falloff on the pistol"), FirearmMath::DamageAtDistance(Pistol->Damage, 2900.0f, Pistol->FalloffStartCm, Pistol->RangeCm, Pistol->FalloffMinScale), Pistol->Damage);
			TestTrue(TEXT("a full shotgun shot at point blank kills a tiki warrior (80)"), Shotgun->Damage * Shotgun->PelletCount >= 80.0f);
		});
	});

	Describe("Pistol", [this]()
	{
		It("counts its charges: one in the barrel, the rest in the pouch", [this]()
		{
			FirearmMath::FPistolState State = FirearmMath::StartingPistol(Pistol->MaxCharges, Pistol->bStartLoaded, Pistol->bStartCocked, Pistol->Barrels);
			TestTrue(TEXT("loaded"), State.IsLoaded());
			TestEqual(TEXT("one charge in the one barrel"), State.Loaded, 1);
			TestTrue(TEXT("cocked"), State.bCocked);
			TestEqual(TEXT("total"), State.Total(), Pistol->MaxCharges);
			TestEqual(TEXT("fire"), FirearmMath::OnTrigger(State, false, true), FirearmMath::EPistolTrigger::Fire);
			TestEqual(TEXT("busy: nothing"), FirearmMath::OnTrigger(State, true, true), FirearmMath::EPistolTrigger::None);
			TestEqual(TEXT("the shot leaves the first (only) barrel"), FirearmMath::Fire(State), 0);
			TestFalse(TEXT("hammer down after the shot"), State.bCocked);
			TestEqual(TEXT("empty: reload"), FirearmMath::OnTrigger(State, false, true), FirearmMath::EPistolTrigger::Reload);
			TestTrue(TEXT("reload"), FirearmMath::CompleteReload(State));
			TestTrue(TEXT("the reload cocks"), State.IsLoaded() && State.bCocked);
			TestEqual(TEXT("one charge spent"), State.Total(), Pistol->MaxCharges - 1);
			TestFalse(TEXT("no second charge in the barrel"), FirearmMath::CompleteReload(State));
		});

		It("cocks a lowered hammer before firing, and clicks when nothing is left", [this]()
		{
			FirearmMath::FPistolState State{ 1, false, 0 };
			TestEqual(TEXT("cock first"), FirearmMath::OnTrigger(State, false, true), FirearmMath::EPistolTrigger::Cock);
			TestEqual(TEXT("R cocks a loaded pistol"), FirearmMath::OnReloadKey(State, false), FirearmMath::EPistolTrigger::Cock);
			FirearmMath::Cock(State);
			FirearmMath::Fire(State);
			FirearmMath::Cock(State);
			TestEqual(TEXT("cocked, empty, no charge: dry fire"), FirearmMath::OnTrigger(State, false, true), FirearmMath::EPistolTrigger::DryFire);
			TestEqual(TEXT("R does nothing without charges"), FirearmMath::OnReloadKey(State, false), FirearmMath::EPistolTrigger::None);
			FirearmMath::DryFire(State);
			TestFalse(TEXT("hammer down after the click"), State.bCocked);
			TestEqual(TEXT("then cock again"), FirearmMath::OnTrigger(State, false, true), FirearmMath::EPistolTrigger::Cock);
		});

		It("starts empty when asked, and never with a negative pouch", [this]()
		{
			const FirearmMath::FPistolState Empty = FirearmMath::StartingPistol(0, true, true);
			TestFalse(TEXT("nothing to load"), Empty.IsLoaded());
			TestEqual(TEXT("no charge"), Empty.Reserve, 0);
		});
	});

	Describe("DoublePistol", [this]()
	{
		It("loads two charges, fires twice without a reload, the second shot from the second barrel", [this]()
		{
			FirearmMath::FPistolState State = FirearmMath::StartingPistol(16, true, true, 2);
			TestEqual(TEXT("both barrels loaded"), State.Loaded, 2);
			TestEqual(TEXT("pouch"), State.Reserve, 14);
			TestEqual(TEXT("first barrel"), FirearmMath::Fire(State), 0);
			TestEqual(TEXT("one left"), State.Loaded, 1);
			// the hammer fell: the next press cocks and fires at once (bAutoCockOnFire)
			TestEqual(TEXT("second press fires"), FirearmMath::OnTrigger(State, false, true, true), FirearmMath::EPistolTrigger::Fire);
			TestEqual(TEXT("without auto cock: cock first"), FirearmMath::OnTrigger(State, false, true, false), FirearmMath::EPistolTrigger::Cock);
			TestEqual(TEXT("second barrel"), FirearmMath::Fire(State), 1);
			TestEqual(TEXT("empty"), State.Loaded, 0);
			TestEqual(TEXT("then the reload"), FirearmMath::OnTrigger(State, false, true, true), FirearmMath::EPistolTrigger::Reload);
		});

		It("fills both barrels at the reload, or what the pouch holds", [this]()
		{
			FirearmMath::FPistolState State{ 0, false, 5, 2 };
			TestTrue(TEXT("reloaded"), FirearmMath::CompleteReload(State));
			TestEqual(TEXT("two in"), State.Loaded, 2);
			TestEqual(TEXT("pouch"), State.Reserve, 3);
			TestTrue(TEXT("cocked"), State.bCocked);
			FirearmMath::Fire(State);
			TestEqual(TEXT("R tops the empty barrel up"), FirearmMath::OnReloadKey(State, false), FirearmMath::EPistolTrigger::Reload);
			FirearmMath::FPistolState Last{ 0, false, 1, 2 };
			TestTrue(TEXT("one charge left loads one barrel"), FirearmMath::CompleteReload(Last));
			TestEqual(TEXT("one in"), Last.Loaded, 1);
			TestEqual(TEXT("pouch empty"), Last.Reserve, 0);
			TestEqual(TEXT("R cocks a half loaded pistol with nothing to load"), FirearmMath::OnReloadKey(FirearmMath::FPistolState{ 1, false, 0, 2 }, false), FirearmMath::EPistolTrigger::Cock);
		});

		It("has two barrels and a second muzzle in its rules", [this]()
		{
			const UDoublePistolRules* Double = GetDefault<UDoublePistolRules>();
			TestEqual(TEXT("barrels"), Double->Barrels, 2);
			TestEqual(TEXT("single pistol"), Pistol->Barrels, 1);
			TestEqual(TEXT("second muzzle"), Double->SecondMuzzleSocket, FName(TEXT("socket_muzzle2")));
			TestTrue(TEXT("its gestures"), Double->FindClip(EFirearmGesture::Fire) != nullptr && Double->FindClip(EFirearmGesture::Reload) != nullptr);
			TestEqual(TEXT("charges"), Double->AmmoKind, EAmmoKind::PistolCharge);
		});
	});

	Describe("Shotgun", [this]()
	{
		It("counts its shells: Capacity in the gun at most, the rest in the pouch, one per shot", [this]()
		{
			FirearmMath::FShotgunState State = FirearmMath::StartingShotgun(Shotgun->MaxShells, Shotgun->Capacity, Shotgun->StartLoadedShells);
			TestEqual(TEXT("in the gun"), State.Rounds, FMath::Min(Shotgun->StartLoadedShells, Shotgun->Capacity));
			TestEqual(TEXT("total"), State.Total(), Shotgun->MaxShells);
			const FirearmMath::FShotgunState Few = FirearmMath::StartingShotgun(2, 4, 4);
			TestEqual(TEXT("fewer shells than the gun holds"), Few.Rounds, 2);
			TestEqual(TEXT("pouch empty"), Few.Reserve, 0);
			TestTrue(TEXT("can fire"), FirearmMath::CanFire(State, false));
			TestFalse(TEXT("not while busy (pump)"), FirearmMath::CanFire(State, true));
			FirearmMath::Fire(State);
			TestEqual(TEXT("one spent"), State.Total(), Shotgun->MaxShells - 1);
		});

		It("reloads shell by shell until full: Start, Insert x N, End", [this]()
		{
			const int32 Capacity = 4;
			FirearmMath::FShotgunState State{ 1, 10 };
			FirearmMath::EReloadPhase Phase = FirearmMath::FirstReloadPhase(State, Capacity);
			TestEqual(TEXT("start"), Phase, FirearmMath::EReloadPhase::Start);
			Phase = FirearmMath::NextReloadPhase(Phase, State, Capacity, false);
			int32 Inserts = 0;
			while (Phase == FirearmMath::EReloadPhase::Insert && Inserts < 10)
			{
				TestTrue(TEXT("shell in"), FirearmMath::InsertShell(State, Capacity));
				++Inserts;
				Phase = FirearmMath::NextReloadPhase(Phase, State, Capacity, false);
			}
			TestEqual(TEXT("three inserts"), Inserts, 3);
			TestEqual(TEXT("then End"), Phase, FirearmMath::EReloadPhase::End);
			TestEqual(TEXT("full"), State.Rounds, Capacity);
			TestEqual(TEXT("pouch"), State.Reserve, 7);
			TestEqual(TEXT("End is the last step"), FirearmMath::NextReloadPhase(Phase, State, Capacity, false), FirearmMath::EReloadPhase::None);
			TestEqual(TEXT("full: no reload"), FirearmMath::FirstReloadPhase(State, Capacity), FirearmMath::EReloadPhase::None);
		});

		It("stops the inserts when the pouch runs out", [this]()
		{
			FirearmMath::FShotgunState State{ 1, 1 };
			FirearmMath::EReloadPhase Phase = FirearmMath::NextReloadPhase(FirearmMath::EReloadPhase::Start, State, 4, false);
			TestEqual(TEXT("one insert"), Phase, FirearmMath::EReloadPhase::Insert);
			FirearmMath::InsertShell(State, 4);
			TestEqual(TEXT("out of shells: End"), FirearmMath::NextReloadPhase(Phase, State, 4, false), FirearmMath::EReloadPhase::End);
			TestFalse(TEXT("nothing more to insert"), FirearmMath::InsertShell(State, 4));
		});

		It("lets a shot cut the reload once a shell is in, never during ReloadEmpty", [this]()
		{
			FirearmMath::FShotgunState State{ 1, 10 };
			TestTrue(TEXT("cut during Start"), FirearmMath::CanInterruptReloadForFire(FirearmMath::EReloadPhase::Start, State));
			TestTrue(TEXT("cut during Insert"), FirearmMath::CanInterruptReloadForFire(FirearmMath::EReloadPhase::Insert, State));
			TestTrue(TEXT("cut during End"), FirearmMath::CanInterruptReloadForFire(FirearmMath::EReloadPhase::End, State));
			TestFalse(TEXT("not ReloadEmpty"), FirearmMath::CanInterruptReloadForFire(FirearmMath::EReloadPhase::Empty, State));
			// a stop asked between two inserts closes the gun (End) with the shells already in
			FirearmMath::InsertShell(State, 4);
			TestEqual(TEXT("asked to stop: End"), FirearmMath::NextReloadPhase(FirearmMath::EReloadPhase::Insert, State, 4, true), FirearmMath::EReloadPhase::End);
			TestEqual(TEXT("the shells in stay in"), State.Rounds, 2);
			FirearmMath::Fire(State);
			TestEqual(TEXT("and one goes off"), State.Rounds, 1);
			const FirearmMath::FShotgunState Empty{ 0, 10 };
			TestFalse(TEXT("no shell, no cut"), FirearmMath::CanInterruptReloadForFire(FirearmMath::EReloadPhase::Insert, Empty));
		});

		It("uses ReloadEmpty on an empty gun, for at most three shells", [this]()
		{
			FirearmMath::FShotgunState State{ 0, 10 };
			TestEqual(TEXT("empty gun"), FirearmMath::FirstReloadPhase(State, Shotgun->Capacity), FirearmMath::EReloadPhase::Empty);
			const FFirearmClip* Empty = Shotgun->FindClip(EFirearmGesture::ReloadEmpty);
			const int32 Inserts = Empty ? Empty->CountEvents(Shotgun->InsertEvent) : 0;
			for (int32 Index = 0; Index < Inserts; ++Index)
			{
				FirearmMath::InsertShell(State, Shotgun->Capacity);
			}
			TestEqual(TEXT("three in"), State.Rounds, 3);
			TestEqual(TEXT("ReloadEmpty ends the reload"), FirearmMath::NextReloadPhase(FirearmMath::EReloadPhase::Empty, State, Shotgun->Capacity, false), FirearmMath::EReloadPhase::None);
			const FirearmMath::FShotgunState Dry{ 0, 0 };
			TestEqual(TEXT("nothing to load: no reload"), FirearmMath::FirstReloadPhase(Dry, Shotgun->Capacity), FirearmMath::EReloadPhase::None);
		});

		It("fills an empty gun past the three shells of ReloadEmpty, shell by shell (T-335)", [this]()
		{
			const int32 Capacity = 5;
			FirearmMath::FShotgunState State{ 0, 10 };
			TestEqual(TEXT("empty gun"), FirearmMath::FirstReloadPhase(State, Capacity, 3), FirearmMath::EReloadPhase::Empty);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FirearmMath::InsertShell(State, Capacity);
			}
			FirearmMath::EReloadPhase Phase = FirearmMath::NextReloadPhase(FirearmMath::EReloadPhase::Empty, State, Capacity, false, true);
			TestEqual(TEXT("then the shell by shell reload"), Phase, FirearmMath::EReloadPhase::Start);
			Phase = FirearmMath::NextReloadPhase(Phase, State, Capacity, false, true);
			int32 Inserts = 0;
			while (Phase == FirearmMath::EReloadPhase::Insert && Inserts < 10)
			{
				FirearmMath::InsertShell(State, Capacity);
				++Inserts;
				Phase = FirearmMath::NextReloadPhase(Phase, State, Capacity, false, true);
			}
			TestEqual(TEXT("two more"), Inserts, 2);
			TestEqual(TEXT("then End"), Phase, FirearmMath::EReloadPhase::End);
			TestEqual(TEXT("full"), State.Rounds, Capacity);
			const FirearmMath::FShotgunState Full{ 4, 10 };
			TestEqual(TEXT("full after ReloadEmpty: it ends there"), FirearmMath::NextReloadPhase(FirearmMath::EReloadPhase::Empty, Full, 4, false, true),
				FirearmMath::EReloadPhase::None);
			const FirearmMath::FShotgunState Out{ 3, 0 };
			TestEqual(TEXT("no shell left: it ends there"), FirearmMath::NextReloadPhase(FirearmMath::EReloadPhase::Empty, Out, 4, false, true),
				FirearmMath::EReloadPhase::None);
		});

		It("loads an empty gun short of shells one by one, never showing shells the pouch does not have (T-335)", [this]()
		{
			const FirearmMath::FShotgunState Short{ 0, 2 };
			TestEqual(TEXT("two shells for an empty gun: shell by shell"), FirearmMath::FirstReloadPhase(Short, 4, 3), FirearmMath::EReloadPhase::Start);
			const FirearmMath::FShotgunState Enough{ 0, 3 };
			TestEqual(TEXT("three shells: ReloadEmpty"), FirearmMath::FirstReloadPhase(Enough, 4, 3), FirearmMath::EReloadPhase::Empty);
			TestEqual(TEXT("without the rule: ReloadEmpty whatever"), FirearmMath::FirstReloadPhase(Short, 4), FirearmMath::EReloadPhase::Empty);
			TestTrue(TEXT("the shotgun's rules ask for both"), Shotgun->bTopUpAfterEmptyReload && Shotgun->bShellByShellWhenShort);
		});
	});

	Describe("TikiReactions", [this]()
	{
		It("takes the musket reactions for the pistol and the shotgun, the shotgun bursting at close range", [this]()
		{
			const uint32 All = ~0u;
			TikiCombat::FTuning Tuning;
			TikiCombat::FBlow Blow;
			Blow.bLethal = true;
			Blow.Roll = 0.99f;
			Blow.Distance = 300.0f;
			Blow.Weapon = TikiCombat::EWeapon::Pistol;
			TestEqual(TEXT("pistol, lethal: articulated fall"), TikiCombat::ChooseClip(Blow, Tuning, All), ETikiCombatClip::MusketArticulatedFall);
			Blow.Weapon = TikiCombat::EWeapon::Shotgun;
			Blow.CloseBurstDistance = Shotgun->CloseBurstDistanceCm;
			TestEqual(TEXT("shotgun at 3 m: burst"), TikiCombat::ChooseClip(Blow, Tuning, All), ETikiCombatClip::MusketBurst);
			Blow.Distance = Shotgun->CloseBurstDistanceCm + 100.0f;
			TestEqual(TEXT("shotgun further: fall"), TikiCombat::ChooseClip(Blow, Tuning, All), ETikiCombatClip::MusketArticulatedFall);
			Blow.bLethal = false;
			TestEqual(TEXT("not lethal: stagger"), TikiCombat::ChooseClip(Blow, Tuning, All), ETikiCombatClip::MusketStaggerRecover);
		});
	});
}

#endif
