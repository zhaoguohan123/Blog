#include "CFileInfo.h"

namespace clipboard 
{
    VOID CFileInfo::count_file_info_with_folder_path(const std::wstring& current_path, const std::wstring& relative_path)
    {
        WIN32_FIND_DATA find_data = { 0 };

        HANDLE hFind = FindFirstFile((current_path + L"\\*.*").c_str(), &find_data);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            LOGGER_ERROR("FindFirstFile failed! errcode is{}", GetLastError());
            goto end;
        }

        do
        {
            if (wcscmp(find_data.cFileName, L".") != 0 && wcscmp(find_data.cFileName, L"..") != 0)
            {
                FileInfoData data;

                data.name = find_data.cFileName;
                data.full_path = current_path + L"\\" + find_data.cFileName;
                data.relative_path = relative_path.empty() ? find_data.cFileName : (relative_path + L"\\" + find_data.cFileName);
                data.attributes = find_data.dwFileAttributes;
                data.creation_time = find_data.ftCreationTime;
                data.last_access_time = find_data.ftLastAccessTime;
                data.last_write_time = find_data.ftLastWriteTime;
                data.file_size.HighPart = find_data.nFileSizeHigh;
                data.file_size.LowPart = find_data.nFileSizeLow;
                data.is_directory = ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

                add_file_data_entry(data);

                if ((find_data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY))
                    count_file_info_with_folder_path(data.full_path, data.relative_path);
            }
        } while (FindNextFile(hFind, &find_data) != 0);

    end:
        if (hFind)
        {
            FindClose(hFind);
        }
        return ;
    }

    VOID CFileInfo::add_file_data_entry(const FileInfoData & file_data)
    {
        ret.push_back(file_data);
    }

    std::vector<FileInfoData> CFileInfo::get_result()
    {
        return ret;
    }
    VOID CFileInfo::reset()
    {
        if (ret.empty())
        {
            return;
        }
        ret.clear();
    }
}