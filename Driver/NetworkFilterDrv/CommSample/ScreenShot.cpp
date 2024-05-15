#include <ctime>
#include <sstream>
#include "ScreenShot.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Gdiplus.lib")

using namespace Gdiplus;
using namespace std;

// 生成唯一文件名
std::wstring GenerateUniqueFilename(const std::wstring& baseFilename, const std::wstring& fileExtension) 
{
    std::wstringstream ss;
    ss << baseFilename;

    // 获取当前时间戳
    std::time_t currentTime;
    std::time(&currentTime);
    ss << L"_" << currentTime;
    ss << fileExtension;
    return ss.str();
}

// 加密器Clsid
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return -1; // Failure
    }

    ImageCodecInfo* pImageCodecInfo = NULL;
    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) {
        return -1;
    }

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// 截图
void CapScreen(const wstring& titleName, OUT std::wstring& imagePath)
{
    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    HBITMAP hBitmapOld = (HBITMAP)SelectObject(hdcMemory, hBitmap);

    BitBlt(hdcMemory, 0, 0, screenWidth, screenHeight, hdcScreen, screenX, screenY, SRCCOPY);
    hBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmapOld);

    Gdiplus::Bitmap bmp(hBitmap, NULL);
    CLSID clsid;
    GetEncoderClsid(L"image/png", &clsid);     // 或者使用 "image/png" 来保存为PNG格式

    wstring prefixName = L".\\shot\\mail_" + titleName;
    imagePath = GenerateUniqueFilename(prefixName, L".png");
    bmp.Save(imagePath.c_str(), &clsid, NULL);
    
    DeleteObject(hBitmap);
    DeleteDC(hdcMemory);
    ReleaseDC(NULL, hdcScreen);
}

void MainCapScrren(const std::wstring& titleName,OUT std::wstring& imagePath)
{
    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    CapScreen(titleName, imagePath);

    GdiplusShutdown(gdiplusToken);
}