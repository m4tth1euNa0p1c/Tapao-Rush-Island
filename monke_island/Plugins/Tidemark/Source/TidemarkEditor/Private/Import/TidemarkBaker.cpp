// Tidemark plugin

#include "TidemarkEditor/Import/TidemarkBaker.h"
#include "TidemarkEditor/TidemarkEditorModule.h"

#include "HAL/PlatformTime.h"
#include "Tidemark/Grid/TidemarkGrid.h"

namespace Tidemark::Bake
{
	namespace
	{
		/** Rounds a height to the nearest int16, saturating. */
		int16 ToInt16Cm(float HeightCm)
		{
			if (!FMath::IsFinite(HeightCm))
			{
				return 0;
			}
			const float Clamped = FMath::Clamp(HeightCm, static_cast<float>(TNumericLimits<int16>::Min()), static_cast<float>(TNumericLimits<int16>::Max()));
			return static_cast<int16>(FMath::RoundToInt(Clamped));
		}

		FTidemarkBakeResult Fail(const FString& Error, double StartSeconds)
		{
			FTidemarkBakeResult Result;
			Result.bOk = false;
			Result.Error = Error;
			Result.Seconds = FPlatformTime::Seconds() - StartSeconds;
			UE_LOG(LogTidemarkEditor, Error, TEXT("Tidemark bake failed: %s"), *Error);
			return Result;
		}
	}

	bool ComputeDimensions(const FBox2D& Bounds, float CellSizeCm, int32& OutSizeX, int32& OutSizeY)
	{
		OutSizeX = 0;
		OutSizeY = 0;
		if (!Bounds.bIsValid || !(CellSizeCm > 0.0f))
		{
			return false;
		}
		const FVector2D Extent = Bounds.GetSize();
		// A small epsilon keeps an extent that is an exact multiple of the cell from gaining a cell by rounding.
		const double Epsilon = 1.0e-6 * CellSizeCm;
		const double CellsX = FMath::CeilToDouble((Extent.X - Epsilon) / CellSizeCm);
		const double CellsY = FMath::CeilToDouble((Extent.Y - Epsilon) / CellSizeCm);
		if (CellsX < 1.0 || CellsY < 1.0 || CellsX > static_cast<double>(FTidemarkGrid::MaxCells) || CellsY > static_cast<double>(FTidemarkGrid::MaxCells))
		{
			return false;
		}
		OutSizeX = static_cast<int32>(CellsX);
		OutSizeY = static_cast<int32>(CellsY);
		return true;
	}

	FTidemarkBakeResult BakeGrid(const ITidemarkHeightSource& Source, const FTidemarkBakeSettings& Settings, FTidemarkGrid& Out)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		Out.Reset();

		if (!(Settings.CellSizeCm > 0.0f))
		{
			return Fail(TEXT("CellSizeCm must be positive"), StartSeconds);
		}
		if (Settings.OpenSeaMarginCm < 0.0f)
		{
			return Fail(TEXT("OpenSeaMarginCm must not be negative"), StartSeconds);
		}

		FBox2D Bounds = Settings.ResolveBounds(Source);
		if (!Bounds.bIsValid)
		{
			return Fail(FString::Printf(TEXT("source '%s' has no bounds"), *Source.GetSourceDescription()), StartSeconds);
		}
		Bounds = Bounds.ExpandBy(static_cast<double>(Settings.OpenSeaMarginCm));

		int32 SizeX = 0;
		int32 SizeY = 0;
		if (!ComputeDimensions(Bounds, Settings.CellSizeCm, SizeX, SizeY))
		{
			return Fail(FString::Printf(TEXT("bounds (%s) give no cell at %.1f cm"), *Bounds.ToString(), Settings.CellSizeCm), StartSeconds);
		}
		const int64 CellCount = static_cast<int64>(SizeX) * static_cast<int64>(SizeY);
		if (CellCount > FTidemarkGrid::MaxCells)
		{
			return Fail(FString::Printf(TEXT("%d x %d cells = %lld, above the %d cell budget (16 MiB): use a larger cell or tighter bounds"),
				SizeX, SizeY, CellCount, FTidemarkGrid::MaxCells), StartSeconds);
		}

		Out.Init(SizeX, SizeY, Settings.CellSizeCm, Bounds.Min);
		if (!Out.IsValid())
		{
			return Fail(TEXT("grid allocation failed"), StartSeconds);
		}

		// 2. Sample every cell centre, row-major. Heights are kept as float until the fill so the neighbour minimum is exact.
		TArray<float> Heights;
		Heights.Init(0.0f, static_cast<int32>(CellCount));
		TBitArray<> SampledMask(false, static_cast<int32>(CellCount));
		FTidemarkBakeResult Result;
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				const FVector2D Centre = Out.CellToWorld(X, Y);
				float HeightCm = 0.0f;
				const int32 Index = Out.Index(X, Y);
				if (Source.SampleHeightCm(Centre.X, Centre.Y, HeightCm) && FMath::IsFinite(HeightCm))
				{
					Heights[Index] = HeightCm;
					SampledMask[Index] = true;
					++Result.Sampled;
				}
				else
				{
					++Result.Missing;
				}
			}
		}

		// 3. Fill the missing cells from the sampled neighbours only (one pass reads SampledMask, never a filled cell:
		//    the result does not depend on the order of the pass).
		const float SeaFloorCm = Settings.SeaLevelCm - Settings.OpenSeaDepthCm;
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				const int32 Index = Out.Index(X, Y);
				float HeightCm = Heights[Index];
				if (!SampledMask[Index])
				{
					bool bFound = false;
					float Lowest = 0.0f;
					for (int32 DY = -1; DY <= 1; ++DY)
					{
						for (int32 DX = -1; DX <= 1; ++DX)
						{
							const int32 NX = X + DX;
							const int32 NY = Y + DY;
							if ((DX == 0 && DY == 0) || !Out.IsInside(NX, NY))
							{
								continue;
							}
							const int32 NIndex = Out.Index(NX, NY);
							if (SampledMask[NIndex] && (!bFound || Heights[NIndex] < Lowest))
							{
								Lowest = Heights[NIndex];
								bFound = true;
							}
						}
					}
					HeightCm = bFound ? Lowest : Settings.MissingElevationCm;
					Out.Flags[Index] |= static_cast<uint8>(ETidemarkCellFlag::Forbidden);
				}

				// 4. and 5.
				const int16 Elevation = ToInt16Cm(HeightCm);
				Out.ElevationCm[Index] = Elevation;
				if (X == 0 || Y == 0 || X == SizeX - 1 || Y == SizeY - 1)
				{
					Out.Flags[Index] |= static_cast<uint8>(ETidemarkCellFlag::Border);
				}
				if (static_cast<float>(Elevation) < SeaFloorCm)
				{
					Out.Flags[Index] |= static_cast<uint8>(ETidemarkCellFlag::OpenSea);
				}
			}
		}

		Result.bOk = true;
		Result.Seconds = FPlatformTime::Seconds() - StartSeconds;
		UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark bake of '%s': %d x %d cells at %.0f cm, %d sampled, %d missing, %.2f MiB, %.2f s"),
			*Source.GetSourceDescription(), SizeX, SizeY, Settings.CellSizeCm, Result.Sampled, Result.Missing,
			static_cast<double>(Out.NumBytes()) / (1024.0 * 1024.0), Result.Seconds);
		return Result;
	}
}
