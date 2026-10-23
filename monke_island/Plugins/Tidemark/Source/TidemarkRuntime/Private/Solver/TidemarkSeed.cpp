// Tidemark plugin

#include "Tidemark/Solver/TidemarkSeed.h"

#include "Tidemark/Solver/TidemarkHash.h"

namespace
{
	/** FNV-1a 64 of the UTF-8 bytes of a name (null terminator excluded). */
	uint64 HashName(FName Name)
	{
		const FString String = Name.ToString();
		const FTCHARToUTF8 Utf8(*String);
		return Tidemark::Hash::Fnv1a64(Utf8.Get(), static_cast<SIZE_T>(Utf8.Length()));
	}
}

uint64 FTidemarkSeed::Mix(uint64 X)
{
	// SplitMix64 output function (Steele, Lea, Flood 2014), as used by java.util.SplittableRandom.
	uint64 Z = X + 0x9E3779B97F4A7C15ULL;
	Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
	return Z ^ (Z >> 31);
}

FTidemarkSeed FTidemarkSeed::FromName(FName Name)
{
	return FTidemarkSeed(HashName(Name));
}

FTidemarkSeed FTidemarkSeed::Derive(FName Stream) const
{
	return FTidemarkSeed(Mix(Value ^ HashName(Stream)));
}

FRandomStream FTidemarkSeed::ToRandomStream() const
{
	const uint32 Upper = static_cast<uint32>(Mix(Value) >> 32);
	return FRandomStream(static_cast<int32>(Upper)); // tidemark-allow-random: single seeded construction point, seed derived by SplitMix64 from FTidemarkSeed
}

uint64 FTidemarkRng::NextU64()
{
	State += 0x9E3779B97F4A7C15ULL;
	// Scrambler of SplitMix64 applied to the advanced state (the increment is already in State, so Mix() is not reused).
	uint64 Z = State;
	Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
	return Z ^ (Z >> 31);
}

float FTidemarkRng::NextFloat()
{
	// 24 significant bits: exact in a float, so the division is the same on every IEEE-754 platform.
	const uint32 Bits = static_cast<uint32>(NextU64() >> 40);
	return static_cast<float>(Bits) * (1.0f / 16777216.0f);
}

int32 FTidemarkRng::NextRange(int32 Min, int32 Max)
{
	if (Max <= Min)
	{
		return Min;
	}
	const uint64 Span = static_cast<uint64>(static_cast<int64>(Max) - static_cast<int64>(Min)) + 1;
	// Modulo bias is below 2^-32 for any gameplay span: accepted for a fixed cost per draw.
	return static_cast<int32>(static_cast<int64>(Min) + static_cast<int64>(NextU64() % Span));
}
