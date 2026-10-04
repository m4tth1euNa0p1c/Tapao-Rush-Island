// monke_island

#include "Misc/AutomationTest.h"
#include "Flood/FloodOutsideSea.h"
#include "Flood/FloodSimulationActor.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Flood.Outside
// T-141 (REC finding 3 and 6), lot L3, cahier §6.1, recette n°16. Outside the flood grid the sea of the fragment takes the
// same rise as the open sea of the grid (no step at the border), then fades to none: the spawn and the rest of the map,
// more than 10 km away, never flood (owner, 2026-10-04). One flood actor per world, the same one on every machine.
BEGIN_DEFINE_SPEC(FFloodOutsideSeaSpec, "monke_island.Flood.Outside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	/** Sea level of the map (cm), not 0 on purpose */
	const double Base = -37.5;
	/** Rise of the expedition (cm): end of the submersion */
	const double Rise = 650.0;
	/** Grid of 320 cells of 8 m (DA_FloodRules defaults): 0 to 2.56 km */
	const FBox2D GridBox = FBox2D(FVector2D(0.0, 0.0), FVector2D(256000.0, 256000.0));
	/** Land of the fragment in the middle of the grid */
	const FBox2D LandBox = FBox2D(FVector2D(100000.0, 100000.0), FVector2D(150000.0, 150000.0));
	FFloodOutsideSea Sea;

	/** Surface at a point outside the grid, NaN when the sea is left to the map */
	double Outside(double X, double Y) const
	{
		double Surface = 0.0;
		return Sea.Surface(FVector2D(X, Y), false, false, 0.0, 0.0, Base, Rise, Surface) ? Surface : TNumericLimits<double>::Max();
	}
END_DEFINE_SPEC(FFloodOutsideSeaSpec)

void FFloodOutsideSeaSpec::Define()
{
	BeforeEach([this]()
	{
		// margin 1.5 km past the land, fade 500 m, band of 48 m inside the grid border (DA_FloodRules defaults)
		Sea = FFloodOutsideSea::Make(GridBox, LandBox, 150000.0, 50000.0, 4800.0);
	});

	Describe("Border", [this]()
	{
		It("gives the same surface on both sides of the border of the grid", [this]()
		{
			// open sea on the border of the grid: held at the sea level of the expedition
			double Inside = 0.0;
			TestTrue(TEXT("inside answers"), Sea.Surface(FVector2D(1.0, 128000.0), true, true, Base + Rise, -2000.0, Base, Rise, Inside));
			TestEqual(TEXT("no step at the border"), Outside(-1.0, 128000.0), Inside, 0.5);
			TestEqual(TEXT("outside on the rise"), Outside(-1.0, 128000.0), Base + Rise, 0.01);
		});

		It("blends a grid surface that differs into the outside sea, without a jump", [this]()
		{
			// the grid lags 50 cm behind (sloshing): walk across the border by 50 cm steps
			double Previous = 0.0;
			double MaxJump = 0.0;
			bool bFirst = true;
			for (double X = -2000.0; X <= 8000.0; X += 50.0)
			{
				double Surface = 0.0;
				const bool bInGrid = X >= 0.0;
				if (!TestTrue(TEXT("water everywhere along the walk"), Sea.Surface(FVector2D(X, 128000.0), bInGrid, true, Base + Rise - 50.0, -2000.0, Base, Rise, Surface)))
				{
					return;
				}
				if (!bFirst)
				{
					MaxJump = FMath::Max(MaxJump, FMath::Abs(Surface - Previous));
				}
				Previous = Surface;
				bFirst = false;
			}
			TestTrue(*FString::Printf(TEXT("largest jump %.2f cm under 2 cm"), MaxJump), MaxJump < 2.0);
			double Deep = 0.0;
			Sea.Surface(FVector2D(10000.0, 128000.0), true, true, Base + Rise - 50.0, -2000.0, Base, Rise, Deep);
			TestEqual(TEXT("past the band the grid speaks alone"), Deep, Base + Rise - 50.0, 0.01);
		});

		It("keeps dry land away from the border dry, and the wet cells of the grid as they are", [this]()
		{
			double Surface = 0.0;
			TestFalse(TEXT("dry cell in the middle"), Sea.Surface(FVector2D(128000.0, 128000.0), true, false, 0.0, 300.0, Base, Rise, Surface));
			TestTrue(TEXT("wet cell in the middle"), Sea.Surface(FVector2D(128000.0, 128000.0), true, true, 420.0, 300.0, Base, Rise, Surface));
			TestEqual(TEXT("its own surface"), Surface, 420.0, 0.01);
		});
	});

	Describe("Fade", [this]()
	{
		It("keeps the full rise over the land widened by the margin, then fades smoothly to none", [this]()
		{
			// the full rise reaches 1.5 km past the land: X = 100000 - 150000 = -50000
			TestEqual(TEXT("full rise at the margin"), Outside(-50000.0, 125000.0), Base + Rise, 0.01);
			double Previous = Base + Rise;
			double MaxJump = 0.0;
			for (double X = -50000.0; X >= -100000.0; X -= 500.0)
			{
				const double Surface = Sea.SurfaceCm(FVector2D(X, 125000.0), Base, Rise);
				TestTrue(TEXT("never rises going away"), Surface <= Previous + 0.001);
				MaxJump = FMath::Max(MaxJump, Previous - Surface);
				Previous = Surface;
			}
			// 650 cm over 500 m, smoothstep: the steepest step of 5 m is about 1.5 x 6.5 cm
			TestTrue(*FString::Printf(TEXT("largest drop per 5 m %.2f cm under 12 cm"), MaxJump), MaxJump < 12.0);
			TestEqual(TEXT("back to the sea of the map at the end of the fade"), Previous, Base, 0.01);
		});

		It("covers the anchorage of the galleon, 330 to 410 m off the beach", [this]()
		{
			// a beach on the edge of the land box, the galleon 410 m offshore, still inside the grid or the margin
			TestEqual(TEXT("galleon 410 m off"), Outside(100000.0 - 41000.0 - 60000.0, 125000.0), Base + Rise, 0.01);
		});

		It("reaches the margin plus the fade past the grid, and nothing beyond", [this]()
		{
			TestEqual(TEXT("reach"), Sea.GetReachCm(), 100000.0, 0.01);
			TestEqual(TEXT("weight past the reach"), Sea.RiseWeight(FVector2D(-100001.0, 125000.0)), 0.0);
			TestEqual(TEXT("no answer past the reach"), Outside(-100001.0, 125000.0), TNumericLimits<double>::Max());
			// the spawn of Lvl_Raraka and the rest of the map: more than 10 km away
			TestEqual(TEXT("10 km away"), Outside(-1000000.0, 125000.0), TNumericLimits<double>::Max());
			TestEqual(TEXT("10 km away diagonally"), Outside(1200000.0, 1200000.0), TNumericLimits<double>::Max());
		});

		It("answers nothing outside the grid before the rise", [this]()
		{
			double Surface = 0.0;
			TestFalse(TEXT("no rise"), Sea.Surface(FVector2D(-1000.0, 125000.0), false, false, 0.0, 0.0, Base, 0.0, Surface));
		});

		It("without land read, takes the full rise over the grid only", [this]()
		{
			const FFloodOutsideSea GridOnly = FFloodOutsideSea::Make(GridBox, FBox2D(ForceInit), 150000.0, 50000.0, 4800.0);
			TestEqual(TEXT("full on the border"), GridOnly.RiseWeight(FVector2D(0.0, 1000.0)), 1.0);
			TestEqual(TEXT("none past the fade"), GridOnly.RiseWeight(FVector2D(-50001.0, 1000.0)), 0.0);
		});

		It("without outside sea (day / night, or off) only the wet cells of the grid answer", [this]()
		{
			const FFloodOutsideSea None;
			double Surface = 0.0;
			TestFalse(TEXT("outside"), None.Surface(FVector2D(-1.0, 0.0), false, false, 0.0, 0.0, Base, Rise, Surface));
			TestTrue(TEXT("wet cell"), None.Surface(FVector2D(1.0, 1.0), true, true, 200.0, 0.0, Base, Rise, Surface));
			TestEqual(TEXT("its surface"), Surface, 200.0);
			TestFalse(TEXT("dry cell"), None.Surface(FVector2D(1.0, 1.0), true, false, 0.0, 0.0, Base, Rise, Surface));
		});
	});

	Describe("Duplicates", [this]()
	{
		It("keeps the actor of the persistent level, then the smallest name, then the smallest level", [this]()
		{
			const FFloodActorRank Map{true, TEXT("FloodSimulationActor_1"), TEXT("/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01")};
			const FFloodActorRank Core{false, TEXT("FloodSimulationActor_0"), TEXT("/Game/MonkeIsland/Maps/LI_ExpeditionCore")};
			const FFloodActorRank CoreB{false, TEXT("FloodSimulationActor_0"), TEXT("/Game/MonkeIsland/Maps/LI_Other")};
			const FFloodActorRank CoreC{false, TEXT("FloodSimulationActor_2"), TEXT("/Game/MonkeIsland/Maps/LI_A")};
			TestTrue(TEXT("persistent level first"), Map.IsBefore(Core));
			TestFalse(TEXT("persistent level first (other way)"), Core.IsBefore(Map));
			TestTrue(TEXT("then the name"), Core.IsBefore(CoreC));
			TestTrue(TEXT("then the level"), Core.IsBefore(CoreB));
			TestFalse(TEXT("never before itself"), Core.IsBefore(Core));
		});
	});
}

#endif
