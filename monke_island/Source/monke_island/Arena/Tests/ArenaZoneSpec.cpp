// monke_island

#include "Misc/AutomationTest.h"
#include "Arena/ArenaZone.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FArenaZoneSpec, "monke_island.Arena.Zone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FArenaZoneSpec)

void FArenaZoneSpec::Define()
{
	Describe("ClampIntoCircle", [this]()
	{
		It("leaves a location inside the circle untouched", [this]()
		{
			FVector Location(300.0, -400.0, 120.0);
			TestFalse(TEXT("moved"), ArenaZoneMath::ClampIntoCircle(FVector::ZeroVector, 2500.0f, Location));
			TestEqual(TEXT("location"), Location, FVector(300.0, -400.0, 120.0));
		});

		It("puts a location outside back on the edge, on the same bearing, keeping its height", [this]()
		{
			FVector Location(3000.0, 4000.0, 75.0);
			TestTrue(TEXT("moved"), ArenaZoneMath::ClampIntoCircle(FVector(0.0, 0.0, 500.0), 2450.0f, Location));
			TestEqual(TEXT("x"), Location.X, 1470.0, 0.01);
			TestEqual(TEXT("y"), Location.Y, 1960.0, 0.01);
			TestEqual(TEXT("z kept"), Location.Z, 75.0);
		});

		It("works around an off-origin centre", [this]()
		{
			FVector Location(1000.0, 0.0, 0.0);
			TestTrue(TEXT("moved"), ArenaZoneMath::ClampIntoCircle(FVector(-1000.0, 0.0, 0.0), 500.0f, Location));
			TestEqual(TEXT("x"), Location.X, -500.0, 0.01);
		});
	});
}

#endif
