#include "CommonHead.h"

/* ����˵������ӡ��־��DebugView
������
Format [in] ��Ҫ��ӡ���ַ���
����ֵ����
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

// ���ַ���
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