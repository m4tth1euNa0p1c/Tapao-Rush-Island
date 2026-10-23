// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"
#include "TextureHeightSource.generated.h"

/** How the pixels of a heightmap file are turned into centimetres. */
UENUM(BlueprintType)
enum class ETidemarkHeightEncoding : uint8
{
	/** Pixel in [0, 1] (integer formats are divided by their maximum): height = MinCm + value * (MaxCm - MinCm). */
	Normalized,
	/** Pixel value is already a height in centimetres (float EXR written by a GIS or by Tools/tmk.py). MinCm/MaxCm unused. */
	RawCm
};

/** Where a heightmap file sits in the world and how its pixels are decoded. */
USTRUCT(BlueprintType)
struct TIDEMARKEDITOR_API FTidemarkTextureHeightParams
{
	GENERATED_BODY()

	/** Height of a pixel at 0 (cm, world Z) with the Normalized encoding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap", meta = (Units = "cm"))
	float MinCm = -2000.0f;

	/** Height of a pixel at 1 (cm, world Z) with the Normalized encoding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap", meta = (Units = "cm"))
	float MaxCm = 2000.0f;

	/** World XY (cm) of the centre of pixel (0, 0). Row 0 of the file is the lowest Y (Unreal top view: +Y goes down), as Landscape exports it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap")
	FVector2D OriginWorld = FVector2D::ZeroVector;

	/** World distance between two neighbouring pixels (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap", meta = (ClampMin = "1.0", Units = "cm"))
	float CellSizeCm = 100.0f;

	/** Pixel decoding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap")
	ETidemarkHeightEncoding Encoding = ETidemarkHeightEncoding::Normalized;

	/** When true, row 0 of the file is the highest Y instead (image editors, north-up GIS exports). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap")
	bool bFlipY = false;

	/** Width in pixels of a raw .r16 file; 0 deduces a square from the file size. Ignored for PNG and EXR. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap", meta = (ClampMin = "0"))
	int32 RawWidth = 0;

	/** Height in pixels of a raw .r16 file; 0 deduces a square from the file size. Ignored for PNG and EXR. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Heightmap", meta = (ClampMin = "0"))
	int32 RawHeight = 0;
};

/**
 * Height source backed by a heightmap file, decoded once into a float array (cm): raw little-endian R16 (.r16 / .raw),
 * 16-bit PNG (grey or RGB(A), first channel) and EXR (float, first channel) through IImageWrapperModule::DecompressImage.
 * SampleHeightCm is bilinear between the four nearest pixel centres and refuses positions outside the pixel centres.
 * Budget: a 4 k x 4 k map decodes and bakes in under 5 s.
 */
class TIDEMARKEDITOR_API FTidemarkTextureHeightSource final : public ITidemarkHeightSource
{
public:
	FTidemarkTextureHeightSource() = default;

	/**
	 * Decodes FilePath with Params. Format from the extension: .r16 / .raw / .bin (raw uint16 LE), .png, .exr.
	 * False with OutError when the file is missing, unreadable, of another format or of inconsistent size.
	 */
	bool LoadFile(const FString& FilePath, const FTidemarkTextureHeightParams& Params, FString& OutError);

	/** Takes an already decoded map (row-major, Width * Height values, cm). For tools and specs. */
	bool SetHeightsCm(TArray<float> InHeightsCm, int32 InWidth, int32 InHeight, const FTidemarkTextureHeightParams& Params, FString& OutError);

	/** Pixels along X. */
	int32 GetWidth() const { return Width; }

	/** Pixels along Y. */
	int32 GetHeight() const { return Height; }

	/** Decoded height (cm) of pixel (X, Y), row 0 = lowest world Y once bFlipY is applied. Zero outside. */
	float GetPixelCm(int32 X, int32 Y) const;

	//~ Begin ITidemarkHeightSource Interface
	virtual FBox2D GetBoundsWorld() const override;
	virtual bool SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const override;
	virtual FString GetSourceDescription() const override { return Description; }
	//~ End ITidemarkHeightSource Interface

private:
	/** Decodes a raw uint16 little-endian file. */
	bool DecodeRaw(const TArray<uint8>& Bytes, const FTidemarkTextureHeightParams& Params, TArray<float>& OutValues, FString& OutError);

	/** Decodes PNG or EXR through the ImageWrapper module into normalized or raw float values. */
	bool DecodeImage(const TArray<uint8>& Bytes, const FTidemarkTextureHeightParams& Params, TArray<float>& OutValues, FString& OutError);

	/** Converts decoded values (normalized or raw) to cm, applies bFlipY and stores the map. */
	void Commit(TArray<float>&& Values, const FTidemarkTextureHeightParams& Params);

	TArray<float> HeightsCm;
	int32 Width = 0;
	int32 Height = 0;
	FVector2D OriginWorld = FVector2D::ZeroVector;
	float CellSizeCm = 100.0f;
	FString Description = TEXT("heightmap (empty)");
};
