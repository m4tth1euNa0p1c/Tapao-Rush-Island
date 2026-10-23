// Tidemark plugin

#include "Tidemark/Solver/TidemarkHash.h"

namespace Tidemark::Hash
{
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
			return Fnv1a64(Bytes, sizeof(IntType), Hash);
		}
	}

	uint64 Fnv1a64(const void* Data, SIZE_T Size, uint64 Basis)
	{
		uint64 Hash = Basis;
		const uint8* Bytes = static_cast<const uint8*>(Data);
		for (SIZE_T Index = 0; Index < Size; ++Index)
		{
			Hash ^= static_cast<uint64>(Bytes[Index]);
			Hash *= Fnv1a64Prime;
		}
		return Hash;
	}

	int32 QuantizeDepth(float DepthCm, float QuantizeCm)
	{
		if (!FMath::IsFinite(DepthCm) || !(QuantizeCm > 0.0f))
		{
			return MIN_int32;
		}
		const float Scaled = DepthCm / QuantizeCm;
		if (Scaled >= static_cast<float>(MAX_int32) || Scaled <= static_cast<float>(MIN_int32))
		{
			return MIN_int32;
		}
		return FMath::RoundToInt32(Scaled);
	}

	FTidemarkStateHash HashDepth(const TArray<float>& Depth, int32 SizeX, int32 SizeY, uint32 StepIndex, int32 BlockSize, float QuantizeCm)
	{
		FTidemarkStateHash Result;
		Result.StepIndex = StepIndex;

		if (SizeX <= 0 || SizeY <= 0 || BlockSize <= 0 || Depth.Num() < SizeX * SizeY)
		{
			Result.Combined = FeedLittleEndian(Fnv1a64Basis, StepIndex);
			return Result;
		}

		Result.BlocksX = FMath::DivideAndRoundUp(SizeX, BlockSize);
		Result.BlocksY = FMath::DivideAndRoundUp(SizeY, BlockSize);
		Result.Blocks.SetNumUninitialized(Result.BlocksX * Result.BlocksY);

		// Sequential by design: a fixed order is what makes the hash comparable between machines.
		for (int32 BlockY = 0; BlockY < Result.BlocksY; ++BlockY)
		{
			const int32 Y0 = BlockY * BlockSize;
			const int32 Y1 = FMath::Min(Y0 + BlockSize, SizeY);
			for (int32 BlockX = 0; BlockX < Result.BlocksX; ++BlockX)
			{
				const int32 X0 = BlockX * BlockSize;
				const int32 X1 = FMath::Min(X0 + BlockSize, SizeX);
				uint64 Hash = Fnv1a64Basis;
				for (int32 Y = Y0; Y < Y1; ++Y)
				{
					const float* Row = Depth.GetData() + Y * SizeX;
					for (int32 X = X0; X < X1; ++X)
					{
						Hash = FeedLittleEndian(Hash, QuantizeDepth(Row[X], QuantizeCm));
					}
				}
				Result.Blocks[BlockY * Result.BlocksX + BlockX] = Hash;
			}
		}

		uint64 Combined = FeedLittleEndian(Fnv1a64Basis, StepIndex);
		for (const uint64 Block : Result.Blocks)
		{
			Combined = FeedLittleEndian(Combined, Block);
		}
		Result.Combined = Combined;
		return Result;
	}

	TArray<FIntPoint> DiffBlocks(const FTidemarkStateHash& A, const FTidemarkStateHash& B)
	{
		TArray<FIntPoint> Diff;
		const bool bSameLayout = A.BlocksX == B.BlocksX && A.BlocksY == B.BlocksY && A.Blocks.Num() == B.Blocks.Num();
		const int32 BlocksX = FMath::Max(A.BlocksX, B.BlocksX);
		const int32 BlocksY = FMath::Max(A.BlocksY, B.BlocksY);
		for (int32 BlockY = 0; BlockY < BlocksY; ++BlockY)
		{
			for (int32 BlockX = 0; BlockX < BlocksX; ++BlockX)
			{
				const int32 Index = BlockY * BlocksX + BlockX;
				if (!bSameLayout || !A.Blocks.IsValidIndex(Index) || !B.Blocks.IsValidIndex(Index) || A.Blocks[Index] != B.Blocks[Index])
				{
					Diff.Add(FIntPoint(BlockX, BlockY));
				}
			}
		}
		return Diff;
	}
}
