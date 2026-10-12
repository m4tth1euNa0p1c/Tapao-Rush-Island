// monke_island

#include "Misc/AutomationTest.h"
#include "Player/IslanderKnockoutMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Player.Knockout (T-287, review T-268 R-22: the server alone knocks
// out and stands up, every machine plays the ragdoll from the server's start and stands up at the server's place)
BEGIN_DEFINE_SPEC(FIslanderKnockoutSpec, "monke_island.Player.Knockout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FIslanderKnockoutSpec)

void FIslanderKnockoutSpec::Define()
{
	using IslanderKnockoutMath::EAction;

	Describe("Replicated state", [this]()
	{
		It("starts the ragdoll of a body the server has down, once", [this]()
		{
			TestTrue(TEXT("knocked out there, standing here"), IslanderKnockoutMath::Resolve(true, false, false, false) == EAction::Start);
			TestTrue(TEXT("already a ragdoll here"), IslanderKnockoutMath::Resolve(true, false, true, false) == EAction::None);
			TestTrue(TEXT("dead there, joined late"), IslanderKnockoutMath::Resolve(false, true, false, false) == EAction::Start);
		});

		It("stands up only on the server's word, never a corpse", [this]()
		{
			TestTrue(TEXT("stood up there"), IslanderKnockoutMath::Resolve(false, false, true, false) == EAction::Stand);
			TestTrue(TEXT("still down there"), IslanderKnockoutMath::Resolve(true, false, true, false) == EAction::None);
			TestTrue(TEXT("dead here (death multicast first)"), IslanderKnockoutMath::Resolve(false, false, true, true) == EAction::None);
			TestTrue(TEXT("dead there"), IslanderKnockoutMath::Resolve(true, true, true, false) == EAction::None);
			TestTrue(TEXT("standing on both"), IslanderKnockoutMath::Resolve(false, false, false, false) == EAction::None);
		});

		It("numbers the knockouts without ever going back to 0", [this]()
		{
			TestEqual(TEXT("first"), static_cast<int32>(IslanderKnockoutMath::NextSerial(0)), 1);
			TestEqual(TEXT("next"), static_cast<int32>(IslanderKnockoutMath::NextSerial(41)), 42);
			TestEqual(TEXT("wraps to 1"), static_cast<int32>(IslanderKnockoutMath::NextSerial(MAX_uint8)), 1);
		});
	});

	Describe("Catch-up of a client", [this]()
	{
		It("follows the flight of the server's body under gravity", [this]()
		{
			const FVector Start(0.0, 0.0, 100.0);
			const FVector Throw(1891.0, 0.0, 1478.0);
			const float GravityZ = -980.0f;
			TestEqual(TEXT("at once"), IslanderKnockoutMath::BallisticLocation(Start, Throw, GravityZ, 0.0f), Start, 0.01f);
			TestEqual(TEXT("0.1 s later"), IslanderKnockoutMath::BallisticLocation(Start, Throw, GravityZ, 0.1f), FVector(189.1, 0.0, 242.9), 0.01f);
			TestEqual(TEXT("its speed then"), IslanderKnockoutMath::BallisticVelocity(Throw, GravityZ, 0.1f), FVector(1891.0, 0.0, 1380.0), 0.01f);
			TestEqual(TEXT("no flight back in time"), IslanderKnockoutMath::BallisticLocation(Start, Throw, GravityZ, -1.0f), Start, 0.01f);
		});

		It("catches up a fresh knockout, drops an old one where it is", [this]()
		{
			TestEqual(TEXT("one way delay"), IslanderKnockoutMath::CatchUpAge(100.15, 100.0, 0.4f), 0.15f, 0.001f);
			TestEqual(TEXT("clock estimate behind"), IslanderKnockoutMath::CatchUpAge(99.95, 100.0, 0.4f), 0.0f, 0.001f);
			TestEqual(TEXT("at the bound"), IslanderKnockoutMath::CatchUpAge(100.4, 100.0, 0.4f), 0.4f, 0.001f);
			TestTrue(TEXT("joined 3 s later"), IslanderKnockoutMath::CatchUpAge(103.0, 100.0, 0.4f) < 0.0f);
			TestTrue(TEXT("no catch-up"), IslanderKnockoutMath::CatchUpAge(100.1, 100.0, 0.0f) < 0.0f);
		});
	});

	Describe("Capsule of the server", [this]()
	{
		It("stands where the body would stand up, above its pelvis", [this]()
		{
			TestEqual(TEXT("bean"), IslanderKnockoutMath::FollowLocation(FVector(500.0, -20.0, 15.0), 52.5f), FVector(500.0, -20.0, 67.5), 0.01f);
		});

		It("follows a moving body, not one at rest", [this]()
		{
			const FVector Capsule(0.0, 0.0, 90.0);
			TestFalse(TEXT("3 cm"), IslanderKnockoutMath::ShouldFollow(Capsule, Capsule + FVector(3.0, 0.0, 0.0), 5.0f));
			TestTrue(TEXT("5 cm"), IslanderKnockoutMath::ShouldFollow(Capsule, Capsule + FVector(0.0, 5.0, 0.0), 5.0f));
			TestTrue(TEXT("thrown 2.4 m in 0.1 s"), IslanderKnockoutMath::ShouldFollow(Capsule, Capsule + FVector(240.0, 0.0, 0.0), 5.0f));
		});
	});
}

#endif
