#pragma once
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>

extern "C" _declspec(dllexport) BOOL _cdecl StoreData(DWORD dw);
extern "C" _declspec(dllexport) BOOL _cdecl GetData(DWORD * pdw);

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