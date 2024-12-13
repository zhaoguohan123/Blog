#pragma once

#include "resource.h"

#include "framework.h"

shared_ptr_t<HANDLE> g_clip_share_memery;
shared_ptr_t<HANDLE> g_clip_mutex;
shared_ptr_t<HANDLE> g_clip_trans_file_info;
shared_ptr_t<HANDLE> g_clip_trans_file_data_begain;
shared_ptr_t<HANDLE> g_clip_trans_file_data_end;

std::wstring GetDownloadFolder();

LONG WINAPI UnhandledExceptionFilterFunc(struct _EXCEPTION_POINTERS* exception_info);


void CreateDumpFile(const std::string& dmp_file_path, PEXCEPTION_POINTERS excep, MINIDUMP_TYPE dmp_type);