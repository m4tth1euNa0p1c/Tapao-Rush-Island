// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "TidemarkSeed.generated.h"

/**
 * Single 64-bit seed of a Tidemark scenario and its named sub-streams.
 *
 * Every random choice of the simulation derives from this value, never from the engine's unseeded random,
 * the clock or the CPU cycle counter (CONVENTIONS.md §4). Derivation is SplitMix64, so the same (seed, stream name) pair yields the
 * same sub-seed on every machine and every engine version.
 *
 * FromName hashes the UTF-8 bytes of the name with FNV-1a 64 (identical to ANSI for ASCII names): unlike
 * GetTypeHash(FName), the result does not depend on the FName table nor on the engine version.
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FTidemarkSeed
{
	GENERATED_BODY()

	/** Raw 64-bit seed. Zero is a valid seed. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Seed")
	uint64 Value = 0;

	FTidemarkSeed() = default;
	explicit FTidemarkSeed(uint64 InValue) : Value(InValue) {}

	/** Seed from a raw value. */
	static FTidemarkSeed FromValue(uint64 InValue) { return FTidemarkSeed(InValue); }

	/** Seed from a name: FNV-1a 64 of its UTF-8 string, stable across runs, machines and engine versions. */
	static FTidemarkSeed FromName(FName Name);

	/** Sub-seed of a named stream: SplitMix64(Value ^ FNV-1a(Stream)). Pure. */
	FTidemarkSeed Derive(FName Stream) const;

	/** Engine random stream seeded from the upper 32 bits of SplitMix64(Value). Pure. */
	FRandomStream ToRandomStream() const;

	/** SplitMix64 output function: bijective 64-bit mix, the single scrambler used by Derive, ToRandomStream and FTidemarkRng. */
	static uint64 Mix(uint64 X);

	bool operator==(const FTidemarkSeed& Other) const { return Value == Other.Value; }
	bool operator!=(const FTidemarkSeed& Other) const { return Value != Other.Value; }
};

/**
 * Small deterministic generator owned by a stream (SplitMix64: 64-bit state, period 2^64, one add and one mix per draw).
 * Chosen over xoshiro for its single-word state, which is trivial to replicate, snapshot and hash.
 * Not suitable for cryptography; plenty for gameplay noise.
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FTidemarkRng
{
	GENERATED_BODY()

	/** Current SplitMix64 state. Advances by the golden-ratio increment at every draw. */
	UPROPERTY(VisibleAnywhere, Category = "Tidemark|Seed")
	uint64 State = 0;

	FTidemarkRng() = default;
	explicit FTidemarkRng(FTidemarkSeed Seed) : State(Seed.Value) {}

	/** Next 64-bit value, uniform. */
	uint64 NextU64();

	/** Next 32-bit value, uniform (upper half of NextU64). */
	uint32 NextU32() { return static_cast<uint32>(NextU64() >> 32); }

	/** Next float in [0, 1), 24 bits of resolution, same bits on every platform. */
	float NextFloat();

	/** Next integer in [Min, Max] inclusive; returns Min when Max <= Min. */
	int32 NextRange(int32 Min, int32 Max);
};
