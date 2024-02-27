#include <windows.h>
#include <iostream>

using namespace std;
int main(int argc, char** argv)
{
    DWORD PID = GetCurrentProcessId();
    ULONG parameter = 5;

    while (true) {
        printf("Waiting to be hooked!: PID: %d\n", PID);
        SleepEx(1000, TRUE);
    }
    return (EXIT_SUCCESS);
}
