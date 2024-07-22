#include <Windows.h>
#include <iostream>
#include <tchar.h>
#include <Psapi.h> // 获取进程名所需的头文件

int ProcessProtectTest();

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam);