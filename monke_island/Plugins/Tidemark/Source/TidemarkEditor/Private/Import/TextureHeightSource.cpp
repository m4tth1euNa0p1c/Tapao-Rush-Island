// Tidemark plugin

#include "TidemarkEditor/Import/TextureHeightSource.h"
#include "TidemarkEditor/TidemarkEditorModule.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	enum class ETidemarkHeightFileKind : uint8
	{
		Unknown,
		Raw16,
		Image
	};

	ETidemarkHeightFileKind KindFromExtension(const FString& FilePath)
	{
		const FString Extension = FPaths::GetExtension(FilePath).ToLower();
		if (Extension == TEXT("r16") || Extension == TEXT("raw") || Extension == TEXT("bin"))
		{
			return ETidemarkHeightFileKind::Raw16;
		}
		if (Extension == TEXT("png") || Extension == TEXT("exr"))
		{
			return ETidemarkHeightFileKind::Image;
		}
		return ETidemarkHeightFileKind::Unknown;
	}
}

bool FTidemarkTextureHeightSource::LoadFile(const FString& FilePath, const FTidemarkTextureHeightParams& Params, FString& OutError)
{
	HeightsCm.Reset();
	Width = 0;
	Height = 0;

	if (!(Params.CellSizeCm > 0.0f))
	{
		OutError = TEXT("CellSizeCm must be positive");
		return false;
	}
	if (Params.Encoding == ETidemarkHeightEncoding::Normalized && !(Params.MaxCm > Params.MinCm))
	{
		OutError = TEXT("MaxCm must be above MinCm");
		return false;
	}
	if (!FPaths::FileExists(FilePath))
	{
		OutError = FString::Printf(TEXT("file not found: %s"), *FilePath);
		return false;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
	{
		OutError = FString::Printf(TEXT("cannot read: %s"), *FilePath);
		return false;
	}

	TArray<float> Values;
	bool bDecoded = false;
	switch (KindFromExtension(FilePath))
	{
	case ETidemarkHeightFileKind::Raw16:
		bDecoded = DecodeRaw(Bytes, Params, Values, OutError);
		break;
	case ETidemarkHeightFileKind::Image:
		bDecoded = DecodeImage(Bytes, Params, Values, OutError);
		break;
	default:
		OutError = FString::Printf(TEXT("unsupported extension (expected .r16/.raw/.bin, .png or .exr): %s"), *FilePath);
		break;
	}
	if (!bDecoded)
	{
		Width = 0;
		Height = 0;
		return false;
	}

	Commit(MoveTemp(Values), Params);
	Description = FString::Printf(TEXT("heightmap %s (%d x %d)"), *FPaths::GetCleanFilename(FilePath), Width, Height);
	return true;
}

bool FTidemarkTextureHeightSource::SetHeightsCm(TArray<float> InHeightsCm, int32 InWidth, int32 InHeight, const FTidemarkTextureHeightParams& Params, FString& OutError)
{
	HeightsCm.Reset();
	Width = 0;
	Height = 0;
	if (InWidth <= 0 || InHeight <= 0 || InHeightsCm.Num() != InWidth * InHeight)
	{
		OutError = FString::Printf(TEXT("%d values for %d x %d pixels"), InHeightsCm.Num(), InWidth, InHeight);
		return false;
	}
	if (!(Params.CellSizeCm > 0.0f))
	{
		OutError = TEXT("CellSizeCm must be positive");
		return false;
	}
	Width = InWidth;
	Height = InHeight;
	FTidemarkTextureHeightParams RawParams = Params;
	RawParams.Encoding = ETidemarkHeightEncoding::RawCm;
	Commit(MoveTemp(InHeightsCm), RawParams);
	Description = FString::Printf(TEXT("heightmap in memory (%d x %d)"), Width, Height);
	return true;
}

bool FTidemarkTextureHeightSource::DecodeRaw(const TArray<uint8>& Bytes, const FTidemarkTextureHeightParams& Params, TArray<float>& OutValues, FString& OutError)
{
	const int64 PixelCount = Bytes.Num() / 2;
	if (Bytes.Num() == 0 || (Bytes.Num() % 2) != 0)
	{
		OutError = FString::Printf(TEXT("raw R16 file has %d bytes, not a multiple of 2"), Bytes.Num());
		return false;
	}

	int32 W = Params.RawWidth;
	int32 H = Params.RawHeight;
	if (W <= 0 && H <= 0)
	{
		const int64 Side = static_cast<int64>(FMath::FloorToDouble(FMath::Sqrt(static_cast<double>(PixelCount))));
		if (Side * Side != PixelCount)
		{
			OutError = FString::Printf(TEXT("raw R16 file holds %lld pixels, not a square: pass RawWidth and RawHeight"), PixelCount);
			return false;
		}
		W = static_cast<int32>(Side);
		H = W;
	}
	else if (W > 0 && H <= 0)
	{
		H = static_cast<int32>(PixelCount / W);
	}
	else if (H > 0 && W <= 0)
	{
		W = static_cast<int32>(PixelCount / H);
	}
	if (W <= 0 || H <= 0 || static_cast<int64>(W) * H != PixelCount)
	{
		OutError = FString::Printf(TEXT("raw R16 file holds %lld pixels, not %d x %d"), PixelCount, W, H);
		return false;
	}

	OutValues.SetNumUninitialized(static_cast<int32>(PixelCount));
	const bool bNormalized = Params.Encoding == ETidemarkHeightEncoding::Normalized;
	const uint8* Data = Bytes.GetData();
	for (int32 Index = 0; Index < OutValues.Num(); ++Index)
	{
		// Little-endian whatever the host.
		const uint16 Raw = static_cast<uint16>(Data[2 * Index]) | (static_cast<uint16>(Data[2 * Index + 1]) << 8);
		OutValues[Index] = bNormalized ? static_cast<float>(Raw) / 65535.0f : static_cast<float>(Raw);
	}
	Width = W;
	Height = H;
	return true;
}

bool FTidemarkTextureHeightSource::DecodeImage(const TArray<uint8>& Bytes, const FTidemarkTextureHeightParams& Params, TArray<float>& OutValues, FString& OutError)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	FImage Image;
	if (!ImageWrapperModule.DecompressImage(Bytes.GetData(), Bytes.Num(), Image))
	{
		OutError = TEXT("ImageWrapper could not decode the file (expected 16-bit PNG or EXR)");
		return false;
	}
	if (Image.GetWidth() <= 0 || Image.GetHeight() <= 0 || Image.NumSlices != 1)
	{
		OutError = FString::Printf(TEXT("image is %d x %d x %d"), Image.GetWidth(), Image.GetHeight(), Image.NumSlices);
		return false;
	}

	// A heightmap is linear data: an 8-bit PNG is refused (256 levels on a 40 m range = 16 cm steps, and sRGB would bend it).
	switch (Image.Format)
	{
	case ERawImageFormat::G16:
	case ERawImageFormat::RGBA16:
	case ERawImageFormat::R16F:
	case ERawImageFormat::R32F:
	case ERawImageFormat::RGBA16F:
	case ERawImageFormat::RGBA32F:
		break;
	default:
		OutError = TEXT("image must be 16-bit (PNG) or float (EXR); 8-bit images are refused");
		return false;
	}

	// First channel as linear float; integer formats come out normalized in [0, 1], float formats keep their value.
	FImage Float;
	Image.CopyTo(Float, ERawImageFormat::R32F, EGammaSpace::Linear);
	Width = Float.GetWidth();
	Height = Float.GetHeight();
	const int64 Count = static_cast<int64>(Width) * Height;
	if (Float.RawData.Num() != Count * static_cast<int64>(sizeof(float)))
	{
		OutError = TEXT("decoded image has an unexpected size");
		return false;
	}
	// RawData is the R32F pixel buffer, row-major, one float per pixel.
	const float* Pixels = reinterpret_cast<const float*>(Float.RawData.GetData());
	OutValues.SetNumUninitialized(static_cast<int32>(Count));
	FMemory::Memcpy(OutValues.GetData(), Pixels, Count * sizeof(float));
	return true;
}

void FTidemarkTextureHeightSource::Commit(TArray<float>&& Values, const FTidemarkTextureHeightParams& Params)
{
	OriginWorld = Params.OriginWorld;
	CellSizeCm = Params.CellSizeCm;
	HeightsCm = MoveTemp(Values);

	if (Params.Encoding == ETidemarkHeightEncoding::Normalized)
	{
		const float Range = Params.MaxCm - Params.MinCm;
		for (float& Value : HeightsCm)
		{
			Value = Params.MinCm + Value * Range;
		}
	}

	if (Params.bFlipY)
	{
		for (int32 Y = 0; Y < Height / 2; ++Y)
		{
			float* RowA = HeightsCm.GetData() + static_cast<int64>(Y) * Width;
			float* RowB = HeightsCm.GetData() + static_cast<int64>(Height - 1 - Y) * Width;
			for (int32 X = 0; X < Width; ++X)
			{
				Swap(RowA[X], RowB[X]);
			}
		}
	}
}

float FTidemarkTextureHeightSource::GetPixelCm(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= Width || Y >= Height)
	{
		return 0.0f;
	}
	return HeightsCm[Y * Width + X];
}

FBox2D FTidemarkTextureHeightSource::GetBoundsWorld() const
{
	if (Width <= 0 || Height <= 0)
	{
		return FBox2D(ForceInit);
	}
	// Pixel centres cover [Origin, Origin + (N - 1) * Cell]; each pixel owns half a cell around its centre.
	const FVector2D Half(0.5 * CellSizeCm, 0.5 * CellSizeCm);
	return FBox2D(OriginWorld - Half, OriginWorld + FVector2D((Width - 1) * CellSizeCm, (Height - 1) * CellSizeCm) + Half);
}

bool FTidemarkTextureHeightSource::SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const
{
	if (Width <= 0 || Height <= 0)
	{
		return false;
	}
	// Continuous pixel coordinates: integer values fall on pixel centres. Half a pixel of slack lets the cell centres of a
	// grid aligned on the file's own footprint be sampled; beyond that the source has no data.
	const double PX = (WorldX - OriginWorld.X) / CellSizeCm;
	const double PY = (WorldY - OriginWorld.Y) / CellSizeCm;
	if (PX < -0.5 || PY < -0.5 || PX > Width - 0.5 || PY > Height - 0.5)
	{
		return false;
	}
	const double CX = FMath::Clamp(PX, 0.0, static_cast<double>(Width - 1));
	const double CY = FMath::Clamp(PY, 0.0, static_cast<double>(Height - 1));
	const int32 X0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(CX)), 0, Width - 1);
	const int32 Y0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(CY)), 0, Height - 1);
	const int32 X1 = FMath::Min(X0 + 1, Width - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
	const float TX = static_cast<float>(CX - X0);
	const float TY = static_cast<float>(CY - Y0);
	const float Top = FMath::Lerp(GetPixelCm(X0, Y0), GetPixelCm(X1, Y0), TX);
	const float Bottom = FMath::Lerp(GetPixelCm(X0, Y1), GetPixelCm(X1, Y1), TX);
	OutHeightCm = FMath::Lerp(Top, Bottom, TY);
	return true;
}
