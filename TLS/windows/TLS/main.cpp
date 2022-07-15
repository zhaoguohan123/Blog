#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
//#include "../TestDll/testdll.h"

#define  THREADCOUNT 4
#define DLL_NAME L"TestDll"


// Õ­×Ö·û°æ
void DebugPrintA(const char *format, ...)
{
	if (NULL == format)
	{
		return;
	}
	char buffer[1024] = { 0 };
	va_list ap;
	va_start(ap, format);
	(void)StringCchVPrintfA(buffer, _countof(buffer), format, ap);
	va_end(ap);
	OutputDebugStringA(buffer);
}

typedef BOOL(_cdecl  * PFUNC_StoreData)(DWORD);
typedef BOOL(_cdecl  * PFUNC_GetData)(DWORD *);

PFUNC_StoreData pfnStoreData = NULL;
PFUNC_GetData pfnGetData = NULL;

//extern "C" BOOL  StoreData(DWORD dw);
//extern "C" BOOL  GetData(DWORD *pdw);

DWORD WINAPI ThreadFunc(void)
{
	int i;

	if (!pfnStoreData(GetCurrentThreadId()))
	{
		DebugPrintA("store error");
	}
	for (i = 0; i < THREADCOUNT;i++)
	{
		DWORD dwOut;
		if (!pfnGetData(&dwOut))
		{
			DebugPrintA("Getdata error");
		}
		if (dwOut != GetCurrentThreadId())
		{
			DebugPrintA("thread %d: data is incorrect (%d)\n", GetCurrentThreadId(), dwOut);
		}
		else
		{
			DebugPrintA("thread %d: data is correct\n", GetCurrentThreadId());
		}
	}
	return 0;
}

int main(VOID)
{
	DWORD IDThread;
	HANDLE hThread[THREADCOUNT];
	int i;
	HMODULE hm;
	
	hm = LoadLibraryW(DLL_NAME);
	if (!hm)
	{
		DebugPrintA("dll load failed !!");
	}
	pfnStoreData = (PFUNC_StoreData)GetProcAddress(hm, "StoreData");
	if (pfnStoreData == NULL)
	{
		DebugPrintA(" load  pfnStoreData failed,errocode = %u !!",GetLastError());
		return 0;
	}

	pfnGetData = (PFUNC_GetData)GetProcAddress(hm, "GetData");
	if (pfnGetData == NULL)
	{
		DebugPrintA(" load  pfnGetData failed,errocode = %u !!", GetLastError());
		return 0;
	}
	for (i = 0; i < THREADCOUNT; i++) 
	{
		hThread[i] = CreateThread(NULL,
			0,
			(LPTHREAD_START_ROUTINE)ThreadFunc,
			NULL,
			0,
			&IDThread);
		if (hThread[i] == NULL)
		{
			DebugPrintA("create thread error \n ");
		}
	}

	WaitForMultipleObjects(THREADCOUNT, hThread, TRUE, INFINITE);

	FreeLibrary(hm);
	return 0;
}
