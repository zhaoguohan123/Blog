#ifndef _COMMONM_HEADf_
#define _COMMONM_HEADf_

#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <strsafe.h>

/* 函数说明：打印日志到DebugView
参数：
Format [in] 需要打印的字符串
返回值：无
*/

//多字符版
void MyDebugPrintA(const char* format, ...);

// 宽字符版
void MyDebugPrintW(const wchar_t *format, ...);

#ifdef _UNICODE
#define MyDebugPrint MyDebugPrintW
#else
#define MyDebugPrint MyDebugPrintA
#endif // _UNICODE



#endif