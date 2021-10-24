#ifndef _COMMONM_HEADf_
#define _COMMONM_HEADf_

#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <strsafe.h>

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