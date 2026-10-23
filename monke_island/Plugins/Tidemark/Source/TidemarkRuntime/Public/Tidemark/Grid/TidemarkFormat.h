// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Grid/TidemarkTerrainAsset.h"

/**
 * Open .tmk format: a folder holding manifest.json and one little-endian binary file per layer
 * (elevation_cm.bin as int16, flags.bin as uint8, both row-major). Specification: Plugins/Tidemark/Docs/TMK.md.
 * Tools/tmk.py reads and writes the same folders without Unreal.
 *
 * Save refuses incomplete credits (Tidemark conventions section 7); Load verifies the schema, the dimensions, the size
 * and the SHA-256 of every layer file before accepting the grid. No randomness, no clock, no world access.
 */
namespace Tidemark::Format
{
	/** Schema written by SaveTmk and the only one LoadTmk accepts. */
	constexpr int32 SchemaVersion = UTidemarkTerrainAsset::CurrentSchemaVersion;

	/** File names inside a .tmk folder. */
	inline const TCHAR* ManifestFileName = TEXT("manifest.json");
	inline const TCHAR* ElevationFileName = TEXT("elevation_cm.bin");
	inline const TCHAR* FlagsFileName = TEXT("flags.bin");

	/** Layer names in the manifest. */
	inline const TCHAR* ElevationLayerName = TEXT("elevation_cm");
	inline const TCHAR* FlagsLayerName = TEXT("flags");

	/**
	 * Writes Grid and Credits as a .tmk folder at Dir (created if needed). False with OutError when the grid is invalid,
	 * a credit is incomplete or empty, or a file cannot be written.
	 */
	TIDEMARKRUNTIME_API bool SaveTmk(const FTidemarkGrid& Grid, const TArray<FTidemarkCredit>& Credits, const FString& Dir, FString& OutError);

	/**
	 * Reads a .tmk folder into OutGrid and OutCredits. False with OutError (and OutGrid reset) when the manifest is missing or
	 * malformed, the schema differs, a layer file is missing, has a wrong size or a wrong SHA-256, or the grid exceeds FTidemarkGrid::MaxCells.
	 * Missing credits are accepted with an empty OutCredits (the CI, not the loader, enforces them).
	 */
	TIDEMARKRUNTIME_API bool LoadTmk(const FString& Dir, FTidemarkGrid& OutGrid, TArray<FTidemarkCredit>& OutCredits, FString& OutError);

	/** SHA-256 of a byte buffer as 64 lower-case hex digits (FIPS 180-4, own implementation: platform independent). */
	TIDEMARKRUNTIME_API FString Sha256Hex(const uint8* Data, int64 Size);

	/** SHA-256 of an array as 64 lower-case hex digits. */
	TIDEMARKRUNTIME_API FString Sha256Hex(const TArray<uint8>& Data);
}
