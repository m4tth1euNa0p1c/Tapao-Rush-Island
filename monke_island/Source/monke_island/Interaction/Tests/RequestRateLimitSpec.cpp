// monke_island

#include "Misc/AutomationTest.h"
#include "Core/RequestRateLimiter.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Weapons/IslanderInventoryComponent.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Interaction.RateLimit
// The server's token bucket per player on the repeatable requests: use, lift, let go, throw, put in a container, slot
// keys (security review T-269 M1, T-290)
BEGIN_DEFINE_SPEC(FRequestRateLimitSpec, "monke_island.Interaction.RateLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** Requests accepted out of Count sent every Interval seconds from Start */
	static int32 Send(FRequestRateLimiter& Limiter, int32 Count, double Start, double Interval, float Burst, float Rate)
	{
		int32 Accepted = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Accepted += Limiter.Consume(Start + Index * Interval, Burst, Rate) ? 1 : 0;
		}
		return Accepted;
	}
END_DEFINE_SPEC(FRequestRateLimitSpec)

void FRequestRateLimitSpec::Define()
{
	It("lets a burst through at once, then refuses until tokens come back", [this]()
	{
		FRequestRateLimiter Limiter;
		TestEqual(TEXT("the burst"), Send(Limiter, 10, 5.0, 0.0, 10.0f, 10.0f), 10);
		TestFalse(TEXT("one more at the same instant"), Limiter.Consume(5.0, 10.0f, 10.0f));
		TestEqual(TEXT("one refusal counted"), Limiter.Refused, 1);
		// half a second at 10 per second: 5 tokens back
		TestEqual(TEXT("five back"), Send(Limiter, 6, 5.5, 0.0, 10.0f, 10.0f), 5);
	});

	It("never keeps more than the burst, however long the player waited", [this]()
	{
		FRequestRateLimiter Limiter;
		Limiter.Consume(0.0, 10.0f, 10.0f);
		TestEqual(TEXT("a burst after a long wait"), Send(Limiter, 50, 1000.0, 0.0, 10.0f, 10.0f), 10);
	});

	It("lets a player mash a key through and holds a flood to the refill rate", [this]()
	{
		FRequestRateLimiter Human;
		TestEqual(TEXT("10 presses a second for 5 s: all"), Send(Human, 50, 0.0, 0.1, 10.0f, 10.0f), 50);
		FRequestRateLimiter Flood;
		// 1 000 requests in one second: the burst, then 10 a second
		const int32 Passed = Send(Flood, 1000, 0.0, 0.001, 10.0f, 10.0f);
		TestTrue(TEXT("a flood held to about the burst plus the refill"), Passed >= 10 && Passed <= 21);
		TestEqual(TEXT("the rest refused"), Flood.Refused, 1000 - Passed);
	});

	It("gives nothing back on a clock that went back, and survives broken settings", [this]()
	{
		FRequestRateLimiter Limiter;
		Send(Limiter, 10, 100.0, 0.0, 10.0f, 10.0f);
		TestFalse(TEXT("a new world's clock gives nothing"), Limiter.Consume(3.0, 10.0f, 10.0f));
		TestTrue(TEXT("then counts from it"), Limiter.Consume(3.2, 10.0f, 10.0f));
		FRequestRateLimiter Broken;
		TestTrue(TEXT("a burst not a number counts one"), Broken.Consume(0.0, NAN, NAN));
		TestFalse(TEXT("no refill not a number"), Broken.Consume(10.0, NAN, NAN));
		FRequestRateLimiter Zero;
		TestTrue(TEXT("a burst of zero counts one"), Zero.Consume(0.0, 0.0f, 1.0f));
	});

	It("ships settings that never refuse a player mashing the keys", [this]()
	{
		const UIslanderGrabComponent* Grab = GetDefault<UIslanderGrabComponent>();
		const UIslanderInventoryComponent* Inventory = GetDefault<UIslanderInventoryComponent>();
		FRequestRateLimiter GrabLimiter;
		FRequestRateLimiter SlotLimiter;
		// F, G, a click, a key: 10 a second for 3 s
		TestEqual(TEXT("interactions"), Send(GrabLimiter, 30, 0.0, 0.1, Grab->RequestBurst, Grab->RequestsPerSecond), 30);
		TestEqual(TEXT("slot keys"), Send(SlotLimiter, 30, 0.0, 0.1, Inventory->SlotRequestBurst, Inventory->SlotRequestsPerSecond), 30);
		TestTrue(TEXT("interactions limited"), Grab->RequestsPerSecond <= 30.0f);
		TestTrue(TEXT("slot keys limited"), Inventory->SlotRequestsPerSecond <= 30.0f);
	});
}

#endif
