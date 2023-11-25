/*
   Copyright (C) 2009 Red Hat, Inc.

   This software is licensed under the GNU General Public License,
   version 2 (GPLv2) (see COPYING for details), subject to the
   following clarification.

   With respect to binaries built using the Microsoft(R) Windows
   Driver Kit (WDK), GPLv2 does not extend to any code contained in or
   derived from the WDK ("WDK Code").  As to WDK Code, by using or
   distributing such binaries you agree to be bound by the Microsoft
   Software License Terms for the WDK.  All WDK Code is considered by
   the GPLv2 licensors to qualify for the special exception stated in
   section 3 of GPLv2 (commonly known as the system library
   exception).

   There is NO WARRANTY for this software, express or implied,
   including the implied warranties of NON-INFRINGEMENT, TITLE,
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "stddef.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "os_dep.h"

#include "winerror.h"
#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"

#include "qxldd.h"
#include "utils.h"
#include "mspace.h"
#include "res.h"
#include "surface.h"

#define DEVICE_NAME L"qxldd"

#define QXLDD_DEBUG_PREFIX "qxldd: "

static DRVFN drv_calls[] = {
    {INDEX_DrvDisableDriver, (PFN)DrvDisableDriver},
    {INDEX_DrvEscape, (PFN)DrvEscape},
    {INDEX_DrvEnablePDEV, (PFN)DrvEnablePDEV},
    {INDEX_DrvDisablePDEV, (PFN)DrvDisablePDEV},
    {INDEX_DrvCompletePDEV, (PFN)DrvCompletePDEV},
    {INDEX_DrvEnableSurface, (PFN)DrvEnableSurface},
    {INDEX_DrvDisableSurface, (PFN)DrvDisableSurface},
    {INDEX_DrvAssertMode, (PFN)DrvAssertMode},
    {INDEX_DrvGetModes, (PFN)DrvGetModes},
    {INDEX_DrvSynchronize, (PFN)DrvSynchronize},
    {INDEX_DrvCopyBits, (PFN)DrvCopyBits},
    {INDEX_DrvBitBlt, (PFN)DrvBitBlt},
    {INDEX_DrvTextOut, (PFN)DrvTextOut},
    {INDEX_DrvStrokePath, (PFN)DrvStrokePath},
    {INDEX_DrvRealizeBrush, (PFN)DrvRealizeBrush},
    {INDEX_DrvSetPointerShape, (PFN)DrvSetPointerShape},
    {INDEX_DrvMovePointer, (PFN)DrvMovePointer},
    {INDEX_DrvStretchBlt, (PFN)DrvStretchBlt},
    {INDEX_DrvStretchBltROP, (PFN)DrvStretchBltROP},
    {INDEX_DrvTransparentBlt, (PFN)DrvTransparentBlt},
    {INDEX_DrvAlphaBlend, (PFN)DrvAlphaBlend},
    {INDEX_DrvCreateDeviceBitmap, (PFN)DrvCreateDeviceBitmap},
    {INDEX_DrvDeleteDeviceBitmap, (PFN)DrvDeleteDeviceBitmap},

#ifdef CALL_TEST
    {INDEX_DrvFillPath, (PFN)DrvFillPath},
    {INDEX_DrvGradientFill, (PFN)DrvGradientFill},
    {INDEX_DrvLineTo, (PFN)DrvLineTo},
    {INDEX_DrvPlgBlt, (PFN)DrvPlgBlt},
    {INDEX_DrvStrokeAndFillPath, (PFN)DrvStrokeAndFillPath},
#endif
};

#ifdef CALL_TEST

typedef struct CallCounter {
    const char *name;
    BOOL effective;
} CallCounterInfo;

static CallCounterInfo counters_info[NUM_CALL_COUNTERS] = {
    { "DrvCopyBits", FALSE},
    { "DrvBitBlt", TRUE},
    { "DrvTextOut", TRUE},
    { "DrvStrokePath", TRUE},
    { "DrvStretchBlt", FALSE},
    { "DrvStretchBltROP", TRUE},
    { "TransparentBlt", FALSE},
    { "DrvAlphaBlend", FALSE},

    { "DrvFillPath", FALSE},
    { "DrvGradientFill", FALSE},
    { "DrvLineTo", FALSE},
    { "DrvPlgBlt", FALSE},
    { "DrvStrokeAndFillPath", FALSE},
};

#endif

#define DBG_LEVEL 0

void DebugPrintV(PDev *pdev, const char *message, va_list ap)
{
    if (pdev && pdev->log_buf) {
        EngAcquireSemaphore(pdev->print_sem);
        _snprintf(pdev->log_buf, QXL_LOG_BUF_SIZE, QXLDD_DEBUG_PREFIX);
        _vsnprintf(pdev->log_buf + strlen(QXLDD_DEBUG_PREFIX),
                   QXL_LOG_BUF_SIZE - strlen(QXLDD_DEBUG_PREFIX), message, ap);
        sync_io(pdev, pdev->log_port, 0);
        EngReleaseSemaphore(pdev->print_sem);
    } else {
        EngDebugPrint(QXLDD_DEBUG_PREFIX, (PCHAR)message, ap);
    }
}

void DebugPrint(PDev *pdev, int level, const char *message, ...)
{
    va_list ap;

    if (level > (pdev && pdev->log_level ? (int)*pdev->log_level : DBG_LEVEL)) {
        return;
    }
    va_start(ap, message);
    DebugPrintV(pdev, message, ap);
    va_end(ap);
}

#define DRIVER_VERSION 1
#define OS_VERSION_MAJOR 5
#define OS_VERSION_MINOR 0
#define MK_GDIINFO_VERSION(os_major, os_minor, drv_vers) \
    ((drv_vers) | ((os_minor) << 8) | ((os_major) << 12))


GDIINFO gdi_default = {
    MK_GDIINFO_VERSION(OS_VERSION_MAJOR, OS_VERSION_MINOR, DRIVER_VERSION),
    DT_RASDISPLAY,
    0,                      //ulHorzSize
    0,                      //ulVertSize
    0,                      //ulHorzRes
    0,                      //ulVertRes
    0,                      //cBitsPixel
    0,                      //cPlanes
    0,                      //ulNumColors
    0,                      //flRaster
    0,                      //ulLogPixelsX
    0,                      //ulLogPixelsY
    TC_RA_ABLE,             //flTextCaps
    0,                      //ulDACRed
    0,                      //ulDACGreen
    0,                      //ulDACBlue
    0x0024,                 //ulAspectX
    0x0024,                 //ulAspectY
    0x0033,                 //ulAspectXY
    1,                      //xStyleStep
    1,                      //yStyleSte;
    3,                      //denStyleStep
    { 0, 0},               //ptlPhysOffset
    { 0, 0},               //szlPhysSize
    0,                      //ulNumPalReg

    {                       //ciDevice
        { 6700, 3300, 0},  //Red
        { 2100, 7100, 0},  //Green
        { 1400,  800, 0},  //Blue
        { 1750, 3950, 0},  //Cyan
        { 4050, 2050, 0},  //Magenta
        { 4400, 5200, 0},  //Yellow
        { 3127, 3290, 0},  //AlignmentWhite
        20000,              //RedGamma
        20000,              //GreenGamma
        20000,              //BlueGamma
        0, 0, 0, 0, 0, 0    //No dye correction for raster displays
    },

    0,                      //ulDevicePelsDPI
    PRIMARY_ORDER_CBA,      //ulPrimaryOrder
    HT_PATSIZE_4x4_M,       //ulHTPatternSize
    HT_FORMAT_8BPP,         //ulHTOutputFormat
    HT_FLAG_ADDITIVE_PRIMS, //flHTFlags
    0,                      //ulVRefresh
    0,                      //ulPanningHorzRes
    0,                      //ulPanningVertRes
    0,                      //ulBltAlignment
    //more
};

#define SYSTM_LOGFONT {16, 7, 0, 0, 700, 0, 0, 0,ANSI_CHARSET, OUT_DEFAULT_PRECIS,\
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,\
                       VARIABLE_PITCH | FF_DONTCARE, L"System"}
#define HELVE_LOGFONT {12, 9, 0, 0, 400, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,\
                       CLIP_STROKE_PRECIS, PROOF_QUALITY,\
                       VARIABLE_PITCH | FF_DONTCARE, L"MS Sans Serif"}
#define COURI_LOGFONT {12, 9, 0, 0, 400, 0, 0, 0, ANSI_CHARSET,OUT_DEFAULT_PRECIS,\
                       CLIP_STROKE_PRECIS,PROOF_QUALITY,\
                       FIXED_PITCH | FF_DONTCARE, L"Courier"}

DEVINFO dev_default = {
    GCAPS_ARBRUSHOPAQUE | GCAPS_ARBRUSHTEXT | GCAPS_ASYNCMOVE | /* GCAPS_BEZIERS | */
    GCAPS_GRAY16 | GCAPS_OPAQUERECT |
    GCAPS_WINDINGFILL /*| GCAPS_LAYERED*/,
    SYSTM_LOGFONT,      //lfDefaultFont
    HELVE_LOGFONT,      //lfAnsiVarFont
    COURI_LOGFONT,      //lfAnsiFixFont
    0,                  //cFonts
    0,                  //iDitherFormat
    0,                  //cxDither
    0,                  //cyDither
    0,                  //hpalDefault
#if (WINVER >= 0x0501)
    GCAPS2_MOUSETRAILS |
#endif
    GCAPS2_ALPHACURSOR,
};

static BOOL PrepareHardware(PDev *pdev);

static void mspace_print(void *user_data, char *format, ...)
{
    PDev *pdev = (PDev *)user_data;
    va_list ap;

    va_start(ap, format);
    DebugPrintV(pdev, format, ap);
    va_end(ap);
}

static void mspace_abort(void *user_data)
{
    mspace_print(user_data, "mspace abort");
    EngDebugBreak();
}

BOOL DrvEnableDriver(ULONG engine_version, ULONG enable_data_size, PDRVENABLEDATA enable_data)
{
    DEBUG_PRINT((NULL, 1, "%s\n", __FUNCTION__));
    enable_data->iDriverVersion = DDI_DRIVER_VERSION_NT5;
    enable_data->c = sizeof(drv_calls) / sizeof(DRVFN);
    enable_data->pdrvfn = drv_calls;
    mspace_set_abort_func(mspace_abort);
    mspace_set_print_func(mspace_print);
    ResInitGlobals();
#ifndef _WIN64
    CheckAndSetSSE2();
#endif
    DEBUG_PRINT((NULL, 1, "%s: end\n", __FUNCTION__));
    return TRUE;
}

static void SetMonitorConfig(PDev *pdev,    QXLHead * esc_config)
{
    QXLHead             *heads;

    pdev->monitor_config->count = 1;
    pdev->monitor_config->max_allowed = 1;

    heads = &pdev->monitor_config->heads[0];
    heads->id = 0;
    heads->surface_id = 0;
    heads->x = esc_config->x;
    heads->y = esc_config->y;
    heads->width = esc_config->width;
    heads->height = esc_config->height;
    DEBUG_PRINT((pdev, 2, "%s Monitor %d (%d, %d) (%d x %d)\n",
        __FUNCTION__, pdev->dev_id, heads->x, heads->y, heads->width, heads->height));
    async_io(pdev, ASYNCABLE_MONITOR_CONFIG, 0);
}

ULONG DrvEscape(SURFOBJ *pso, ULONG iEsc, ULONG cjIn, PVOID pvIn,
                ULONG cjOut, PVOID pvOut)
{
    PDev* pdev = pso ? (PDev*)pso->dhpdev : NULL;
    int RetVal = -1;

    switch (iEsc) {
    case QXL_ESCAPE_SET_CUSTOM_DISPLAY: {
        ULONG length;

        DEBUG_PRINT((pdev, 1, "set custom display %p\n", pdev));
        if (pdev == NULL)
            break;

        if (EngDeviceIoControl(pdev->driver, IOCTL_QXL_SET_CUSTOM_DISPLAY,
                               pvIn, cjIn, NULL, 0, &length)) {
            DEBUG_PRINT((pdev, 0, "%s: IOCTL_QXL_SET_CUSTOM_DISPLAY failed\n", __FUNCTION__));
            break;
        }
        RetVal = 1;
        break;
    }
    case QXL_ESCAPE_MONITOR_CONFIG: {
        ULONG length;
        DEBUG_PRINT((pdev, 2, "%s - 0x%p \n", __FUNCTION__, pdev));
        if (pdev == NULL || cjIn != sizeof(QXLHead))
            break;

        SetMonitorConfig(pdev, (QXLHead * )pvIn);
        RetVal = 1;
        break;
    }
    default:
        DEBUG_PRINT((NULL, 1, "%s: unhandled escape code %d\n", __FUNCTION__, iEsc));
        RetVal = 0;
    }

    DEBUG_PRINT((NULL, 1, "%s: end\n", __FUNCTION__));
    return RetVal;
}

VOID DrvDisableDriver(VOID)
{
    DEBUG_PRINT((NULL, 1, "%s\n", __FUNCTION__));
    ResDestroyGlobals();
}

DWORD GetAvailableModes(HANDLE driver, PVIDEO_MODE_INFORMATION *mode_info,
                        DWORD *mode_info_size)
{
    ULONG n;
    VIDEO_NUM_MODES modes;
    PVIDEO_MODE_INFORMATION info;

    DEBUG_PRINT((NULL, 1, "%s\n", __FUNCTION__));

    if (EngDeviceIoControl(driver, IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES, NULL, 0,
                           &modes, sizeof(VIDEO_NUM_MODES), &n)) {
        DEBUG_PRINT((NULL, 0, "%s: query num modes failed\n", __FUNCTION__));
        return 0;
    }

    info = (PVIDEO_MODE_INFORMATION)EngAllocMem(FL_ZERO_MEMORY,
                                                modes.NumModes * modes.ModeInformationLength,
                                                ALLOC_TAG);
    if (!info) {
        DEBUG_PRINT((NULL, 0, "%s: memory allocation failed\n", __FUNCTION__));
        return 0;
    }

    if (EngDeviceIoControl(driver, IOCTL_VIDEO_QUERY_AVAIL_MODES, NULL, 0, info,
                           modes.NumModes * modes.ModeInformationLength, &n)) {
        DEBUG_PRINT((NULL, 0, "%s: query modes failed\n", __FUNCTION__));
        EngFreeMem(info);
        return 0;
    }

    *mode_info = info;
    *mode_info_size = modes.ModeInformationLength;

    n = modes.NumModes;
    while ( n-- ) {
        if ( (info->NumberOfPlanes != 1 ) ||!(info->AttributeFlags & VIDEO_MODE_GRAPHICS)
             ||((info->BitsPerPlane != 16) && (info->BitsPerPlane != 32))) {

            DEBUG_PRINT((NULL, 1, "%s: unsuported mode rejecting miniport mode\n",  __FUNCTION__));
            DEBUG_PRINT((NULL, 1, "                   width = %li height = %li\n",
                         info->VisScreenWidth, info->VisScreenHeight));
            DEBUG_PRINT((NULL, 1, "                   bpp = %li freq = %li\n",
                         info->BitsPerPlane * info->NumberOfPlanes, info->Frequency));
            info->Length = 0;
        }

        info = (PVIDEO_MODE_INFORMATION) (((PUCHAR)info) + modes.ModeInformationLength);
    }
    DEBUG_PRINT((NULL, 1, "%s: OK num modes %lu\n", __FUNCTION__, modes.NumModes));
    return modes.NumModes;
}

BOOL InitializeModeFields(PDev *pdev, GDIINFO *gdi_info, DEVINFO *dev_info,
                          DEVMODEW *dev_mode)
{
    ULONG n_modes;
    PVIDEO_MODE_INFORMATION video_buff;
    PVIDEO_MODE_INFORMATION selected_mode;
    PVIDEO_MODE_INFORMATION video_mode;
    VIDEO_MODE_INFORMATION vmi;
    ULONG video_mode_size;

    DEBUG_PRINT((NULL, 1, "%s\n", __FUNCTION__));

    n_modes = GetAvailableModes(pdev->driver, &video_buff, &video_mode_size);
    if ( n_modes == 0 ) {
        DEBUG_PRINT((NULL, 0, "%s: get available modes failed\n", __FUNCTION__));
        return FALSE;
    }

#if (WINVER < 0x0501)
    DEBUG_PRINT((NULL, 1, "%s: req mode: fields %u bits %u w %u h %u frequency %u\n",
                 __FUNCTION__,
                 dev_mode->dmFields,
                 dev_mode->dmBitsPerPel,
                 dev_mode->dmPelsWidth,
                 dev_mode->dmPelsHeight,
                 dev_mode->dmDisplayFrequency));
#else
    DEBUG_PRINT((NULL, 1, "%s: req mode: fields %u bits %u w %u h %u frequency %u orientation %u\n",
                 __FUNCTION__,
                 dev_mode->dmFields,
                 dev_mode->dmBitsPerPel,
                 dev_mode->dmPelsWidth,
                 dev_mode->dmPelsHeight,
                 dev_mode->dmDisplayFrequency,
                 dev_mode->dmDisplayOrientation));
#endif


    selected_mode = NULL;
    video_mode = video_buff;

    while (n_modes--) {
        if ( video_mode->Length != 0 ) {
            DEBUG_PRINT((NULL, 1, "%s: check width = %li height = %li\n",
                         __FUNCTION__,
                         video_mode->VisScreenWidth,
                         video_mode->VisScreenHeight));
            DEBUG_PRINT((NULL, 1, "                             bpp = %li freq = %li\n",
                         video_mode->BitsPerPlane * video_mode->NumberOfPlanes,
                         video_mode->Frequency));

            if ( (video_mode->VisScreenWidth  == dev_mode->dmPelsWidth)
                 && (video_mode->VisScreenHeight == dev_mode->dmPelsHeight)
                 && (video_mode->BitsPerPlane * video_mode->NumberOfPlanes
                     == dev_mode->dmBitsPerPel)
                 && (video_mode->Frequency == dev_mode->dmDisplayFrequency)
#if (WINVER >= 0x0501)
                 && (video_mode->DriverSpecificAttributeFlags
                     == dev_mode->dmDisplayOrientation)
#endif
            ) {
                selected_mode = video_mode;
                DEBUG_PRINT((NULL, 1, "%s: found\n", __FUNCTION__));
                break;
            }
        }
        video_mode = (PVIDEO_MODE_INFORMATION)(((PUCHAR)video_mode) + video_mode_size);
    }

    if (!selected_mode) {
        DEBUG_PRINT((NULL, 0, "%s: not found\n"));
        EngFreeMem(video_buff);
        return FALSE;
    }

    vmi = *selected_mode;
    EngFreeMem(video_buff);

    pdev->video_mode_index = vmi.ModeIndex;
    pdev->resolution.cx = vmi.VisScreenWidth;
    pdev->resolution.cy = vmi.VisScreenHeight;
    pdev->max_bitmap_size = pdev->resolution.cx * pdev->resolution.cy;
    pdev->max_bitmap_size += pdev->max_bitmap_size / 2;
    pdev->stride = vmi.ScreenStride;

    *gdi_info = gdi_default;

    gdi_info->ulHorzSize = vmi.XMillimeter;
    gdi_info->ulVertSize = vmi.YMillimeter;
    gdi_info->ulHorzRes = vmi.VisScreenWidth;
    gdi_info->ulVertRes = vmi.VisScreenHeight;
    gdi_info->cBitsPixel = vmi.BitsPerPlane;
    gdi_info->cPlanes = vmi.NumberOfPlanes;
    gdi_info->ulVRefresh = vmi.Frequency;
    gdi_info->ulDACRed = vmi.NumberRedBits;
    gdi_info->ulDACGreen = vmi.NumberGreenBits;
    gdi_info->ulDACBlue = vmi.NumberBlueBits;
    gdi_info->ulLogPixelsX = dev_mode->dmLogPixels;
    gdi_info->ulLogPixelsY = dev_mode->dmLogPixels;

    *dev_info = dev_default;

    switch ( vmi.BitsPerPlane ) {
    case 16:
        pdev->bitmap_format = BMF_16BPP;
        pdev->red_mask = vmi.RedMask;
        pdev->green_mask = vmi.GreenMask;
        pdev->blue_mask = vmi.BlueMask;

        gdi_info->ulNumColors = (ULONG)-1;
        gdi_info->ulNumPalReg = 0;
        gdi_info->ulHTOutputFormat = HT_FORMAT_16BPP;

        dev_info->iDitherFormat = BMF_16BPP;
        break;
    case 32:
        pdev->bitmap_format = BMF_32BPP;
        pdev->red_mask = vmi.RedMask;
        pdev->green_mask = vmi.GreenMask;
        pdev->blue_mask = vmi.BlueMask;

        gdi_info->ulNumColors = (ULONG)-1;
        gdi_info->ulNumPalReg = 0;
        gdi_info->ulHTOutputFormat = HT_FORMAT_32BPP;

        dev_info->iDitherFormat = BMF_32BPP;
        break;
    default:
        DEBUG_PRINT((NULL, 0, "%s: bit depth not supported\n", __FUNCTION__));
        return FALSE;
    }
    DEBUG_PRINT((NULL, 1, "%s: exit\n", __FUNCTION__));
    return TRUE;
}

void DestroyPalette(PDev *pdev)
{
    DEBUG_PRINT((NULL, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));
    if (pdev->palette) {
        EngDeletePalette(pdev->palette);
        pdev->palette = NULL;
    }
    DEBUG_PRINT((NULL, 1, "%s: 0x%lx exit\n", __FUNCTION__, pdev));
}

BOOL InitPalette(PDev *pdev, DEVINFO *dev_info)
{
    DEBUG_PRINT((NULL, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));

    if ((pdev->palette = EngCreatePalette(PAL_BITFIELDS, 0, NULL,
                                          pdev->red_mask,
                                          pdev->green_mask,
                                          pdev->blue_mask)) == NULL) {
        DEBUG_PRINT((NULL, 0, "%s: create palette failed\n", __FUNCTION__));
        return FALSE;
    }
    dev_info->hpalDefault = pdev->palette;

    DEBUG_PRINT((NULL, 1, "%s: OK\n", __FUNCTION__));
    return TRUE;
}

DHPDEV DrvEnablePDEV(DEVMODEW *dev_mode, PWSTR ignore1, ULONG ignore2, HSURF *ignore3,
                     ULONG dev_caps_size, ULONG *dev_caps, ULONG dev_inf_size,
                     DEVINFO *in_dev_info, HDEV gdi_dev, PWSTR device_name, HANDLE driver)
{
    PDev *pdev;
    GDIINFO gdi_info;
    DEVINFO dev_info;

    DEBUG_PRINT((NULL, 1, "%s\n", __FUNCTION__));

    if (!(pdev = (PDev*)EngAllocMem(FL_ZERO_MEMORY, sizeof(PDev), ALLOC_TAG))) {
        DEBUG_PRINT((NULL, 0, "%s: pdev alloc failed\n", __FUNCTION__));
        return NULL;
    }

    RtlZeroMemory(&gdi_info, sizeof(GDIINFO));
    RtlCopyMemory(&gdi_info, dev_caps, MIN(dev_caps_size, sizeof(GDIINFO)));

    RtlZeroMemory(&dev_info, sizeof(DEVINFO));
    RtlCopyMemory(&dev_info, in_dev_info, MIN(dev_inf_size, sizeof(DEVINFO)));

    pdev->driver = driver;

    if (!InitializeModeFields(pdev, &gdi_info, &dev_info, dev_mode)) {
        DEBUG_PRINT((NULL, 0, "%s: init mode failed\n", __FUNCTION__));
        goto err1;
    }

    if (!InitPalette(pdev, &dev_info)) {
        DEBUG_PRINT((NULL, 0, "%s: init palet failed\n", __FUNCTION__));
        goto err1;
    }

    if (!ResInit(pdev)) {
        DEBUG_PRINT((NULL, 0, "%s: init res failed\n", __FUNCTION__));
        goto err2;
    }

    RtlCopyMemory(dev_caps, &gdi_info, dev_caps_size);
    RtlCopyMemory(in_dev_info, &dev_info, dev_inf_size);

    pdev->enabled = TRUE; /* assume no operations before a DrvEnablePDEV. */
    DEBUG_PRINT((NULL, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));
    return(DHPDEV)pdev;

err2:
    DestroyPalette(pdev);

err1:
    EngFreeMem(pdev);

    return NULL;
}

#ifdef DBG
static void DebugCountAliveSurfaces(PDev *pdev)
{
    UINT32 i;
    SurfaceInfo *surface_info;
    int total = 0;
    int of_pdev = 0;
    int no_surf_obj = 0;

    for (i = 0 ; i < pdev->n_surfaces; ++i) {
        surface_info = GetSurfaceInfo(pdev, i);
        if (surface_info->draw_area.base_mem != NULL) {
            total++;
            // all should belong to the same pdev
            if (surface_info->u.pdev == pdev) {
                of_pdev++;
                if (surface_info->draw_area.surf_obj == NULL) {
                    no_surf_obj++;
                }
            }
        }
    }
    DEBUG_PRINT((pdev, 1, "%s: %p: %d / %d / %d (total,pdev,no_surf_obj)\n", __FUNCTION__, pdev,
                total, of_pdev, no_surf_obj));
}
#else
static void DebugCountAliveSurfaces(PDev *pdev)
{
}
#endif

VOID DrvDisablePDEV(DHPDEV in_pdev)
{
    PDev* pdev = (PDev*)in_pdev;

    DEBUG_PRINT((pdev, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));
    ResDestroy(pdev);
    DestroyPalette(pdev);
    EngFreeMem(pdev);
    DEBUG_PRINT((NULL, 1, "%s: 0x%lx exit\n", __FUNCTION__, pdev));
}

VOID DrvCompletePDEV(DHPDEV in_pdev, HDEV gdi_dev)
{
    PDev* pdev = (PDev*)in_pdev;

    DEBUG_PRINT((NULL, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));
    pdev->eng = gdi_dev;
    DEBUG_PRINT((NULL, 1, "%s: 0x%lx exit\n", __FUNCTION__, pdev));
}

static VOID HideMouse(PDev *pdev)
{
    QXLCursorCmd *cursor_cmd;

    cursor_cmd = CursorCmd(pdev);
    cursor_cmd->type = QXL_CURSOR_HIDE;

    PushCursorCmd(pdev, cursor_cmd);
}

static VOID CreatePrimarySurface(PDev *pdev, UINT32 depth, UINT32 format,
                                 UINT32 width, UINT32 height, INT32 stride,
                                 QXLPHYSICAL phys_mem)
{
    pdev->primary_surface_create->format = format;
    pdev->primary_surface_create->width = width;
    pdev->primary_surface_create->height = height;
    pdev->primary_surface_create->stride = -stride;
    pdev->primary_surface_create->mem = phys_mem;

    pdev->primary_surface_create->flags = 0;
    pdev->primary_surface_create->type = QXL_SURF_TYPE_PRIMARY;

    async_io(pdev, ASYNCABLE_CREATE_PRIMARY, 0);
}

static void DestroyPrimarySurface(PDev *pdev, int hide_mouse)
{
    if (hide_mouse) {
        HideMouse(pdev);
    }
    async_io(pdev, ASYNCABLE_DESTROY_PRIMARY, 0);
}

static void DestroyAllSurfaces(PDev *pdev)
{
    HideMouse(pdev);
    async_io(pdev, ASYNCABLE_DESTROY_ALL_SURFACES, 0);
}

BOOL SetHardwareMode(PDev *pdev)
{
    VIDEO_MODE_INFORMATION video_info;
    DWORD length;

    DEBUG_PRINT((NULL, 1, "%s: 0x%lx mode %lu\n", __FUNCTION__, pdev, pdev->video_mode_index));

    if (EngDeviceIoControl(pdev->driver, IOCTL_VIDEO_SET_CURRENT_MODE,
                           &pdev->video_mode_index, sizeof(DWORD),
                           NULL, 0, &length)) {
        DEBUG_PRINT((NULL, 0, "%s: set mode failed, 0x%lx\n", __FUNCTION__, pdev));
        return FALSE;
    }

    DEBUG_PRINT((NULL, 1, "%s: 0x%lx OK\n", __FUNCTION__, pdev));
    return TRUE;
}

static VOID UpdateMainSlot(PDev *pdev, MemSlot *slot)
{
    QXLPHYSICAL high_bits;


    pdev->mem_slots[pdev->main_mem_slot].slot = *slot;

    high_bits = pdev->main_mem_slot << pdev->slot_gen_bits;
    high_bits |= slot->generation;
    high_bits <<= (64 - (pdev->slot_gen_bits + pdev->slot_id_bits));
    pdev->mem_slots[pdev->main_mem_slot].high_bits = high_bits;

    pdev->va_slot_mask = (~(QXLPHYSICAL)0) >> (pdev->slot_id_bits + pdev->slot_gen_bits);
}

static void RemoveVRamSlot(PDev *pdev)
{
    sync_io(pdev, pdev->memslot_del_port, pdev->vram_mem_slot);
    pdev->vram_slot_initialized = FALSE;
}

static BOOLEAN CreateVRamSlot(PDev *pdev)
{
    QXLMemSlot *slot;
    UINT64 high_bits;
    UINT8 slot_id = pdev->main_mem_slot + 1;

    if (slot_id >= pdev->num_mem_slot) {
        return FALSE;
    }

    pdev->va_slot_mask = (~(QXLPHYSICAL)0) >> (pdev->slot_id_bits + pdev->slot_gen_bits);


    *pdev->ram_slot_start = pdev->fb_phys;
    *pdev->ram_slot_end = pdev->fb_phys + pdev->fb_size;

    async_io(pdev, ASYNCABLE_MEMSLOT_ADD, slot_id);

    pdev->vram_mem_slot = slot_id;

    pdev->mem_slots[slot_id].slot.generation = *pdev->slots_generation;
    pdev->mem_slots[slot_id].slot.start_phys_addr = pdev->fb_phys;
    pdev->mem_slots[slot_id].slot.end_phys_addr = pdev->fb_phys + pdev->fb_size;
    pdev->mem_slots[slot_id].slot.start_virt_addr = (UINT64)pdev->fb;
    pdev->mem_slots[slot_id].slot.end_virt_addr = (UINT64)pdev->fb + pdev->fb_size;

    high_bits = slot_id << pdev->slot_gen_bits;
    high_bits |= pdev->mem_slots[slot_id].slot.generation;
    high_bits <<= (64 - (pdev->slot_gen_bits + pdev->slot_id_bits));
    pdev->mem_slots[slot_id].high_bits = high_bits;

    pdev->vram_slot_initialized = TRUE;

    return TRUE;
}

static BOOL PrepareHardware(PDev *pdev)
{
    VIDEO_MEMORY video_mem;
    VIDEO_MEMORY_INFORMATION video_mem_Info;
    DWORD length;
    QXLDriverInfo dev_info;
    QXLPHYSICAL high_bits;

    DEBUG_PRINT((pdev, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));

    if (!SetHardwareMode(pdev)) {
        DEBUG_PRINT((pdev, 0, "%s: set mode failed, 0x%lx\n", __FUNCTION__, pdev));
        return FALSE;
    }

    if (EngDeviceIoControl( pdev->driver, IOCTL_QXL_GET_INFO, NULL,
                            0, &dev_info, sizeof(QXLDriverInfo), &length) ) {
        DEBUG_PRINT((pdev, 0, "%s: get qxl info failed, 0x%lx\n", __FUNCTION__, pdev));
        return FALSE;
    }

    if (dev_info.version != QXL_DRIVER_INFO_VERSION) {
        DEBUG_PRINT((pdev, 0, "%s: get qxl info mismatch, 0x%lx\n", __FUNCTION__, pdev));
        return FALSE;
    }

    pdev->pci_revision = dev_info.pci_revision;
    pdev->use_async = (pdev->pci_revision >= QXL_REVISION_STABLE_V10);
    pdev->cmd_ring = dev_info.cmd_ring;
    pdev->cursor_ring = dev_info.cursor_ring;
    pdev->release_ring = dev_info.release_ring;
    pdev->notify_cmd_port = dev_info.notify_cmd_port;
    pdev->notify_cursor_port = dev_info.notify_cursor_port;
    pdev->notify_oom_port = dev_info.notify_oom_port;

    pdev->asyncable[ASYNCABLE_UPDATE_AREA][ASYNC] = dev_info.update_area_async_port;
    pdev->asyncable[ASYNCABLE_UPDATE_AREA][SYNC] = dev_info.update_area_port;
    pdev->asyncable[ASYNCABLE_MEMSLOT_ADD][ASYNC] = dev_info.memslot_add_async_port;
    pdev->asyncable[ASYNCABLE_MEMSLOT_ADD][SYNC] = dev_info.memslot_add_port;
    pdev->asyncable[ASYNCABLE_CREATE_PRIMARY][ASYNC] = dev_info.create_primary_async_port;
    pdev->asyncable[ASYNCABLE_CREATE_PRIMARY][SYNC] = dev_info.create_primary_port;
    pdev->asyncable[ASYNCABLE_DESTROY_PRIMARY][ASYNC] = dev_info.destroy_primary_async_port;
    pdev->asyncable[ASYNCABLE_DESTROY_PRIMARY][SYNC] = dev_info.destroy_primary_port;
    pdev->asyncable[ASYNCABLE_DESTROY_SURFACE][ASYNC] = dev_info.destroy_surface_async_port;
    pdev->asyncable[ASYNCABLE_DESTROY_SURFACE][SYNC] = dev_info.destroy_surface_wait_port;
    pdev->asyncable[ASYNCABLE_DESTROY_ALL_SURFACES][ASYNC] = dev_info.destroy_all_surfaces_async_port;
    pdev->asyncable[ASYNCABLE_DESTROY_ALL_SURFACES][SYNC] = dev_info.destroy_all_surfaces_port;
    pdev->asyncable[ASYNCABLE_FLUSH_SURFACES][ASYNC] = dev_info.flush_surfaces_async_port;
    pdev->asyncable[ASYNCABLE_FLUSH_SURFACES][SYNC] = NULL;

    pdev->asyncable[ASYNCABLE_MONITOR_CONFIG][ASYNC] = dev_info.monitors_config_port;
    pdev->asyncable[ASYNCABLE_MONITOR_CONFIG][SYNC] = NULL;

    pdev->display_event = dev_info.display_event;
    pdev->cursor_event = dev_info.cursor_event;
    pdev->sleep_event = dev_info.sleep_event;
    pdev->io_cmd_event = dev_info.io_cmd_event;
#if (WINVER < 0x0501)
    pdev->WaitForEvent = dev_info.WaitForEvent;
#endif

    pdev->num_io_pages = dev_info.num_pages;
    pdev->io_pages_virt = dev_info.io_pages_virt;
    pdev->io_pages_phys = dev_info.io_pages_phys;

    pdev->dev_update_id = dev_info.update_id;

    pdev->update_area = dev_info.update_area;
    pdev->update_surface = dev_info.update_surface;

    pdev->mm_clock = dev_info.mm_clock;

    pdev->compression_level = dev_info.compression_level;

    pdev->log_port = dev_info.log_port;
    pdev->log_buf = dev_info.log_buf;
    pdev->log_level = dev_info.log_level;

    pdev->n_surfaces = dev_info.n_surfaces;

    pdev->mem_slots = EngAllocMem(FL_ZERO_MEMORY, sizeof(PMemSlot) * dev_info.num_mem_slot,
                                  ALLOC_TAG);
    if (!pdev->mem_slots) {
        DEBUG_PRINT((pdev, 0, "%s: mem slots alloc failed, 0x%lx\n", __FUNCTION__, pdev));
        return FALSE;
    }

    pdev->slots_generation = dev_info.slots_generation;
    pdev->ram_slot_start = dev_info.ram_slot_start;
    pdev->ram_slot_end = dev_info.ram_slot_end;
    pdev->slot_id_bits = dev_info.slot_id_bits;
    pdev->slot_gen_bits = dev_info.slot_gen_bits;
    pdev->main_mem_slot = dev_info.main_mem_slot_id;
    pdev->num_mem_slot = dev_info.num_mem_slot;

    UpdateMainSlot(pdev, &dev_info.main_mem_slot);

    video_mem.RequestedVirtualAddress = NULL;

    if (EngDeviceIoControl( pdev->driver, IOCTL_VIDEO_MAP_VIDEO_MEMORY, &video_mem,
                            sizeof(VIDEO_MEMORY), &video_mem_Info,
                            sizeof(video_mem_Info), &length) ) {
        DEBUG_PRINT((pdev, 0, "%s: mapping failed, 0x%lx\n", __FUNCTION__, pdev));
        return FALSE;
    }
    DEBUG_PRINT((pdev, 1, "%s: 0x%lx vals 0x%lx %ul\n", __FUNCTION__, pdev,
                 video_mem_Info.FrameBufferBase, video_mem_Info.FrameBufferLength));
    pdev->fb = (BYTE*)video_mem_Info.FrameBufferBase;
    pdev->fb_size = video_mem_Info.FrameBufferLength;
    pdev->fb_phys = dev_info.fb_phys;

    pdev->memslot_del_port = dev_info.memslot_del_port;

    pdev->flush_release_port = dev_info.flush_release_port;

    pdev->primary_memory_start = dev_info.surface0_area;
    pdev->primary_memory_size = dev_info.surface0_area_size;

    pdev->primary_surface_create = dev_info.primary_surface_create;

    pdev->dev_id = dev_info.dev_id;

    pdev->create_non_primary_surfaces = dev_info.create_non_primary_surfaces;
    DEBUG_PRINT((pdev, 1, "%s: create_non_primary_surfaces = %d\n", __FUNCTION__,
                 pdev->create_non_primary_surfaces));

    pdev->monitor_config_pa = dev_info.monitors_config;
    CreateVRamSlot(pdev);

    DEBUG_PRINT((NULL, 1, "%s: 0x%lx exit: 0x%lx %ul\n", __FUNCTION__, pdev,
                 pdev->fb, pdev->fb_size));
    return TRUE;
}

static VOID UnmapFB(PDev *pdev)
{
    VIDEO_MEMORY video_mem;
    DWORD length;

    if (!pdev->fb) {
        return;
    }

    video_mem.RequestedVirtualAddress = pdev->fb;
    pdev->fb = 0;
    pdev->fb_size = 0;
    if (EngDeviceIoControl(pdev->driver,
                           IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
                           &video_mem,
                           sizeof(video_mem),
                           NULL,
                           0,
                           &length)) {
        DEBUG_PRINT((NULL, 0, "%s: unmpap failed, 0x%lx\n", __FUNCTION__, pdev));
    }
}

VOID EnableQXLPrimarySurface(PDev *pdev)
{
    UINT32 depth, format;

    switch (pdev->bitmap_format) {
        case BMF_8BPP:
            PANIC(pdev, "bad formart type 8bpp\n");
        case BMF_16BPP:
            depth = 16;
            format = SPICE_SURFACE_FMT_16_555;
            break;
        case BMF_24BPP:
        case BMF_32BPP:
            depth = 32;
            format = SPICE_SURFACE_FMT_32_xRGB;
            break;
        default:
            PANIC(pdev, "bad formart type\n");
    };

    CreatePrimarySurface(pdev, depth, format,
                         pdev->resolution.cx, pdev->resolution.cy,
                         pdev->stride, pdev->surf_phys);
    pdev->surf_enable = TRUE;
}

HSURF DrvEnableSurface(DHPDEV in_pdev)
{
    PDev *pdev;
    HSURF surf;
    DWORD length;
    QXLPHYSICAL phys_mem;
    UINT8 *base_mem;

    pdev = (PDev*)in_pdev;
    DEBUG_PRINT((pdev, 1, "%s: 0x%lx\n", __FUNCTION__, in_pdev));

    if (!PrepareHardware(pdev)) {
        return FALSE;
    }
    InitResources(pdev);

    if (!(surf = (HSURF)CreateDeviceBitmap(pdev, pdev->resolution, pdev->bitmap_format, &phys_mem,
                                           &base_mem, 0, DEVICE_BITMAP_ALLOCATION_TYPE_SURF0))) {
        DEBUG_PRINT((pdev, 0, "%s: create device surface failed, 0x%lx\n",
                     __FUNCTION__, pdev));
        goto err;
    }

    DEBUG_PRINT((pdev, 1, "%s: EngModifySurface(0x%lx, 0x%lx, 0, MS_NOTSYSTEMMEMORY, "
                 "0x%lx, 0x%lx, %lu, NULL)\n",
                 __FUNCTION__,
                 surf,
                 pdev->eng,
                 pdev,
                 pdev->fb,
                 pdev->stride));

    pdev->surf = surf;
    pdev->surf_phys = phys_mem;
    pdev->surf_base = base_mem;

    EnableQXLPrimarySurface(pdev);

    DEBUG_PRINT((pdev, 1, "%s: 0x%lx exit\n", __FUNCTION__, pdev));
    return surf;

err:
    DrvDisableSurface((DHPDEV)pdev);
    DEBUG_PRINT((pdev, 0, "%s: 0x%lx err\n", __FUNCTION__, pdev));
    return NULL;
}

VOID DisableQXLPrimarySurface(PDev *pdev, int hide_mouse)
{
    DrawArea drawarea;

    if (pdev->surf_enable) {
        DestroyPrimarySurface(pdev, hide_mouse);
        pdev->surf_enable = FALSE;
    }
}

VOID DisableQXLAllSurfaces(PDev *pdev)
{
    DestroyAllSurfaces(pdev);
}

VOID DrvDisableSurface(DHPDEV in_pdev)
{
    PDev *pdev = (PDev*)in_pdev;
    DrawArea drawarea;

    DEBUG_PRINT((pdev, 1, "%s: 0x%lx\n", __FUNCTION__, pdev));

    // Don't destroy the primary - it's destroyed by destroy_all_surfaces
    // at AssertModeDisable. Also, msdn specifically mentions DrvDisableSurface
    // should not touch the hardware, that should be done just via DrvAssertMode
    // (http://msdn.microsoft.com/en-us/library/ff556200%28VS.85%29.aspx)
    pdev->surf_enable = FALSE;
    UnmapFB(pdev);

    if (pdev->surf) {
        DeleteDeviceBitmap(pdev, 0, DEVICE_BITMAP_ALLOCATION_TYPE_SURF0);
        EngDeleteSurface(pdev->surf);
        pdev->surf = NULL;
    }

    if (pdev->mem_slots) {
        EngFreeMem(pdev->mem_slots);
        pdev->mem_slots = NULL;
    }

    DebugCountAliveSurfaces(pdev);
    ClearResources(pdev);
    DEBUG_PRINT((pdev, 1, "%s: 0x%lx exit\n", __FUNCTION__, pdev));
}

static void FlushSurfaces(PDev *pdev)
{
    UINT32 surface_id;
    SurfaceInfo *surface_info;
    SURFOBJ *surf_obj;
    RECTL area = {0, 0, 0, 0};

    if (pdev->pci_revision < QXL_REVISION_STABLE_V10) {
        DEBUG_PRINT((pdev, 1, "%s: revision too old for QXL_IO_FLUSH_SURFACES\n", __FUNCTION__));
        for (surface_id = pdev->n_surfaces - 1; surface_id >  0 ; --surface_id) {
            surface_info = GetSurfaceInfo(pdev, surface_id);

            if (!surface_info->draw_area.base_mem) {
                continue;
            }
            surf_obj = surface_info->draw_area.surf_obj;
            if (!surf_obj) {
                continue;
            }
            area.right = surf_obj->sizlBitmap.cx;
            area.bottom = surf_obj->sizlBitmap.cy;
            UpdateArea(pdev,&area, surface_id);
        }
    } else {
        async_io(pdev, ASYNCABLE_FLUSH_SURFACES, 0);
    }
}

static BOOL FlushRelease(PDev *pdev)
{
    if (pdev->pci_revision<  QXL_REVISION_STABLE_V10) {
        DWORD length;

        DEBUG_PRINT((pdev, 1, "%s: revision too old for QXL_IO_FLUSH_RELEASE\n", __FUNCTION__));
        if (EngDeviceIoControl(pdev->driver, IOCTL_VIDEO_RESET_DEVICE,
                               NULL, 0, NULL, 0, &length)) {
            DEBUG_PRINT((NULL, 0, "%s: reset failed 0x%lx\n", __FUNCTION__, pdev));
            return FALSE;
        }
    } else {
        /* Free release ring contents */
        ReleaseCacheDeviceMemoryResources(pdev);
        EmptyReleaseRing(pdev);
        /* Get the last free list onto the release ring */
        sync_io(pdev, pdev->flush_release_port, 0);
        DEBUG_PRINT((pdev, 4, "%s after FLUSH_RELEASE\n", __FUNCTION__));
        /* And release that. mspace allocators should be clean after. */
        EmptyReleaseRing(pdev);
    }
    return TRUE;
}

static BOOL AssertModeDisable(PDev *pdev)
{
    DEBUG_PRINT((pdev, 3, "%s entry\n", __FUNCTION__));
    /* flush command ring and update all surfaces */
    FlushSurfaces(pdev);
    DebugCountAliveSurfaces(pdev);
    /*
     * this call is redundant for
     * pci_revision <  QXL_REVISION_STABLE_V10, due to the
     * IOCTL_VIDEO_RESET_DEVICE in FlushRelease. However,
     * MoveAllSurfacesToRam depends on destroy_all_surfaces
     * in case of failure.
     * TODO: make MoveAllSurfacesToRam send destroy_surface
     * commands instead of create_surface commands in case
     * of failure
     */
    async_io(pdev, ASYNCABLE_DESTROY_ALL_SURFACES, 0);
    /* move all surfaces from device to system memory */
    if (!MoveAllSurfacesToRam(pdev)) {
        EnableQXLPrimarySurface(pdev);
        return FALSE;
    }
    if (!FlushRelease(pdev)) {
        return FALSE;
    }
    RemoveVRamSlot(pdev);
    DebugCountAliveSurfaces(pdev);
    DEBUG_PRINT((pdev, 4, "%s: [%d,%d] [%d,%d] [%d,%d] %lx\n", __FUNCTION__,
        pdev->cmd_ring->prod, pdev->cmd_ring->cons,
        pdev->cursor_ring->prod, pdev->cursor_ring->cons,
        pdev->release_ring->prod, pdev->release_ring->cons,
        pdev->free_outputs));
    DEBUG_PRINT((pdev, 3, "%s exit\n", __FUNCTION__));
    return TRUE;
}

static void AssertModeEnable(PDev *pdev)
{
    InitDeviceMemoryResources(pdev);
    DEBUG_PRINT((pdev, 3, "%s: [%d,%d] [%d,%d] [%d,%d] %lx\n", __FUNCTION__,
        pdev->cmd_ring->prod, pdev->cmd_ring->cons,
        pdev->cursor_ring->prod, pdev->cursor_ring->cons,
        pdev->release_ring->prod, pdev->release_ring->cons,
        pdev->free_outputs));
    EnableQXLPrimarySurface(pdev);
    CreateVRamSlot(pdev);
    DebugCountAliveSurfaces(pdev);
    MoveAllSurfacesToVideoRam(pdev);
    DebugCountAliveSurfaces(pdev);
}

BOOL DrvAssertMode(DHPDEV in_pdev, BOOL enable)
{
    PDev* pdev = (PDev*)in_pdev;
    BOOL ret = TRUE;

    DEBUG_PRINT((pdev, 1, "%s: 0x%lx revision %d enable %d\n", __FUNCTION__, pdev, pdev->pci_revision, enable));
    if (pdev->enabled == enable) {
        DEBUG_PRINT((pdev, 1, "%s: called twice with same argument (%d)\n", __FUNCTION__,
            enable));
        return TRUE;
    }
    pdev->enabled = enable;
    if (enable) {
        AssertModeEnable(pdev);
    } else {
        ret = AssertModeDisable(pdev);
        if (!ret) {
            pdev->enabled = !enable;
        }
    }
    DEBUG_PRINT((pdev, 1, "%s: 0x%lx exit %d\n", __FUNCTION__, pdev, enable));
    return ret;
}

ULONG DrvGetModes(HANDLE driver, ULONG dev_modes_size, DEVMODEW *dev_modes)
{
    PVIDEO_MODE_INFORMATION video_modes;
    PVIDEO_MODE_INFORMATION curr_video_mode;
    DWORD mode_size;
    DWORD output_size;
    DWORD n_modes;

    DEBUG_PRINT((NULL, 1, "%s\n", __FUNCTION__));

    n_modes = GetAvailableModes(driver, &video_modes, &mode_size);

    if (!n_modes) {
        DEBUG_PRINT((NULL, 0, "%s: get available modes failed\n", __FUNCTION__));
        return 0;
    }

    if (!dev_modes) {
        DEBUG_PRINT((NULL, 1, "%s: query size\n", __FUNCTION__));
        output_size = n_modes * sizeof(DEVMODEW);
        goto out;
    }

    if (dev_modes_size < n_modes * sizeof(DEVMODEW)) {
        DEBUG_PRINT((NULL, 0, "%s: buf to small\n", __FUNCTION__));
        output_size = 0;
        goto out;
    }

    output_size = 0;
    curr_video_mode = video_modes;
    do {
        if (curr_video_mode->Length != 0) {
            RtlZeroMemory(dev_modes, sizeof(DEVMODEW));
            ASSERT(NULL, sizeof(DEVICE_NAME) < sizeof(dev_modes->dmDeviceName));
            RtlCopyMemory(dev_modes->dmDeviceName, DEVICE_NAME, sizeof(DEVICE_NAME));
            dev_modes->dmSpecVersion = DM_SPECVERSION;
            dev_modes->dmDriverVersion = DM_SPECVERSION;
            dev_modes->dmSize = sizeof(DEVMODEW);
            dev_modes->dmBitsPerPel =   curr_video_mode->NumberOfPlanes *
                                        curr_video_mode->BitsPerPlane;
            dev_modes->dmPelsWidth = curr_video_mode->VisScreenWidth;
            dev_modes->dmPelsHeight = curr_video_mode->VisScreenHeight;
            dev_modes->dmDisplayFrequency = curr_video_mode->Frequency;
            dev_modes->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                                  DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;
#if (WINVER >= 0x0501)
            dev_modes->dmDisplayOrientation = curr_video_mode->DriverSpecificAttributeFlags;
            dev_modes->dmFields |= DM_DISPLAYORIENTATION;
#endif

            DEBUG_PRINT((NULL, 1, "%s: mode: w %u h %u bits %u frequency %u\n",
                         __FUNCTION__,
                         dev_modes->dmPelsWidth,
                         dev_modes->dmPelsHeight,
                         dev_modes->dmBitsPerPel,
                         dev_modes->dmDisplayFrequency));
#if (WINVER >= 0x0501)
            DEBUG_PRINT((NULL, 1, "             orientation %u\n",
                         dev_modes->dmDisplayOrientation));
#endif
            output_size += sizeof(DEVMODEW);
            dev_modes++;
        }
        curr_video_mode = (PVIDEO_MODE_INFORMATION)(((PUCHAR)curr_video_mode) + mode_size);
    } while (--n_modes);

    out:

    EngFreeMem(video_modes);
    DEBUG_PRINT((NULL, 1, "%s: exit %u\n", __FUNCTION__, output_size));
    return output_size;
}

VOID DrvSynchronize(DHPDEV in_pdev, RECTL *ignored)
{
    PDev* pdev = (PDev*)in_pdev;
    int notify;

    DEBUG_PRINT((pdev, 3, "%s: 0x%lx\n", __FUNCTION__, pdev));

    DEBUG_PRINT((pdev, 4, "%s: 0x%lx done\n", __FUNCTION__, pdev));
}

char *BitmapFormatToStr(int format)
{
    switch (format) {
    case BMF_1BPP:
        return  "BMF_1BPP";
    case BMF_4BPP:
        return "BMF_4BPP";
    case BMF_8BPP:
        return "BMF_8BPP";
    case BMF_16BPP:
        return "BMF_16BPP";
    case BMF_24BPP:
        return "BMF_24BPP";
    case BMF_32BPP:
        return "BMF_32BPP";
    case BMF_4RLE:
        return "BMF_4RLE";
    case BMF_8RLE:
        return "BMF_8RLE";
    case BMF_JPEG:
        return "BMF_JPEG";
    case BMF_PNG:
        return "BMF_PNG";
    default:
        return "?";
    }
}

char *BitmapTypeToStr(int type)
{
    switch (type) {
    case STYPE_BITMAP:
        return  "STYPE_BITMAP";
    case STYPE_DEVICE:
        return "STYPE_DEVICE";
    case STYPE_DEVBITMAP:
        return "STYPE_DEVBITMAP";
    default:
        return "?";
    }
}

#include "rop.h"
#include "utils.h"
#include "res.h"

FIX FlotaToFixed(FLOATL val, FLOATL scale)
{
    FLOATOBJ float_obj;
    FIX ret;

    FLOATOBJ_SetFloat(&float_obj, val);
    FLOATOBJ_MulFloat(&float_obj, scale);

    ret = FLOATOBJ_GetLong(&float_obj) << 4;
    FLOATOBJ_MulLong(&float_obj, 16);
    ret |= (0x0f & FLOATOBJ_GetLong(&float_obj));
    return ret;
}

static BOOL GetCosmeticAttr(PDev *pdev, QXLDrawable *drawable, QXLLineAttr *q_line_attr,
                            LINEATTRS *line_attr)
{
    q_line_attr->join_style = JOIN_MITER;
    q_line_attr->end_style = ENDCAP_BUTT;
    q_line_attr->width = 1 << 4;
    q_line_attr->miter_limit = 0;

    if (line_attr->fl & LA_STYLED) {
        PFLOAT_LONG src_style = line_attr->pstyle;
        FIX *style;
        FIX *end;
        UINT32 nseg;

        q_line_attr->flags = (UINT8)(line_attr->fl & (LA_STYLED | LA_STARTGAP));
        nseg = (line_attr->fl & LA_ALTERNATE) ? 2 : line_attr->cstyle;
        if ( nseg > 100) {
            return FALSE;
        }

        if (!(style = (FIX *)QXLGetBuf(pdev, drawable, &q_line_attr->style,
                                       nseg * sizeof(UINT32)))) {
            return FALSE;
        }

        if (line_attr->fl & LA_ALTERNATE) {
            style[0] = style[1] = 1 << 4;
        } else {
            for ( end = style + nseg;  style < end; style++, src_style++) {
                *style = (*src_style).l << 4;
            }
        }
        q_line_attr->style_nseg = (UINT8)nseg;
    } else {
        q_line_attr->flags = 0;
        q_line_attr->style_nseg = 0;
        q_line_attr->style = 0;
    }
    return TRUE;
}

BOOL APIENTRY DrvStrokePath(SURFOBJ *surf, PATHOBJ *path, CLIPOBJ *clip, XFORMOBJ *width_transform,
                            BRUSHOBJ *brush, POINTL *brush_pos, LINEATTRS *line_attr,
                            MIX mix /*rop*/)
{
    QXLDrawable *drawable;
    RECTFX fx_area;
    RECTL area;
    PDev *pdev;
    ROP3Info *fore_rop;
    ROP3Info *back_rop;
    BOOL h_or_v_line;

    if (!(pdev = (PDev *)surf->dhpdev)) {
        DEBUG_PRINT((NULL, 0, "%s: err no pdev\n", __FUNCTION__));
        return TRUE;
    }

    PUNT_IF_DISABLED(pdev);

    CountCall(pdev, CALL_COUNTER_STROKE_PATH);

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));
    ASSERT(pdev, surf && path && line_attr && clip);


    if (line_attr->fl & (LA_STYLED | LA_ALTERNATE | LA_GEOMETRIC)) { //for now
        // punt back to GDI result in infinite recursion
        //return EngStrokePath(surf, path, clip, width_transform, brush, brush_pos, line_attr, mix);
    }

    ASSERT(pdev, (line_attr->fl & LA_GEOMETRIC) == 0); /* We should not get these */

    PATHOBJ_vGetBounds(path, &fx_area);
    FXToRect(&area, &fx_area);

    h_or_v_line = area.bottom == area.top + 1  || area.right == area.left + 1;

    if (clip) {
        if (clip->iDComplexity == DC_TRIVIAL) {
            clip = NULL;
        } else {
            SectRect(&clip->rclBounds, &area, &area);
            if (IsEmptyRect(&area)) {
                DEBUG_PRINT((pdev, 1, "%s: empty rect after clip\n", __FUNCTION__));
                return TRUE;
            }
        }
    }

    if (!(drawable = Drawable(pdev, QXL_DRAW_STROKE, &area, clip, GetSurfaceId(surf)))) {
        return FALSE;
    }

    fore_rop = &rops2[(mix - 1) & 0x0f];
    back_rop = &rops2[((mix >> 8) - 1) & 0x0f];

    if (!((fore_rop->flags | back_rop->flags) & ROP3_BRUSH)) {
        drawable->u.stroke.brush.type = SPICE_BRUSH_TYPE_NONE;
    } else if (!QXLGetBrush(pdev, drawable, &drawable->u.stroke.brush, brush, brush_pos,
                            &drawable->surfaces_dest[0], &drawable->surfaces_rects[0])) {
        goto err;
    }

    if (!QXLGetPath(pdev, drawable, &drawable->u.stroke.path, path)) {
        goto err;
    }
    // DrvStrokePath only draws foreground pixels, unless you support dotted
    // lines, so you only care about the low-order byte.
    drawable->u.stroke.fore_mode = fore_rop->method_data;
    drawable->u.stroke.back_mode = back_rop->method_data;

    drawable->effect = (h_or_v_line) ? QXL_EFFECT_OPAQUE: QXL_EFFECT_BLEND;

    if (!GetCosmeticAttr(pdev, drawable, &drawable->u.stroke.attr, line_attr)) {
        goto err;
    }

    if (drawable->u.stroke.attr.flags & LA_STYLED) {
        drawable->effect = (fore_rop->effect == back_rop->effect) ? fore_rop->effect :
                                                                                   QXL_EFFECT_BLEND;
    } else {
        drawable->effect = fore_rop->effect;
    }

    if (drawable->effect == QXL_EFFECT_OPAQUE && !h_or_v_line) {
        drawable->effect = QXL_EFFECT_OPAQUE_BRUSH;
    }

    PushDrawable(pdev, drawable);
    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));
    return TRUE;

err:
    ReleaseOutput(pdev, drawable->release_info.id);
    return FALSE;
}

HBITMAP APIENTRY DrvCreateDeviceBitmap(DHPDEV dhpdev, SIZEL size, ULONG format)
{
    PDev *pdev;
    UINT8 *base_mem;
    UINT32 surface_id;
    QXLPHYSICAL phys_mem;
    HBITMAP hbitmap;

    pdev = (PDev *)dhpdev;

    PUNT_IF_DISABLED(pdev);

    if (!pdev->create_non_primary_surfaces) {
        return FALSE;
    }

    if (!pdev->vram_slot_initialized || pdev->bitmap_format != format || pdev->fb == 0) {
        DEBUG_PRINT((pdev, 3, "%s failed: %p: slot_initialized %d, format(%d,%d), fb %p\n",
                    __FUNCTION__, pdev, pdev->vram_slot_initialized,
                    pdev->bitmap_format, format, pdev->fb));
        return 0;
    }

    PUNT_IF_DISABLED(pdev);

    surface_id = GetFreeSurface(pdev);
    if (!surface_id) {
        DEBUG_PRINT((pdev, 3, "%s:%p GetFreeSurface failed\n", __FUNCTION__, pdev));
        goto out_error;
    }
    DEBUG_PRINT((pdev, 3, "%s: %p: %d\n", __FUNCTION__, pdev, surface_id));

    hbitmap = CreateDeviceBitmap(pdev, size, pdev->bitmap_format, &phys_mem, &base_mem, surface_id,
                                 DEVICE_BITMAP_ALLOCATION_TYPE_VRAM);
    if (!hbitmap) {
         DEBUG_PRINT((pdev, 3, "%s:%p CreateDeviceBitmap failed\n", __FUNCTION__, pdev));
        goto out_error2;
    }

    return hbitmap;

    // to optimize the failure case
out_error2:
    FreeSurfaceInfo(pdev, surface_id);
out_error:
    return 0;
}

VOID APIENTRY DrvDeleteDeviceBitmap(DHSURF dhsurf)
{
    UINT32 surface_id;
    SurfaceInfo *surface;
    PDev *pdev;

    surface = (SurfaceInfo *)dhsurf;
    surface_id = GetSurfaceIdFromInfo(surface);
    pdev = surface->u.pdev;

    DEBUG_PRINT((pdev, 3, "%s: %p: %d\n", __FUNCTION__, pdev, surface_id));

    if (!pdev->enabled) {
        DEBUG_PRINT((pdev, 3, "%s: device disabled, still destroying\n",
                    __FUNCTION__));
    }

    ASSERT(pdev, surface_id < pdev->n_surfaces);

    DeleteDeviceBitmap(surface->u.pdev, surface_id,
                       surface->copy ? DEVICE_BITMAP_ALLOCATION_TYPE_RAM
                                     : DEVICE_BITMAP_ALLOCATION_TYPE_VRAM);
}

#ifdef CALL_TEST

void CountCall(PDev *pdev, int counter)
{
    if (pdev->count_calls) {
        int i;

        pdev->call_counters[counter]++;
        if((++pdev->total_calls % 500) == 0) {
            DEBUG_PRINT((pdev, 0, "total eng calls is %u\n", pdev->total_calls));
            for (i = 0; i < NUM_CALL_COUNTERS; i++) {
                DEBUG_PRINT((pdev, 0, "%s count is %u\n",
                             counters_info[i].name, pdev->call_counters[i]));
            }
        }
        pdev->count_calls = FALSE;
    } else if (counters_info[counter].effective) {
        pdev->count_calls = TRUE;
    }
}

BOOL APIENTRY DrvFillPath(
    SURFOBJ  *pso,
    PATHOBJ  *ppo,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX       mix,
    FLONG     flOptions)
{
    PDev *pdev;

    pdev = (PDev *)pso->dhpdev;
    CountCall(pdev, CALL_COUNTER_FILL_PATH);

    return EngFillPath(pso, ppo, pco, pbo, pptlBrushOrg, mix, flOptions);
}

BOOL APIENTRY DrvGradientFill(
    SURFOBJ         *psoDest,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    TRIVERTEX       *pVertex,
    ULONG            nVertex,
    PVOID            pMesh,
    ULONG            nMesh,
    RECTL           *prclExtents,
    POINTL          *pptlDitherOrg,
    ULONG            ulMode)
{
    PDev *pdev;

    pdev = (PDev *)psoDest->dhpdev;
    CountCall(pdev, CALL_COUNTER_GRADIENT_FILL);
    return EngGradientFill(psoDest, pco, pxlo, pVertex, nVertex, pMesh, nMesh, prclExtents,
                           pptlDitherOrg, ulMode);
}

BOOL APIENTRY DrvLineTo(
    SURFOBJ   *pso,
    CLIPOBJ   *pco,
    BRUSHOBJ  *pbo,
    LONG       x1,
    LONG       y1,
    LONG       x2,
    LONG       y2,
    RECTL     *prclBounds,
    MIX        mix)
{
    PDev *pdev;

    pdev = (PDev *)pso->dhpdev;
    CountCall(pdev, CALL_COUNTER_LINE_TO);
    return EngLineTo(pso, pco, pbo, x1, y1, x2, y2, prclBounds, mix);
}

BOOL APIENTRY DrvPlgBlt(
    SURFOBJ         *psoTrg,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMsk,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    POINTFIX        *pptfx,
    RECTL           *prcl,
    POINTL          *pptl,
    ULONG            iMode)
{
    if (psoSrc->iType == STYPE_BITMAP) {
        PDev *pdev;

        ASSERT(NULL, psoTrg && psoTrg->iType != STYPE_BITMAP && psoTrg->dhpdev);
        pdev = (PDev *)psoTrg->dhpdev;
        CountCall(pdev, CALL_COUNTER_PLG_BLT);
    }
    return EngPlgBlt(psoTrg, psoSrc, psoMsk, pco, pxlo, pca, pptlBrushOrg, pptfx, prcl, pptl,
                     iMode);
}

BOOL APIENTRY DrvStrokeAndFillPath(
    SURFOBJ   *pso,
    PATHOBJ   *ppo,
    CLIPOBJ   *pco,
    XFORMOBJ  *pxo,
    BRUSHOBJ  *pboStroke,
    LINEATTRS *plineattrs,
    BRUSHOBJ  *pboFill,
    POINTL    *pptlBrushOrg,
    MIX        mixFill,
    FLONG      flOptions)
{
    PDev *pdev = (PDev *)pso->dhpdev;
    CountCall(pdev, CALL_COUNTER_STROKE_AND_FILL_PATH);
    return EngStrokeAndFillPath(pso, ppo, pco, pxo, pboStroke, plineattrs, pboFill, pptlBrushOrg,
                                mixFill, flOptions);
}

#endif
