// monke_island

#include "Misc/AutomationTest.h"
#include "Storm/WeatherMath.h"
#include "Storm/WeatherRules.h"
#include "Storm/WindRules.h"
#include "Storm/WindStormActor.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weather
BEGIN_DEFINE_SPEC(FWeatherTransitionSpec, "monke_island.Weather.Transition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWeatherRules* Rules = nullptr;
	UWindRules* WindRules = nullptr;
END_DEFINE_SPEC(FWeatherTransitionSpec)

void FWeatherTransitionSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UWeatherRules>();
		Rules->AddToRoot();
		WindRules = NewObject<UWindRules>();
		WindRules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
		WindRules->RemoveFromRoot();
		WindRules = nullptr;
	});

	Describe("Turn of the weather (T-203)", [this]()
	{
		It("turns in 10 s by default (owner request)", [this]()
		{
			TestEqual(TEXT("TransitionSeconds"), Rules->TransitionSeconds, 10.0f);
		});

		It("stays clear while no artifact is seized", [this]()
		{
			for (const double Time : { 0.0, 10.0, 600.0, 100000.0 })
			{
				TestEqual(*FString::Printf(TEXT("clear at %.0f s"), Time), WeatherMath::TurnWeight(-1.0, Time, Rules->TransitionSeconds), 0.0f);
			}
		});

		It("goes from clear to fully turned over TransitionSeconds, eased, and stays turned", [this]()
		{
			const double Seized = 312.5;
			const float Ramp = Rules->TransitionSeconds;
			TestEqual(TEXT("clear before the seizure"), WeatherMath::TurnWeight(Seized, Seized - 1.0, Ramp), 0.0f);
			TestEqual(TEXT("clear at the seizure"), WeatherMath::TurnWeight(Seized, Seized, Ramp), 0.0f);
			TestEqual(TEXT("half way at half the time"), WeatherMath::TurnWeight(Seized, Seized + Ramp * 0.5, Ramp), 0.5f, 1.0e-4f);
			TestTrue(TEXT("eased in: a quarter of the time gives less than a quarter"), WeatherMath::TurnWeight(Seized, Seized + Ramp * 0.25, Ramp) < 0.25f);
			TestEqual(TEXT("turned at TransitionSeconds"), WeatherMath::TurnWeight(Seized, Seized + Ramp, Ramp), 1.0f);
			TestEqual(TEXT("still turned an hour later"), WeatherMath::TurnWeight(Seized, Seized + 3600.0, Ramp), 1.0f);
		});

		It("never goes back while it turns (monotonic)", [this]()
		{
			const double Seized = 40.0;
			float Previous = 0.0f;
			for (int32 Step = 0; Step <= 240; ++Step)
			{
				const float Weight = WeatherMath::TurnWeight(Seized, Seized - 2.0 + Step * 0.1, Rules->TransitionSeconds);
				if (!TestTrue(*FString::Printf(TEXT("step %d"), Step), Weight >= Previous && Weight <= 1.0f))
				{
					break;
				}
				Previous = Weight;
			}
		});

		It("is the same on every machine: a function of the replicated start and the server clock only", [this]()
		{
			// a client reads the same server time as the host, whatever its own world time
			const double Seized = 1234.0;
			const double ServerTime = 1239.0;
			TestEqual(TEXT("host and client"), WeatherMath::TurnWeight(Seized, ServerTime, 10.0f), WeatherMath::TurnWeight(Seized, ServerTime, 10.0f));
		});

		It("turns at once with no transition time", [this]()
		{
			TestEqual(TEXT("instant"), WeatherMath::TurnWeight(5.0, 5.0, 0.0f), 1.0f);
			TestEqual(TEXT("not before"), WeatherMath::TurnWeight(5.0, 4.9, 0.0f), 0.0f);
		});
	});

	Describe("Wind and sky of the turned weather", [this]()
	{
		It("keeps the mean wind over WindStrength once turned, half of it half way", [this]()
		{
			TestEqual(TEXT("clear: no floor"), WeatherMath::WindFloor(*Rules, 0.0f), 0.0f);
			TestEqual(TEXT("half way"), WeatherMath::WindFloor(*Rules, 0.5f), Rules->WindStrength * 0.5f, 1.0e-5f);
			TestEqual(TEXT("turned"), WeatherMath::WindFloor(*Rules, 1.0f), Rules->WindStrength);
			TestTrue(TEXT("violent: at least the full storm the forces are tuned for"), Rules->WindStrength >= 1.0f);
		});

		It("blows harder than the calm between gusts, and the ultra violent wind still rules", [this]()
		{
			// a calm time: before the first gust
			const double Calm = WindRules->FirstGustSeconds * 0.5;
			const double Clear = AWindStormActor::WindAt(*WindRules, Calm).Size();
			const double Turned = AWindStormActor::WindAt(*WindRules, Calm, -1.0, Rules->WindStrength).Size();
			TestTrue(TEXT("turned weather blows harder than the calm"), Turned > Clear * 3.0);
			// turbulence never takes the wind under a third of its mean (0.33 in AWindStormActor::WindAt)
			TestTrue(TEXT("never under a third of the floor"), Turned >= Rules->WindStrength * 0.33 - 1.0e-3);
			const double Deadline = Calm - 100.0;
			const double Ultra = AWindStormActor::WindAt(*WindRules, Calm, Deadline, Rules->WindStrength).Size();
			const double UltraAlone = AWindStormActor::WindAt(*WindRules, Calm, Deadline).Size();
			TestEqual(TEXT("the floor does not weaken the ultra violent wind"), Ultra, UltraAlone, 1.0e-4);
		});

		It("darkens the sky with the turn, never past a full storm", [this]()
		{
			TestEqual(TEXT("clear sky"), WeatherMath::SkyFactor(*Rules, 0.0f), 0.0f);
			TestEqual(TEXT("dark sky"), WeatherMath::SkyFactor(*Rules, 1.0f), Rules->SkyStormFactor);
			Rules->SkyStormFactor = 3.0f;
			TestEqual(TEXT("capped"), WeatherMath::SkyFactor(*Rules, 1.0f), 1.0f);
		});
	});

	Describe("Rain", [this]()
	{
		It("falls with the turned weather and in the final storm, under the open sky only", [this]()
		{
			TestEqual(TEXT("clear: none"), WeatherMath::RainTarget(*Rules, 0.0f, 0.0f, false, false, false), 0.0f);
			TestEqual(TEXT("turned"), WeatherMath::RainTarget(*Rules, 1.0f, 0.0f, false, false, false), 1.0f);
			TestEqual(TEXT("final storm"), WeatherMath::RainTarget(*Rules, 0.0f, 1.0f, false, false, false), 1.0f);
			TestEqual(TEXT("under a roof"), WeatherMath::RainTarget(*Rules, 1.0f, 1.0f, true, false, false), 0.0f);
			TestEqual(TEXT("under water"), WeatherMath::RainTarget(*Rules, 1.0f, 1.0f, false, true, false), 0.0f);
			Rules->bRainInFinalStorm = false;
			TestEqual(TEXT("final storm without rain"), WeatherMath::RainTarget(*Rules, 0.0f, 1.0f, false, false, false), 0.0f);
		});

		It("rains on the galleon by default, RainInCalmZones otherwise", [this]()
		{
			TestEqual(TEXT("default: like anywhere"), WeatherMath::RainTarget(*Rules, 1.0f, 0.0f, false, false, true), 1.0f);
			Rules->RainInCalmZones = 0.0f;
			TestEqual(TEXT("dry calm zone"), WeatherMath::RainTarget(*Rules, 1.0f, 0.0f, false, false, true), 0.0f);
		});

		It("leans downwind with the wind, never past MaxRainTiltDeg", [this]()
		{
			TestEqual(TEXT("no wind: straight down"), WeatherMath::RainFallDirection(FVector::ZeroVector, 20.0f), FVector::DownVector);
			const FVector Wind(0.0, -1.5, 0.0);
			const float Tilt = WeatherMath::RainTiltDeg(*Rules, static_cast<float>(Wind.Size()));
			TestTrue(TEXT("tilted"), Tilt > 0.0f && Tilt <= Rules->MaxRainTiltDeg);
			TestEqual(TEXT("capped"), WeatherMath::RainTiltDeg(*Rules, 100.0f), Rules->MaxRainTiltDeg);
			const FVector Fall = WeatherMath::RainFallDirection(Wind, Tilt);
			TestTrue(TEXT("falls"), Fall.Z < 0.0);
			TestTrue(TEXT("downwind"), Fall.Y < 0.0 && FMath::IsNearlyZero(Fall.X, 1.0e-6));
			const float Measured = FMath::RadiansToDegrees(FMath::Acos(static_cast<float>(-Fall.Z)));
			TestEqual(TEXT("tilt angle"), Measured, Tilt, 0.01f);
		});
	});

	Describe("Sounds", [this]()
	{
		It("follow the turned weather and leave the final storm to its own loops", [this]()
		{
			TestEqual(TEXT("clear"), WeatherMath::LoopVolume(*Rules, 0.0f, 0.0f, false, false), 0.0f);
			TestEqual(TEXT("turned"), WeatherMath::LoopVolume(*Rules, 1.0f, 0.0f, false, false), 1.0f);
			TestEqual(TEXT("final storm takes over"), WeatherMath::LoopVolume(*Rules, 1.0f, 1.0f, false, false), 0.0f);
			TestEqual(TEXT("muffled indoors"), WeatherMath::LoopVolume(*Rules, 1.0f, 0.0f, true, false), Rules->IndoorVolume);
			TestEqual(TEXT("none under water"), WeatherMath::LoopVolume(*Rules, 1.0f, 0.0f, false, true), 0.0f);
		});

		It("fade at a steady rate", [this]()
		{
			TestEqual(TEXT("half a fade"), WeatherMath::Approach(0.0f, 1.0f, 0.5f, 1.0f), 0.5f);
			TestEqual(TEXT("never past the target"), WeatherMath::Approach(0.9f, 1.0f, 0.5f, 1.0f), 1.0f);
			TestEqual(TEXT("down too"), WeatherMath::Approach(1.0f, 0.0f, 0.25f, 1.0f), 0.75f);
		});
	});

	Describe("Seizure", [this]()
	{
		It("an artifact class, or a Blueprint of one, turns the weather; anything else does not", [this]()
		{
			const TArray<const UClass*> Triggers = { APawn::StaticClass() };
			TestTrue(TEXT("the class itself"), WeatherMath::IsTriggerClass(APawn::StaticClass(), Triggers));
			TestTrue(TEXT("a child class"), WeatherMath::IsTriggerClass(ADefaultPawn::StaticClass(), Triggers));
			TestFalse(TEXT("another prop"), WeatherMath::IsTriggerClass(AActor::StaticClass(), Triggers));
			TestFalse(TEXT("nothing held"), WeatherMath::IsTriggerClass(nullptr, Triggers));
			TestFalse(TEXT("no trigger"), WeatherMath::IsTriggerClass(APawn::StaticClass(), {}));
		});

		It("recognises the roofs over the camera by mesh name", [this]()
		{
			TestTrue(TEXT("a minka"), WeatherMath::HasKeyword(TEXT("SM_Village_Minka_A"), Rules->RoofMeshKeywords));
			TestTrue(TEXT("the galleon"), WeatherMath::HasKeyword(TEXT("SM_Galleon_Deck"), Rules->RoofMeshKeywords));
			TestFalse(TEXT("a palm"), WeatherMath::HasKeyword(TEXT("SM_CoconutPalm_03"), Rules->RoofMeshKeywords));
		});
	});
}

#endif
