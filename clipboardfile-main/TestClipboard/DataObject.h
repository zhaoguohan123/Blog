#pragma once
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
// Sample data object implementation that demonstrates how to leverage the
// shell provided data object for the SetData() support

#include "framework.h"

namespace clipboard {

	class CDataObject : public IDataObject
	{
	public:
		CDataObject() : _cRef(1), _pdtobjShell(NULL)
		{
		}

		virtual ~CDataObject()
		{
			if (_pdtobjShell) {
				_pdtobjShell->Release();
			}
		}

		// IUnknown
		IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
		{
			static const QITAB qit[] = {
				QITABENT(CDataObject, IDataObject),
				{ 0 },
			};
			return QISearch(this, qit, riid, ppv);
		}

		IFACEMETHODIMP_(ULONG) AddRef()
		{
			return InterlockedIncrement(&_cRef);
		}

		IFACEMETHODIMP_(ULONG) Release()
		{
			long cRef = InterlockedDecrement(&_cRef);
			if (0 == cRef)
			{
				delete this;
			}
			return cRef;
		}

		// IDataObject
		IFACEMETHODIMP GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium);

		IFACEMETHODIMP GetDataHere(FORMATETC* /* pformatetc */, STGMEDIUM* /* pmedium */)
		{
			return DATA_E_FORMATETC;;
		}

		IFACEMETHODIMP QueryGetData(FORMATETC *pformatetc);

		IFACEMETHODIMP GetCanonicalFormatEtc(FORMATETC *pformatetcIn, FORMATETC *pFormatetcOut)
		{
			pformatetcIn->ptd = NULL;
			return E_NOTIMPL;
		}
		IFACEMETHODIMP SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease);
		IFACEMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);

		IFACEMETHODIMP DAdvise(FORMATETC* /* pformatetc */, DWORD /* advf */, IAdviseSink* /* pAdvSnk */, DWORD* /* pdwConnection */)
		{
			return E_NOTIMPL;
		}

		IFACEMETHODIMP DUnadvise(DWORD /* dwConnection */)
		{
			return E_NOTIMPL;
		}

		IFACEMETHODIMP EnumDAdvise(IEnumSTATDATA** /* ppenumAdvise */)
		{
			return E_NOTIMPL;
		}

	protected:
		HRESULT _EnsureShellDataObject()
		{
			// the shell data object imptlements ::SetData() in a way that will store any format
			// this code delegates to that implementation to avoid having to implement ::SetData()
			return _pdtobjShell ? S_OK : SHCreateDataObject(NULL, 0, NULL, NULL, IID_PPV_ARGS(&_pdtobjShell));
		}

		IDataObject *_pdtobjShell;

	private:

		long _cRef;
	};


	STDAPI CDataObject_CreateInstance(REFIID riid, void **ppv);

}	// namespace clipboard

