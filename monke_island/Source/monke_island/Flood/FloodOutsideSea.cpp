// monke_island

#include "Flood/FloodOutsideSea.h"

FFloodOutsideSea FFloodOutsideSea::Make(const FBox2D& InGridBox, const FBox2D& LandBox, double MarginCm, double InFadeCm, double InEdgeBlendCm)
{
	FFloodOutsideSea Sea;
	Sea.GridBox = InGridBox;
	Sea.FullRiseBox = InGridBox;
	if (LandBox.bIsValid)
	{
		// the grid always takes the full rise, so its border never shows a step
		Sea.FullRiseBox += LandBox.ExpandBy(FMath::Max(0.0, MarginCm));
	}
	Sea.FadeCm = FMath::Max(0.0, InFadeCm);
	Sea.EdgeBlendCm = FMath::Max(0.0, InEdgeBlendCm);
	return Sea;
}

double FFloodOutsideSea::RiseWeight(const FVector2D& Point) const
{
	if (!IsValid())
	{
		return 0.0;
	}
	const double Distance = FMath::Sqrt(FullRiseBox.ComputeSquaredDistanceToPoint(Point));
	if (Distance <= 0.0)
	{
		return 1.0;
	}
	if (FadeCm <= 0.0)
	{
		return 0.0;
	}
	// smooth at both ends: no kink where the fade starts nor where it reaches the sea of the map
	return 1.0 - FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Distance / FadeCm, 0.0, 1.0));
}

double FFloodOutsideSea::GetReachCm() const
{
	if (!IsValid())
	{
		return 0.0;
	}
	const FVector2D Low = GridBox.Min - FullRiseBox.Min;
	const FVector2D High = FullRiseBox.Max - GridBox.Max;
	return FMath::Max(FMath::Max(Low.X, Low.Y), FMath::Max(High.X, High.Y)) + FadeCm;
}

double FFloodOutsideSea::EdgeBlend(const FVector2D& Point) const
{
	if (!GridBox.bIsValid)
	{
		return 0.0;
	}
	if (!GridBox.IsInside(Point))
	{
		return 1.0;
	}
	if (EdgeBlendCm <= 0.0)
	{
		return 0.0;
	}
	const double ToBorder = FMath::Min(
		FMath::Min(Point.X - GridBox.Min.X, GridBox.Max.X - Point.X),
		FMath::Min(Point.Y - GridBox.Min.Y, GridBox.Max.Y - Point.Y));
	return 1.0 - FMath::Clamp(ToBorder / EdgeBlendCm, 0.0, 1.0);
}

bool FFloodOutsideSea::Surface(const FVector2D& Point, bool bInGrid, bool bGridWet, double GridSurfaceCm, double GroundCm,
	double BaseSeaLevelCm, double RiseCm, double& OutSurfaceCm) const
{
	if (!IsValid())
	{
		if (bInGrid && bGridWet)
		{
			OutSurfaceCm = GridSurfaceCm;
			return true;
		}
		return false;
	}
	const double Rise = FMath::Max(0.0, RiseCm);
	const double Outside = SurfaceCm(Point, BaseSeaLevelCm, Rise);
	if (!bInGrid)
	{
		// far from the fragment the sea of the map is untouched: nothing to add to it
		if (Rise * RiseWeight(Point) <= UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		OutSurfaceCm = Outside;
		return true;
	}

	const double Blend = EdgeBlend(Point);
	if (Blend <= 0.0)
	{
		if (bGridWet)
		{
			OutSurfaceCm = GridSurfaceCm;
			return true;
		}
		return false;
	}
	if (bGridWet)
	{
		OutSurfaceCm = FMath::Lerp(GridSurfaceCm, Outside, Blend);
		return true;
	}
	if (Outside > GroundCm)
	{
		// a dry cell of the band under the outside sea: from the ground inwards to the outside sea on the border
		OutSurfaceCm = FMath::Lerp(GroundCm, Outside, Blend);
		return true;
	}
	return false;
}
