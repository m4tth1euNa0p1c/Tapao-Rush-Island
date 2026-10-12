// monke_island

#include "Misc/AutomationTest.h"
#include "Water/WaveMath.h"
#include "Water/WaveRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Water.Waves
BEGIN_DEFINE_SPEC(FWaveSpec, "monke_island.Water.Waves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TArray<WaveMath::FTrain> DefaultTrains() const
	{
		const UWaveRules* Rules = GetDefault<UWaveRules>();
		return WaveMath::MakeTrains(Rules->Waves, Rules->Intensity, Rules->Gravity, UWaveRules::MaxWaves);
	}
END_DEFINE_SPEC(FWaveSpec)

void FWaveSpec::Define()
{
	It("gives the height of the drawn surface above a world point, horizontal motion included", [this]()
	{
		const TArray<WaveMath::FTrain> Trains = DefaultTrains();
		for (int32 Sample = 0; Sample < 50; ++Sample)
		{
			const FVector2D Source(Sample * 173.0, Sample * -91.0);
			const float Time = Sample * 0.37f;
			// the vertex that starts at Source is drawn at Source + shift, at this height
			const FVector Moved = WaveMath::Displacement(Trains, Source, Time);
			const FVector2D Drawn = Source + FVector2D(Moved.X, Moved.Y);
			if (!TestEqual(TEXT("height under the drawn vertex"), WaveMath::HeightAt(Trains, Drawn, Time), static_cast<float>(Moved.Z), 0.5f))
			{
				break;
			}
		}
	});

	It("never goes beyond the sum of the amplitudes, and moves with time", [this]()
	{
		const TArray<WaveMath::FTrain> Trains = DefaultTrains();
		const float Max = WaveMath::MaxHeight(Trains);
		TestTrue(TEXT("a real sea: 20 to 80 cm crests"), Max > 20.0f && Max < 80.0f);
		float Lowest = 0.0f;
		float Highest = 0.0f;
		for (int32 Step = 0; Step < 200; ++Step)
		{
			const float Height = WaveMath::HeightAt(Trains, FVector2D(1234.0, 5678.0), Step * 0.1f);
			Lowest = FMath::Min(Lowest, Height);
			Highest = FMath::Max(Highest, Height);
		}
		TestTrue(TEXT("within the bound"), Highest <= Max + 0.01f && Lowest >= -Max - 0.01f);
		TestTrue(TEXT("the water rises and falls"), Highest - Lowest > 10.0f);
	});

	It("is flat with no intensity", [this]()
	{
		const UWaveRules* Rules = GetDefault<UWaveRules>();
		const TArray<WaveMath::FTrain> Trains = WaveMath::MakeTrains(Rules->Waves, 0.0f, Rules->Gravity, UWaveRules::MaxWaves);
		TestEqual(TEXT("flat"), WaveMath::HeightAt(Trains, FVector2D(300.0, 400.0), 12.0f), 0.0f, 0.001f);
	});

	It("follows deep water dispersion: a 24 m wave takes about 3.9 s", [this]()
	{
		const TArray<WaveMath::FTrain> Trains = WaveMath::MakeTrains({ FIslandGerstnerWave(0.0f, 2400.0f, 10.0f, 0.5f) }, 1.0f, 980.0f, 8);
		TestEqual(TEXT("period"), UE_TWO_PI / Trains[0].Omega, 3.92f, 0.02f);
	});

	It("builds a surface dense at the camera that reaches the horizon", [this]()
	{
		const UWaveRules* Rules = GetDefault<UWaveRules>();
		const int32 Resolution = Rules->SurfaceResolution;
		const int32 Half = Resolution / 2;
		const float Inner = Rules->SurfaceInnerSpacing;
		const float Edge = Rules->SurfaceHalfSize;
		TestEqual(TEXT("centre"), WaveMath::SurfaceCoordinate(Half, Resolution, Inner, Edge), 0.0f, 0.001f);
		TestEqual(TEXT("first step"), WaveMath::SurfaceCoordinate(Half + 1, Resolution, Inner, Edge), Inner, 0.01f);
		TestEqual(TEXT("edge"), WaveMath::SurfaceCoordinate(Resolution, Resolution, Inner, Edge), Edge, Edge * 0.001f);
		TestEqual(TEXT("symmetric"), WaveMath::SurfaceCoordinate(0, Resolution, Inner, Edge), -WaveMath::SurfaceCoordinate(Resolution, Resolution, Inner, Edge), 0.01f);
		// a wave is only drawn where it has at least 4 vertices per wavelength
		const double Ratio = WaveMath::SurfaceRatio(Resolution, Inner, Edge);
		for (int32 Index = Half + 1; Index < Resolution; ++Index)
		{
			const float Distance = WaveMath::SurfaceCoordinate(Index, Resolution, Inner, Edge);
			const float Cell = WaveMath::SurfaceCoordinate(Index + 1, Resolution, Inner, Edge) - Distance;
			TestEqual(TEXT("cell size formula"), WaveMath::CellSizeAt(Distance, Ratio, Inner), Cell, Cell * 0.01f + 0.01f);
			for (const FIslandGerstnerWave& Wave : Rules->Waves)
			{
				if (WaveMath::DrawWeight(Wave.Wavelength, Cell) > 0.0f && !TestTrue(FString::Printf(TEXT("%.0f cm wave sampled at %.0f m"),
					Wave.Wavelength, Distance / 100.0f), Cell < Wave.Wavelength / 4.0f))
				{
					return;
				}
			}
		}
		TestEqual(TEXT("the longest swell is drawn up close"), WaveMath::DrawWeight(2400.0f, Inner), 1.0f);
	});
}

#endif
