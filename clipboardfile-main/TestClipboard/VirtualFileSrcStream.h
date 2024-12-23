#pragma once
#include "framework.h"
#include "CFileInfo.h"
#include "DataObject.h"
#include "pipe_wrapper.h"

namespace clipboard {

	class FileStream : public IStream
	{
	public:

		FileStream(const FileInfoData & file_info)
			: ref_(1)
			, file_(file_info)
			, file_handle_(nullptr)
			, pPipeServer_(nullptr)
		{
			if (file_.is_directory) // 如果是目录则无需打开管道
			{
				return;
			}
			do
			{
				std::wstring pipeName = L"\\\\.\\pipe\\FileTransferPipe";
				pPipeServer_ = new PipeFileTransfer(pipeName + file_info.name);
				if (pPipeServer_ == nullptr) 
				{
					LOGGER_ERROR("create pPipeServer_ failed");
					break;
				}
				//创建管道
				if (!pPipeServer_->HelpCreateNamedPipe()) {
					LOGGER_ERROR("create pipe failed {}", GetLastError());
					break;
				}
				LOGGER_INFO("SetEvent g_clip_trans_file_data_begain");
				if(0 == SetEvent(g_clip_trans_file_data_begain.get()))
				{
					LOGGER_ERROR("SetEvent g_clip_trans_file_data_begain failed. errcode:{}",GetLastError());
					break;
				}

				// 连接到管道
				if (!pPipeServer_->ServerConnect()) {
					LOGGER_ERROR("connect pipe failed {}", GetLastError());
					break;
				}
				file_handle_ = pPipeServer_->m_hPipe;
				LOGGER_INFO("ServerConnect success!pipeName:{} ", U2A(pipeName + file_info.name));
			} while (FALSE);

		}

		virtual ~FileStream()
		{
			if (file_handle_)
			{
				LOGGER_INFO("~FileStream");
				CloseHandle(file_handle_);
				file_handle_ = NULL;
			}
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

		ULONG STDMETHODCALLTYPE AddRef()
		{
			return InterlockedIncrement(&ref_);
		}

		ULONG STDMETHODCALLTYPE Release()
		{
			ULONG newRef = InterlockedDecrement(&ref_);
			if (newRef == 0)
			{
				delete this;
			}

			return newRef;
		}

		virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*,
			ULARGE_INTEGER*)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE Commit(DWORD)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE Revert(void)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE Clone(IStream **)
		{
			return E_NOTIMPL;
		}

		virtual HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead);

		virtual HRESULT STDMETHODCALLTYPE Write(const void *pv, ULONG cb, ULONG *pcbWritten)
		{
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);

		virtual HRESULT WINAPI Stat(STATSTG *pstatstg, DWORD grfStatFlag);

		PipeFileTransfer* pPipeServer_;
		FileInfoData file_;
		HANDLE file_handle_;
	private:
		LONG ref_;
		//ULARGE_INTEGER file_size_;
		//ULARGE_INTEGER current_position_;
	};

	class VirtualFileSrcStream : public CDataObject
								, public IDataObjectAsyncCapability
	{
	public:
		VirtualFileSrcStream(){}
		~VirtualFileSrcStream();

		void init(std::shared_ptr<CFileInfo>& file_infos);

		bool set_to_clipboard();

		IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
		{
			if (IsEqualIID(IID_IDataObjectAsyncCapability, riid)) {
				*ppv = (IDataObjectAsyncCapability*)this;
				AddRef();
				return S_OK;
			}

			return CDataObject::QueryInterface(riid, ppv);
		}

		IFACEMETHODIMP_(ULONG) AddRef()
		{
			return CDataObject::AddRef();
		}

		IFACEMETHODIMP_(ULONG) Release()
		{
			return CDataObject::Release();
		}

		IFACEMETHODIMP GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium);

		IFACEMETHODIMP QueryGetData(FORMATETC *pformatetc);

		IFACEMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);

		// IDataObjectAsyncCapability
		virtual HRESULT STDMETHODCALLTYPE SetAsyncMode(
			/* [in] */ BOOL fDoOpAsync);

		virtual HRESULT STDMETHODCALLTYPE GetAsyncMode(
			/* [out] */ __RPC__out BOOL *pfIsOpAsync);

		virtual HRESULT STDMETHODCALLTYPE StartOperation(
			/* [optional][unique][in] */ __RPC__in_opt IBindCtx *pbcReserved);

		virtual HRESULT STDMETHODCALLTYPE InOperation(
			/* [out] */ __RPC__out BOOL *pfInAsyncOp);

		virtual HRESULT STDMETHODCALLTYPE EndOperation(
			/* [in] */ HRESULT hResult,
			/* [unique][in] */ __RPC__in_opt IBindCtx *pbcReserved,
			/* [in] */ DWORD dwEffects);

	private:
		uint32_t clip_format_filedesc_ = 0;
		uint32_t clip_format_filecontent_ = 0;
		BOOL	 in_async_op_ = true;

		std::vector<FileStream*> file_streams_;
		//FileStream* file_streams_;
		std::shared_ptr<CFileInfo> file_infos_;

	};

	STDAPI VirtualFileSrcStream_CreateInstance(REFIID riid, void **ppv , std::shared_ptr<CFileInfo>& file_infos);

};

