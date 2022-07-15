#pragma once

#include <stdio.h>
#include <windows.h>
#include <strsafe.h>

#define  MAX_LOG_LEN  1024 * 4

//ANSI->UNICODE 
#ifndef UNICODETOANSI
#define UNICODETOANSI_(x)	L##x
#define UNICODETOANSI(x)	UNICODETOANSI_(x)
#endif

void DebugPrintA(const char* format, ...);


// ¿í×Ö·û°æ
void DebugPrintW(const wchar_t *format, ...);
