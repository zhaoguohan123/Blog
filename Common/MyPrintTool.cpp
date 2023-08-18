#include "CommonHead.h"

/* 函数说明：打印日志到DebugView
参数：
Format [in] 需要打印的字符串
返回值：无
*/
void MyDebugPrintA(const char* format, ...)
{
	if (format == NULL)
	{
		return;
	}
	char buffer[1024 * 4] = { "[ZGHTEST]" };

	va_list ap;
	va_start(ap, format);
	(void)StringCchVPrintfA(buffer + 9, _countof(buffer) - 9, format, ap);
	va_end(ap);

	OutputDebugStringA(buffer);
}

// 宽字符版
void MyDebugPrintW(const wchar_t *format, ...)
{
	if (NULL == format)
	{
		return;
	}
	wchar_t buffer[1024*4] = { L"[ZGHTEST]" };
	va_list ap;
	va_start(ap, format);
	(void)StringCchVPrintfW(buffer + 18, _countof(buffer) - 18, format, ap);
	va_end(ap);
	OutputDebugStringW(buffer);
}