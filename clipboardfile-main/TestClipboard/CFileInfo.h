#pragma once

#include "framework.h"

namespace clipboard
{
    typedef struct _FileInfoData_ {
        std::wstring name;
        std::wstring full_path;
        std::wstring relative_path; 
        DWORD attributes;
        FILETIME creation_time;
        FILETIME last_access_time;
        FILETIME last_write_time;
        ULARGE_INTEGER file_size;
        BOOL  is_directory;    
    }FileInfoData, * pFileInfoData;

    class CFileInfo
    {
    public:
        CFileInfo() {}
        ~CFileInfo() {}

        VOID count_file_info_with_folder_path(const std::wstring& current_path, const std::wstring& relative_path);

        VOID add_file_data_entry(const FileInfoData & file_data);

        std::vector<FileInfoData> get_result();

        VOID reset();
    private:
        
        std::vector<FileInfoData> ret;
    };
    
}