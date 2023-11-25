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

#include "os_dep.h"
#include "qxl.h"
#if (WINVER < 0x0501)
#include "wdmhelper.h"
#endif
#include "minimal_snprintf.h"

#define WIN_QXL_INT_MASK ((QXL_INTERRUPT_DISPLAY) | \
                          (QXL_INTERRUPT_CURSOR) | \
                          (QXL_INTERRUPT_IO_CMD))

VP_STATUS FindAdapter(PVOID dev_extension,
                      PVOID reserved,
                      PWSTR arg_str,
                      PVIDEO_PORT_CONFIG_INFO conf_info,
                      PUCHAR again);

BOOLEAN Initialize(PVOID dev_extension);

VP_STATUS GetPowerState(PVOID dev_extension,
                        ULONG hw_id,
                        PVIDEO_POWER_MANAGEMENT state);

VP_STATUS SetPowerState(PVOID dev_extension,
                        ULONG hw_wId,
                        PVIDEO_POWER_MANAGEMENT state);

VP_STATUS GetChildDescriptor(IN PVOID dev_extension,
                             IN PVIDEO_CHILD_ENUM_INFO  enum_info,
                             OUT PVIDEO_CHILD_TYPE  type,
                             OUT PUCHAR descriptor,
                             OUT PULONG uid,
                             OUT PULONG unused);

BOOLEAN StartIO(PVOID dev_extension, PVIDEO_REQUEST_PACKET packet);

BOOLEAN Interrupt(PVOID  HwDeviceExtension);

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE, DriverEntry)
#pragma alloc_text(PAGE, FindAdapter)
#pragma alloc_text(PAGE, Initialize)
#pragma alloc_text(PAGE, GetPowerState)
#pragma alloc_text(PAGE, SetPowerState)
#pragma alloc_text(PAGE, GetChildDescriptor)
#pragma alloc_text(PAGE, StartIO)
#endif

typedef struct QXLExtension {
    PVOID io_base;
    PUCHAR io_port;

    UCHAR pci_revision;

    QXLRom *rom;
    ULONG rom_size;

    PHYSICAL_ADDRESS ram_physical;
    UINT8 *ram_start;
    QXLRam *ram_header;
    ULONG ram_size;

    PHYSICAL_ADDRESS vram_physical;
    ULONG vram_size;
    UINT8 *vram_start;

    ULONG current_mode;
    ULONG n_modes;
    ULONG custom_mode;
    PVIDEO_MODE_INFORMATION modes;

    PEVENT display_event;
    PEVENT cursor_event;
    PEVENT sleep_event;
    PEVENT io_cmd_event;

    MemSlot *mem_slots;

    char *log_buf;
    PUCHAR log_port;

    UINT8 create_non_primary_surfaces;
} QXLExtension;

#define QXL_ALLOC_TAG '_lxq'

#define DBG_LEVEL 10

#define QXL_MINIPORT_DEBUG_PREFIX "qxlmp: "

void DebugPrintV(char *log_buf, PUCHAR log_port, const char *message, const char *func, va_list ap)
{
    int n, n_strlen;

    if (log_buf && log_port) {
        /*
         * TODO: use a shared semaphore with display code.
         * In practice this is not a problem, since miniport runs either on ioctls (sync)
         * or before display is brought up or when it is brought down.
         * Also the worst that can happen is overwriting a message (not seen in practice).
         */
        snprintf(log_buf, QXL_LOG_BUF_SIZE, QXL_MINIPORT_DEBUG_PREFIX);
        vsnprintf(log_buf + strlen(QXL_MINIPORT_DEBUG_PREFIX),
                   QXL_LOG_BUF_SIZE - strlen(QXL_MINIPORT_DEBUG_PREFIX),
                   message, ap);
        VideoPortWritePortUchar(log_port, 0);
    } else {
        VideoDebugPrint((0, (PCHAR)message, ap));
    }
}

void DebugPrint(QXLExtension *dev, UINT32 level, const char *message, const char *func, ...)
{
    va_list ap;

    if (dev && dev->rom && level > dev->rom->log_level) {
        return;
    }
    va_start(ap, message);
    DebugPrintV(dev ? dev->log_buf : NULL, dev ? dev->log_port : 0, message, func, ap);
    va_end(ap);
}

ULONG DriverEntry(PVOID context1, PVOID context2)
{
    VIDEO_HW_INITIALIZATION_DATA init_data;
    ULONG ret;

    PAGED_CODE();

    DEBUG_PRINT((NULL, 0, "%s: enter\n", __FUNCTION__));

    VideoPortZeroMemory(&init_data, sizeof(VIDEO_HW_INITIALIZATION_DATA));
    init_data.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);
    init_data.HwDeviceExtensionSize = sizeof(QXLExtension);

    init_data.HwFindAdapter = FindAdapter;
    init_data.HwInitialize = Initialize;
    init_data.HwGetPowerState = GetPowerState;
    init_data.HwSetPowerState = SetPowerState;
    init_data.HwGetVideoChildDescriptor = GetChildDescriptor;
    init_data.HwStartIO = StartIO;
    init_data.HwInterrupt = Interrupt;

    ret = VideoPortInitialize(context1, context2, &init_data, NULL);

    if (ret != NO_ERROR) {
        DEBUG_PRINT((NULL, 0, "%s: try W2K %u\n", __FUNCTION__, ret));
        init_data.HwInitDataSize = SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA;
        ret = VideoPortInitialize(context1, context2, &init_data, NULL);
    }
    DEBUG_PRINT((NULL, 0, "%s: exit %u\n", __FUNCTION__, ret));
    return ret;
}


#if defined(ALLOC_PRAGMA)
VP_STATUS InitIO(QXLExtension *dev, PVIDEO_ACCESS_RANGE range);
#pragma alloc_text(PAGE, InitIO)
#endif

VP_STATUS InitIO(QXLExtension *dev, PVIDEO_ACCESS_RANGE range)
{
    PVOID io_base;

    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s\n", __FUNCTION__));

    if ((dev->pci_revision == QXL_REVISION_STABLE_V06 &&
         range->RangeLength < QXL_IO_DESTROY_ALL_SURFACES + 1)
        || (dev->pci_revision > QXL_REVISION_STABLE_V06 &&
         range->RangeLength < QXL_IO_FLUSH_RELEASE + 1)
        || !range->RangeInIoSpace) {
        DEBUG_PRINT((dev, 0, "%s: bad io range\n", __FUNCTION__));
        return ERROR_INVALID_DATA;
    }

    io_base = VideoPortGetDeviceBase(dev, range->RangeStart, range->RangeLength,
                                     range->RangeInIoSpace);

    if (!io_base) {
        DEBUG_PRINT((dev, 0, "%s: get io base failed\n", __FUNCTION__));
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    dev->io_base = io_base;
    dev->io_port = (PUCHAR)range->RangeStart.LowPart;
    dev->log_port = dev->io_port + QXL_IO_LOG;
    DEBUG_PRINT((dev, 0, "%s: OK, io 0x%x size %lu\n", __FUNCTION__,
                 (ULONG)range->RangeStart.LowPart, range->RangeLength));

    return NO_ERROR;
}

#if defined(ALLOC_PRAGMA)
VP_STATUS InitRom(QXLExtension *dev, PVIDEO_ACCESS_RANGE range);
#pragma alloc_text(PAGE, InitRom)
#endif

VP_STATUS InitRom(QXLExtension *dev, PVIDEO_ACCESS_RANGE range)
{
    PVOID rom = NULL;
    ULONG rom_size = range->RangeLength;
    ULONG io_space = VIDEO_MEMORY_SPACE_MEMORY;
    VP_STATUS error;

    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s\n", __FUNCTION__));

    if (rom_size < sizeof(QXLRom) || range->RangeInIoSpace) {
        DEBUG_PRINT((dev, 0, "%s: bad rom range\n", __FUNCTION__));
        return ERROR_INVALID_DATA;
    }
    if ((error = VideoPortMapMemory(dev, range->RangeStart,
                                    &rom_size, &io_space,
                                    &rom)) != NO_ERROR ) {
        DEBUG_PRINT((dev, 0, "%s: map rom filed\n", __FUNCTION__));
        return error;
    }

    if (rom_size < range->RangeLength) {
        DEBUG_PRINT((dev, 0, "%s: short rom map\n", __FUNCTION__));
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }

    if (((QXLRom*)rom)->magic != QXL_ROM_MAGIC) {
        DEBUG_PRINT((dev, 0, "%s: bad rom magic\n", __FUNCTION__));
        error = ERROR_INVALID_DATA;
        goto err;
    }

    dev->rom = rom;
    dev->rom_size = range->RangeLength;
    DEBUG_PRINT((dev, 0, "%s OK: rom 0x%lx size %lu\n",
                 __FUNCTION__, (ULONG)range->RangeStart.QuadPart, range->RangeLength));
    return NO_ERROR;

err:
    VideoPortUnmapMemory(dev, rom, NULL);
    DEBUG_PRINT((dev, 0, "%s ERR\n", __FUNCTION__));
    return error;
}

#if defined(ALLOC_PRAGMA)
VP_STATUS InitRam(QXLExtension *dev, PVIDEO_ACCESS_RANGE range);
#pragma alloc_text(PAGE, InitRam)
#endif

VP_STATUS InitRam(QXLExtension *dev, PVIDEO_ACCESS_RANGE range)
{
    UINT8 *ram = NULL;
    QXLRam *ram_header;
    ULONG ram_size = range->RangeLength;
    ULONG io_space = VIDEO_MEMORY_SPACE_MEMORY;
    VP_STATUS error;

    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s\n", __FUNCTION__));

    if (ram_size < sizeof(QXLRam) + dev->rom->ram_header_offset || range->RangeInIoSpace) {
        DEBUG_PRINT((dev, 0, "%s: bad ram range\n", __FUNCTION__));
        return ERROR_INVALID_DATA;
    }

    if (ram_size < dev->rom->num_pages << PAGE_SHIFT) {
        DEBUG_PRINT((dev, 0, "%s: bad ram size\n", __FUNCTION__));
        return ERROR_INVALID_DATA;
    }

    if ((error = VideoPortMapMemory(dev, range->RangeStart,
                                    &ram_size, &io_space,
                                    &ram)) != NO_ERROR ) {
        DEBUG_PRINT((dev, 0, "%s: map ram filed\n", __FUNCTION__));
        return error;
    }

    if (ram_size < range->RangeLength) {
        DEBUG_PRINT((dev, 0, "%s: short ram map\n", __FUNCTION__));
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }
    ram_header = (QXLRam *)(ram + dev->rom->ram_header_offset);
    if (ram_header->magic != QXL_RAM_MAGIC) {
        DEBUG_PRINT((dev, 0, "%s: bad ram magic\n", __FUNCTION__));
        error = ERROR_INVALID_DATA;
        goto err;
    }

    dev->ram_physical = range->RangeStart;
    dev->ram_start = ram;
    dev->ram_header = ram_header;
    dev->ram_size = range->RangeLength;
    dev->log_buf = dev->ram_header->log_buf;
    DEBUG_PRINT((dev, 0, "%s OK: ram 0x%lx size %lu\n",
                 __FUNCTION__, (ULONG)range->RangeStart.QuadPart, range->RangeLength));
    return NO_ERROR;

    err:
    VideoPortUnmapMemory(dev, ram, NULL);
    DEBUG_PRINT((dev, 0, "%s ERR\n", __FUNCTION__));
    return error;
}


#if defined(ALLOC_PRAGMA)
VP_STATUS InitVRAM(QXLExtension *dev, PVIDEO_ACCESS_RANGE range);
#pragma alloc_text(PAGE, InitVRAM)
#endif

VP_STATUS InitVRAM(QXLExtension *dev, PVIDEO_ACCESS_RANGE range)
{
    UINT8 *vram_addr = NULL;
    ULONG vram_mapped_size = range->RangeLength;
    ULONG io_space = VIDEO_MEMORY_SPACE_MEMORY;
    VP_STATUS error;

    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s\n", __FUNCTION__));

    if (range->RangeLength == 0 || range->RangeInIoSpace) {
        DEBUG_PRINT((dev, 0, "%s: bad mem range\n", __FUNCTION__));
        return ERROR_INVALID_DATA;
    }

    if ((error = VideoPortMapMemory(dev, range->RangeStart,
                                    &vram_mapped_size,
                                    &io_space,
                                    &vram_addr))) {
        DEBUG_PRINT((dev, 0, "%s: map vram failed\n",  __FUNCTION__));
        return error;
    }

    if (vram_mapped_size < range->RangeLength) {
        DEBUG_PRINT((dev, 0, "%s: vram shrinked\n", __FUNCTION__));
        VideoPortUnmapMemory(dev, vram_addr, NULL);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    dev->vram_physical = range->RangeStart;
    dev->vram_start = vram_addr;
    dev->vram_size = range->RangeLength;
    DEBUG_PRINT((dev, 0, "%s: OK, vram 0x%lx size %lu vaddr 0x%lx\n", __FUNCTION__,
                (ULONG)range->RangeStart.QuadPart, range->RangeLength, dev->vram_start));
    return NO_ERROR;
}

#if defined(ALLOC_PRAGMA)
VP_STATUS Prob(QXLExtension *dev, VIDEO_PORT_CONFIG_INFO *conf_info,
               PVIDEO_ACCESS_RANGE ranges, int n_ranges);
#pragma alloc_text(PAGE, Prob)
#endif

VP_STATUS Prob(QXLExtension *dev, VIDEO_PORT_CONFIG_INFO *conf_info,
               PVIDEO_ACCESS_RANGE ranges, int n_ranges)
{
    PCI_COMMON_CONFIG pci_conf;
    ULONG  bus_data_size;
    VP_STATUS error;

    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s\n", __FUNCTION__));

    bus_data_size = VideoPortGetBusData(dev,
                                        PCIConfiguration,
                                        0,
                                        &pci_conf,
                                        0,
                                        sizeof(PCI_COMMON_CONFIG));

    if (bus_data_size != sizeof(PCI_COMMON_CONFIG)) {
        DEBUG_PRINT((dev, 0,  "%s: GetBusData size %d expectes %d\n",
                     __FUNCTION__, bus_data_size, sizeof(PCI_COMMON_CONFIG)));
        return ERROR_INVALID_PARAMETER;
    }

    if (pci_conf.VendorID != REDHAT_PCI_VENDOR_ID) {
        DEBUG_PRINT((dev, 0,  "%s: bad vendor id 0x%x expectes 0x%x\n",
                     __FUNCTION__, pci_conf.VendorID, REDHAT_PCI_VENDOR_ID));
        return ERROR_INVALID_PARAMETER;
    }

    if (pci_conf.DeviceID != QXL_DEVICE_ID_STABLE) {
        DEBUG_PRINT((dev, 0,  "%s: bad vendor id 0x%x expectes 0x%x\n",
                     __FUNCTION__, pci_conf.DeviceID, QXL_DEVICE_ID_STABLE));
        return ERROR_INVALID_PARAMETER;
    }

    if (pci_conf.RevisionID < QXL_REVISION_STABLE_V06) {
        DEBUG_PRINT((dev, 0,  "%s: bad revision 0x%x expectes at least 0x%x\n",
                     __FUNCTION__, pci_conf.RevisionID, QXL_REVISION_STABLE_V06));
        return ERROR_INVALID_PARAMETER;
    }
    dev->pci_revision = pci_conf.RevisionID;

    VideoPortZeroMemory(ranges, sizeof(VIDEO_ACCESS_RANGE) * n_ranges);
    if ((error = VideoPortGetAccessRanges(dev, 0, NULL, n_ranges,
                                          ranges, NULL, NULL,
                                          NULL)) != NO_ERROR ) {
        DEBUG_PRINT((dev, 0, "%s: get access ranges failed status %u\n", __FUNCTION__, error));
    }

    if (conf_info->BusInterruptLevel == 0 && conf_info->BusInterruptVector == 0) {
        DEBUG_PRINT((dev, 0, "%s: no interrupt\n", __FUNCTION__));
        error = ERROR_INVALID_DATA;
    }

#ifdef DBG
    if (error == NO_ERROR) {
        int i;

        DEBUG_PRINT((dev, 0, "%s: interrupt: vector %lu level %lu mode %s\n",
                     __FUNCTION__,
                     conf_info->BusInterruptVector,
                     conf_info->BusInterruptLevel,
                     (conf_info->InterruptMode == LevelSensitive) ? "LevelSensitive" : "Latched"));

        for (i = 0; i < n_ranges; i++) {
            DEBUG_PRINT((dev, 0, "%s: range %d start 0x%lx length %lu space %lu\n", __FUNCTION__, i,
                         (ULONG)ranges[i].RangeStart.QuadPart,
                         ranges[i].RangeLength,
                         (ULONG)ranges[i].RangeInIoSpace));
        }
    }
#endif

    DEBUG_PRINT((dev, 0, "%s exit %lu\n", __FUNCTION__, error));
    return error;
}

#if defined(ALLOC_PRAGMA)
void FillVidModeBPP(VIDEO_MODE_INFORMATION *pMode, ULONG bitsR, ULONG bitsG, ULONG bitsB,
                    ULONG maskR, ULONG maskG, ULONG maskB);
#pragma alloc_text(PAGE, FillVidModeBPP)
#endif

/* Fills given video mode BPP related fields */
void FillVidModeBPP(VIDEO_MODE_INFORMATION *pMode, ULONG bitsR, ULONG bitsG, ULONG bitsB,
                    ULONG maskR, ULONG maskG, ULONG maskB)
{
    pMode->NumberRedBits   = bitsR;
    pMode->NumberGreenBits = bitsG;
    pMode->NumberBlueBits  = bitsB;
    pMode->RedMask         = maskR;
    pMode->GreenMask       = maskG;
    pMode->BlueMask        = maskB;
}

#if defined(ALLOC_PRAGMA)
VP_STATUS FillVidModeInfo(VIDEO_MODE_INFORMATION *pMode, ULONG xres, ULONG yres, ULONG bpp, ULONG index);
#pragma alloc_text(PAGE, FillVidModeInfo)
#endif
/* Fills given video mode structure */
VP_STATUS FillVidModeInfo(VIDEO_MODE_INFORMATION *pMode, ULONG xres, ULONG yres, ULONG bpp, ULONG index)
{
    unsigned bytes_pp = (bpp + 7) / 8;

    if (xres <= 0 || yres <= 0)
        return ERROR_INVALID_DATA;

    VideoPortZeroMemory(pMode, sizeof(VIDEO_MODE_INFORMATION));

    /*Common entries*/
    pMode->Length                       = sizeof(VIDEO_MODE_INFORMATION);
    pMode->ModeIndex                    = index;
    pMode->VisScreenWidth               = xres;
    pMode->VisScreenHeight              = yres;
    pMode->ScreenStride                 = (xres * bytes_pp + 3) & ~0x3; /* Pixman requirement */
    pMode->NumberOfPlanes               = 1;
    pMode->BitsPerPlane                 = bpp;
    pMode->Frequency                    = 60;
    pMode->XMillimeter                  = 320;
    pMode->YMillimeter                  = 240;
    pMode->VideoMemoryBitmapWidth       = xres;
    pMode->VideoMemoryBitmapHeight      = yres;
    pMode->DriverSpecificAttributeFlags = 0;
    pMode->AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR;

    /*BPP related entries*/
    switch (bpp)
    {
        case 16:
            FillVidModeBPP(pMode, 5, 5, 5, 0x7C00, 0x3E0, 0x1F);
            break;
        case 24:
        case 32:
            FillVidModeBPP(pMode, 8, 8, 8, 0xFF0000, 0xFF00, 0xFF);
            break;
        default:
            return ERROR_INVALID_DATA;
    }

    return NO_ERROR;
}

#if defined(ALLOC_PRAGMA)
VP_STATUS SetVideoModeInfo(QXLExtension *dev, PVIDEO_MODE_INFORMATION video_mode, QXLMode *qxl_mode);
#pragma alloc_text(PAGE, SetVideoModeInfo)
#endif

VP_STATUS SetVideoModeInfo(QXLExtension *dev, PVIDEO_MODE_INFORMATION video_mode, QXLMode *qxl_mode)
{
    ULONG color_bits;
    PAGED_CODE();
    DEBUG_PRINT((dev, 5, "%s: x %u y %u bits %u stride %u orientation %u\n",
                 __FUNCTION__, qxl_mode->x_res, qxl_mode->y_res,
                 qxl_mode->bits, qxl_mode->stride, qxl_mode->orientation));

    video_mode->Length = sizeof(VIDEO_MODE_INFORMATION);
    video_mode->ModeIndex = qxl_mode->id;
    video_mode->VisScreenWidth = qxl_mode->x_res;
    video_mode->VisScreenHeight = qxl_mode->y_res;
    video_mode->ScreenStride = qxl_mode->stride;
    video_mode->NumberOfPlanes = 1;
    video_mode->BitsPerPlane = qxl_mode->bits;
    video_mode->Frequency = 100;
    video_mode->XMillimeter = qxl_mode->x_mili;
    video_mode->YMillimeter = qxl_mode->y_mili;
    color_bits = (qxl_mode->bits == 16) ? 5 : 8;
    video_mode->NumberRedBits = color_bits;
    video_mode->NumberGreenBits = color_bits;
    video_mode->NumberBlueBits = color_bits;

    video_mode->BlueMask = (1 << color_bits) - 1;
    video_mode->GreenMask = video_mode->BlueMask << color_bits;
    video_mode->RedMask = video_mode->GreenMask << color_bits;

    video_mode->AttributeFlags = VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS;
    video_mode->VideoMemoryBitmapWidth = qxl_mode->x_res;
    video_mode->VideoMemoryBitmapHeight = qxl_mode->y_res;
    video_mode->DriverSpecificAttributeFlags = qxl_mode->orientation;
    DEBUG_PRINT((dev, 5, "%s OK\n", __FUNCTION__));
    return NO_ERROR;
}


#if defined(ALLOC_PRAGMA)
VP_STATUS InitModes(QXLExtension *dev);
#pragma alloc_text(PAGE, InitModes)
#endif

void DestroyMemSlots(QXLExtension *dev)
{
    if (dev->mem_slots) {
        VideoPortFreePool(dev, dev->mem_slots);
        dev->mem_slots = NULL;
    }
}

VP_STATUS InitMemSlots(QXLExtension *dev)
{
#if (WINVER < 0x0501) //Win2K
    error = VideoPortAllocateBuffer(dev, dev->rom->slots_end * sizeof(MemSlot), &dev->mem_slots);

    if(!dev->mem_slots || error != NO_ERROR) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
#else
    if (!(dev->mem_slots = VideoPortAllocatePool(dev, VpPagedPool,
                                                 dev->rom->slots_end * sizeof(MemSlot),
                                                 QXL_ALLOC_TAG))) {
        DEBUG_PRINT((dev, 0, "%s: alloc mem failed\n", __FUNCTION__));
        return ERROR_NOT_ENOUGH_MEMORY;
    }
#endif
    VideoPortZeroMemory(dev->mem_slots, dev->rom->slots_end * sizeof(MemSlot));

    return NO_ERROR;
}

VP_STATUS InitModes(QXLExtension *dev)
{
    QXLRom *rom;
    QXLModes *modes;
    PVIDEO_MODE_INFORMATION modes_info;
    ULONG n_modes;
    ULONG i;
    VP_STATUS error;
    ULONG custom_mode_id = 0;

    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s\n", __FUNCTION__));
    rom = dev->rom;
    modes = (QXLModes *)((UCHAR*)rom + rom->modes_offset);
    if (dev->rom_size < rom->modes_offset + sizeof(QXLModes) ||
        (n_modes = modes->n_modes) == 0 || dev->rom_size <
        rom->modes_offset + sizeof(QXLModes) + n_modes * sizeof(QXLMode)) {
        DEBUG_PRINT((dev, 0, "%s: bad rom size\n", __FUNCTION__));
        return ERROR_INVALID_DATA;
    }

    n_modes += 2;
#if (WINVER < 0x0501) //Win2K
    error = VideoPortAllocateBuffer(dev, n_modes * sizeof(VIDEO_MODE_INFORMATION), &modes_info);

    if(!modes_info || error != NO_ERROR) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
#else
    if (!(modes_info = VideoPortAllocatePool(dev, VpPagedPool,
                                             n_modes * sizeof(VIDEO_MODE_INFORMATION),
                                             QXL_ALLOC_TAG))) {
        DEBUG_PRINT((dev, 0, "%s: alloc mem failed\n", __FUNCTION__));
        return ERROR_NOT_ENOUGH_MEMORY;
    }
#endif
    VideoPortZeroMemory(modes_info, sizeof(VIDEO_MODE_INFORMATION) * n_modes);
    for (i = 0; i < modes->n_modes; i++) {
        error = SetVideoModeInfo(dev, &modes_info[i], &modes->modes[i]);
        if (error != NO_ERROR) {
            VideoPortFreePool(dev, modes_info);
            DEBUG_PRINT((dev, 0, "%s: set video mode failed\n", __FUNCTION__));
            return error;
        }
    }
    /*  Mode ids are increasing in the buffer but may not be consecutive,
     *  so set the first custom id to be 1 larger than the last id in the buffer.
     *  (ModeIndex isn't really an index, it is an ID).
     */
    custom_mode_id = modes->modes[modes->n_modes-1].id + 1;

    /* 2 dummy modes for custom display resolution */
    /* This is necessary to bypass Windows mode index check, that
       would prevent reusing the same index */
    dev->custom_mode = modes->n_modes;

    for (i = dev->custom_mode; i <= dev->custom_mode + 1; ++i) {
        memcpy(&modes_info[i], &modes_info[0], sizeof(VIDEO_MODE_INFORMATION));
        modes_info[i].ModeIndex = custom_mode_id++;
    }

    dev->n_modes = n_modes;
    dev->modes = modes_info;
    DEBUG_PRINT((dev, 0, "%s OK\n", __FUNCTION__));
    return NO_ERROR;
}

#if defined(ALLOC_PRAGMA)
void DevExternsionCleanup(QXLExtension *dev);
#pragma alloc_text(PAGE, DevExternsionCleanup)
#endif

void DevExternsionCleanup(QXLExtension *dev)
{
    if (dev->sleep_event) {
        VideoPortDeleteEvent(dev, dev->sleep_event);
    }

    if (dev->cursor_event) {
        VideoPortDeleteEvent(dev, dev->cursor_event);
    }

    if (dev->display_event) {
        VideoPortDeleteEvent(dev, dev->display_event);
    }

    if (dev->io_cmd_event) {
        VideoPortDeleteEvent(dev, dev->io_cmd_event);
    }

    if (dev->rom) {
        VideoPortUnmapMemory(dev, dev->rom, NULL);
    }

    if (dev->ram_start) {
        VideoPortUnmapMemory(dev, dev->ram_start, NULL);
    }

    if (dev->vram_start) {
        VideoPortUnmapMemory(dev, dev->vram_start, NULL);
    }

    if (dev->modes) {
        VideoPortFreePool(dev, dev->modes);
    }

    if (dev->mem_slots) {
        DestroyMemSlots(dev);
    }

    VideoPortZeroMemory(dev, sizeof(QXLExtension));
}

VP_STATUS FindAdapter(PVOID dev_extension,
                      PVOID reserved,
                      PWSTR arg_str,
                      PVIDEO_PORT_CONFIG_INFO conf_info,
                      PUCHAR again)
{
    QXLExtension *dev_ext = dev_extension;
    VP_STATUS status;
    VIDEO_ACCESS_RANGE ranges[QXL_PCI_RANGES];
    PEVENT display_event = NULL;
    PEVENT cursor_event = NULL;
    PEVENT sleep_event = NULL;
    PEVENT io_cmd_event = NULL;
#if (WINVER >= 0x0501)
    VPOSVERSIONINFO  sys_info;
#endif

    PAGED_CODE();

    DEBUG_PRINT((dev_ext, 0, "%s: enter\n", __FUNCTION__));

#if (WINVER >= 0x0501)
    VideoPortZeroMemory(&sys_info, sizeof(VPOSVERSIONINFO));
    sys_info.Size = sizeof(VPOSVERSIONINFO);
    if ((status = VideoPortGetVersion(dev_ext, &sys_info)) != NO_ERROR ||
        sys_info.MajorVersion < 5 || (sys_info.MajorVersion == 5 && sys_info.MinorVersion < 1) ) {
        DEBUG_PRINT((dev_ext, 0, "%s: err not supported, status %lu major %lu minor %lu\n",
                     __FUNCTION__, status, sys_info.MajorVersion, sys_info.MinorVersion));
        return ERROR_NOT_SUPPORTED;
    }
#endif

    if (conf_info->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {
        DEBUG_PRINT((dev_ext, 0, "%s: err configInfo size\n", __FUNCTION__));
        return ERROR_INVALID_PARAMETER;
    }

    if (conf_info->AdapterInterfaceType != PCIBus) {
        DEBUG_PRINT((dev_ext, 0,  "%s: not a PCI device %d\n",
                     __FUNCTION__, conf_info->AdapterInterfaceType));
        return ERROR_DEV_NOT_EXIST;
    }

    if ((status = VideoPortCreateEvent(dev_ext, 0, NULL, &display_event)) != NO_ERROR) {
        DEBUG_PRINT((dev_ext, 0,  "%s: create display event failed %lu\n",
                     __FUNCTION__, status));
        return status;
    }

    if ((status = VideoPortCreateEvent(dev_ext, 0, NULL, &cursor_event)) != NO_ERROR) {
        DEBUG_PRINT((dev_ext, 0,  "%s: create cursor event failed %lu\n",
                     __FUNCTION__, status));
        VideoPortDeleteEvent(dev_ext, display_event);
        return status;
    }

    if ((status = VideoPortCreateEvent(dev_ext, 0, NULL, &sleep_event)) != NO_ERROR) {
        DEBUG_PRINT((dev_ext, 0,  "%s: create sleep event failed %lu\n",
                     __FUNCTION__, status));
        VideoPortDeleteEvent(dev_ext, display_event);
        VideoPortDeleteEvent(dev_ext, cursor_event);
        return status;
    }

    if ((status = VideoPortCreateEvent(dev_ext, 0, NULL, &io_cmd_event)) != NO_ERROR) {
        DEBUG_PRINT((dev_ext, 0,  "%s: create io_cmd event failed %lu\n",
                     __FUNCTION__, status));
        VideoPortDeleteEvent(dev_ext, sleep_event);
        VideoPortDeleteEvent(dev_ext, display_event);
        VideoPortDeleteEvent(dev_ext, cursor_event);
        return status;
    }

    dev_ext->display_event = display_event;
    dev_ext->cursor_event = cursor_event;
    dev_ext->sleep_event = sleep_event;
    dev_ext->io_cmd_event = io_cmd_event;

    if ((status = Prob(dev_ext, conf_info, ranges, QXL_PCI_RANGES)) != NO_ERROR ||
        (status = InitIO(dev_ext, &ranges[QXL_IO_RANGE_INDEX])) != NO_ERROR ||
        (status = InitRom(dev_ext, &ranges[QXL_ROM_RANGE_INDEX])) != NO_ERROR ||
        (status = InitRam(dev_ext, &ranges[QXL_RAM_RANGE_INDEX])) != NO_ERROR ||
        (status = InitVRAM(dev_ext, &ranges[QXL_VRAM_RANGE_INDEX])) != NO_ERROR ||
        (status = InitModes(dev_ext)) != NO_ERROR ||
        (status = InitMemSlots(dev_ext)) != NO_ERROR) {
        DEBUG_PRINT((dev_ext, 0,  "%s: findAdapter failed\n", __FUNCTION__));
        DevExternsionCleanup(dev_ext);
    }

    if (VideoPortSetRegistryParameters(dev_extension, L"QxlDeviceID",
                                       &dev_ext->rom->id, sizeof(UINT32)) != NO_ERROR) {
        DEBUG_PRINT((dev_ext, 0, "%s: write QXL ID failed\n", __FUNCTION__));
    }

    DEBUG_PRINT((dev_ext, 0, "%s: exit %lu\n", __FUNCTION__, status));
    return status;
}

static BOOLEAN CreateMemSlots(QXLExtension *dev_ext)
{
    QXLMemSlot *slot;
    UINT8 slot_id = dev_ext->rom->slots_start;

    if (slot_id >= dev_ext->rom->slots_end) {
        DEBUG_PRINT((dev_ext, 0, "%s: start_memslot bigger than nmem_slot\n", __FUNCTION__));
        return FALSE;
    }

    dev_ext->mem_slots[slot_id].start_phys_addr = dev_ext->ram_physical.QuadPart;
    dev_ext->mem_slots[slot_id].end_phys_addr = dev_ext->mem_slots[slot_id].start_phys_addr +
                                                dev_ext->rom->surface0_area_size +
                                                dev_ext->rom->num_pages * PAGE_SIZE;

    dev_ext->mem_slots[slot_id].start_virt_addr = (UINT64)dev_ext->ram_start;
    dev_ext->mem_slots[slot_id].end_virt_addr = dev_ext->mem_slots[slot_id].start_virt_addr +
                                                dev_ext->rom->surface0_area_size +
                                                dev_ext->rom->num_pages * PAGE_SIZE;

    dev_ext->ram_header->mem_slot.mem_start = dev_ext->mem_slots[slot_id].start_phys_addr;
    dev_ext->ram_header->mem_slot.mem_end = dev_ext->mem_slots[slot_id].end_phys_addr;

    VideoPortWritePortUchar((PUCHAR)dev_ext->io_port + QXL_IO_MEMSLOT_ADD, slot_id);

    dev_ext->mem_slots[slot_id].generation = dev_ext->rom->slot_generation;

    return TRUE;
}

#if defined(ALLOC_PRAGMA)
void HWReset(QXLExtension *dev_ext);
#pragma alloc_text(PAGE, HWReset)
#endif

/* called from HWReset or after returning from sleep from SetPowerState,
 * when returning from sleep we don't want to do a redundant QXL_IO_RESET */
static void ResetDeviceWithoutIO(QXLExtension *dev_ext)
{
    dev_ext->ram_header->int_mask = WIN_QXL_INT_MASK;
    CreateMemSlots(dev_ext);
}

void HWReset(QXLExtension *dev_ext)
{
    PAGED_CODE();
    DEBUG_PRINT((dev_ext, 0, "%s\n", __FUNCTION__));
    VideoPortWritePortUchar((PUCHAR)dev_ext->io_base + QXL_IO_RESET, 0);
    ResetDeviceWithoutIO(dev_ext);
    DEBUG_PRINT((dev_ext, 0, "%s: done\n", __FUNCTION__));
}

BOOLEAN Initialize(PVOID dev_ext)
{
    PAGED_CODE();
    DEBUG_PRINT((dev_ext, 0, "%s: enter\n", __FUNCTION__));
    HWReset(dev_ext);
    DEBUG_PRINT((dev_ext, 0, "%s: done\n", __FUNCTION__));
    return TRUE;
}

VP_STATUS GetPowerState(PVOID dev_extension,
                        ULONG hw_id,
                        PVIDEO_POWER_MANAGEMENT pm_stat)
{
    QXLExtension *dev = dev_extension;
    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s: %lu\n", __FUNCTION__, pm_stat->PowerState));

    switch (hw_id) {
    case DISPLAY_ADAPTER_HW_ID:
        switch (pm_stat->PowerState) {
        case VideoPowerOn:
        case VideoPowerStandBy:
        case VideoPowerSuspend:
        case VideoPowerOff:
        case VideoPowerShutdown:
        case VideoPowerHibernate:
            DEBUG_PRINT((dev, 0, "%s: OK\n", __FUNCTION__));
            return NO_ERROR;
        }
        break;
    default:
        DEBUG_PRINT((dev, 0, "%s: unexpected hw_id %lu\n", __FUNCTION__, hw_id));
    }
    DEBUG_PRINT((dev, 0, "%s: ERROR_DEVICE_REINITIALIZATION_NEEDED\n", __FUNCTION__));
    return ERROR_DEVICE_REINITIALIZATION_NEEDED;
}

#ifdef DBG
static void DebugZeroDeviceMemory(QXLExtension *dev_ext)
{
    // don't zero the memory if the ram_start and vram_start are not initialized (a
    // device has been installed but the monitor is disabled)
    if (dev_ext->ram_start == 0 || dev_ext->vram_start == 0) {
        DEBUG_PRINT((dev_ext, 0, "%s: not zeroing memory (addresses not initialized)\n", __FUNCTION__));
        return;
    }
    VideoPortZeroMemory(dev_ext->ram_start, dev_ext->ram_size);
    VideoPortZeroMemory(dev_ext->vram_start, dev_ext->vram_size);
}
#else
static _inline void DebugZeroDeviceMemory(QXLExtension *dev_ext)
{
}
#endif

VP_STATUS SetPowerState(PVOID dev_extension,
                        ULONG hw_id,
                        PVIDEO_POWER_MANAGEMENT pm_stat)
{
    QXLExtension *dev_ext = dev_extension;
    PAGED_CODE();
    DEBUG_PRINT((dev_ext, 0, "%s (%d): %d: %lu\n", __FUNCTION__, dev_ext->rom->id, hw_id, pm_stat->PowerState));

    switch (hw_id) {
    case DISPLAY_ADAPTER_HW_ID:
        switch (pm_stat->PowerState) {
        case VideoPowerOn:
            ResetDeviceWithoutIO(dev_ext);
            break;
        case VideoPowerStandBy:
            break;
        case VideoPowerSuspend:
            break;
        case VideoPowerOff:
            DebugZeroDeviceMemory(dev_ext);
            break;
        case VideoPowerShutdown:
            /* Important: you cannot call out to qxldd.dll here or you get a BSOD. */
            break;
        case VideoPowerHibernate:
            DebugZeroDeviceMemory(dev_ext);
            break;
        default:
            DEBUG_PRINT((dev_ext, 0, "%s: unexpected power state\n", __FUNCTION__));
            return ERROR_DEVICE_REINITIALIZATION_NEEDED;
        }
        break;
    default:
        DEBUG_PRINT((dev_ext, 0, "%s: unexpected hw_id %lu\n", __FUNCTION__, hw_id));
        return ERROR_DEVICE_REINITIALIZATION_NEEDED;
    }
    return NO_ERROR;
}

VP_STATUS GetChildDescriptor(IN PVOID dev_extension,
                             IN PVIDEO_CHILD_ENUM_INFO enum_info,
                             OUT PVIDEO_CHILD_TYPE type,
                             OUT PUCHAR descriptor,
                             OUT PULONG uid,
                             OUT PULONG unused)
{
    QXLExtension *dev = dev_extension;
    PAGED_CODE();
    DEBUG_PRINT((dev, 0, "%s: enter\n", __FUNCTION__));

    switch (enum_info->ChildIndex) {
    case 0:
        DEBUG_PRINT((dev, 0, "%s: ACPI id %u\n", __FUNCTION__, enum_info->ACPIHwId));
        return ERROR_NO_MORE_DEVICES;
    case 1:
        DEBUG_PRINT((dev, 0, "%s: Monitor\n", __FUNCTION__));
        /*
        *pChildType = Monitor;
        todo: handle EDID
        return ERROR_MORE_DATA;
        */
        return ERROR_NO_MORE_DEVICES;
    }
    DEBUG_PRINT((dev, 0, "%s: ERROR_NO_MORE_DEVICES\n", __FUNCTION__));
    return ERROR_NO_MORE_DEVICES;
}

#if defined(ALLOC_PRAGMA)
PVIDEO_MODE_INFORMATION FindMode(QXLExtension *dev_ext, ULONG mode);
#pragma alloc_text(PAGE, FindMode)
#endif

#define IsValidMode(dev, mode) (FindMode(dev, mode) != NULL)

PVIDEO_MODE_INFORMATION FindMode(QXLExtension *dev_ext, ULONG mode)
{
    VIDEO_MODE_INFORMATION *inf;
    VIDEO_MODE_INFORMATION *end;

    PAGED_CODE();
    DEBUG_PRINT((dev_ext, 0, "%s\n", __FUNCTION__));

    inf = dev_ext->modes;
    end = inf + dev_ext->n_modes;
    for (; inf < end; inf++) {
        if (inf->ModeIndex == mode) {
            DEBUG_PRINT((dev_ext, 0, "%s: OK mode %lu res %lu-%lu orientation %lu\n", __FUNCTION__,
                         mode, inf->VisScreenWidth, inf->VisScreenHeight,
                         inf->DriverSpecificAttributeFlags ));
            return inf;
        }
    }
    DEBUG_PRINT((dev_ext, 0, "%s: mod info not found\n", __FUNCTION__));
    return NULL;
}

static VP_STATUS SetCustomDisplay(QXLExtension *dev_ext, QXLEscapeSetCustomDisplay *custom_display)
{
    VP_STATUS ret;
    uint32_t xres = custom_display->xres;
    uint32_t yres = custom_display->yres;
    uint32_t bpp = custom_display->bpp;
    PVIDEO_MODE_INFORMATION pMode;

    /* alternate custom mode index */
    if (dev_ext->custom_mode == (dev_ext->n_modes - 1))
        dev_ext->custom_mode = dev_ext->n_modes - 2;
    else
        dev_ext->custom_mode = dev_ext->n_modes - 1;

    if ((xres * yres * bpp / 8) > dev_ext->rom->surface0_area_size) {
        DEBUG_PRINT((dev_ext, 0, "%s: Mode (%dx%d#%d) doesn't fit in memory (%d)\n",
                    __FUNCTION__, xres, yres, bpp, dev_ext->rom->surface0_area_size));
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    pMode = &dev_ext->modes[dev_ext->custom_mode];
    ret = FillVidModeInfo(pMode,
                          custom_display->xres, custom_display->yres,
                          custom_display->bpp,
                          pMode->ModeIndex);
    DEBUG_PRINT((dev_ext, 0, "%s: Returning %d\n", __FUNCTION__, ret));
    return ret;
}

VP_STATUS QXLRegistryCallback(
  PVOID HwDeviceExtension,
  PVOID Context,
  PWSTR ValueName,
  PVOID ValueData,
  ULONG ValueLength
)
{
    QXLExtension *dev_ext = HwDeviceExtension;
    ULONG *key_ret = (ULONG *)Context;

    DEBUG_PRINT((dev_ext, 60, "%s: length %d, first byte %d\n", __FUNCTION__,
                ValueLength, (UINT8)ValueData));

    if (key_ret) {
        *key_ret = *(PULONG)ValueData;
    }
    return NO_ERROR;
}

static UINT8 check_non_primary_surfaces_registry_key(QXLExtension *dev_ext)
{
    VP_STATUS ret;
    ULONG key_ret;

    ret = VideoPortGetRegistryParameters(
              dev_ext,
              L"SurfacesEnabled",
              FALSE,
              QXLRegistryCallback,
              &key_ret);
    if (ret == ERROR_INVALID_PARAMETER) {
        dev_ext->create_non_primary_surfaces = 0;
        DEBUG_PRINT((dev_ext, 0, "%s: SurfacesEnabled key doesn't exist, disabling surfaces\n",
                    __FUNCTION__));
    } else {
        dev_ext->create_non_primary_surfaces = 1;
        DEBUG_PRINT((dev_ext, 0, "%s: SurfacesEnabled key exists, enabling surfaces\n",
                    __FUNCTION__));
    }
    return dev_ext->create_non_primary_surfaces;
}

BOOLEAN StartIO(PVOID dev_extension, PVIDEO_REQUEST_PACKET packet)
{
    QXLExtension *dev_ext = dev_extension;
    VP_STATUS error;

    PAGED_CODE();
    DEBUG_PRINT((dev_ext, 0, "%s %d\n", __FUNCTION__, packet->IoControlCode));

    switch (packet->IoControlCode) {
    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
        DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES\n", __FUNCTION__));
        if (packet->OutputBufferLength < (packet->StatusBlock->Information =
                                          sizeof(VIDEO_NUM_MODES))) {
            error = ERROR_INSUFFICIENT_BUFFER;
            goto err;
        }
        ((PVIDEO_NUM_MODES)packet->OutputBuffer)->NumModes = dev_ext->n_modes;
        ((PVIDEO_NUM_MODES)packet->OutputBuffer)->ModeInformationLength =
        sizeof(VIDEO_MODE_INFORMATION);
        break;
    case IOCTL_VIDEO_QUERY_AVAIL_MODES: {
            VIDEO_MODE_INFORMATION *inf;
            VIDEO_MODE_INFORMATION *end;
            VIDEO_MODE_INFORMATION *out;

            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_QUERY_AVAIL_MODES\n", __FUNCTION__));
            if (packet->OutputBufferLength < (packet->StatusBlock->Information =
                                              dev_ext->n_modes * sizeof(VIDEO_MODE_INFORMATION))) {
                error = ERROR_INSUFFICIENT_BUFFER;
                goto err;
            }
            out = packet->OutputBuffer;
            inf = dev_ext->modes;
            end = inf + dev_ext->n_modes;
            for ( ;inf < end; out++, inf++) {
                *out = *inf;
            }
        }
        break;
    case IOCTL_VIDEO_SET_CURRENT_MODE: {
            ULONG request_mode;
            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_SET_CURRENT_MODE\n", __FUNCTION__));
            if (packet->InputBufferLength < sizeof(VIDEO_MODE)) {
                error = ERROR_INSUFFICIENT_BUFFER;
                goto err;
            }
            request_mode = ((PVIDEO_MODE)packet->InputBuffer)->RequestedMode;

            dev_ext->current_mode = request_mode;
            DEBUG_PRINT((dev_ext, 0, "%s: mode %u\n", __FUNCTION__, request_mode));
            if (!IsValidMode(dev_ext, request_mode)) {
                error = ERROR_INVALID_PARAMETER;
                goto err;
            }
        }
        break;
    case IOCTL_VIDEO_QUERY_CURRENT_MODE: {
            PVIDEO_MODE_INFORMATION inf;

            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_QUERY_CURRENT_MODE\n", __FUNCTION__));

            if (packet->OutputBufferLength < (packet->StatusBlock->Information =
                                              sizeof(VIDEO_MODE_INFORMATION))) {
                error = ERROR_INSUFFICIENT_BUFFER;
                goto err;
            }

            if ((inf = FindMode(dev_ext, dev_ext->current_mode)) == NULL) {
                DEBUG_PRINT((dev_ext, 0, "%s: mod info not found\n", __FUNCTION__));
                error = ERROR_INVALID_DATA;
                goto err;
            }
            *(PVIDEO_MODE_INFORMATION)packet->OutputBuffer = *inf;
        }
        break;
    case IOCTL_VIDEO_MAP_VIDEO_MEMORY: {
            PVIDEO_MEMORY_INFORMATION mem_info;

            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_MAP_VIDEO_MEMORY\n", __FUNCTION__));

            if (packet->OutputBufferLength < (packet->StatusBlock->Information =
                                              sizeof(VIDEO_MEMORY_INFORMATION)) ||
                                            ( packet->InputBufferLength < sizeof(VIDEO_MEMORY) ) ) {
                error = ERROR_INSUFFICIENT_BUFFER;
                goto err;
            }

            ASSERT(((PVIDEO_MEMORY)(packet->InputBuffer))->RequestedVirtualAddress == NULL);
            mem_info = packet->OutputBuffer;
            mem_info->VideoRamBase = mem_info->FrameBufferBase = dev_ext->vram_start;
            mem_info->VideoRamLength = mem_info->FrameBufferLength = dev_ext->vram_size;
#if 0
#ifdef DBG
            DEBUG_PRINT((dev, 0, "%s: zap\n", __FUNCTION__));
            VideoPortZeroMemory(mem_info->VideoRamBase, mem_info->VideoRamLength);
            DEBUG_PRINT((dev, 0, "%s: zap done\n", __FUNCTION__));
#endif
#endif
        }
        break;
    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY: {
            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_UNMAP_VIDEO_MEMORY (do nothing) \n", __FUNCTION__));
        }
        break;
    case IOCTL_VIDEO_RESET_DEVICE:
        DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_VIDEO_RESET_DEVICE\n", __FUNCTION__));
        HWReset(dev_ext);
        break;
    case IOCTL_QXL_GET_INFO: {
            QXLDriverInfo *driver_info;
            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_QXL_GET_INFO\n", __FUNCTION__));

            if (packet->OutputBufferLength < (packet->StatusBlock->Information =
                                              sizeof(QXLDriverInfo))) {
                error = ERROR_INSUFFICIENT_BUFFER;
                goto err;
            }

            driver_info = packet->OutputBuffer;
            driver_info->version = QXL_DRIVER_INFO_VERSION;
            driver_info->pci_revision = dev_ext->pci_revision;
            driver_info->display_event = dev_ext->display_event;
            driver_info->cursor_event = dev_ext->cursor_event;
            driver_info->sleep_event = dev_ext->sleep_event;
            driver_info->io_cmd_event = dev_ext->io_cmd_event;
            driver_info->cmd_ring = &dev_ext->ram_header->cmd_ring;
            driver_info->cursor_ring = &dev_ext->ram_header->cursor_ring;
            driver_info->release_ring = &dev_ext->ram_header->release_ring;
            driver_info->notify_cmd_port = dev_ext->io_port + QXL_IO_NOTIFY_CMD;
            driver_info->notify_cursor_port = dev_ext->io_port + QXL_IO_NOTIFY_CURSOR;
            driver_info->notify_oom_port = dev_ext->io_port + QXL_IO_NOTIFY_OOM;
            driver_info->update_area_async_port = dev_ext->io_port + QXL_IO_UPDATE_AREA_ASYNC;
            driver_info->memslot_add_async_port = dev_ext->io_port + QXL_IO_MEMSLOT_ADD_ASYNC;
            driver_info->create_primary_async_port =
                dev_ext->io_port + QXL_IO_CREATE_PRIMARY_ASYNC;
            driver_info->destroy_primary_async_port =
                dev_ext->io_port + QXL_IO_DESTROY_PRIMARY_ASYNC;
            driver_info->destroy_surface_async_port =
                dev_ext->io_port + QXL_IO_DESTROY_SURFACE_ASYNC;
            driver_info->destroy_all_surfaces_async_port =
                dev_ext->io_port + QXL_IO_DESTROY_ALL_SURFACES_ASYNC;
            driver_info->flush_surfaces_async_port = dev_ext->io_port + QXL_IO_FLUSH_SURFACES_ASYNC;
            driver_info->flush_release_port = dev_ext->io_port + QXL_IO_FLUSH_RELEASE;

            driver_info->log_port = dev_ext->io_port + QXL_IO_LOG;
            driver_info->log_buf = dev_ext->ram_header->log_buf;

            driver_info->surface0_area = dev_ext->ram_start;
            driver_info->surface0_area_size = dev_ext->rom->surface0_area_size;
            driver_info->update_id = &dev_ext->rom->update_id;
            driver_info->mm_clock = &dev_ext->rom->mm_clock;
            driver_info->compression_level = &dev_ext->rom->compression_level;
            driver_info->log_level = &dev_ext->rom->log_level;
            driver_info->update_area_port = dev_ext->io_port + QXL_IO_UPDATE_AREA;
            driver_info->update_area = &dev_ext->ram_header->update_area;
            driver_info->update_surface = &dev_ext->ram_header->update_surface;

            driver_info->num_pages = dev_ext->rom->num_pages;
            driver_info->io_pages_virt = dev_ext->ram_start + driver_info->surface0_area_size;
            driver_info->io_pages_phys = dev_ext->ram_physical.QuadPart +
                                                                    driver_info->surface0_area_size;

            driver_info->main_mem_slot_id = dev_ext->rom->slots_start;
            driver_info->num_mem_slot = dev_ext->rom->slots_end;
            driver_info->slot_gen_bits = dev_ext->rom->slot_gen_bits;
            driver_info->slot_id_bits = dev_ext->rom->slot_id_bits;
            driver_info->slots_generation = &dev_ext->rom->slot_generation;
            driver_info->ram_slot_start = &dev_ext->ram_header->mem_slot.mem_start;
            driver_info->ram_slot_end = &dev_ext->ram_header->mem_slot.mem_end;
            driver_info->main_mem_slot = dev_ext->mem_slots[driver_info->main_mem_slot_id];

#if (WINVER < 0x0501)
            driver_info->WaitForEvent = QXLWaitForEvent;
#endif
            driver_info->destroy_surface_wait_port = dev_ext->io_port + QXL_IO_DESTROY_SURFACE_WAIT;
            driver_info->destroy_all_surfaces_port = dev_ext->io_port + QXL_IO_DESTROY_ALL_SURFACES;
            driver_info->create_primary_port = dev_ext->io_port + QXL_IO_CREATE_PRIMARY;
            driver_info->destroy_primary_port = dev_ext->io_port + QXL_IO_DESTROY_PRIMARY;
            driver_info->memslot_add_port = dev_ext->io_port + QXL_IO_MEMSLOT_ADD;
            driver_info->memslot_del_port = dev_ext->io_port + QXL_IO_MEMSLOT_DEL;
            driver_info->monitors_config_port = dev_ext->io_port + QXL_IO_MONITORS_CONFIG_ASYNC;

            driver_info->primary_surface_create = &dev_ext->ram_header->create_surface;

            driver_info->n_surfaces = dev_ext->rom->n_surfaces;

            driver_info->fb_phys = dev_ext->vram_physical.QuadPart;

            driver_info->dev_id = dev_ext->rom->id;

            driver_info->create_non_primary_surfaces = check_non_primary_surfaces_registry_key(dev_ext);

            driver_info->monitors_config = &dev_ext->ram_header->monitors_config;
        }
        break;

    case IOCTL_QXL_SET_CUSTOM_DISPLAY: {
            QXLEscapeSetCustomDisplay *custom_display;
            DEBUG_PRINT((dev_ext, 0, "%s: IOCTL_QXL_SET_CUSTOM_DISPLAY\n", __FUNCTION__));

            if (packet->InputBufferLength < (packet->StatusBlock->Information =
                                             sizeof(QXLEscapeSetCustomDisplay))) {
                error = ERROR_INSUFFICIENT_BUFFER;
                goto err;
            }

            custom_display = packet->InputBuffer;
            DEBUG_PRINT((dev_ext, 0, "%s: %dx%d@%d\n", __FUNCTION__,
                         custom_display->xres, custom_display->yres,
                         custom_display->bpp));
            error = SetCustomDisplay(dev_ext, custom_display);
            if (error != NO_ERROR) {
                goto err;
            }
        }
        break;

    default:
        DEBUG_PRINT((dev_ext, 0, "%s: invalid command 0x%lx\n", __FUNCTION__, packet->IoControlCode));
        error = ERROR_INVALID_FUNCTION;
        goto err;
    }
    packet->StatusBlock->Status = NO_ERROR;
    DEBUG_PRINT((dev_ext, 0, "%s: OK\n", __FUNCTION__));
    return TRUE;
err:
    packet->StatusBlock->Information = 0;
    packet->StatusBlock->Status = error;
    DEBUG_PRINT((dev_ext, 0, "%s: ERR\n", __FUNCTION__));
    return TRUE;
}

VOID InterruptCallback(PVOID dev_extension, PVOID Context)
{
    QXLExtension *dev_ext = dev_extension;
    UINT32 pending = VideoPortInterlockedExchange(&dev_ext->ram_header->int_pending, 0);

    if (pending & QXL_INTERRUPT_DISPLAY) {
        VideoPortSetEvent(dev_ext, dev_ext->display_event);
    }
    if (pending & QXL_INTERRUPT_CURSOR) {
        VideoPortSetEvent(dev_ext, dev_ext->cursor_event);
    }
    if (pending & QXL_INTERRUPT_IO_CMD) {
        VideoPortSetEvent(dev_ext, dev_ext->io_cmd_event);
    }
    dev_ext->ram_header->int_mask = WIN_QXL_INT_MASK;
    VideoPortWritePortUchar((PUCHAR)dev_ext->io_base + QXL_IO_UPDATE_IRQ, 0);
}

BOOLEAN Interrupt(PVOID dev_extension)
{
    QXLExtension *dev_ext = dev_extension;

    if (!(dev_ext->ram_header->int_pending & dev_ext->ram_header->int_mask)) {
        return FALSE;
    }
    dev_ext->ram_header->int_mask = 0;
    VideoPortWritePortUchar((PUCHAR)dev_ext->io_base + QXL_IO_UPDATE_IRQ, 0);

    if (!VideoPortQueueDpc(dev_extension, InterruptCallback, NULL)) {
        VideoPortLogError(dev_extension, NULL, E_UNEXPECTED, QXLERR_INT_DELIVERY);
        dev_ext->ram_header->int_mask = WIN_QXL_INT_MASK;
        VideoPortWritePortUchar((PUCHAR)dev_ext->io_base + QXL_IO_UPDATE_IRQ, 0);
    }
    return TRUE;
}
