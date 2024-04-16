#include <windows.h>
#include <iostream>

using namespace std;

// the exe for test inject
int main(int argc, char** argv)
{
    DWORD PID = GetCurrentProcessId();
    ULONG parameter = 5;

    while (true) {
        printf("Waiting to be hooked!: PID: %d\n", PID);
        MessageBoxW(NULL, L"main", L"Testmain", MB_OK);
        SleepEx(5000, TRUE);
    }
    return (EXIT_SUCCESS);
}
