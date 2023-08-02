#ifndef _SET_DIR_ONLY_READ_
#define _SET_DIR_ONLY_READ_
#include "CommonHead.h"

namespace set_dir_only_read
{
    //使用方法SetFilesReadOnly(L"C:\\path\\to\\your\\directory");
    void SetFilesReadOnlyRecursive(const std::wstring & directory, bool isSetReadOnly) {
        DWORD attr = 0;
        if (isSetReadOnly)
        {
            attr = FILE_ATTRIBUTE_READONLY;
        }
        else
        {
            attr = FILE_ATTRIBUTE_NORMAL;
        }
        WIN32_FIND_DATA findFileData = { 0 };
        HANDLE hFind = NULL;
        hFind = FindFirstFile((directory + L"\\*").c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Error finding files in directory: " << directory << std::endl;
            return;
        }

        do {

            if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
            {
                std::wstring filePath = directory + L"\\" + findFileData.cFileName;
                if (!SetFileAttributes(filePath.c_str(), attr))
                {
                    std::wcout << L"file name path : ";
                    std::wcout << filePath.c_str() ;
                    std::cout << "SetFileAttributes : "<< "error: "<< GetLastError() << std::endl;
                }
            }
            else if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) 
            {
                std::wstring subDirectory = directory + L"\\" + findFileData.cFileName;
                SetFilesReadOnlyRecursive(subDirectory, isSetReadOnly);
            }
        } while (FindNextFile(hFind, &findFileData) != 0);

        if (hFind)
        {
            FindClose(hFind);
            hFind = NULL;
        }
       
    }
}
#endif