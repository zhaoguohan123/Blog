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
void MyDebugPrintW(const wchar_t* lpcwszOutputString, ...)
{
    std::wstring strResult;
    if (NULL != lpcwszOutputString)
    {
        va_list marker = NULL;
        va_start(marker, lpcwszOutputString); //初始化变量参数
        size_t nLength = _vscwprintf(lpcwszOutputString, marker) + 1; //获取格式化字符串长度
        std::vector<wchar_t> vBuffer(nLength, '\0'); //创建用于存储格式化字符串的字符数组
        int nWritten = _vsnwprintf_s(&vBuffer[0], vBuffer.size(), nLength, lpcwszOutputString, marker);
        if (nWritten > 0)
        {
            strResult = &vBuffer[0];
        }
        va_end(marker); //重置变量参数
    }
    if (!strResult.empty())
    {
        std::wstring strFormated = L"[zgh]";
        strFormated.append(strResult);
        OutputDebugStringW(strFormated.c_str());
    }
}
