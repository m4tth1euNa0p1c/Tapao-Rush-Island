// monke_island

#include "Misc/AutomationTest.h"
#include "Ambience/ShoreAmbienceMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Ambience
BEGIN_DEFINE_SPEC(FShoreAmbienceSpec, "monke_island.Ambience.Shore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const TArray<float> Radii = { 400.0f, 1200.0f, 2500.0f };
END_DEFINE_SPEC(FShoreAmbienceSpec)

void FShoreAmbienceSpec::Define()
{
	It("finds the nearest crossing between land and water, half way between two probes", [this]()
	{
		// 4 directions: +X has water from the second ring, the others stay on land
		TArray<bool> Wet;
		Wet.Init(false, 4 * Radii.Num());
		Wet[0 * 3 + 1] = true;
		Wet[0 * 3 + 2] = true;
		Wet[2 * 3 + 2] = true;
		const ShoreAmbienceMath::FShore Shore = ShoreAmbienceMath::FindShore(false, Wet, Radii, 4);
		TestTrue(TEXT("found"), Shore.bFound);
		TestEqual(TEXT("direction"), Shore.Direction, 0);
		TestEqual(TEXT("distance"), Shore.Distance, 800.0f, 0.01f);
		TestEqual(TEXT("water share"), Shore.WaterShare, 3.0f / 13.0f, 0.001f);
	});

	It("hears the shore from the water too (swimming, in the boat)", [this]()
	{
		TArray<bool> Wet;
		Wet.Init(true, 4 * Radii.Num());
		Wet[1 * 3 + 2] = false;
		const ShoreAmbienceMath::FShore Shore = ShoreAmbienceMath::FindShore(true, Wet, Radii, 4);
		TestTrue(TEXT("found"), Shore.bFound);
		TestEqual(TEXT("direction"), Shore.Direction, 1);
		TestEqual(TEXT("distance"), Shore.Distance, 1850.0f, 0.01f);
	});

	It("finds no shore inland nor in open sea", [this]()
	{
		TArray<bool> Dry;
		Dry.Init(false, 4 * Radii.Num());
		const ShoreAmbienceMath::FShore Inland = ShoreAmbienceMath::FindShore(false, Dry, Radii, 4);
		TestFalse(TEXT("inland"), Inland.bFound);
		TestEqual(TEXT("no water inland"), Inland.WaterShare, 0.0f);

		TArray<bool> Wet;
		Wet.Init(true, 4 * Radii.Num());
		const ShoreAmbienceMath::FShore OpenSea = ShoreAmbienceMath::FindShore(true, Wet, Radii, 4);
		TestFalse(TEXT("open sea"), OpenSea.bFound);
		TestEqual(TEXT("all water"), OpenSea.WaterShare, 1.0f);
	});

	It("probes evenly around the listener", [this]()
	{
		TestTrue(TEXT("first along +X"), ShoreAmbienceMath::ProbeDirection(0, 4).Equals(FVector(1.0, 0.0, 0.0), 0.001));
		TestTrue(TEXT("second along +Y"), ShoreAmbienceMath::ProbeDirection(1, 4).Equals(FVector(0.0, 1.0, 0.0), 0.001));
	});
}

#endif
