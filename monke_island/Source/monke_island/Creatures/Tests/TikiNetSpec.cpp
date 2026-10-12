// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"

#if WITH_AUTOMATION_TESTS

// T-306 (review R-27): a telegraph received late starts as far into its clip as the server is, ends with it, and is
// never replayed once over; T-312 (review R-28): the network rates of the creatures
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.Net
BEGIN_DEFINE_SPEC(FTikiNetSpec, "monke_island.Tiki.Net",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiNetSpec)

void FTikiNetSpec::Define()
{
	Describe("LateAction", [this]()
	{
		It("plays a blow received on time from its start, as long as its clip", [this]()
		{
			float Start = -1.0f;
			float Left = -1.0f;
			TestTrue(TEXT("plays"), TikiMath::LateActionStart(0.0f, 0.0f, 1.0f, 1.2f, false, Start, Left));
			TestEqual(TEXT("from its start"), Start, 0.0f, 0.0001f);
			TestEqual(TEXT("its whole clip"), Left, 1.2f, 0.0001f);
		});

		It("starts a blow received late that far in, so it lands with the server's", [this]()
		{
			float Start = 0.0f;
			float Left = 0.0f;
			// 150 ms late (100 ms of lag and a resend): 0.15 s into a 1.2 s clip, 1.05 s left
			TestTrue(TEXT("plays"), TikiMath::LateActionStart(0.15f, 0.0f, 1.0f, 1.2f, false, Start, Left));
			TestEqual(TEXT("in the clip"), Start, 0.15f, 0.0001f);
			TestEqual(TEXT("left"), Left, 1.05f, 0.0001f);
			// the Coco's heavy blow: 1.5 s in all at rate 0.8 of a 1.0 s clip, 0.5 s late: 0.4 s into the clip, 1.0 s left
			TestTrue(TEXT("heavy blow"), TikiMath::LateActionStart(0.5f, 1.5f, 0.8f, 1.0f, false, Start, Left));
			TestEqual(TEXT("heavy blow in the clip"), Start, 0.4f, 0.0001f);
			TestEqual(TEXT("heavy blow left"), Left, 1.0f, 0.0001f);
			// the clip holds its last frame when the action lasts longer than it
			TestTrue(TEXT("held"), TikiMath::LateActionStart(1.4f, 1.5f, 1.0f, 1.0f, false, Start, Left));
			TestEqual(TEXT("last frame"), Start, 1.0f, 0.0001f);
			TestEqual(TEXT("held left"), Left, 0.1f, 0.0001f);
		});

		It("never replays an action already over (a creature coming back in relevancy)", [this]()
		{
			float Start = 0.0f;
			float Left = 0.0f;
			TestFalse(TEXT("blow over"), TikiMath::LateActionStart(1.3f, 0.0f, 1.0f, 1.2f, false, Start, Left));
			TestEqual(TEXT("nothing left"), Left, 0.0f, 0.0001f);
			TestFalse(TEXT("exactly over"), TikiMath::LateActionStart(1.5f, 1.5f, 1.0f, 1.0f, false, Start, Left));
			TestFalse(TEXT("long ago"), TikiMath::LateActionStart(30.0f, 4.0f, 1.0f, 1.0f, true, Start, Left));
		});

		It("wraps a looped activity, and takes a clock behind the server as on time", [this]()
		{
			float Start = 0.0f;
			float Left = 0.0f;
			// a villager's work: 4 s of a 1.5 s loop, received 2.5 s late: 1.0 s into the loop, 1.5 s left
			TestTrue(TEXT("loop"), TikiMath::LateActionStart(2.5f, 4.0f, 1.0f, 1.5f, true, Start, Left));
			TestEqual(TEXT("wrapped"), Start, 1.0f, 0.0001f);
			TestEqual(TEXT("loop left"), Left, 1.5f, 0.0001f);
			TestTrue(TEXT("early clock"), TikiMath::LateActionStart(-0.2f, 0.0f, 1.0f, 1.2f, false, Start, Left));
			TestEqual(TEXT("from its start"), Start, 0.0f, 0.0001f);
			TestEqual(TEXT("whole clip"), Left, 1.2f, 0.0001f);
		});
	});

	Describe("Rates", [this]()
	{
		It("send the warriors more often than the villagers, and the villagers less far, below the engine's 100 Hz", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("warriors below 100 Hz"), Rules->WarriorNet.NetUpdateFrequency < 100.0f);
			TestTrue(TEXT("villagers below the warriors"), Rules->VillagerNet.NetUpdateFrequency < Rules->WarriorNet.NetUpdateFrequency);
			TestTrue(TEXT("villagers relevant nearer"), Rules->VillagerNet.NetCullDistance < Rules->WarriorNet.NetCullDistance);
			TestTrue(TEXT("minimum below the rate"), Rules->WarriorNet.MinNetUpdateFrequency <= Rules->WarriorNet.NetUpdateFrequency
				&& Rules->VillagerNet.MinNetUpdateFrequency <= Rules->VillagerNet.NetUpdateFrequency);
			TestTrue(TEXT("corpses below 100 Hz"), Rules->CorpseNetUpdateFrequency < 100.0f);
		});
	});
}

#endif
