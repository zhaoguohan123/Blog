#include "VirtualFileSrcStream.h"

namespace clipboard {

#define FAKE_FILE_SIZE 0
//#define USE_HGLOBAL


	HRESULT STDMETHODCALLTYPE FileStream::QueryInterface(REFIID riid, void **ppvObject)
	{
		if (ppvObject == NULL)
			return E_INVALIDARG;

		*ppvObject = NULL; 

		if (IsEqualIID(IID_IUnknown, riid) ||
			IsEqualIID(IID_ISequentialStream, riid) ||
			IsEqualIID(IID_IStream, riid))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE FileStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
	{		
		HRESULT hr = S_FALSE;
		ULONG total_read = 0;
		ULONG bytes_read = 0;
		if (file_.is_directory) //目录不需要读文件
		{
			return S_OK;
		}
		while (total_read <= cb) {
			
			if (!ReadFile(pPipeServer_->m_hPipe, (char *)pv + total_read, cb - total_read, &bytes_read, NULL))
			{
				LOGGER_ERROR("ReadFile failed! err:{}", GetLastError());
				break;
			}
			if (bytes_read == 0) {
				// 没有更多数据可读
				LOGGER_ERROR("no more data to read!");
				break;
			}
			total_read += bytes_read;
		}
		if (pcbRead) {
			*pcbRead = total_read;
		}
#ifdef OLE_DEBUG
		LOGGER_INFO("FileStream total_read:{}",  total_read);
#endif // OLE_DEBUG

		return hr;
	}

	HRESULT STDMETHODCALLTYPE FileStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
	{
		DWORD move_method;
		switch (dwOrigin) {
		case STREAM_SEEK_SET: move_method = FILE_BEGIN; break;
		case STREAM_SEEK_CUR: move_method = FILE_CURRENT; break;
		case STREAM_SEEK_END: move_method = FILE_END; break;
		default: return E_INVALIDARG;
		}

		LARGE_INTEGER new_pos;
		if (SetFilePointerEx(file_handle_, dlibMove, &new_pos, move_method)) {
			if (plibNewPosition) {
				plibNewPosition->QuadPart = new_pos.QuadPart;
			}
			return S_OK;
		}
		return E_FAIL;
	}

	HRESULT WINAPI FileStream::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
	{
		memset(pstatstg, 0, sizeof(STATSTG));

		pstatstg->pwcsName = NULL;
		pstatstg->type = STGTY_STREAM;
		pstatstg->cbSize = file_.file_size;
		return S_OK;
	}

	//////////////////////////////////////////////////////
	VirtualFileSrcStream::~VirtualFileSrcStream()
	{
		/*for (auto i:file_streams_)
		{
			if (i)
			{
				i->Release();
				i = nullptr;
			}
		}*/

	}
	void VirtualFileSrcStream::init(std::shared_ptr<CFileInfo>& file_infos)
	{
		clip_format_filedesc_ = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
		clip_format_filecontent_ = RegisterClipboardFormat(CFSTR_FILECONTENTS); 
		file_infos_ = file_infos;
	}

	bool VirtualFileSrcStream::set_to_clipboard()
	{
		if (clip_format_filedesc_ == 0) {
			init(file_infos_);
		}
		return SUCCEEDED(::OleSetClipboard(this));
	}
	
	STDMETHODIMP VirtualFileSrcStream::GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium)
	{
		
		ZeroMemory(pmedium, sizeof(*pmedium));

		std::vector<FileInfoData> file_data = file_infos_->get_result();

		if (file_data.size() == 0)
		{
			LOGGER_ERROR("get file data from file_data failed!");
		}

		HRESULT hr = DATA_E_FORMATETC;

		// 文件描述符设置到剪切板中
		if (pformatetcIn->cfFormat == clip_format_filedesc_ 
			&& (pformatetcIn->tymed & TYMED_HGLOBAL))
		{
			uint32_t file_count = file_data.size();
			UINT cb = sizeof(FILEGROUPDESCRIPTOR) +	(file_count - 1) * sizeof(FILEDESCRIPTOR);
			HGLOBAL h = GlobalAlloc(GHND | GMEM_SHARE, cb);
			if (!h) 
				return E_OUTOFMEMORY;

			FILEGROUPDESCRIPTOR* pGroupDescriptor =	(FILEGROUPDESCRIPTOR*)::GlobalLock(h);
			if (pGroupDescriptor == NULL)
				return E_OUTOFMEMORY;

			pGroupDescriptor->cItems = file_count;
			FILEDESCRIPTOR* pFileDescriptorArray = (FILEDESCRIPTOR*)((LPBYTE)pGroupDescriptor + sizeof(UINT));
						
			for (uint32_t index = 0; index < file_count; ++index) {
				wcsncpy_s(pFileDescriptorArray[index].cFileName, _countof(pFileDescriptorArray[index].cFileName), file_data[index].relative_path.c_str(), _TRUNCATE);

				pFileDescriptorArray[index].dwFlags = FD_FILESIZE | FD_ATTRIBUTES | FD_CREATETIME | FD_WRITESTIME | FD_PROGRESSUI;
				pFileDescriptorArray[index].nFileSizeLow = file_data[index].file_size.LowPart;
				pFileDescriptorArray[index].nFileSizeHigh = file_data[index].file_size.HighPart;
				pFileDescriptorArray[index].dwFileAttributes = file_data[index].attributes; 
				pFileDescriptorArray[index].ftLastAccessTime = file_data[index].last_access_time;
				pFileDescriptorArray[index].ftCreationTime = file_data[index].creation_time;
				pFileDescriptorArray[index].ftLastWriteTime = file_data[index].last_write_time;
			}
			::GlobalUnlock(h);

			pmedium->hGlobal = h;
			pmedium->tymed = TYMED_HGLOBAL;
			hr = S_OK;

		}
		else if ( pformatetcIn->cfFormat == clip_format_filecontent_ && (pformatetcIn->tymed & TYMED_ISTREAM)) // 文件内容的处理
		{
#ifdef OLE_DEBUG
			LOGGER_INFO("[VirtualFileSrcStream::GetData]paste file data. index = {} name = {}", pformatetcIn->lindex, to_byte_string(file_data[pformatetcIn->lindex].name));
#endif // OLE_DEBUG
			DWORD index = pformatetcIn->lindex;
			
			if (index >= 0 && index < file_data.size())
			{
				if (file_streams_.size()<=index)
				{
					file_streams_.resize(index + 1);
				}
				
				if (file_streams_[index] == nullptr)
				{
					file_streams_[index] = new(std::nothrow) FileStream(file_data[index]);
				}
				else 
				{
					LARGE_INTEGER mov;
					mov.QuadPart = 0;
					file_streams_[index]->Seek(mov, STREAM_SEEK_SET, nullptr);
				}
			}

			pmedium->pstm = (IStream*)file_streams_[index];
			pmedium->pstm->AddRef();
			pmedium->tymed = TYMED_ISTREAM;
			hr = S_OK;
		}
		else if (SUCCEEDED(_EnsureShellDataObject()))
		{
			hr = _pdtobjShell->GetData(pformatetcIn, pmedium);
		}

		return hr;
	}


	STDMETHODIMP VirtualFileSrcStream::QueryGetData(FORMATETC *pformatetc)
	{
		HRESULT hr = S_FALSE;
		if (pformatetc->cfFormat == clip_format_filedesc_ ||
			pformatetc->cfFormat == clip_format_filecontent_)
		{
			hr = S_OK;
		}
		else if (SUCCEEDED(_EnsureShellDataObject()))
		{
			hr = _pdtobjShell->QueryGetData(pformatetc);
		}
		
		return hr;
	}

	STDMETHODIMP VirtualFileSrcStream::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc)
	{
		*ppenumFormatEtc = NULL;
		HRESULT hr = E_NOTIMPL;
		if (dwDirection == DATADIR_GET)
		{
			FORMATETC rgfmtetc[] =
			{
				// the order here defines the accuarcy of rendering
				{ clip_format_filedesc_, NULL, DVASPECT_CONTENT,  -1, TYMED_HGLOBAL },

				{ clip_format_filecontent_, NULL, DVASPECT_CONTENT,  -1, TYMED_ISTREAM },

			};
			hr = SHCreateStdEnumFmtEtc(ARRAYSIZE(rgfmtetc), rgfmtetc, ppenumFormatEtc);
		}
		return hr;
	}

	HRESULT STDMETHODCALLTYPE VirtualFileSrcStream::SetAsyncMode(
		/* [in] */ BOOL fDoOpAsync)
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE VirtualFileSrcStream::GetAsyncMode(
		/* [out] */ __RPC__out BOOL *pfIsOpAsync)
	{
		*pfIsOpAsync = true;// VARIANT_TRUE;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE VirtualFileSrcStream::StartOperation(
		/* [optional][unique][in] */ __RPC__in_opt IBindCtx *pbcReserved)
	{
		in_async_op_ = true;
		//IOperationsProgressDialog *pDlg = nullptr;
		//::CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER, IID_IOperationsProgressDialog, (LPVOID*)&pDlg);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE VirtualFileSrcStream::InOperation(
		/* [out] */ __RPC__out BOOL *pfInAsyncOp)
	{
		*pfInAsyncOp = in_async_op_;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE VirtualFileSrcStream::EndOperation(
		/* [in] */ HRESULT hResult,
		/* [unique][in] */ __RPC__in_opt IBindCtx *pbcReserved,
		/* [in] */ DWORD dwEffects)
	{
		for (auto i : file_streams_) 
		{
			if (i == NULL) {
				continue;
			}
			if (i->pPipeServer_ == NULL) {
				continue;
			}
			i->pPipeServer_->Cleanup();
		}
		in_async_op_ = true;
		return S_OK;
	}


	STDAPI VirtualFileSrcStream_CreateInstance(REFIID riid, void **ppv, std::shared_ptr<CFileInfo>& file_infos)
	{
		*ppv = NULL;
		VirtualFileSrcStream *p = new VirtualFileSrcStream();
		p->init(file_infos);
		HRESULT hr = p ? S_OK : E_OUTOFMEMORY;
		if (SUCCEEDED(hr))
		{
			hr = p->QueryInterface(riid, ppv);
			p->Release();
		}
		return hr;
	}
};