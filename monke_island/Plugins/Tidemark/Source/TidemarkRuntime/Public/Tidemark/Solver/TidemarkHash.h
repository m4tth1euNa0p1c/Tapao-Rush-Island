// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "TidemarkHash.generated.h"

/**
 * Hash of a simulation state, computed per 32x32 block so that a divergence between two machines can be located.
 * Blocks are stored row by row (BlockY * BlocksX + BlockX). Combined folds StepIndex then every block hash in order.
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FTidemarkStateHash
{
	GENERATED_BODY()

	/** FNV-1a 64 of StepIndex followed by every block hash, in fixed order. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Hash")
	uint64 Combined = 0;

	/** Simulation step the state was hashed at. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Hash")
	uint32 StepIndex = 0;

	/** One FNV-1a 64 per block, row-major. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Hash")
	TArray<uint64> Blocks;

	/** Number of blocks along X (ceil(SizeX / BlockSize)). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Hash")
	int32 BlocksX = 0;

	/** Number of blocks along Y (ceil(SizeY / BlockSize)). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Hash")
	int32 BlocksY = 0;
};

namespace Tidemark::Hash
{
	/** FNV-1a 64 offset basis. */
	constexpr uint64 Fnv1a64Basis = 0xcbf29ce484222325ULL;

	/** FNV-1a 64 prime. */
	constexpr uint64 Fnv1a64Prime = 0x100000001b3ULL;

	/** Default block edge of the state hash, in cells. */
	constexpr int32 DefaultBlockSize = 32;

	/** Default quantum of the depth before hashing: 0.1 cm = 1 mm. */
	constexpr float DefaultQuantizeCm = 0.1f;

	/** FNV-1a 64 over raw bytes; pass the previous result as Basis to chain. */
	TIDEMARKRUNTIME_API uint64 Fnv1a64(const void* Data, SIZE_T Size, uint64 Basis = Fnv1a64Basis);

	/** Quantum of a depth as hashed: round(Depth / QuantizeCm), non-finite values map to MIN_int32. */
	TIDEMARKRUNTIME_API int32 QuantizeDepth(float DepthCm, float QuantizeCm = DefaultQuantizeCm);

	/**
	 * Hash of a depth field (row-major, SizeX * SizeY floats, centimetres).
	 * Each depth is quantized to an integer (mm by default) and fed as 4 little-endian bytes; each block is hashed
	 * row by row within the block; blocks are then combined with StepIndex in fixed order. Never parallel.
	 */
	TIDEMARKRUNTIME_API FTidemarkStateHash HashDepth(const TArray<float>& Depth, int32 SizeX, int32 SizeY, uint32 StepIndex,
		int32 BlockSize = DefaultBlockSize, float QuantizeCm = DefaultQuantizeCm);

	/**
	 * Block coordinates (BlockX, BlockY) whose hashes differ between A and B, in row-major order.
	 * When the two layouts differ, every block of the larger extent is reported.
	 */
	TIDEMARKRUNTIME_API TArray<FIntPoint> DiffBlocks(const FTidemarkStateHash& A, const FTidemarkStateHash& B);
}
