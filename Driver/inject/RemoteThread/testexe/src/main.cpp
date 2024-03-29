#include <iostream>
#include <windows.h>

using namespace std;

int main(int argc, char** argv)
{
    system("pause");

    ::MessageBoxW(NULL, L"Hello, World!", L"Test", MB_OK);

    system("pause");
    return 0;
}
