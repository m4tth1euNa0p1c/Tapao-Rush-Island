// monke_island

#include "Misc/AutomationTest.h"
#include "DayNight/DayNightMath.h"
#include "DayNight/DayNightRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.DayNight
BEGIN_DEFINE_SPEC(FDayNightSpec, "monke_island.DayNight.Clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UDayNightRules* Rules = nullptr;
END_DEFINE_SPEC(FDayNightSpec)

void FDayNightSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UDayNightRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Cahier defaults", [this]()
	{
		It("starts in full day, with the sun high", [this]()
		{
			const FVector Sun = DayNightMath::SunDirectionENU(DayNightMath::SolarHourAt(*Rules, 0.0), Rules->LatitudeDeg, Rules->DayOfYear);
			TestTrue(TEXT("sun above 30 degrees"), DayNightMath::ElevationDeg(Sun) > 30.0);
		});

		It("reaches dusk at 120 s: sun at the horizon", [this]()
		{
			const FVector Sun = DayNightMath::SunDirectionENU(DayNightMath::SolarHourAt(*Rules, 120.0), Rules->LatitudeDeg, Rules->DayOfYear);
			const double Elevation = DayNightMath::ElevationDeg(Sun);
			TestTrue(TEXT("sun between -6 and +6 degrees"), Elevation > -6.0 && Elevation < 6.0);
		});

		It("is night at 240 s: sun well under the horizon", [this]()
		{
			const FVector Sun = DayNightMath::SunDirectionENU(DayNightMath::SolarHourAt(*Rules, 240.0), Rules->LatitudeDeg, Rules->DayOfYear);
			TestTrue(TEXT("sun under -6 degrees"), DayNightMath::ElevationDeg(Sun) < -6.0);
		});

		It("loops back to the start hour after one cycle", [this]()
		{
			TestTrue(TEXT("looping"), DayNightMath::IsLooping(*Rules));
			const double Period = Rules->Keys.Last().ElapsedSeconds;
			TestEqual(TEXT("hour after one loop"), FMath::Fmod(DayNightMath::SolarHourAt(*Rules, Period + 30.0), 24.0),
				FMath::Fmod(DayNightMath::SolarHourAt(*Rules, 30.0), 24.0), 0.001);
		});

		It("never goes back in time", [this]()
		{
			double Previous = DayNightMath::SolarHourAt(*Rules, 0.0);
			for (double Elapsed = 1.0; Elapsed < 2000.0; Elapsed += 1.0)
			{
				const double Hour = DayNightMath::SolarHourAt(*Rules, Elapsed);
				if (!TestTrue(TEXT("monotonic"), Hour >= Previous))
				{
					break;
				}
				Previous = Hour;
			}
		});
	});

	Describe("Sun path", [this]()
	{
		It("rises in the east and sets in the west", [this]()
		{
			TestTrue(TEXT("morning east"), DayNightMath::SunDirectionENU(7.0, Rules->LatitudeDeg, Rules->DayOfYear).X > 0.5);
			TestTrue(TEXT("evening west"), DayNightMath::SunDirectionENU(17.0, Rules->LatitudeDeg, Rules->DayOfYear).X < -0.5);
		});

		It("maps north to -Y in the map frame (X = east, Y = south)", [this]()
		{
			const FVector North = DayNightMath::ENUToWorld(FVector(0.0, 1.0, 0.0), Rules->NorthYawDeg);
			const FVector East = DayNightMath::ENUToWorld(FVector(1.0, 0.0, 0.0), Rules->NorthYawDeg);
			TestTrue(TEXT("north = -Y"), North.Equals(FVector(0.0, -1.0, 0.0), 0.001));
			TestTrue(TEXT("east = +X"), East.Equals(FVector(1.0, 0.0, 0.0), 0.001));
		});
	});

	Describe("Debug jump", [this]()
	{
		It("finds the elapsed time of a wanted hour", [this]()
		{
			for (const double Hour : {6.0, 12.0, 18.25, 22.0, 3.0})
			{
				const double Elapsed = DayNightMath::ElapsedForHour(*Rules, Hour, 10.0);
				TestTrue(TEXT("not in the past"), Elapsed >= 10.0);
				TestEqual(FString::Printf(TEXT("hour %.2f"), Hour), FMath::Fmod(DayNightMath::SolarHourAt(*Rules, Elapsed), 24.0), Hour, 0.01);
			}
		});
	});
}

#endif
