// ----------------------------------------------------------------------------
// dllmain.cpp 
// ----------------------------------------------------------------------------
#include <windows.h>
#include <guiddef.h>
#include <ShlObj.h>
#include "ClassFactory.h"
#include "Reg.h"

const CLSID CLSID_dds_thumbnail_provider =
{ 0xda0c8aa7, 0x9fb, 0x4790, { 0x81, 0xc5, 0xdd, 0x69, 0xe2, 0xe0, 0x76, 0xd7 } };


const CLSID CLSID_dds_fileinfo_provider =
{ 0xea3d8ea8, 0x7919, 0x4bba, { 0xab, 0xdf, 0xe6, 0xc4, 0xe1, 0x95, 0x8f, 0x38 } };


HINSTANCE g_hInst		= NULL;
long      g_cDllRef		= 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hInst = hModule;
		DisableThreadLibraryCalls(hModule);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
	HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

	if (IsEqualCLSID(CLSID_dds_thumbnail_provider, rclsid))
	{
		hr = E_OUTOFMEMORY;
		ClassFactory *pClassFactory = new ClassFactory();
		if (pClassFactory)
		{
			hr = pClassFactory->QueryInterface(riid, ppv);
			pClassFactory->Release();
		}
	}
	if (IsEqualCLSID(CLSID_dds_fileinfo_provider, rclsid))
	{
		hr = E_OUTOFMEMORY;
		ClassFactory2 *pClassFactory = new ClassFactory2();
		if (pClassFactory)
		{
			hr = pClassFactory->QueryInterface(riid, ppv);
			pClassFactory->Release();
		}
	}

	return hr;
}

STDAPI DllCanUnloadNow(void)
{
	return g_cDllRef > 0 ? S_FALSE : S_OK;
}

STDAPI DllRegisterServer(void)
{
	HRESULT hr;

	wchar_t szModule[MAX_PATH];
	if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	// Register the component.
	hr = RegisterInprocServer(szModule, CLSID_dds_thumbnail_provider,
		L"dds_thumbnail.DDSThumbnailProvider Class",
		L"Apartment");
	if (SUCCEEDED(hr))
	{
		// Register the thumbnail handler. The thumbnail handler is associated
		// with the .dds file class.
		hr = RegisterShellExtThumbnailHandler((L".dds"),
			CLSID_dds_thumbnail_provider);
		if (SUCCEEDED(hr))
		{
			// This tells the shell to invalidate the thumbnail cache. It is 
			// important because any .dds files viewed before registering 
			// this handler would otherwise show cached blank thumbnails.
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
		}
	}

	// Register the component.
	hr = RegisterInprocServer(szModule, CLSID_dds_fileinfo_provider,
		L"dds_fileinfo.DDSFileInfoProvider Class",
		L"Apartment");
	if (SUCCEEDED(hr))
	{
		hr = RegisterShellExtInfotipHandler(L".dds", CLSID_dds_fileinfo_provider);
	}

	return hr;
}

STDAPI DllUnregisterServer(void)
{
	HRESULT hr = S_OK;

	wchar_t szModule[MAX_PATH];
	if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	// Unregister the component.
	hr = UnregisterInprocServer(CLSID_dds_thumbnail_provider);
	if (SUCCEEDED(hr))
	{
		// Unregister the thumbnail handler.
		hr = UnregisterShellExtThumbnailHandler((L".dds"));
	}

	// Unregister the component.
	hr = UnregisterInprocServer(CLSID_dds_fileinfo_provider);
	if (SUCCEEDED(hr))
	{
		// Unregister the thumbnail handler.
		hr = UnregisterShellExtInfotipHandler((L".dds"));
	}

	return hr;
}

