#ifndef _COMMONM_HEAD_
#define _COMMONM_HEAD_

#include <Windows.h>
#include <winternl.h>
#include <stdio.h>
#include <iostream>
#include <strsafe.h>

#include <vector>

/* ����˵������ӡ��־��DebugView
������
Format [in] ��Ҫ��ӡ���ַ���
����ֵ����
*/

//���ַ���
void MyDebugPrintA(const char* format, ...);

// ���ַ���
void MyDebugPrintW(const wchar_t *format, ...);

#ifdef _UNICODE
#define MyDebugPrint MyDebugPrintW
#else
#define MyDebugPrint MyDebugPrintA
#endif // _UNICODE



#endif