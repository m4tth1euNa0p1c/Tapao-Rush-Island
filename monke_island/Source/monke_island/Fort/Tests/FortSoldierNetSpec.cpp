// monke_island

#include "Misc/AutomationTest.h"
#include "Fort/FortSoldierNetMath.h"
#include "Fort/FortSoldierRules.h"
#include <limits>

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Fort.Net
// Network of the fort soldiers (T-317, network review T-268 R-28): rates of the living and the dead, dormancy of a dead
// soldier once its body lies still, removal of the body out of the crew's sight
BEGIN_DEFINE_SPEC(FFortSoldierNetSpec, "monke_island.Fort.Net",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFortSoldierNetSpec)

void FFortSoldierNetSpec::Define()
{
	Describe("Rates", [this]()
	{
		It("sends a living soldier below the engine's 100 Hz and a dead one less often still (DA_FortSoldierRules defaults)", [this]()
		{
			const UFortSoldierRules* Rules = GetDefault<UFortSoldierRules>();
			TestTrue(TEXT("living below 100 Hz"), Rules->NetUpdateFrequency < 100.0f);
			TestTrue(TEXT("minimum below the rate"), Rules->MinNetUpdateFrequency <= Rules->NetUpdateFrequency);
			TestTrue(TEXT("dead below the living"), Rules->DeadNetUpdateFrequency < Rules->NetUpdateFrequency);
			TestTrue(TEXT("relevant at least 10 m away"), Rules->NetCullDistance >= 1000.0f);
			TestTrue(TEXT("dormant before the body goes"), Rules->DeadLifetimeSeconds <= 0.0f || Rules->DeadDormancySeconds < Rules->DeadLifetimeSeconds);
		});

		It("keeps the rates within the engine's bounds, the dead one never above the living one", [this]()
		{
			const FortSoldierNet::FRates Alive = FortSoldierNet::PickRates(false, 30.0f, 2.0f, 10.0f);
			TestEqual(TEXT("living"), Alive.Frequency, 30.0f);
			TestEqual(TEXT("living minimum"), Alive.MinFrequency, 2.0f);
			const FortSoldierNet::FRates Dead = FortSoldierNet::PickRates(true, 30.0f, 2.0f, 10.0f);
			TestEqual(TEXT("dead"), Dead.Frequency, 10.0f);
			TestTrue(TEXT("dead minimum below its rate"), Dead.MinFrequency <= Dead.Frequency);
			TestEqual(TEXT("a dead rate above the living one"), FortSoldierNet::PickRates(true, 20.0f, 2.0f, 60.0f).Frequency, 20.0f);
			TestEqual(TEXT("above 100"), FortSoldierNet::PickRates(false, 500.0f, 2.0f, 10.0f).Frequency, 100.0f);
			TestEqual(TEXT("a minimum above the rate"), FortSoldierNet::PickRates(false, 5.0f, 50.0f, 1.0f).MinFrequency, 5.0f);
			TestEqual(TEXT("not a number"), FortSoldierNet::PickRates(false, std::numeric_limits<float>::quiet_NaN(), 2.0f, 1.0f).Frequency, 30.0f);
		});
	});

	Describe("Dead", [this]()
	{
		It("goes dormant once dead for the delay, its body settled", [this]()
		{
			TestFalse(TEXT("too soon"), FortSoldierNet::ShouldGoDormant(1.0, 3.0f, true));
			TestTrue(TEXT("on time"), FortSoldierNet::ShouldGoDormant(3.0, 3.0f, true));
			TestFalse(TEXT("still falling or carried"), FortSoldierNet::ShouldGoDormant(10.0, 3.0f, false));
			TestTrue(TEXT("no delay"), FortSoldierNet::ShouldGoDormant(0.0, 0.0f, true));
		});

		It("removes the body after its lifetime, never in front of a player, never with a lifetime of 0", [this]()
		{
			TestFalse(TEXT("before its lifetime"), FortSoldierNet::ShouldRemoveBody(100.0, 180.0f, 1.0e9, 3000.0f));
			TestTrue(TEXT("after, nobody near"), FortSoldierNet::ShouldRemoveBody(180.0, 180.0f, 5000.0, 3000.0f));
			TestFalse(TEXT("after, a player near"), FortSoldierNet::ShouldRemoveBody(400.0, 180.0f, 1500.0, 3000.0f));
			TestFalse(TEXT("lifetime 0: it stays"), FortSoldierNet::ShouldRemoveBody(1.0e6, 0.0f, 1.0e9, 3000.0f));
		});

		It("looks again when the dormancy or the lifetime is due, never sooner than the check, and stops when nothing is left", [this]()
		{
			TestEqual(TEXT("first look at the dormancy"), FortSoldierNet::NextCheckDelay(0.0, false, 3.0f, 180.0f, 2.0f), 3.0);
			TestEqual(TEXT("still falling: the check"), FortSoldierNet::NextCheckDelay(5.0, false, 3.0f, 180.0f, 2.0f), 2.0);
			TestEqual(TEXT("dormant: the lifetime"), FortSoldierNet::NextCheckDelay(5.0, true, 3.0f, 180.0f, 2.0f), 175.0);
			TestEqual(TEXT("a player near: the check"), FortSoldierNet::NextCheckDelay(200.0, true, 3.0f, 180.0f, 2.0f), 2.0);
			TestTrue(TEXT("dormant for good"), FortSoldierNet::NextCheckDelay(5.0, true, 3.0f, 0.0f, 2.0f) < 0.0);
		});
	});
}

#endif
