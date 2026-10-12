// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/FirearmTracer.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FFirearmTracerSpec, "monke_island.Weapons.Tracer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFirearmTracerSpec)

void FFirearmTracerSpec::Define()
{
	It("draws every round with EveryNth 1, then one round in N", [this]()
	{
		for (int32 Round = 0; Round < 6; ++Round)
		{
			TestTrue(TEXT("every round"), FirearmTracerMath::IsTracerRound(Round, 1));
		}
		TestTrue(TEXT("1st of 3"), FirearmTracerMath::IsTracerRound(0, 3));
		TestFalse(TEXT("2nd of 3"), FirearmTracerMath::IsTracerRound(1, 3));
		TestFalse(TEXT("3rd of 3"), FirearmTracerMath::IsTracerRound(2, 3));
		TestTrue(TEXT("4th: next tracer"), FirearmTracerMath::IsTracerRound(3, 3));
		TestTrue(TEXT("0 behaves like 1"), FirearmTracerMath::IsTracerRound(5, 0));
	});

	It("ends at the hit, or where the drawn flight is cut", [this]()
	{
		const FVector From(0.0f, 0.0f, 0.0f);
		TestEqual(TEXT("a near hit"), FirearmTracerMath::FlightEnd(From, FVector(1000.0f, 0.0f, 0.0f), 25000.0f), FVector(1000.0f, 0.0f, 0.0f));
		TestEqual(TEXT("a shot into the sky"), FirearmTracerMath::FlightEnd(From, FVector(0.0f, 0.0f, 100000.0f), 25000.0f), FVector(0.0f, 0.0f, 25000.0f));
	});

	It("grows the streak out of the muzzle, then keeps its length", [this]()
	{
		TestEqual(TEXT("just fired"), FirearmTracerMath::StreakLength(30.0f, 120.0f), 30.0f);
		TestEqual(TEXT("in flight"), FirearmTracerMath::StreakLength(4000.0f, 120.0f), 120.0f);
		TestEqual(TEXT("never negative"), FirearmTracerMath::StreakLength(-5.0f, 120.0f), 0.0f);
	});
}

#endif
