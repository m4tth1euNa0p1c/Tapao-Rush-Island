// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiShredDemo.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	using enum ETikiCombatClip;

	/** Mask of the demos the pack delivers for a mob in the project: Musket_Impact, Musket_Burst and the four cuts */
	uint32 PackMask()
	{
		uint32 Mask = 0;
		for (const ETikiCombatClip Clip : { MusketImpact, MusketBurst, SabreLeftToRight, SabreRightToLeft, SabreDown, SabreLow })
		{
			Mask |= 1u << static_cast<uint8>(Clip);
		}
		return Mask;
	}

	/** A lethal blow of the bean on an intact mob with the whole pack, rules at their defaults */
	TikiShredDemo::FChoice Lethal(ETikiCombatClip Clip)
	{
		const UTikiRules* Rules = GetDefault<UTikiRules>();
		TikiShredDemo::FChoice Choice;
		Choice.bEnabled = Rules->bShredDemos;
		Choice.bSabreNeedsCut = Rules->bShredDemoSabreNeedsCut;
		Choice.bCloseShotBurstDemo = Rules->bCloseShotBurstDemo;
		Choice.Clip = Clip;
		Choice.DemoMask = PackMask();
		return Choice;
	}

	/** A lethal shotgun shot at Distance cm (close range 5 m, DA_ShotgunRules) that T-150 answered with Clip */
	TikiShredDemo::FChoice Shotgun(float Distance, ETikiCombatClip Clip)
	{
		TikiShredDemo::FChoice Choice = Lethal(Clip);
		Choice.bFirearm = true;
		Choice.Distance = Distance;
		Choice.CloseDistance = 500.0f;
		return Choice;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.ShredDemo
BEGIN_DEFINE_SPEC(FTikiShredDemoSpec, "monke_island.Tiki.ShredDemo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiShredDemoSpec)

void FTikiShredDemoSpec::Define()
{
	Describe("Choose", [this]()
	{
		It("replaces a lethal Musket_Burst and each lethal sabre cut by the demo of the same clip", [this]()
		{
			for (const ETikiCombatClip Clip : { MusketBurst, SabreLeftToRight, SabreRightToLeft, SabreDown, SabreLow })
			{
				TestTrue(*UEnum::GetValueAsString(Clip), TikiShredDemo::Choose(Lethal(Clip)) == Clip);
			}
		});

		It("keeps the reaction of an articulated fall, an impact and a stagger", [this]()
		{
			for (const ETikiCombatClip Clip : { MusketArticulatedFall, MusketImpact, MusketStaggerRecover, None })
			{
				TestTrue(*UEnum::GetValueAsString(Clip), TikiShredDemo::Choose(Lethal(Clip)) == None);
			}
		});

		It("gives a lethal shotgun shot under 5 m the Musket_Burst demo, whatever reaction T-150 picked", [this]()
		{
			TestTrue(TEXT("burst picked, 3 m"), TikiShredDemo::Choose(Shotgun(300.0f, MusketBurst)) == MusketBurst);
			TestTrue(TEXT("fall picked, 3 m"), TikiShredDemo::Choose(Shotgun(300.0f, MusketArticulatedFall)) == MusketBurst);
			TestTrue(TEXT("no reaction clip, 4.9 m"), TikiShredDemo::Choose(Shotgun(490.0f, None)) == MusketBurst);
			TestTrue(TEXT("at 5 m exactly"), TikiShredDemo::Choose(Shotgun(500.0f, MusketArticulatedFall)) == MusketBurst);
		});

		It("leaves a shotgun shot at 10 m to its reaction (T-150 / T-167)", [this]()
		{
			TestTrue(TEXT("fall at 10 m"), TikiShredDemo::Choose(Shotgun(1000.0f, MusketArticulatedFall)) == None);
			TestTrue(TEXT("unknown distance"), TikiShredDemo::Choose(Shotgun(0.0f, MusketArticulatedFall)) == None);
			TestTrue(TEXT("a burst T-150 picked at 10 m still has its demo"), TikiShredDemo::Choose(Shotgun(1000.0f, MusketBurst)) == MusketBurst);
		});

		It("gives a musket burst the same demo, the musket having no close range", [this]()
		{
			TikiShredDemo::FChoice Musket = Lethal(MusketBurst);
			Musket.bFirearm = true;
			Musket.Distance = 300.0f;
			TestTrue(TEXT("musket burst"), TikiShredDemo::Choose(Musket) == MusketBurst);
			Musket.Clip = MusketArticulatedFall;
			TestTrue(TEXT("musket fall"), TikiShredDemo::Choose(Musket) == None);
		});

		It("falls back on the reaction without a demo for the clip, with the rules off or a body already cut", [this]()
		{
			TikiShredDemo::FChoice NoDemo = Lethal(SabreLow);
			NoDemo.DemoMask = PackMask() & ~(1u << static_cast<uint8>(SabreLow));
			TestTrue(TEXT("no Sabre_Low demo"), TikiShredDemo::Choose(NoDemo) == None);
			TikiShredDemo::FChoice NoSet = Shotgun(300.0f, MusketBurst);
			NoSet.DemoMask = 0;
			TestTrue(TEXT("no set (BrambleStalker)"), TikiShredDemo::Choose(NoSet) == None);
			TikiShredDemo::FChoice Off = Shotgun(300.0f, MusketBurst);
			Off.bEnabled = false;
			TestTrue(TEXT("rules off"), TikiShredDemo::Choose(Off) == None);
			TikiShredDemo::FChoice Cut = Lethal(SabreDown);
			Cut.CutsBefore = 1;
			TestTrue(TEXT("an arm cut off before"), TikiShredDemo::Choose(Cut) == None);
			TikiShredDemo::FChoice CloseOff = Shotgun(300.0f, MusketArticulatedFall);
			CloseOff.bCloseShotBurstDemo = false;
			TestTrue(TEXT("close-shot rule off"), TikiShredDemo::Choose(CloseOff) == None);
		});

		It("asks a sabre blow to cut only when the rules want it", [this]()
		{
			TikiShredDemo::FChoice Sabre = Lethal(SabreLeftToRight);
			Sabre.bSabreNeedsCut = true;
			TestTrue(TEXT("no cut"), TikiShredDemo::Choose(Sabre) == None);
			Sabre.bBlowCuts = true;
			TestTrue(TEXT("cut"), TikiShredDemo::Choose(Sabre) == SabreLeftToRight);
		});

		It("has the rules on by default, the close shot taking the burst demo", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("bShredDemos"), Rules->bShredDemos);
			TestTrue(TEXT("bCloseShotBurstDemo"), Rules->bCloseShotBurstDemo);
			TestFalse(TEXT("bShredDemoSabreNeedsCut"), Rules->bShredDemoSabreNeedsCut);
		});
	});

	Describe("Set", [this]()
	{
		It("only offers a demo with both its mesh and its animation", [this]()
		{
			UTikiShredDemoSet* Set = NewObject<UTikiShredDemoSet>();
			FTikiShredDemoClip Full;
			Full.Clip = MusketBurst;
			Full.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Test/SK_Demo_Test_Musket_Burst.SK_Demo_Test_Musket_Burst")));
			Full.Anim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Test/SK_Demo_Test_Musket_Burst_Anim.SK_Demo_Test_Musket_Burst_Anim")));
			FTikiShredDemoClip NoAnim;
			NoAnim.Clip = SabreDown;
			NoAnim.Mesh = Full.Mesh;
			Set->Demos = { Full, NoAnim };
			TestTrue(TEXT("mask"), Set->AvailableMask() == 1u << static_cast<uint8>(MusketBurst));
			TestNotNull(TEXT("burst found"), Set->Find(MusketBurst));
			TestNull(TEXT("down without animation"), Set->Find(SabreDown));
			TestNull(TEXT("none"), Set->Find(None));
		});
	});

	Describe("Timeline", [this]()
	{
		It("starts at the contact frame, later by the age of the reaction on a client", [this]()
		{
			TestEqual(TEXT("server"), TikiShredDemo::StartTime(0.4f, 0.0f, 3.0f), 0.4f);
			TestEqual(TEXT("client 150 ms late"), TikiShredDemo::StartTime(0.4f, 0.15f, 3.0f), 0.55f);
			TestEqual(TEXT("clock ahead"), TikiShredDemo::StartTime(0.4f, -0.2f, 3.0f), 0.4f);
			TestEqual(TEXT("past the end"), TikiShredDemo::StartTime(0.4f, 10.0f, 3.0f), 3.0f);
		});

		It("plays, holds, fades then is done; kept pieces stay", [this]()
		{
			using TikiShredDemo::EPhase;
			// 3 s clip from 0.4 s: 2.6 s of play, 8 s of hold, 2 s of fade
			TestTrue(TEXT("play"), TikiShredDemo::Phase(1.0f, 0.4f, 3.0f, 8.0f, 2.0f, false) == EPhase::Play);
			TestTrue(TEXT("hold"), TikiShredDemo::Phase(5.0f, 0.4f, 3.0f, 8.0f, 2.0f, false) == EPhase::Hold);
			TestTrue(TEXT("fade"), TikiShredDemo::Phase(11.0f, 0.4f, 3.0f, 8.0f, 2.0f, false) == EPhase::Fade);
			TestTrue(TEXT("done"), TikiShredDemo::Phase(13.0f, 0.4f, 3.0f, 8.0f, 2.0f, false) == EPhase::Done);
			TestTrue(TEXT("kept"), TikiShredDemo::Phase(13.0f, 0.4f, 3.0f, 8.0f, 2.0f, true) == EPhase::Kept);
			TestEqual(TEXT("half faded"), TikiShredDemo::FadeAlpha(11.6f, 0.4f, 3.0f, 8.0f, 2.0f), 0.5f, 1.e-4f);
		});
	});

	Describe("Facing", [this]()
	{
		It("turns the victim toward where the blow came from", [this]()
		{
			// a blow travelling along +X came from -X: the victim faces -X (180)
			TestEqual(TEXT("blow +X"), FMath::Abs(TikiShredDemo::FacingYaw(FVector(1.0, 0.0, 0.0), 0.0f)), 180.0f, 1.e-3f);
			TestEqual(TEXT("blow -Y"), TikiShredDemo::FacingYaw(FVector(0.0, -1.0, 0.0), 0.0f), 90.0f, 1.e-3f);
			TestEqual(TEXT("vertical blow keeps the mob's yaw"), TikiShredDemo::FacingYaw(FVector(0.0, 0.0, -1.0), 37.0f), 37.0f, 1.e-3f);
		});
	});
}

#endif
