// ----------------------------------------------------------------------------
// dds_thumbnail_provider.cpp 
// ----------------------------------------------------------------------------
#include "dds_thumbnail_provider.h"
#include <Shlwapi.h>
#include <strsafe.h>
#include <fstream>
#include <cassert>
#include <functional>

#include "./DirectXTex/DirectXTex.h"
#include "./DirectXTex/DDS.h"
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

	const wchar_t* DDSFormatString(DXGI_FORMAT fmt)
	{
		static struct DXGI_FORMAT_caption
		{
			DXGI_FORMAT		format;
			const wchar_t*	caption;
		} captions[] =
		{
			{ DXGI_FORMAT_R32G32B32A32_TYPELESS,	L"R32G32B32A32_TYPELESS" },
			{ DXGI_FORMAT_R32G32B32A32_FLOAT,		L"R32G32B32A32_FLOAT" },
			{ DXGI_FORMAT_R32G32B32A32_UINT,		L"R32G32B32A32_UINT" },
			{ DXGI_FORMAT_R32G32B32A32_SINT,		L"R32G32B32A32_SINT" },
			{ DXGI_FORMAT_R32G32B32_TYPELESS,		L"R32G32B32_TYPELESS" },
			{ DXGI_FORMAT_R32G32B32_FLOAT,			L"R32G32B32_FLOAT" },
			{ DXGI_FORMAT_R32G32B32_UINT,			L"R32G32B32_UINT" },
			{ DXGI_FORMAT_R32G32B32_SINT,			L"R32G32B32_SINT" },
			{ DXGI_FORMAT_R16G16B16A16_TYPELESS,	L"R16G16B16A16_TYPELESS" },
			{ DXGI_FORMAT_R16G16B16A16_FLOAT,		L"R16G16B16A16_FLOAT" },
			{ DXGI_FORMAT_R16G16B16A16_UNORM,		L"R16G16B16A16_UNORM" },
			{ DXGI_FORMAT_R16G16B16A16_UINT,		L"R16G16B16A16_UINT" },
			{ DXGI_FORMAT_R16G16B16A16_SNORM,		L"R16G16B16A16_SNORM" },
			{ DXGI_FORMAT_R16G16B16A16_SINT,		L"R16G16B16A16_SINT" },
			{ DXGI_FORMAT_R32G32_TYPELESS,			L"R32G32_TYPELESS" },
			{ DXGI_FORMAT_R32G32_FLOAT,				L"R32G32_FLOAT" },
			{ DXGI_FORMAT_R32G32_UINT,				L"R32G32_UINT" },
			{ DXGI_FORMAT_R32G32_SINT,				L"R32G32_SINT" },
			{ DXGI_FORMAT_R32G8X24_TYPELESS,		L"R32G8X24_TYPELESS" },
			{ DXGI_FORMAT_D32_FLOAT_S8X24_UINT,		L"D32_FLOAT_S8X24_UINT" },
			{ DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,	L"R32_FLOAT_X8X24_TYPELESS" },
			{ DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,	L"X32_TYPELESS_G8X24_UINT" },
			{ DXGI_FORMAT_R10G10B10A2_TYPELESS,		L"R10G10B10A2_TYPELESS" },
			{ DXGI_FORMAT_R10G10B10A2_UNORM,		L"R10G10B10A2_UNORM" },
			{ DXGI_FORMAT_R10G10B10A2_UINT,			L"R10G10B10A2_UINT" },
			{ DXGI_FORMAT_R11G11B10_FLOAT,			L"R11G11B10_FLOAT" },
			{ DXGI_FORMAT_R8G8B8A8_TYPELESS,		L"R8G8B8A8_TYPELESS" },
			{ DXGI_FORMAT_R8G8B8A8_UNORM,			L"R8G8B8A8_UNORM" },
			{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,		L"R8G8B8A8_UNORM_SRGB" },
			{ DXGI_FORMAT_R8G8B8A8_UINT,			L"R8G8B8A8_UINT" },
			{ DXGI_FORMAT_R8G8B8A8_SNORM ,			L"R8G8B8A8_SNORM" },
			{ DXGI_FORMAT_R8G8B8A8_SINT,			L"R8G8B8A8_SINT" },
			{ DXGI_FORMAT_R16G16_TYPELESS,			L"R16G16_TYPELESS" },
			{ DXGI_FORMAT_R16G16_FLOAT,				L"R16G16_FLOAT" },
			{ DXGI_FORMAT_R16G16_UNORM,				L"R16G16_UNORM" },
			{ DXGI_FORMAT_R16G16_UINT,				L"R16G16_UINT" },
			{ DXGI_FORMAT_R16G16_SNORM,				L"R16G16_SNORM" },
			{ DXGI_FORMAT_R16G16_SINT,				L"R16G16_SINT" },
			{ DXGI_FORMAT_R32_TYPELESS,				L"R32_TYPELESS" },
			{ DXGI_FORMAT_D32_FLOAT,				L"D32_FLOAT" },
			{ DXGI_FORMAT_R32_FLOAT,				L"R32_FLOAT" },
			{ DXGI_FORMAT_R32_UINT,					L"R32_UINT" },
			{ DXGI_FORMAT_R32_SINT,					L"R32_SINT" },
			{ DXGI_FORMAT_R24G8_TYPELESS,			L"R24G8_TYPELESS" },
			{ DXGI_FORMAT_D24_UNORM_S8_UINT,		L"D24_UNORM_S8_UINT" },
			{ DXGI_FORMAT_R24_UNORM_X8_TYPELESS,	L"R24_UNORM_X8_TYPELESS" },
			{ DXGI_FORMAT_X24_TYPELESS_G8_UINT,		L"X24_TYPELESS_G8_UINT" },
			{ DXGI_FORMAT_R8G8_TYPELESS,			L"R8G8_TYPELESS" },
			{ DXGI_FORMAT_R8G8_UNORM,				L"R8G8_UNORM" },
			{ DXGI_FORMAT_R8G8_UINT,				L"R8G8_UINT" },
			{ DXGI_FORMAT_R8G8_SNORM,				L"R8G8_SNORM" },
			{ DXGI_FORMAT_R8G8_SINT,				L"R8G8_SINT" },
			{ DXGI_FORMAT_R16_TYPELESS,				L"R16_TYPELESS" },
			{ DXGI_FORMAT_R16_FLOAT,				L"R16_FLOAT" },
			{ DXGI_FORMAT_D16_UNORM,				L"D16_UNORM" },
			{ DXGI_FORMAT_R16_UNORM,				L"R16_UNORM" },
			{ DXGI_FORMAT_R16_UINT,					L"R16_UINT" },
			{ DXGI_FORMAT_R16_SNORM,				L"R16_SNORM" },
			{ DXGI_FORMAT_R16_SINT,					L"R16_SINT" },
			{ DXGI_FORMAT_R8_TYPELESS,				L"R8_TYPELESS" },
			{ DXGI_FORMAT_R8_UNORM,					L"R8_UNORM" },
			{ DXGI_FORMAT_R8_UINT,					L"R8_UINT" },
			{ DXGI_FORMAT_R8_SNORM,					L"R8_SNORM" },
			{ DXGI_FORMAT_R8_SINT,					L"R8_SINT" },
			{ DXGI_FORMAT_A8_UNORM,					L"A8_UNORM" },
			{ DXGI_FORMAT_R1_UNORM,					L"R1_UNORM" },
			{ DXGI_FORMAT_R9G9B9E5_SHAREDEXP,		L"R9G9B9E5_SHAREDEXP" },
			{ DXGI_FORMAT_R8G8_B8G8_UNORM,			L"R8G8_B8G8_UNORM" },
			{ DXGI_FORMAT_G8R8_G8B8_UNORM,			L"G8R8_G8B8_UNORM" },
			{ DXGI_FORMAT_BC1_TYPELESS,				L"BC1_TYPELESS" },
			{ DXGI_FORMAT_BC1_UNORM,				L"BC1_UNORM" },
			{ DXGI_FORMAT_BC1_UNORM_SRGB,			L"BC1_UNORM_SRGB" },
			{ DXGI_FORMAT_BC2_TYPELESS,				L"BC2_TYPELESS" },
			{ DXGI_FORMAT_BC2_UNORM,				L"BC2_UNORM" },
			{ DXGI_FORMAT_BC2_UNORM_SRGB,			L"BC2_UNORM_SRGB" },
			{ DXGI_FORMAT_BC3_TYPELESS,				L"BC3_TYPELESS" },
			{ DXGI_FORMAT_BC3_UNORM,				L"BC3_UNORM" },
			{ DXGI_FORMAT_BC3_UNORM_SRGB,			L"BC3_UNORM_SRGB" },
			{ DXGI_FORMAT_BC4_TYPELESS,				L"BC4_TYPELESS" },
			{ DXGI_FORMAT_BC4_UNORM,				L"BC4_UNORM" },
			{ DXGI_FORMAT_BC4_SNORM,				L"BC4_SNORM" },
			{ DXGI_FORMAT_BC5_TYPELESS,				L"BC5_TYPELESS" },
			{ DXGI_FORMAT_BC5_UNORM,				L"BC5_UNORM" },
			{ DXGI_FORMAT_BC5_SNORM,				L"BC5_SNORM" },
			{ DXGI_FORMAT_B5G6R5_UNORM,				L"B5G6R5_UNORM" },
			{ DXGI_FORMAT_B5G5R5A1_UNORM,			L"B5G5R5A1_UNORM" },
			{ DXGI_FORMAT_B8G8R8A8_UNORM,			L"B8G8R8A8_UNORM" },
			{ DXGI_FORMAT_B8G8R8X8_UNORM,			L"B8G8R8X8_UNORM" },
			{ DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,L"R10G10B10_XR_BIAS_A2_UNORM" },
			{ DXGI_FORMAT_B8G8R8A8_TYPELESS,		L"B8G8R8A8_TYPELESS" },
			{ DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,		L"B8G8R8A8_UNORM_SRGB" },
			{ DXGI_FORMAT_B8G8R8X8_TYPELESS,		L"B8G8R8X8_TYPELESS" },
			{ DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,		L"B8G8R8X8_UNORM_SRGB" },
			{ DXGI_FORMAT_BC6H_TYPELESS,			L"BC6H_TYPELESS" },
			{ DXGI_FORMAT_BC6H_UF16,				L"BC6H_UF16" },
			{ DXGI_FORMAT_BC6H_SF16,				L"BC6H_SF16" },
			{ DXGI_FORMAT_BC7_TYPELESS,				L"BC7_TYPELESS" },
			{ DXGI_FORMAT_BC7_UNORM,				L"BC7_UNORM" },
			{ DXGI_FORMAT_BC7_UNORM_SRGB,			L"BC7_UNORM_SRGB" },
			{ DXGI_FORMAT_AYUV,						L"AYUV" },
			{ DXGI_FORMAT_Y410,						L"Y410" },
			{ DXGI_FORMAT_Y416,						L"Y416" },
			{ DXGI_FORMAT_NV12,						L"NV12" },
			{ DXGI_FORMAT_P010,						L"P010" },
			{ DXGI_FORMAT_P016,						L"P016" },
			{ DXGI_FORMAT_420_OPAQUE,				L"420_OPAQUE" },
			{ DXGI_FORMAT_YUY2,						L"YUY2" },
			{ DXGI_FORMAT_Y210,						L"Y210" },
			{ DXGI_FORMAT_Y216,						L"Y216" },
			{ DXGI_FORMAT_NV11,						L"NV11" },
			{ DXGI_FORMAT_AI44,						L"AI44" },
			{ DXGI_FORMAT_IA44,						L"IA44" },
			{ DXGI_FORMAT_P8,						L"P8" },
			{ DXGI_FORMAT_A8P8,						L"A8P8" },
			{ DXGI_FORMAT_B4G4R4A4_UNORM,			L"B4G4R4A4_UNORM" },
		};
		size_t n = sizeof(captions) / sizeof(DXGI_FORMAT_caption);

		for (size_t x = 0; x < n; ++x)
		{
			if (captions[x].format == fmt)
			{
				return captions[x].caption;
			}
		}
		static const wchar_t* nill = L"UNKNOWN";
		return nill;
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

DDSFileInfoProvider::DDSFileInfoProvider() : m_cRef(1)
{
	InterlockedIncrement(&g_cDllRef);
}

DDSFileInfoProvider::~DDSFileInfoProvider()
{
	InterlockedDecrement(&g_cDllRef);
}

#pragma region IUnknown

// Query to the interface the component supported.
IFACEMETHODIMP DDSFileInfoProvider::QueryInterface(REFIID riid, void **ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(DDSFileInfoProvider, IQueryInfo),
		QITABENT(DDSFileInfoProvider, IPersistFile),
		{ 0 },
	};
	return QISearch(this, qit, riid, ppv);
}

// Increase the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) DDSFileInfoProvider::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

// Decrease the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) DDSFileInfoProvider::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);
	if (0 == cRef)
	{
		delete this;
	}
	return cRef;
}
#pragma endregion

#pragma region IPersistFile
IFACEMETHODIMP DDSFileInfoProvider::GetClassID(CLSID *pClassID)
{
	return E_NOTIMPL;
}

IFACEMETHODIMP DDSFileInfoProvider::IsDirty(void)
{
	return E_NOTIMPL;
}

IFACEMETHODIMP DDSFileInfoProvider::Load(LPCOLESTR pszFileName, DWORD dwMode)
{
	// pszFileName contains the absolute path of the file to be opened.
	return StringCchCopy(
		m_szSelectedFile,
		ARRAYSIZE(m_szSelectedFile),
		pszFileName);
}

IFACEMETHODIMP DDSFileInfoProvider::Save(LPCOLESTR pszFileName, BOOL fRemember)
{
	return E_NOTIMPL;
}

IFACEMETHODIMP DDSFileInfoProvider::SaveCompleted(LPCOLESTR pszFileName)
{
	return E_NOTIMPL;
}

IFACEMETHODIMP DDSFileInfoProvider::GetCurFile(LPOLESTR *ppszFileName)
{
	return E_NOTIMPL;
}
#pragma endregion


#pragma region IQueryInfo
IFACEMETHODIMP DDSFileInfoProvider::GetInfoTip(DWORD dwFlags, LPWSTR *ppwszTip)
{
	HRESULT hr;
	const int cch = MAX_PATH + 512;
	*ppwszTip = static_cast<LPWSTR>(CoTaskMemAlloc(cch * sizeof(wchar_t)));
	if (*ppwszTip == NULL)
	{
		return E_OUTOFMEMORY;
	}

	// Prepare the text of the infotip. The example infotip is composed of 
	// the file path and the count of code lines.
	size_t width = -1, height = -1;
	size_t mipLevels = 1;
	wchar_t szFormat[50] = L"N/A";
	{
		DirectX::TexMetadata info;
		DirectX::ScratchImage  scratchImage;

		hr = LoadFromDDSFile(m_szSelectedFile, DirectX::DDS_FLAGS_NONE, &info, scratchImage);

		if(SUCCEEDED(hr))
		{
			width  = info.width;
			height = info.height;
			mipLevels = info.mipLevels;
			StringCchPrintf(szFormat, 50, L"%s", DDSFormatString(info.format));
		}
	}

	if(width>0)
	{
		hr = StringCchPrintf(*ppwszTip, cch, L"File: %s\nResolution [%dx%d]\nFormat[%s]\nMipLevels[%d]\n",
			m_szSelectedFile, width, height, szFormat, mipLevels);
	}
	else
	{
		hr = StringCchPrintf(*ppwszTip, cch, L"File: %s\nInvalid .DDS file.\n", m_szSelectedFile);
	}
	if(FAILED(hr))
	{
		CoTaskMemFree(*ppwszTip);
	}

	return hr;
}

IFACEMETHODIMP DDSFileInfoProvider::GetInfoFlags(DWORD *pdwFlags)
{
	return E_NOTIMPL;
}
#pragma endregion

