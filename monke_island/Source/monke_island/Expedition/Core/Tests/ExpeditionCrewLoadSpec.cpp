// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Expedition.CrewLoad
// Network review T-268 R-02 (T-292), cahier §16, §24.2 step 4, §24.5: the clock starts once the crew loaded the island.
BEGIN_DEFINE_SPEC(FExpeditionCrewLoadSpec, "monke_island.Expedition.CrewLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	/** JoinGrace and Timeout of the rules' defaults */
	double Grace = 0.0;
	double Timeout = 0.0;
END_DEFINE_SPEC(FExpeditionCrewLoadSpec)

void FExpeditionCrewLoadSpec::Define()
{
	BeforeEach([this]()
	{
		const UExpeditionRules* Rules = GetDefault<UExpeditionRules>();
		Grace = Rules->CrewJoinGraceSeconds;
		Timeout = Rules->CrewLoadTimeoutSeconds;
	});

	Describe("Defaults", [this]()
	{
		It("waits 60 s at most (cahier §16), a short grace for unannounced clients", [this]()
		{
			TestEqual(TEXT("timeout"), Timeout, 60.0);
			TestTrue(TEXT("grace shorter than the timeout"), Grace > 0.0 && Grace < Timeout);
		});

		It("no longer starts the clock at BeginPlay: it waits for the crew (R-02)", [this]()
		{
			const AExpeditionGameState* Defaults = GetDefault<AExpeditionGameState>();
			TestFalse(TEXT("bStartOnBeginPlay"), Defaults->bStartOnBeginPlay);
			TestTrue(TEXT("bStartWhenCrewLoaded"), Defaults->bStartWhenCrewLoaded);
		});
	});

	Describe("ShouldStartClock", [this]()
	{
		It("starts at once in solo", [this]()
		{
			TestTrue(TEXT("solo, pawn there"), AExpeditionGameState::ShouldStartClock(true, 1, 0, 1, 0.0, Grace, Timeout));
			TestTrue(TEXT("solo, whatever the counts"), AExpeditionGameState::ShouldStartClock(true, 0, 0, 0, 0.0, Grace, Timeout));
		});

		It("waits for every player of the previous map (R-02: the client 20 s behind the host)", [this]()
		{
			// the host is in, the client is not even connected again yet
			TestFalse(TEXT("1 of 2 at BeginPlay"), AExpeditionGameState::ShouldStartClock(false, 1, 0, 2, 0.0, Grace, Timeout));
			// the client connected, loading the island
			TestFalse(TEXT("1 of 2, one loading"), AExpeditionGameState::ShouldStartClock(false, 1, 1, 2, 12.0, Grace, Timeout));
			// in, with a pawn
			TestTrue(TEXT("2 of 2"), AExpeditionGameState::ShouldStartClock(false, 2, 0, 2, 20.0, Grace, Timeout));
		});

		It("starts at once when the host comes alone from the previous map", [this]()
		{
			TestTrue(TEXT("1 of 1"), AExpeditionGameState::ShouldStartClock(false, 1, 0, 1, 0.0, Grace, Timeout));
		});

		It("waits for a client still joining, even past the expected count", [this]()
		{
			TestFalse(TEXT("2 of 2 and a third connecting"), AExpeditionGameState::ShouldStartClock(false, 2, 1, 2, 5.0, Grace, Timeout));
		});

		It("gives a short grace when nobody is announced (a map opened directly, PIE)", [this]()
		{
			TestFalse(TEXT("host alone at BeginPlay"), AExpeditionGameState::ShouldStartClock(false, 1, 0, 0, 0.0, Grace, Timeout));
			TestTrue(TEXT("grace over"), AExpeditionGameState::ShouldStartClock(false, 1, 0, 0, Grace, Grace, Timeout));
			TestFalse(TEXT("a client connected meanwhile"), AExpeditionGameState::ShouldStartClock(false, 1, 1, 0, Grace + 1.0, Grace, Timeout));
		});

		It("starts anyway at the end of the wait, the late ones put on the deck when they come", [this]()
		{
			TestTrue(TEXT("still loading at the timeout"), AExpeditionGameState::ShouldStartClock(false, 1, 1, 2, Timeout, Grace, Timeout));
			TestTrue(TEXT("a player gone during the travel"), AExpeditionGameState::ShouldStartClock(false, 1, 0, 2, Timeout + 0.5, Grace, Timeout));
			TestFalse(TEXT("nobody in yet before it"), AExpeditionGameState::ShouldStartClock(false, 0, 0, 2, Timeout - 1.0, Grace, Timeout));
		});
	});
}

#endif
