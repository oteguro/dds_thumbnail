// ----------------------------------------------------------------------------
// ClassFactory.h 
// ----------------------------------------------------------------------------
#pragma once

#include <unknwn.h>     // For IClassFactory
#include <windows.h>


class ClassFactory : public IClassFactory
{
public:
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP_(ULONG) Release();

	// IClassFactory
	IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
	IFACEMETHODIMP LockServer(BOOL fLock);

	ClassFactory();

protected:
	~ClassFactory();

private:
	long m_cRef;
};

class ClassFactory2 : public IClassFactory
{
public:
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP_(ULONG) Release();

	// IClassFactory
	IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
	IFACEMETHODIMP LockServer(BOOL fLock);

	 ClassFactory2();

protected:
	~ClassFactory2();

private:
	long m_cRef;
};
