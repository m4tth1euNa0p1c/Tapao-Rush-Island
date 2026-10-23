// Tidemark plugin

#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Solver/TidemarkHash.h"
#include "Tidemark/Solver/TidemarkSolver.h"

namespace
{
	/** Feeds an integer as little-endian bytes whatever the host endianness. */
	template <typename IntType>
	uint64 FeedLittleEndian(uint64 Hash, IntType Value)
	{
		uint8 Bytes[sizeof(IntType)];
		for (SIZE_T Index = 0; Index < sizeof(IntType); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(static_cast<uint64>(Value) >> (8 * Index));
		}
		return Tidemark::Hash::Fnv1a64(Bytes, sizeof(IntType), Hash);
	}

	/** Feeds the IEEE-754 bits of a double as 8 little-endian bytes. */
	uint64 FeedDouble(uint64 Hash, double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return FeedLittleEndian(Hash, Bits);
	}

	/** Feeds the IEEE-754 bits of a float as 4 little-endian bytes. */
	uint64 FeedFloat(uint64 Hash, float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return FeedLittleEndian(Hash, Bits);
	}
}

void FTidemarkGrid::Init(int32 InSizeX, int32 InSizeY, float InCellSizeCm, const FVector2D& InOriginWorld)
{
	OriginWorld = InOriginWorld;
	CellSizeCm = FMath::Max(InCellSizeCm, 1.0f);
	SizeX = FMath::Max(InSizeX, 0);
	SizeY = FMath::Max(InSizeY, 0);
	const int32 Count = SizeX * SizeY;
	ElevationCm.Init(0, Count);
	Flags.Init(0, Count);
}

void FTidemarkGrid::Reset()
{
	SizeX = 0;
	SizeY = 0;
	ElevationCm.Reset();
	Flags.Reset();
}

bool FTidemarkGrid::IsValid() const
{
	if (SizeX <= 0 || SizeY <= 0 || !(CellSizeCm > 0.0f))
	{
		return false;
	}
	const int64 Count = static_cast<int64>(SizeX) * static_cast<int64>(SizeY);
	return Count <= MaxCells && ElevationCm.Num() == Count && Flags.Num() == Count;
}

int16 FTidemarkGrid::GetElevationCm(int32 X, int32 Y) const
{
	if (!IsValid())
	{
		return 0;
	}
	return ElevationCm[Index(FMath::Clamp(X, 0, SizeX - 1), FMath::Clamp(Y, 0, SizeY - 1))];
}

ETidemarkCellFlag FTidemarkGrid::GetFlags(int32 X, int32 Y) const
{
	if (!IsValid())
	{
		return ETidemarkCellFlag::None;
	}
	return static_cast<ETidemarkCellFlag>(Flags[Index(FMath::Clamp(X, 0, SizeX - 1), FMath::Clamp(Y, 0, SizeY - 1))]);
}

bool FTidemarkGrid::HasFlag(int32 X, int32 Y, ETidemarkCellFlag Flag) const
{
	return EnumHasAllFlags(GetFlags(X, Y), Flag);
}

void FTidemarkGrid::SetFlag(int32 X, int32 Y, ETidemarkCellFlag Flag, bool bEnabled)
{
	if (!IsValid() || !IsInside(X, Y))
	{
		return;
	}
	uint8& Cell = Flags[Index(X, Y)];
	if (bEnabled)
	{
		Cell |= static_cast<uint8>(Flag);
	}
	else
	{
		Cell &= static_cast<uint8>(~static_cast<uint8>(Flag));
	}
}

float FTidemarkGrid::SampleElevationCm(const FVector2D& World) const
{
	if (!IsValid())
	{
		return 0.0f;
	}

	// Continuous cell coordinates, clamped to the segment between the first and the last cell centre.
	const FVector2D Cell = WorldToCellContinuous(World);
	const double CX = FMath::Clamp(Cell.X, 0.0, static_cast<double>(SizeX - 1));
	const double CY = FMath::Clamp(Cell.Y, 0.0, static_cast<double>(SizeY - 1));

	const int32 X0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(CX)), 0, SizeX - 1);
	const int32 Y0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(CY)), 0, SizeY - 1);
	const int32 X1 = FMath::Min(X0 + 1, SizeX - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, SizeY - 1);
	const float TX = static_cast<float>(CX - static_cast<double>(X0));
	const float TY = static_cast<float>(CY - static_cast<double>(Y0));

	const float E00 = static_cast<float>(ElevationCm[Index(X0, Y0)]);
	const float E10 = static_cast<float>(ElevationCm[Index(X1, Y0)]);
	const float E01 = static_cast<float>(ElevationCm[Index(X0, Y1)]);
	const float E11 = static_cast<float>(ElevationCm[Index(X1, Y1)]);

	const float Bottom = FMath::Lerp(E00, E10, TX);
	const float Top = FMath::Lerp(E01, E11, TX);
	return FMath::Lerp(Bottom, Top, TY);
}

FIntPoint FTidemarkGrid::WorldToCell(const FVector2D& World) const
{
	const double Step = static_cast<double>(FMath::Max(CellSizeCm, 1.0f));
	return FIntPoint(
		static_cast<int32>(FMath::FloorToDouble((World.X - OriginWorld.X) / Step)),
		static_cast<int32>(FMath::FloorToDouble((World.Y - OriginWorld.Y) / Step)));
}

FVector2D FTidemarkGrid::WorldToCellContinuous(const FVector2D& World) const
{
	const double Step = static_cast<double>(FMath::Max(CellSizeCm, 1.0f));
	return FVector2D((World.X - OriginWorld.X) / Step - 0.5, (World.Y - OriginWorld.Y) / Step - 0.5);
}

FVector2D FTidemarkGrid::CellToWorld(int32 X, int32 Y) const
{
	const double Step = static_cast<double>(CellSizeCm);
	return FVector2D(OriginWorld.X + (static_cast<double>(X) + 0.5) * Step, OriginWorld.Y + (static_cast<double>(Y) + 0.5) * Step);
}

uint64 FTidemarkGrid::ComputeSourceHash() const
{
	// Fixed order, explicit little-endian bytes: the same value as Tools/tmk.py source_hash().
	uint64 Hash = Tidemark::Hash::Fnv1a64Basis;
	Hash = FeedDouble(Hash, OriginWorld.X);
	Hash = FeedDouble(Hash, OriginWorld.Y);
	Hash = FeedFloat(Hash, CellSizeCm);
	Hash = FeedLittleEndian(Hash, SizeX);
	Hash = FeedLittleEndian(Hash, SizeY);
	for (const int16 Elevation : ElevationCm)
	{
		Hash = FeedLittleEndian(Hash, static_cast<uint16>(Elevation));
	}
	if (Flags.Num() > 0)
	{
		Hash = Tidemark::Hash::Fnv1a64(Flags.GetData(), Flags.Num(), Hash);
	}
	return Hash;
}

FString FTidemarkGrid::HashToHex(uint64 Hash)
{
	return FString::Printf(TEXT("%016llx"), Hash);
}

void FTidemarkGrid::FillSolver(FTidemarkSolver& Out, float SeaLevelCm, float OpenSeaDepthCm) const
{
	if (!IsValid())
	{
		Out.Init(0, 0, 1.0f);
		return;
	}

	Out.Init(SizeX, SizeY, CellSizeCm / 100.0f);
	const int32 Count = Num();
	for (int32 I = 0; I < Count; ++I)
	{
		Out.Ground[I] = static_cast<float>(ElevationCm[I]) / 100.0f;
	}

	const float SeaLevelM = SeaLevelCm / 100.0f;
	Out.SetupSea(SeaLevelM, OpenSeaDepthCm / 100.0f);

	// SetupSea derives Fixed from the border and the depth only; the baked flags are authoritative on top of it.
	constexpr uint8 FixedMask = static_cast<uint8>(ETidemarkCellFlag::OpenSea) | static_cast<uint8>(ETidemarkCellFlag::Border);
	for (int32 I = 0; I < Count; ++I)
	{
		if ((Flags[I] & FixedMask) != 0)
		{
			Out.Fixed[I] = 1;
			Out.Depth[I] = FMath::Max(0.0f, SeaLevelM - Out.Ground[I]);
		}
	}
}

bool FTidemarkGrid::operator==(const FTidemarkGrid& Other) const
{
	return OriginWorld == Other.OriginWorld
		&& CellSizeCm == Other.CellSizeCm
		&& SizeX == Other.SizeX
		&& SizeY == Other.SizeY
		&& ElevationCm == Other.ElevationCm
		&& Flags == Other.Flags;
}
