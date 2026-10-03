// monke_island

#include "Flood/FloodGrid.h"
#include "Async/ParallelFor.h"

void FFloodGrid::Init(int32 InSizeX, int32 InSizeY, float InCellSizeM)
{
	SizeX = FMath::Max(InSizeX, 0);
	SizeY = FMath::Max(InSizeY, 0);
	CellSize = FMath::Max(InCellSizeM, 0.01f);
	const int32 Count = SizeX * SizeY;
	Ground.Init(0.0f, Count);
	Depth.Init(0.0f, Count);
	Fixed.Init(0, Count);
	Flux.Init(FVector4f(0.0f), Count);
}

void FFloodGrid::SetupSea(float SeaLevel, float OpenSeaDepth)
{
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const int32 I = Index(X, Y);
			const bool bBorder = X == 0 || Y == 0 || X == SizeX - 1 || Y == SizeY - 1;
			const bool bOpenSea = Ground[I] < SeaLevel - OpenSeaDepth;
			Fixed[I] = (bBorder || bOpenSea) ? 1 : 0;
			Depth[I] = FMath::Max(0.0f, SeaLevel - Ground[I]);
		}
	}
	Flux.Init(FVector4f(0.0f), SizeX * SizeY);
}

void FFloodGrid::Step(float Dt, float SeaLevel, float Gravity, float FluxDamping)
{
	if (!IsValid() || Dt <= 0.0f)
	{
		return;
	}

	const float Area = CellSize * CellSize;
	// pipe of cross-section CellSize^2 and length CellSize
	const float Accel = Dt * Gravity * CellSize;

	// 1. Fluxes from the surface differences, scaled so a cell never gives more water than it holds
	ParallelFor(SizeY, [&](int32 Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const int32 I = Index(X, Y);
			const float H = Ground[I] + Depth[I];
			FVector4f F = Flux[I] * FluxDamping;
			F.X = X > 0 ? FMath::Max(0.0f, F.X + Accel * (H - Surface(I - 1))) : 0.0f;
			F.Y = X < SizeX - 1 ? FMath::Max(0.0f, F.Y + Accel * (H - Surface(I + 1))) : 0.0f;
			F.Z = Y > 0 ? FMath::Max(0.0f, F.Z + Accel * (H - Surface(I - SizeX))) : 0.0f;
			F.W = Y < SizeY - 1 ? FMath::Max(0.0f, F.W + Accel * (H - Surface(I + SizeX))) : 0.0f;
			const float Out = (F.X + F.Y + F.Z + F.W) * Dt;
			if (Out > UE_SMALL_NUMBER)
			{
				const float K = FMath::Min(1.0f, Depth[I] * Area / Out);
				F *= K;
			}
			Flux[I] = F;
		}
	});

	// 2. Volume change from the inflows and outflows; fixed cells follow the sea
	ParallelFor(SizeY, [&](int32 Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const int32 I = Index(X, Y);
			if (Fixed[I])
			{
				Depth[I] = FMath::Max(0.0f, SeaLevel - Ground[I]);
				continue;
			}
			const FVector4f& F = Flux[I];
			float In = 0.0f;
			In += X > 0 ? Flux[I - 1].Y : 0.0f;
			In += X < SizeX - 1 ? Flux[I + 1].X : 0.0f;
			In += Y > 0 ? Flux[I - SizeX].W : 0.0f;
			In += Y < SizeY - 1 ? Flux[I + SizeX].Z : 0.0f;
			const float Out = F.X + F.Y + F.Z + F.W;
			Depth[I] = FMath::Max(0.0f, Depth[I] + (In - Out) * Dt / Area);
		}
	});
}

double FFloodGrid::FreeVolume() const
{
	double Volume = 0.0;
	for (int32 I = 0; I < Depth.Num(); ++I)
	{
		Volume += Fixed[I] ? 0.0 : Depth[I];
	}
	return Volume * CellSize * CellSize;
}
