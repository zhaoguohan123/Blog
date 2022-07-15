#pragma once
#include "stdafx.h"

void DebugPrintA(const char* format, ...);

extern "C" BOOL WINAPI SetHooks();

extern "C" BOOL WINAPI RemoveHooks();

LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
