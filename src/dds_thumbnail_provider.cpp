// ----------------------------------------------------------------------------
// dds_thumbnail_provider.cpp 
// ----------------------------------------------------------------------------
#include "dds_thumbnail_provider.h"
#include <Shlwapi.h>
#include <cassert>
#include <functional>

#include "./DirectXTex/DirectXTex.h"
#include "scope_exit.h"

// using namespace Gdiplus;

#pragma comment(lib, "Shlwapi.lib")
// #pragma comment(lib, "gdiplus.lib")

extern HINSTANCE g_hInst;
extern long      g_cDllRef;

namespace
{
	// Is BC format? 
	bool bcFormat(DXGI_FORMAT fmt)
	{
		DXGI_FORMAT bcs[] = 
		{
			DXGI_FORMAT_BC1_TYPELESS,
			DXGI_FORMAT_BC1_UNORM,
			DXGI_FORMAT_BC1_UNORM_SRGB,
			DXGI_FORMAT_BC2_TYPELESS,
			DXGI_FORMAT_BC2_UNORM,
			DXGI_FORMAT_BC2_UNORM_SRGB,
			DXGI_FORMAT_BC3_TYPELESS,
			DXGI_FORMAT_BC3_UNORM,
			DXGI_FORMAT_BC3_UNORM_SRGB,
			DXGI_FORMAT_BC4_TYPELESS,
			DXGI_FORMAT_BC4_UNORM,
			DXGI_FORMAT_BC4_SNORM,
			DXGI_FORMAT_BC5_TYPELESS,
			DXGI_FORMAT_BC5_UNORM,
			DXGI_FORMAT_BC5_SNORM,
			DXGI_FORMAT_BC6H_TYPELESS,
			DXGI_FORMAT_BC6H_UF16,
			DXGI_FORMAT_BC6H_SF16,
			DXGI_FORMAT_BC7_TYPELESS,
			DXGI_FORMAT_BC7_UNORM,
			DXGI_FORMAT_BC7_UNORM_SRGB,
		};

		for(int i=0; i<(sizeof(bcs)/sizeof(DXGI_FORMAT)); ++i)
		{
			if(bcs[i] == fmt)
			{
				return true;
			}
		}
		return false;
	}

	inline float UintAsFloat(uint32_t val)
	{
		union
		{
			uint32_t m_uint;
			float m_flt;
		} un;
		un.m_uint = val;
		return un.m_flt;
	}

	template <class T> static inline uint32_t CountLeadingZeroBitsSlow(T value)
	{
		uint32_t count = 0U;
		uint32_t const bitCount = sizeof(T) * 8;
		T mask = T(1UL) << (bitCount - 1);
		while (0 == (mask&value) && 0 != mask)
		{
			mask >>= 1;
			count++;
		}
		return count;
	}

	uint32_t CountLeadingZeroBits(uint32_t value)
	{
		return CountLeadingZeroBitsSlow<uint32_t>(value);
	}

	float HalfToFloat(uint16_t value)
	{
		int32_t bsgn = (int32_t)(value >> 15) << 31;
		int32_t rexp = (int32_t)(value >> 10) & 0x1f;
		int32_t bexp = rexp + (127 - 15);
		int32_t rmnt = (int32_t)(value & 0x03ff);

		int32_t retval = 0;

		if (rexp == 31)
		{
			if (rmnt == 0)
			{	// inifinity
				retval = bsgn | 0x7F800000;
			}
			else
			{	// NaN
				retval = bsgn | 0x7F800000 | (rmnt << 13U);
			}
		}
		else if (rexp > 0)
		{
			retval = bsgn | (bexp << 23) | (rmnt << 13U);
		}
		else
		{
			if (rmnt)
			{
				int32_t const expShift = CountLeadingZeroBits(static_cast<uint32_t>(rmnt)) - 22;
				int32_t const exp = bexp - expShift;
				int32_t const bmnt = (rmnt << (13 + (expShift + 1))) & 0x7fffff;
				retval = bsgn | (exp << 23U) | bmnt;
			}
			else
			{
				retval = bsgn;
			}
		}
		return UintAsFloat(retval);
	}

	DirectX::ScratchImage Resize(int cx, DirectX::ScratchImage& image)
	{
		HRESULT hr;
		DirectX::ScratchImage resizedImage;

		hr = DirectX::Resize(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
			cx, cx, DirectX::TEX_FILTER_DEFAULT, resizedImage);
		assert(SUCCEEDED(hr));

		return resizedImage;
	}

	void InitializeBMI(BITMAPINFO& bmi, BITMAPINFOHEADER& bmiHeader, UINT cx)
	{
		ZeroMemory(&bmiHeader, sizeof(BITMAPINFOHEADER));
		bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmiHeader.biWidth = cx;
		bmiHeader.biHeight = cx;
		bmiHeader.biPlanes = 1;
		bmiHeader.biBitCount = 32;
		bmi.bmiHeader = bmiHeader;
	}

} // unnamed namespace 

typedef std::function<void(UINT, LPBYTE, const DirectX::Image*)> InflateFunction;

// DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB 
void InflateFunction_FORMAT_R8G8B8A8_UNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[4 * (y    * cx + x)];

			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = src[3];
		}
	}
}

// DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB 
void InflateFunction_FORMAT_B8G8R8A8_UNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[4 * (y    * cx + x)];

			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
		}
	}
}

// DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB 
void InflateFunction_FORMAT_B8G8R8X8_UNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[4 * (y    * cx + x)];

			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
		}
	}
}

// DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8_UNORM 
void InflateFunction_FORMAT_R8_UNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[1 * (y    * cx + x)];

			dst[0] = 0;
			dst[1] = 0;
			dst[2] = src[0];
			dst[3] = 255;
		}
	}
}

// DXGI_FORMAT_R8_SNORM 
void InflateFunction_FORMAT_R8_SNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	InflateFunction_FORMAT_R8_UNORM(cx, lpb, img);
}

// DXGI_FORMAT_R8G8_UNORM 
void InflateFunction_FORMAT_R8G8_UNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[2 * (y    * cx + x)];

			dst[0] = 0;
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = 255;
		}
	}
}

// DXGI_FORMAT_R8G8_SNORM 
void InflateFunction_FORMAT_R8G8_SNORM(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	InflateFunction_FORMAT_R8G8_UNORM(cx, lpb, img);
}

// DXGI_FORMAT_R32G32B32A32_FLOAT 
void InflateFunction_FORMAT_R32G32B32A32_FLOAT(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4  * (line * cx + x)];
			LPBYTE src = &lps[16 * (y    * cx + x)];
			float* pf = (float*)src;

			float fr = pf[0]; if(fr<0.0f){fr=0.0f;} else if(fr>1.0f){fr=1.0f;};
			float fg = pf[1]; if(fg<0.0f){fg=0.0f;} else if(fg>1.0f){fg=1.0f;};
			float fb = pf[2]; if(fb<0.0f){fb=0.0f;} else if(fb>1.0f){fb=1.0f;};

			dst[0] = (BYTE)(fb*255.0f);
			dst[1] = (BYTE)(fg*255.0f);
			dst[2] = (BYTE)(fr*255.0f);
			dst[3] = 255;
		}
	}
}

// DXGI_FORMAT_R32G32_FLOAT 
void InflateFunction_FORMAT_R32G32_FLOAT(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[8 * (y    * cx + x)];
			float* pf = (float*)src;

			float fr = pf[0]; if (fr<0.0f){ fr = 0.0f; } else if (fr > 1.0f){ fr = 1.0f; };
			float fg = pf[1]; if (fg<0.0f){ fg = 0.0f; } else if (fg > 1.0f){ fg = 1.0f; };

			dst[0] = 0;
			dst[1] = (BYTE)(fg*255.0f);
			dst[2] = (BYTE)(fr*255.0f);
			dst[3] = 255;
		}
	}
}

// DXGI_FORMAT_R16G16B16A16_FLOAT 
void InflateFunction_FORMAT_R16G16B16A16_FLOAT(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[8 * (y    * cx + x)];
			uint16_t* ph = (uint16_t*)src;

			float fr = HalfToFloat(ph[0]); if (fr<0.0f){ fr = 0.0f; } else if (fr > 1.0f){ fr = 1.0f; };
			float fg = HalfToFloat(ph[1]); if (fg<0.0f){ fg = 0.0f; } else if (fg > 1.0f){ fg = 1.0f; };
			float fb = HalfToFloat(ph[2]); if (fb<0.0f){ fb = 0.0f; } else if (fb > 1.0f){ fb = 1.0f; };

			dst[0] = (BYTE)(fb*255.0f);
			dst[1] = (BYTE)(fg*255.0f);
			dst[2] = (BYTE)(fr*255.0f);
			dst[3] = 255;
		}
	}
}

// DXGI_FORMAT_R16G16_FLOAT 
void InflateFunction_FORMAT_R16G16_FLOAT(UINT cx, LPBYTE lpb, const DirectX::Image* img)
{
	LPBYTE lps = (LPBYTE)(img->pixels);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < (int)cx; ++y)
	{
		int line = (int)cx - y - 1;
		for (UINT x = 0; x < cx; ++x)
		{
			LPBYTE dst = &lpb[4 * (line * cx + x)];
			LPBYTE src = &lps[4 * (y    * cx + x)];
			uint16_t* ph = (uint16_t*)src;

			float fr = HalfToFloat(ph[0]); if (fr<0.0f){ fr = 0.0f; } else if (fr > 1.0f){ fr = 1.0f; };
			float fg = HalfToFloat(ph[1]); if (fg<0.0f){ fg = 0.0f; } else if (fg > 1.0f){ fg = 1.0f; };

			dst[0] = 0;
			dst[1] = (BYTE)(fg*255.0f);
			dst[2] = (BYTE)(fr*255.0f);
			dst[3] = 255;
		}
	}
}


DDSThumbnailProvider::DDSThumbnailProvider()
	: m_cRef   (1)
	, m_pStream(NULL)
{
	InterlockedIncrement(&g_cDllRef);
}


DDSThumbnailProvider::~DDSThumbnailProvider()
{
	InterlockedDecrement(&g_cDllRef);
}

//-------------------------------------------------------------
// IUnknown 
#pragma region IUnknown
IFACEMETHODIMP DDSThumbnailProvider::QueryInterface(REFIID riid, void **ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(DDSThumbnailProvider, IThumbnailProvider),
		QITABENT(DDSThumbnailProvider, IInitializeWithStream),
		{ 0 },
	};
	return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) DDSThumbnailProvider::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) DDSThumbnailProvider::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);
	if (0 == cRef)
	{
		delete this;
	}
	return cRef;
}
#pragma endregion

//-------------------------------------------------------------
// IInitializeWithStream 
#pragma region IInitializeWithStream
// Initializes the thumbnail handler with a stream.
IFACEMETHODIMP DDSThumbnailProvider::Initialize(IStream *pStream, DWORD grfMode)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
	if (m_pStream == NULL)
	{
		hr = pStream->QueryInterface(&m_pStream);
	}
	return hr;
}
#pragma endregion

//-------------------------------------------------------------
// IThumbnailProvider 
#pragma region IThumbnailProvider
IFACEMETHODIMP DDSThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
	HRESULT hr;

	*phbmp    = NULL;
	*pdwAlpha = WTSAT_UNKNOWN;

	if(!m_pStream)
	{
		return E_NOTIMPL;
	}

	ULONG fileSize = (ULONG)(DDSFileSize(m_pStream));
	if(fileSize==0)
	{
		return E_NOTIMPL;
	}

	// メモリブロックの確保. 
	uint8_t* ddsBlock = reinterpret_cast<uint8_t*>(malloc(fileSize));
	SCOPE_EXIT(if(ddsBlock){free(ddsBlock);});

	// DDSイメージの読み込み. 
	ULONG fileRead;
	m_pStream->Read(ddsBlock, fileSize, &fileRead);

	DirectX::TexMetadata   metaData;
	DirectX::ScratchImage  scratchImage;
	DirectX::ScratchImage  decompressedImage;
	DirectX::ScratchImage  thumbImage;

	BYTE r=0, g=0, b=0, a=255;
	bool imageReady = false;

	hr = DirectX::LoadFromDDSMemory(ddsBlock, fileSize, DirectX::DDS_FLAGS_NONE, &metaData, scratchImage);
	if(SUCCEEDED(hr))
	{
		const DirectX::Image*  image = scratchImage.GetImage(0, 0, 0);
		if(bcFormat(image->format))
		{
			hr = DirectX::Decompress(*image, DXGI_FORMAT_UNKNOWN, decompressedImage);
			if(SUCCEEDED(hr))
			{
				thumbImage = Resize(cx, decompressedImage);
				r = 255;
				g = 255;
				b = 128;
				a = 255;
				imageReady = true;
			}
		}
		else
		{
			thumbImage = Resize(cx, scratchImage);
			r = 255;
			g = 120;
			b = 0;
			a = 255;
			imageReady = true;
		}
		if(!imageReady)
		{
			r = g = b = 0;
			a = 255;
		}
	}

	// サムネイルイメージの作成. 
	bool createHBitmap = false;
	
	if(imageReady)
	{
		createHBitmap = CreateHBITMAP_Image(cx, phbmp, pdwAlpha, thumbImage);
	}
	else
	{
		createHBitmap = CreateHBITMAP_Fill (cx, phbmp, pdwAlpha, r, g, b, a);
	}

	return (createHBitmap) ? S_OK : E_NOTIMPL;
}
#pragma endregion

bool DDSThumbnailProvider::CreateHBITMAP_Image(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha, DirectX::ScratchImage& image)
{
	const DirectX::Image* firstImage = image.GetImage(0, 0, 0);
	if (!firstImage)
	{
		return CreateHBITMAP_Fill(cx, phbmp, pdwAlpha, 0, 0, 0);
	}

	UINT             i;
	LPBYTE           lp;
	LPVOID           lpBits;
	BITMAPINFO       bmi;
	BITMAPINFOHEADER bmiHeader;

	InitializeBMI(bmi, bmiHeader, cx);

	*phbmp = CreateDIBSection(NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, &lpBits, NULL, 0);

	lp = (LPBYTE)lpBits;

	DXGI_FORMAT fmt = firstImage->format;

	struct InflatePixelFunctions
	{
		DXGI_FORMAT		format;
		InflateFunction	func;
	} inflateFunctions[] = 
	{
		{ DXGI_FORMAT_R8G8B8A8_UNORM,         InflateFunction_FORMAT_R8G8B8A8_UNORM },
		{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    InflateFunction_FORMAT_R8G8B8A8_UNORM },
		{ DXGI_FORMAT_R8G8B8A8_UINT,          InflateFunction_FORMAT_R8G8B8A8_UNORM },
		{ DXGI_FORMAT_R8G8B8A8_SNORM,         InflateFunction_FORMAT_R8G8B8A8_UNORM },
		{ DXGI_FORMAT_R8G8B8A8_SINT,          InflateFunction_FORMAT_R8G8B8A8_UNORM },
		{ DXGI_FORMAT_R8G8B8A8_TYPELESS,      InflateFunction_FORMAT_R8G8B8A8_UNORM },

		{ DXGI_FORMAT_B8G8R8A8_TYPELESS,      InflateFunction_FORMAT_B8G8R8A8_UNORM },
		{ DXGI_FORMAT_B8G8R8A8_UNORM,         InflateFunction_FORMAT_B8G8R8A8_UNORM },
		{ DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    InflateFunction_FORMAT_B8G8R8A8_UNORM },

		{ DXGI_FORMAT_B8G8R8X8_TYPELESS,      InflateFunction_FORMAT_B8G8R8X8_UNORM },
		{ DXGI_FORMAT_B8G8R8X8_UNORM,         InflateFunction_FORMAT_B8G8R8X8_UNORM },
		{ DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,    InflateFunction_FORMAT_B8G8R8X8_UNORM },

		{ DXGI_FORMAT_R8_UNORM,               InflateFunction_FORMAT_R8_UNORM       },
		{ DXGI_FORMAT_R8_SNORM,               InflateFunction_FORMAT_R8_SNORM       },

		{ DXGI_FORMAT_R8G8_TYPELESS,          InflateFunction_FORMAT_R8G8_SNORM     },
		{ DXGI_FORMAT_R8G8_UNORM,             InflateFunction_FORMAT_R8G8_UNORM     },
		{ DXGI_FORMAT_R8G8_SNORM,             InflateFunction_FORMAT_R8G8_SNORM     },
		{ DXGI_FORMAT_R8G8_UINT,              InflateFunction_FORMAT_R8G8_UNORM     },
		{ DXGI_FORMAT_R8G8_SINT,              InflateFunction_FORMAT_R8G8_SNORM     },

		{ DXGI_FORMAT_R32G32B32A32_FLOAT,     InflateFunction_FORMAT_R32G32B32A32_FLOAT },
		{ DXGI_FORMAT_R32G32_FLOAT,           InflateFunction_FORMAT_R32G32_FLOAT   },

		{ DXGI_FORMAT_R16G16B16A16_FLOAT,     InflateFunction_FORMAT_R16G16B16A16_FLOAT },
		{ DXGI_FORMAT_R16G16_FLOAT,           InflateFunction_FORMAT_R16G16_FLOAT   },

	};
	size_t n = sizeof(inflateFunctions) / sizeof(InflatePixelFunctions);

	for(size_t x=0; x<n; ++x)
	{
		if(inflateFunctions[x].format == fmt)
		{
			(inflateFunctions[x].func)(cx, lp, firstImage);
			*pdwAlpha = WTSAT_ARGB;
			return true;
		}
	}

	for (i = 0; i < cx * cx; i++)
	{
		lp[0] = 0;
		lp[1] = 0;
		lp[2] = 0;
		lp[3] = 255;
		lp += 4;
	}

	*pdwAlpha = WTSAT_ARGB;

	return true;
}


bool DDSThumbnailProvider::CreateHBITMAP_Fill(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha, BYTE r, BYTE g, BYTE b, BYTE a)
{
	UINT             i;
	LPBYTE           lp;
	LPVOID           lpBits;
	BITMAPINFO       bmi;
	BITMAPINFOHEADER bmiHeader;

	InitializeBMI(bmi, bmiHeader, cx);

	*phbmp  = CreateDIBSection(NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, &lpBits, NULL, 0);

	lp = (LPBYTE)lpBits;

	for(i = 0; i < cx * cx; i++)
	{
		lp[0] = b;
		lp[1] = g;
		lp[2] = r;
		lp[3] = a;
		lp += 4;
	}

	*pdwAlpha = WTSAT_ARGB;

	return true;
}

size_t DDSThumbnailProvider::DDSFileSize(IStream* pStream)
{
	// ファイルサイズの取得. 
	STATSTG statStg;
	::ZeroMemory(&statStg, sizeof(STATSTG));

	LARGE_INTEGER lnPos;
	lnPos.QuadPart = 0;

	pStream->Seek(lnPos,    STREAM_SEEK_SET, NULL);
	pStream->Stat(&statStg, STATFLAG_DEFAULT);

	size_t fileSize = static_cast<size_t>(statStg.cbSize.QuadPart);

	return fileSize;
}


