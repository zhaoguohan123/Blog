#ifndef _COMMONM_HEAD_
#define _COMMONM_HEAD_

#include <Windows.h>
#include <winternl.h>
#include <CommCtrl.h>
#include <TlHelp32.h> 
#include <tchar.h> 
#include <sstream>

#include <stdio.h>
#include <iostream>
#include <strsafe.h>
#include <vector>
#include <string>


#include "logger.h"
#include "json.hpp"    // nlohmann json解析库
#include "EncodingConversion.h"

//#include <boost/thread.hpp>
//#include <boost/bind.hpp>
//#include <boost/make_shared.hpp>

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