// monke_island

#include "Misc/AutomationTest.h"
#include "Flood/FloodGrid.h"
#include "Flood/FloodRules.h"
#include "Flood/FloodSimulationActor.h"
#include "Storm/WindRules.h"
#include "Storm/WindStormActor.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Flood
BEGIN_DEFINE_SPEC(FFloodGridSpec, "monke_island.Flood.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FFloodGrid Grid;
	/** Only the sea side (X < SeaCells) feeds the water: frees the other border cells */
	void KeepSeaSide(int32 SeaCells)
	{
		for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
		{
			for (int32 X = SeaCells; X < Grid.GetSizeX(); ++X)
			{
				Grid.Fixed[Grid.Index(X, Y)] = 0;
				Grid.Depth[Grid.Index(X, Y)] = 0.0f;
			}
		}
	}
	void Run(float Seconds, float SeaLevel)
	{
		for (float T = 0.0f; T < Seconds; T += 0.05f)
		{
			Grid.Step(0.05f, SeaLevel);
		}
	}
END_DEFINE_SPEC(FFloodGridSpec)

void FFloodGridSpec::Define()
{
	Describe("Flow", [this]()
	{
		It("keeps the volume of a closed basin", [this]()
		{
			Grid.Init(16, 16, 4.0f);
			for (int32 I = 0; I < 16 * 16; ++I)
			{
				Grid.Ground[I] = 1.0f;
			}
			Grid.Depth[Grid.Index(8, 8)] = 10.0f;
			const double Before = Grid.FreeVolume();
			Run(20.0f, -100.0f);
			TestEqual(TEXT("volume"), Grid.FreeVolume(), Before, Before * 0.001);
			TestTrue(TEXT("spread out"), Grid.Depth[Grid.Index(2, 2)] > 0.01f);
		});

		It("runs downhill and pools in the hollow", [this]()
		{
			Grid.Init(32, 1, 4.0f);
			for (int32 X = 0; X < 32; ++X)
			{
				Grid.Ground[X] = static_cast<float>(32 - X) * 0.2f;
			}
			Grid.Depth[2] = 2.0f;
			Run(60.0f, -100.0f);
			TestTrue(TEXT("water at the bottom"), Grid.Depth[31] > Grid.Depth[2]);
		});
	});

	Describe("Topography", [this]()
	{
		It("holds a hollow behind a ridge until the sea overflows it", [this]()
		{
			// sea | ridge 2 m | hollow 0.5 m
			Grid.Init(30, 3, 4.0f);
			for (int32 Y = 0; Y < 3; ++Y)
			{
				for (int32 X = 0; X < 30; ++X)
				{
					Grid.Ground[Grid.Index(X, Y)] = X < 8 ? -5.0f : (X < 12 ? 2.0f : 0.5f);
				}
			}
			Grid.SetupSea(0.0f, 1.0f);
			KeepSeaSide(8);
			Run(30.0f, 1.5f);
			TestTrue(TEXT("hollow dry while the sea stays under the ridge"), Grid.Depth[Grid.Index(20, 1)] < 0.01f);
			Run(60.0f, 3.0f);
			TestTrue(TEXT("hollow flooded once the ridge is overtopped"), Grid.Depth[Grid.Index(20, 1)] > 1.5f);
		});

		It("floods low ground before high ground", [this]()
		{
			Grid.Init(40, 3, 4.0f);
			for (int32 Y = 0; Y < 3; ++Y)
			{
				for (int32 X = 0; X < 40; ++X)
				{
					Grid.Ground[Grid.Index(X, Y)] = X < 5 ? -5.0f : (X < 20 ? 0.5f : 3.0f);
				}
			}
			Grid.SetupSea(0.0f, 1.0f);
			KeepSeaSide(5);
			Run(40.0f, 2.0f);
			TestTrue(TEXT("low plain flooded"), Grid.Depth[Grid.Index(12, 1)] > 1.0f);
			TestTrue(TEXT("high ground dry"), Grid.Depth[Grid.Index(30, 1)] < 0.01f);
		});
	});

	Describe("Timeline", [this]()
	{
		It("rises the sea from 0 to RiseCm over RiseSeconds", [this]()
		{
			UFloodRules* Rules = NewObject<UFloodRules>();
			TestEqual(TEXT("start"), AFloodSimulationActor::RiseAt(*Rules, 0.0f), 0.0f);
			TestEqual(TEXT("end"), AFloodSimulationActor::RiseAt(*Rules, Rules->RiseSeconds), Rules->RiseCm, 0.1f);
			TestEqual(TEXT("starts at 4:50"), Rules->StartSeconds, 290.0f);
		});

		It("keeps the wind calm before 1:20 and blowing after", [this]()
		{
			UWindRules* Rules = NewObject<UWindRules>();
			TestTrue(TEXT("calm at 79 s"), AWindStormActor::WindAt(*Rules, 79.0).IsNearlyZero());
			for (double T = Rules->StartSeconds + Rules->RampSeconds; T < 400.0; T += 3.7)
			{
				if (!TestTrue(TEXT("blowing"), AWindStormActor::WindAt(*Rules, T).Size() > 0.3))
				{
					break;
				}
			}
		});
	});
}

#endif
