// ----------------------------------------------------------------------------
// dds_thumbnail_provider.cpp 
// ----------------------------------------------------------------------------
#include "dds_thumbnail_provider.h"
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

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

	return S_OK;
}
#pragma endregion

