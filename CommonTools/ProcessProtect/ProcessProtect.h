#include <Windows.h>
#include <iostream>
#include <tchar.h>
#include <Psapi.h> // ��ȡ�����������ͷ�ļ�

int ProcessProtectTest();

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam);