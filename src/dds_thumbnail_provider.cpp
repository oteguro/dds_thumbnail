// ----------------------------------------------------------------------------
// dds_thumbnail_provider.cpp 
// ----------------------------------------------------------------------------
#include "dds_thumbnail_provider.h"
#include <Shlwapi.h>
#include <gdiplus.h>

using namespace Gdiplus;

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

extern HINSTANCE g_hInst;
extern long      g_cDllRef;

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
IFACEMETHODIMP DDSThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
	*phbmp    = NULL;
	*pdwAlpha = WTSAT_UNKNOWN;

	ULONG_PTR token;
	GdiplusStartupInput input;
	if(Ok == GdiplusStartup(&token, &input, NULL))
	{
		Bitmap * bitmapInstance = new Bitmap(128, 128);
		if(bitmapInstance)
		{
			Color blk(0,0,0);
			bitmapInstance->GetHBITMAP(blk, phbmp);
		}
	}
	GdiplusShutdown(token);

	return (phbmp) ? S_OK : E_NOTIMPL;
}
#pragma endregion

