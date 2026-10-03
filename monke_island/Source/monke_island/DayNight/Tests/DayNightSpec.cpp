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

	Describe("Rhythm (T-067: day 12 min, dusk 15 s, night 6 min, dawn 15 s)", [this]()
	{
		const auto Elevation = [this](double Elapsed)
		{
			return DayNightMath::ElevationDeg(DayNightMath::SunDirectionENU(DayNightMath::SolarHourAt(*Rules, Elapsed), Rules->LatitudeDeg, Rules->DayOfYear));
		};

		It("is day from 0 to 12 min: sun above the twilight", [this, Elevation]()
		{
			for (double T = 1.0; T < 719.0; T += 7.0)
			{
				if (!TestTrue(FString::Printf(TEXT("day at %.0f s"), T), Elevation(T) > Rules->TwilightElevationDeg))
				{
					break;
				}
			}
		});

		It("has a dusk of 15 s and a night of 6 min", [this, Elevation]()
		{
			const double Dusk = Elevation(727.5);
			TestTrue(TEXT("dusk at 12:07"), Dusk > -Rules->TwilightElevationDeg && Dusk < Rules->TwilightElevationDeg);
			for (double T = 737.0; T < 1093.0; T += 7.0)
			{
				if (!TestTrue(FString::Printf(TEXT("night at %.0f s"), T), Elevation(T) < -Rules->TwilightElevationDeg))
				{
					break;
				}
			}
			const double Dawn = Elevation(1102.5);
			TestTrue(TEXT("dawn at 18:22"), Dawn > -Rules->TwilightElevationDeg && Dawn < Rules->TwilightElevationDeg);
		});

		It("lasts 18 min 30 a day", [this]()
		{
			TestEqual(TEXT("period"), DayNightMath::DayPeriod(*Rules), 1110.0, 0.01);
		});
	});

	Describe("Day counter", [this]()
	{
		It("adds a day each time a night has passed", [this]()
		{
			TestEqual(TEXT("first day"), DayNightMath::DayNumberAt(*Rules, 0.0), 1);
			TestEqual(TEXT("still day 1 at night"), DayNightMath::DayNumberAt(*Rules, 1000.0), 1);
			TestEqual(TEXT("day 2 after the first dawn"), DayNightMath::DayNumberAt(*Rules, 1111.0), 2);
			TestEqual(TEXT("day 3"), DayNightMath::DayNumberAt(*Rules, 2221.0), 3);
			TestEqual(TEXT("day 3 begins at 37 min"), DayNightMath::DayStartElapsed(*Rules, 3), 2220.0, 0.01);
		});
	});

	Describe("Clock", [this]()
	{

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
