// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Water/WaveRules.h"

/**
 *  Gerstner waves on the CPU (GPU Gems 1, ch. 1): the very formula of M_Water_Ocean, so what floats sits on what is
 *  drawn. A Gerstner sea moves the water sideways too, so the height above a given (X, Y) needs the point of the
 *  undisturbed surface that ends up there: found by a few fixed point steps.
 */
namespace WaveMath
{
	/** Precomputed train: direction, wave number, angular speed, amplitude, horizontal factor */
	struct FTrain
	{
		FVector2D Direction = FVector2D(1.0, 0.0);
		float K = 0.0f;
		float Omega = 0.0f;
		float Amplitude = 0.0f;
		float Horizontal = 0.0f;
		float Phase = 0.0f;
	};

	/** Trains of the rules (at most MaxWaves, like the material); horizontal motion kept below the loop limit */
	inline TArray<FTrain> MakeTrains(const TArray<FIslandGerstnerWave>& Waves, float Intensity, float Gravity, int32 MaxWaves)
	{
		TArray<FTrain> Trains;
		const int32 Count = FMath::Min(Waves.Num(), MaxWaves);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FIslandGerstnerWave& Wave = Waves[Index];
			FTrain Train;
			const float Radians = FMath::DegreesToRadians(Wave.DirectionDegrees);
			Train.Direction = FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
			Train.K = UE_TWO_PI / FMath::Max(Wave.Wavelength, 10.0f);
			Train.Omega = FMath::Sqrt(FMath::Max(Gravity, 1.0f) * Train.K);
			Train.Amplitude = FMath::Max(Wave.Amplitude, 0.0f) * FMath::Max(Intensity, 0.0f);
			// Q A = steepness / (k N): with every steepness at 1 the crests just close, never loop
			Train.Horizontal = Train.Amplitude > 0.0f ? FMath::Clamp(Wave.Steepness, 0.0f, 1.0f) / (Train.K * FMath::Max(Count, 1)) : 0.0f;
			Train.Phase = Wave.Phase;
			Trains.Add(Train);
		}
		return Trains;
	}

	/** Displacement of the undisturbed surface point (X, Y) at Time: horizontal shift and height (cm) */
	inline FVector Displacement(const TArray<FTrain>& Trains, const FVector2D& Point, float Time)
	{
		FVector Out = FVector::ZeroVector;
		for (const FTrain& Train : Trains)
		{
			const float Angle = Train.K * FVector2D::DotProduct(Train.Direction, Point) - Train.Omega * Time + Train.Phase;
			float Sin;
			float Cos;
			FMath::SinCos(&Sin, &Cos, Angle);
			Out.X += Train.Horizontal * Train.Direction.X * Cos;
			Out.Y += Train.Horizontal * Train.Direction.Y * Cos;
			Out.Z += Train.Amplitude * Sin;
		}
		return Out;
	}

	/** Height of the waves above the mean level at the world point (X, Y) (cm) */
	inline float HeightAt(const TArray<FTrain>& Trains, const FVector2D& Point, float Time, int32 Iterations = 4)
	{
		if (Trains.Num() == 0)
		{
			return 0.0f;
		}
		// find the undisturbed point that the horizontal motion brings onto Point
		FVector2D Source = Point;
		for (int32 Step = 0; Step < Iterations; ++Step)
		{
			const FVector Shift = Displacement(Trains, Source, Time);
			Source = Point - FVector2D(Shift.X, Shift.Y);
		}
		return Displacement(Trains, Source, Time).Z;
	}

	/** Sum of the amplitudes: the highest a crest can reach (cm) */
	inline float MaxHeight(const TArray<FTrain>& Trains)
	{
		float Sum = 0.0f;
		for (const FTrain& Train : Trains)
		{
			Sum += Train.Amplitude;
		}
		return Sum;
	}

	/** Growth ratio of the surface cells: InnerSpacing * (Ratio^(Resolution/2) - 1) / (Ratio - 1) = HalfSize */
	inline double SurfaceRatio(int32 Resolution, float InnerSpacing, float HalfSize)
	{
		const int32 Half = FMath::Max(Resolution / 2, 1);
		const double Target = FMath::Max(static_cast<double>(HalfSize), static_cast<double>(InnerSpacing) * Half);
		double Low = 1.0 + 1e-9;
		double High = 2.0;
		for (int32 Iteration = 0; Iteration < 60; ++Iteration)
		{
			const double Mid = 0.5 * (Low + High);
			const double Reach = InnerSpacing * (FMath::Pow(Mid, static_cast<double>(Half)) - 1.0) / (Mid - 1.0);
			(Reach < Target ? Low : High) = Mid;
		}
		return 0.5 * (Low + High);
	}

	/** Size of the surface cells at Distance from the camera (cm): InnerSpacing + (Ratio - 1) * Distance */
	inline float CellSizeAt(float Distance, double Ratio, float InnerSpacing)
	{
		return InnerSpacing + static_cast<float>(Ratio - 1.0) * FMath::Abs(Distance);
	}

	/** Weight of a wave drawn on cells of CellSize (cm): 1 under an eighth of its length, 0 from a quarter (no shimmer) */
	inline float DrawWeight(float Wavelength, float CellSize)
	{
		return FMath::Clamp(2.0f - 8.0f * CellSize / FMath::Max(Wavelength, 1.0f), 0.0f, 1.0f);
	}

	/** Distance from the camera of vertex Index of a surface line of Resolution vertices, dense at the centre (cm) */
	inline float SurfaceCoordinate(int32 Index, int32 Resolution, float InnerSpacing, float HalfSize)
	{
		// geometric spacing: InnerSpacing at the centre, growing by Ratio each step to reach HalfSize at the edge
		const int32 Half = FMath::Max(Resolution / 2, 1);
		const int32 Steps = Index - Half;
		const int32 N = FMath::Abs(Steps);
		if (N == 0)
		{
			return 0.0f;
		}
		const double Ratio = SurfaceRatio(Resolution, InnerSpacing, HalfSize);
		const double Distance = InnerSpacing * (FMath::Pow(Ratio, static_cast<double>(N)) - 1.0) / (Ratio - 1.0);
		return static_cast<float>(Steps < 0 ? -Distance : Distance);
	}
}
