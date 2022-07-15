#include "utils.h"

void DebugPrintA(const char* format, ...)
{
	if (format == NULL)
	{
		return;
	}
	char buffer[MAX_LOG_LEN] = { 0 };

	va_list ap;
	va_start(ap, format);
	(void)StringCchVPrintfA(buffer, _countof(buffer), format, ap);
	va_end(ap);
	StringCchCatA(buffer, MAX_LOG_LEN, "[cooper]");
	OutputDebugStringA(buffer);
}

// ¿í×Ö·û°æ
void DebugPrintW(const wchar_t *format, ...)
{
	if (NULL == format)
	{
		return;
	}
	wchar_t buffer[MAX_LOG_LEN] = { 0 };
	va_list ap;
	va_start(ap, format);
	(void)StringCchVPrintfW(buffer, _countof(buffer), format, ap);
	va_end(ap);
	StringCchCatW(buffer, MAX_LOG_LEN, L"[cooper]");
	OutputDebugStringW(buffer);
}