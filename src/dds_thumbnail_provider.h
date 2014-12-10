// ----------------------------------------------------------------------------
// dds_thumbnail_provider.h 
// ----------------------------------------------------------------------------
#pragma  once
#include <windows.h>
#include <shlobj.h>
#include <thumbcache.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

namespace DirectX
{
	class ScratchImage;
} // namespace DirectX 

class DDSThumbnailProvider
	: public IInitializeWithStream
	, public IThumbnailProvider
{
public:
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP_(ULONG) Release();

	// IInitializeWithStream
	IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

	// IThumbnailProvider
	IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha);

	 DDSThumbnailProvider();

protected:
	~DDSThumbnailProvider();

private:

	// Reference count of component.
	long m_cRef;

	// Provided during initialization.
	IStream *m_pStream;

	// The name of the selected file.
	wchar_t m_szSelectedFile[MAX_PATH];

	bool CreateHBITMAP_Image(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha, DirectX::ScratchImage& image);

	bool CreateHBITMAP_Fill (UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha, BYTE r, BYTE g, BYTE b, BYTE a = 0xff);

	size_t DDSFileSize(IStream* pStream);

}; // class DDSThumbnailProvider 

class DDSFileInfoProvider : public IPersistFile, public IQueryInfo
{
public:
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP_(ULONG) Release();

	// IPersistFile
	IFACEMETHODIMP GetClassID(CLSID *pClassID);
	IFACEMETHODIMP IsDirty(void);
	IFACEMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode);
	IFACEMETHODIMP Save(LPCOLESTR pszFileName, BOOL fRemember);
	IFACEMETHODIMP SaveCompleted(LPCOLESTR pszFileName);
	IFACEMETHODIMP GetCurFile(LPOLESTR *ppszFileName);

	// IQueryInfo
	IFACEMETHODIMP GetInfoTip(DWORD dwFlags, LPWSTR *ppwszTip);
	IFACEMETHODIMP GetInfoFlags(DWORD *pdwFlags);

	 DDSFileInfoProvider();

protected:
	~DDSFileInfoProvider();

private:
	// Reference count of component.
	long m_cRef;

	// The name of the selected file.
	wchar_t m_szSelectedFile[MAX_PATH];
};
