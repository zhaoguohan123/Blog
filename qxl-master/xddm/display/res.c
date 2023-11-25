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

#ifdef DBG
#include <stdio.h>
#endif
#include <ddraw.h>
#include <dxmini.h>
#include "qxldd.h"
#include "os_dep.h"
#include "res.h"
#include "utils.h"
#include "mspace.h"
#include "quic.h"
#include "murmur_hash2a.h"
#include "surface.h"
#include "rop.h"
#include "devioctl.h"
#include "ntddvdeo.h"

static _inline QXLPHYSICAL PA(PDev *pdev, PVOID virt, UINT8 slot_id)
{
    PMemSlot *p_slot = &pdev->mem_slots[slot_id];

    return p_slot->high_bits | ((UINT64)virt - p_slot->slot.start_virt_addr);
}

static _inline UINT64 VA(PDev *pdev, QXLPHYSICAL paddr, UINT8 slot_id)
{
    UINT64 virt;
    PMemSlot *p_slot = &pdev->mem_slots[slot_id];

    ASSERT(pdev, (paddr >> (64 - pdev->slot_id_bits)) == slot_id);
    ASSERT(pdev, ((paddr << pdev->slot_id_bits) >> (64 - pdev->slot_gen_bits)) ==
           p_slot->slot.generation);

    virt = paddr & pdev->va_slot_mask;
    virt += p_slot->slot.start_virt_addr;;

    return virt;
}

#define RELEASE_RES(pdev, res) if (!--(res)->refs) (res)->free(pdev, res);
#define GET_RES(res) (++(res)->refs)

/* Debug helpers - tag each resource with this enum */
enum {
    RESOURCE_TYPE_DRAWABLE = 1,
    RESOURCE_TYPE_SURFACE,
    RESOURCE_TYPE_PATH,
    RESOURCE_TYPE_CLIP_RECTS,
    RESOURCE_TYPE_QUIC_IMAGE,
    RESOURCE_TYPE_BITMAP_IMAGE,
    RESOURCE_TYPE_SURFACE_IMAGE,
    RESOURCE_TYPE_SRING,
    RESOURCE_TYPE_CURSOR,
    RESOURCE_TYPE_BUF,
    RESOURCE_TYPE_UPDATE,
};

#ifdef DBG
#define RESOURCE_TYPE(res, val) do { res->type = val; } while (0)
#else
#define RESOURCE_TYPE(res, val)
#endif

typedef struct Resource Resource;
struct Resource {
    UINT32 refs;
#ifdef DBG
    UINT32 type;
#endif
    void (*free)(PDev *pdev, Resource *res);
    UINT8 res[0];
};

static void FreeMem(PDev* pdev, UINT32 mspace_type, void *ptr);
static BOOL SetClip(PDev *pdev, CLIPOBJ *clip, QXLDrawable *drawable);


#define PUSH_CMD(pdev) do {                             \
    int notify;                                         \
    SPICE_RING_PUSH(pdev->cmd_ring, notify);            \
    if (notify) {                                       \
        sync_io(pdev, pdev->notify_cmd_port, 0);     \
    }                                                   \
} while (0);

#define PUSH_CURSOR_CMD(pdev) do {                      \
    int notify;                                         \
    SPICE_RING_PUSH(pdev->cursor_ring, notify);         \
    if (notify) {                                       \
        sync_io(pdev, pdev->notify_cursor_port, 0);  \
    }                                                   \
} while (0);


#define MAX_OUTPUT_RES 6

typedef struct QXLOutput {
    UINT32 num_res;
#ifdef DBG
    UINT32 type;
#endif
    Resource *resources[MAX_OUTPUT_RES];
    UINT8 data[0];
} QXLOutput;

static int have_sse2 = FALSE;

#ifndef DBG
static _inline void DebugShowOutput(PDev *pdev, QXLOutput* output)
{
}
#else
const char* resource_type_to_string(QXLOutput *output, UINT32 type)
{
    static char buf[1024];

    switch (type) {
    case 0: return "UNSET";
    case RESOURCE_TYPE_DRAWABLE: return "drawable";
    case RESOURCE_TYPE_SURFACE: {
        QXLSurfaceCmd *surface_cmd = (QXLSurfaceCmd*)output->data;
        _snprintf(buf, sizeof(buf) - 1, "surface %u", surface_cmd->surface_id);
        return buf;
    }
    case RESOURCE_TYPE_PATH: return "path";
    case RESOURCE_TYPE_CLIP_RECTS: return "clip_rects";
    case RESOURCE_TYPE_QUIC_IMAGE: return "quic_image";
    case RESOURCE_TYPE_BITMAP_IMAGE: return "bitmap_image";
    case RESOURCE_TYPE_SURFACE_IMAGE: return "surface_image";
    case RESOURCE_TYPE_SRING: return "sring";
    case RESOURCE_TYPE_CURSOR: return "cursor";
    case RESOURCE_TYPE_BUF: return "buf";
    case RESOURCE_TYPE_UPDATE: return "update";
    }
    return "UNDEFINED";
}

static void DebugShowOutput(PDev *pdev, QXLOutput* output)
{
    UINT32 i;

    DEBUG_PRINT((pdev, 11, "output: %s res %d\n", resource_type_to_string(output, output->type),
                output->num_res));
    for (i = 0 ; i < output->num_res ; ++i) {
        DEBUG_PRINT((pdev, 11, "type %s\n", resource_type_to_string(output,
            output->resources[i]->type)));
    }
}
#endif

UINT64 ReleaseOutput(PDev *pdev, UINT64 output_id)
{
    QXLOutput *output = (QXLOutput *)output_id;
    Resource **now;
    Resource **end;
    UINT64 next;

    ASSERT(pdev, output_id);
    DEBUG_PRINT((pdev, 9, "%s 0x%x\n", __FUNCTION__, output));
    DebugShowOutput(pdev, output);

    for (now = output->resources, end = now + output->num_res; now < end; now++) {
        RELEASE_RES(pdev, *now);
    }
    next = *(UINT64*)output->data;
    FreeMem(pdev, MSPACE_TYPE_DEVRAM, output);
    DEBUG_PRINT((pdev, 10, "%s done\n", __FUNCTION__));
    ONDBG(pdev->num_outputs--); //todo: atomic
    return next;
}

static void AddRes(PDev *pdev, QXLOutput *output, Resource *res)
{
    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    ASSERT(pdev, output->num_res < MAX_OUTPUT_RES);
    res->refs++;
    output->resources[output->num_res++] = res;
    DEBUG_PRINT((pdev, 10, "%s: done\n", __FUNCTION__));
}

static _inline void DrawableAddRes(PDev *pdev, QXLDrawable *drawable, Resource *res)
{
    QXLOutput *output;

    output = (QXLOutput *)((UINT8 *)drawable - sizeof(QXLOutput));
    AddRes(pdev, output, res);
}

 
static _inline void SurfaceAddRes(PDev *pdev, QXLSurfaceCmd *surface, Resource *res)
{
    QXLOutput *output;

    output = (QXLOutput *)((UINT8 *)surface - sizeof(QXLOutput));
    AddRes(pdev, output, res);
}

static _inline void CursorCmdAddRes(PDev *pdev, QXLCursorCmd *cmd, Resource *res)
{
    QXLOutput *output;

    output = (QXLOutput *)((UINT8 *)cmd - sizeof(QXLOutput));
    AddRes(pdev, output, res);
}

#define SUPPORT_SURPRISE_REMOVE


/* Called with cursor_sem held */
static void WaitForCursorRing(PDev* pdev)
{
    int wait;

    DEBUG_PRINT((pdev, 9, "%s: 0x%lx\n", __FUNCTION__, pdev));

    for (;;) {
        SPICE_RING_PROD_WAIT(pdev->cursor_ring, wait);

        if (!wait) {
            break;
        }
#ifdef SUPPORT_SURPRISE_REMOVE
        {
            LARGE_INTEGER timeout; // 1 => 100 nanoseconds
            timeout.QuadPart = -1 * (1000 * 1000 * 10); //negative  => relative // 1s
            WAIT_FOR_EVENT(pdev, pdev->cursor_event, &timeout);
            if (SPICE_RING_IS_FULL(pdev->cursor_ring)) {
                DEBUG_PRINT((pdev, 0, "%s: 0x%lx: timeout\n", __FUNCTION__, pdev));
            }
        }
#else
        WAIT_FOR_EVENT(pdev, pdev->cursor_event, NULL);
#endif //SUPPORT_SURPRISE_REMOVE
    }
}

/* Called with cmd_sem held */
static void WaitForCmdRing(PDev* pdev)
{
    int wait;

    DEBUG_PRINT((pdev, 9, "%s: 0x%lx\n", __FUNCTION__, pdev));

    for (;;) {
        SPICE_RING_PROD_WAIT(pdev->cmd_ring, wait);

        if (!wait) {
            break;
        }
        DEBUG_PRINT((pdev, 9, "%s: 0x%lx\n", __FUNCTION__, pdev));

#ifdef SUPPORT_SURPRISE_REMOVE
        {
            LARGE_INTEGER timeout; // 1 => 100 nanoseconds
            timeout.QuadPart = -1 * (1000 * 1000 * 10); //negative  => relative // 1s
            WAIT_FOR_EVENT(pdev, pdev->display_event, &timeout);
            if (SPICE_RING_IS_FULL(pdev->cmd_ring)) {
                DEBUG_PRINT((pdev, 0, "%s: 0x%lx: timeout\n", __FUNCTION__, pdev));
            }
        }
#else
        WAIT_FOR_EVENT(pdev, pdev->display_event, NULL);
#endif //SUPPORT_SURPRISE_REMOVE
    }
}

static void QXLSleep(PDev* pdev, int msec)
{
    LARGE_INTEGER timeout;

    DEBUG_PRINT((pdev, 18, "%s: 0x%lx msec %u\n", __FUNCTION__, pdev, msec));
    timeout.QuadPart = -msec * 1000 * 10;
    WAIT_FOR_EVENT(pdev, pdev->sleep_event, &timeout);
    DEBUG_PRINT((pdev, 19, "%s: 0x%lx exit\n", __FUNCTION__, pdev));
}

/* Called with malloc_sem held */
static void WaitForReleaseRing(PDev* pdev)
{
    int wait;

    DEBUG_PRINT((pdev, 15, "%s: 0x%lx\n", __FUNCTION__, pdev));

    for (;;) {
        LARGE_INTEGER timeout;

        if (SPICE_RING_IS_EMPTY(pdev->release_ring)) {
            QXLSleep(pdev, 10);
            if (!SPICE_RING_IS_EMPTY(pdev->release_ring)) {
                break;
            }
            sync_io(pdev, pdev->notify_oom_port, 0);
        }
        SPICE_RING_CONS_WAIT(pdev->release_ring, wait);

        if (!wait) {
            break;
        }

        timeout.QuadPart = -30 * 1000 * 10; //30ms
        WAIT_FOR_EVENT(pdev, pdev->display_event, &timeout);

        if (SPICE_RING_IS_EMPTY(pdev->release_ring)) {
#ifdef DBG
            DEBUG_PRINT((pdev, 0, "%s: 0x%lx: timeout\n", __FUNCTION__, pdev));
            DEBUG_PRINT((pdev, 0,
                "\tfree %d out %d path %d rect %d bits %d buf %d glyph %d cursor %d\n",
                         pdev->num_free_pages,
                         pdev->num_outputs,
                         pdev->num_path_pages,
                         pdev->num_rects_pages,
                         pdev->num_bits_pages,
                         pdev->num_buf_pages,
                         pdev->num_glyphs_pages,
                         pdev->num_cursor_pages));
#endif
            //oom
            sync_io(pdev, pdev->notify_oom_port, 0);
        }
    }
    DEBUG_PRINT((pdev, 16, "%s: 0x%lx, done\n", __FUNCTION__, pdev));
}

/* Called with malloc_sem held */
static void FlushReleaseRing(PDev *pdev)
{
    UINT64 output;
    int notify;
    int num_to_release = 50;

    output = pdev->free_outputs;

    while (1) {
        while (output != 0) {
            output = ReleaseOutput(pdev, output);
            if (--num_to_release == 0) {
                break;
            }
        }

        if (output != 0 ||
            SPICE_RING_IS_EMPTY(pdev->release_ring)) {
            break;
        }

        output = *SPICE_RING_CONS_ITEM(pdev->release_ring);
        SPICE_RING_POP(pdev->release_ring, notify);
    }

    pdev->free_outputs = output;
}

void EmptyReleaseRing(PDev *pdev)
{
    int count = 0;

    EngAcquireSemaphore(pdev->malloc_sem);
    while (pdev->free_outputs || !SPICE_RING_IS_EMPTY(pdev->release_ring)) {
        FlushReleaseRing(pdev);
        count++;
    }
    EngReleaseSemaphore(pdev->malloc_sem);
    DEBUG_PRINT((pdev, 3, "%s: complete after %d rounds\n", __FUNCTION__, count));
}

// todo: separate VRAM releases from DEVRAM releases
#define AllocMem(pdev, mspace_type, size) __AllocMem(pdev, mspace_type, size, TRUE)
static void *__AllocMem(PDev* pdev, UINT32 mspace_type, size_t size, BOOL force)
{
    UINT8 *ptr;

    ASSERT(pdev, pdev && pdev->mspaces[mspace_type]._mspace);
    DEBUG_PRINT((pdev, 12, "%s: 0x%lx %p(%d) size %u\n", __FUNCTION__, pdev,
        pdev->mspaces[mspace_type]._mspace,
        mspace_footprint(pdev->mspaces[mspace_type]._mspace),
        size));
#ifdef DBG
    if (pdev && pdev->log_level && *pdev->log_level > 11) {
        mspace_malloc_stats(pdev->mspaces[mspace_type]._mspace);
    }
#endif
    EngAcquireSemaphore(pdev->malloc_sem);

    while (1) {
        /* Release lots of queued resources, before allocating, as we
           want to release early to minimize fragmentation risks. */
        FlushReleaseRing(pdev);

        ptr = mspace_malloc(pdev->mspaces[mspace_type]._mspace, size);
        if (ptr) {
            break;
        }

        if (pdev->free_outputs != 0 ||
            !SPICE_RING_IS_EMPTY(pdev->release_ring)) {
            /* We have more things to free, try that */
            continue;
        }

        if (force) {
            /* Ask spice to free some stuff */
            WaitForReleaseRing(pdev);
        } else {
            /* Fail */
            break;
        }
    }

    EngReleaseSemaphore(pdev->malloc_sem);
    ASSERT(pdev, (!ptr && !force) || (ptr >= pdev->mspaces[mspace_type].mspace_start &&
                                      ptr < pdev->mspaces[mspace_type].mspace_end));
    DEBUG_PRINT((pdev, 13, "%s: 0x%lx done 0x%x\n", __FUNCTION__, pdev, ptr));
    return ptr;
}

static void FreeMem(PDev* pdev, UINT32 mspace_type, void *ptr)
{
    ASSERT(pdev, pdev && pdev->mspaces[mspace_type]._mspace);
#ifdef DBG
    if (!((UINT8 *)ptr >= pdev->mspaces[mspace_type].mspace_start &&
                 (UINT8 *)ptr < pdev->mspaces[mspace_type].mspace_end)) {
        DebugPrint(pdev, 0, "ASSERT failed @ %s, %p not in [%p, %p) (%d)\n", __FUNCTION__,
            ptr, pdev->mspaces[mspace_type].mspace_start,
            pdev->mspaces[mspace_type].mspace_end, mspace_type);
        EngDebugBreak();
    }
#endif
    EngAcquireSemaphore(pdev->malloc_sem);
    mspace_free(pdev->mspaces[mspace_type]._mspace, ptr);
    EngReleaseSemaphore(pdev->malloc_sem);
}

static void InitMspace(PDev *pdev, UINT32 mspace_type, UINT8 *start, size_t capacity)
{
    pdev->mspaces[mspace_type]._mspace = create_mspace_with_base(start, capacity, 0, pdev);
    pdev->mspaces[mspace_type].mspace_start = start;
    pdev->mspaces[mspace_type].mspace_end = start + capacity;
}

static void ResetCache(PDev *pdev)
{
    int i;

    RtlZeroMemory(pdev->image_key_lookup,
                  sizeof(pdev->image_key_lookup));
    RtlZeroMemory(pdev->cache_image_pool,
                  sizeof(pdev->cache_image_pool));
    RingInit(&pdev->cache_image_lru);
    for (i = 0; i < IMAGE_POOL_SIZE; i++) {
        RingAdd(pdev, &pdev->cache_image_lru,
                &pdev->cache_image_pool[i].lru_link);
    }

    RtlZeroMemory(pdev->image_cache, sizeof(pdev->image_cache));
    RtlZeroMemory(pdev->cursor_cache, sizeof(pdev->cursor_cache));
    RingInit(&pdev->cursors_lru);
    pdev->num_cursors = 0;
    pdev->last_cursor_id = 0;

    RtlZeroMemory(pdev->palette_cache, sizeof(pdev->palette_cache));
    RingInit(&pdev->palette_lru);
    pdev->num_palettes = 0;
}

/* Init anything that resides on the device memory (pci vram and devram bars).
 * NOTE: TODO better documentation of what is on the guest ram (saved during sleep)
 * and what is on the pci device bars (bar 0 and 1, devram and vram)
 */
void InitDeviceMemoryResources(PDev *pdev)
{
    UINT32 i;

    DEBUG_PRINT((pdev, 0, "%s: %d, %d\n", __FUNCTION__, pdev->num_io_pages * PAGE_SIZE,
                pdev->fb_size));
    RtlZeroMemory(pdev->update_trace_items, sizeof(pdev->update_trace_items));
    RingInit(&pdev->update_trace);
    for (i = 0; i < NUM_UPDATE_TRACE_ITEMS; i++) {
         RingAdd(pdev, &pdev->update_trace, &pdev->update_trace_items[i].link);
    }
    InitMspace(pdev, MSPACE_TYPE_DEVRAM, pdev->io_pages_virt, pdev->num_io_pages * PAGE_SIZE);
    InitMspace(pdev, MSPACE_TYPE_VRAM, pdev->fb, pdev->fb_size);
    ResetCache(pdev);
    pdev->free_outputs = 0;
}

void InitSurfaces(PDev *pdev)
{
    UINT32 i;
    pdev->surfaces_info = (SurfaceInfo *)EngAllocMem(FL_ZERO_MEMORY,
                                                     sizeof(SurfaceInfo) * pdev->n_surfaces,
                                                     ALLOC_TAG);
    if (!pdev->surfaces_info) {
        PANIC(pdev, "surfaces_info allocation failed\n");
    }

    pdev->free_surfaces = &pdev->surfaces_info[1];
    for (i = 0; i < pdev->n_surfaces - 1; i++) {
        pdev->surfaces_info[i].u.next_free = &pdev->surfaces_info[i+1];
    }
}

void ClearResources(PDev *pdev)
{
    if (pdev->surfaces_info) {
        EngFreeMem(pdev->surfaces_info);
        pdev->surfaces_info = NULL;
    }

    if (pdev->malloc_sem) {
        EngDeleteSemaphore(pdev->malloc_sem);
        pdev->malloc_sem = NULL;
    }

    if (pdev->cmd_sem) {
        EngDeleteSemaphore(pdev->cmd_sem);
        pdev->cmd_sem = NULL;
    }

    if (pdev->cursor_sem) {
        EngDeleteSemaphore(pdev->cursor_sem);
        pdev->cursor_sem = NULL;
    }

    if (pdev->print_sem) {
        EngDeleteSemaphore(pdev->print_sem);
        pdev->print_sem = NULL;
    }
}

/*
 * Tell the spice server where to look for updates to the monitor configuration.
 */
void InitMonitorConfig(PDev *pdev)
{
    size_t monitor_config_size   = sizeof(QXLMonitorsConfig) + sizeof(QXLHead);

    pdev->monitor_config      = AllocMem(pdev, MSPACE_TYPE_DEVRAM, monitor_config_size);
    RtlZeroMemory(pdev->monitor_config, monitor_config_size);

    *pdev->monitor_config_pa  = PA(pdev, pdev->monitor_config, pdev->main_mem_slot);
}

void InitResources(PDev *pdev)
{
    DEBUG_PRINT((pdev, 3, "%s: entry\n", __FUNCTION__));

    InitSurfaces(pdev);
    InitDeviceMemoryResources(pdev);
    InitMonitorConfig(pdev);

    pdev->update_id = *pdev->dev_update_id;

    pdev->malloc_sem = EngCreateSemaphore();
    if (!pdev->malloc_sem) {
        PANIC(pdev, "malloc sem creation failed\n");
    }
    pdev->cmd_sem = EngCreateSemaphore();
    if (!pdev->cmd_sem) {
        PANIC(pdev, "cmd sem creation failed\n");
    }
    pdev->cursor_sem = EngCreateSemaphore();
    if (!pdev->cursor_sem) {
        PANIC(pdev, "cursor sem creation failed\n");
    }
    pdev->print_sem = EngCreateSemaphore();
    if (!pdev->print_sem) {
        PANIC(pdev, "print sem creation failed\n");
    }

    ONDBG(pdev->num_outputs = 0);
    ONDBG(pdev->num_path_pages = 0);
    ONDBG(pdev->num_rects_pages = 0);
    ONDBG(pdev->num_bits_pages = 0);
    ONDBG(pdev->num_buf_pages = 0);
    ONDBG(pdev->num_glyphs_pages = 0);
    ONDBG(pdev->num_cursor_pages = 0);

#ifdef CALL_TEST
    pdev->count_calls = TRUE;
    pdev->total_calls = 0;
    for (i = 0; i < NUM_CALL_COUNTERS; i++) {
        pdev->call_counters[i] = 0;
    }
#endif
    DEBUG_PRINT((pdev, 1, "%s: exit\n", __FUNCTION__));
}

static QXLDrawable *GetDrawable(PDev *pdev)
{
    QXLOutput *output;

    output = (QXLOutput *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(QXLOutput) + sizeof(QXLDrawable));
    output->num_res = 0;
    RESOURCE_TYPE(output, RESOURCE_TYPE_DRAWABLE);
    ((QXLDrawable *)output->data)->release_info.id = (UINT64)output;
    DEBUG_PRINT((pdev, 9, "%s 0x%x\n", __FUNCTION__, output));
    ONDBG(pdev->num_outputs++); //todo: atomic
    return(QXLDrawable *)output->data;
}

QXLDrawable *Drawable(PDev *pdev, UINT8 type, RECTL *area, CLIPOBJ *clip, UINT32 surface_id)
{
    QXLDrawable *drawable;

    ASSERT(pdev, pdev && area);

    drawable = GetDrawable(pdev);
    drawable->surface_id = surface_id;
    drawable->type = type;
    drawable->effect = QXL_EFFECT_BLEND;
    drawable->self_bitmap = 0;
    drawable->mm_time = *pdev->mm_clock;
    drawable->surfaces_dest[0] = -1;
    drawable->surfaces_dest[1] = - 1;
    drawable->surfaces_dest[2] = -1;
    CopyRect(&drawable->bbox, area);

    if (!SetClip(pdev, clip, drawable)) {
        DEBUG_PRINT((pdev, 0, "%s: set clip failed\n", __FUNCTION__));
        ReleaseOutput(pdev, drawable->release_info.id);
        drawable = NULL;
    }
    return drawable;
}

void PushDrawable(PDev *pdev, QXLDrawable *drawable)
{
    QXLCommand *cmd;

    EngAcquireSemaphore(pdev->cmd_sem);
    WaitForCmdRing(pdev);
    cmd = SPICE_RING_PROD_ITEM(pdev->cmd_ring);
    cmd->type = QXL_CMD_DRAW;
    cmd->data = PA(pdev, drawable, pdev->main_mem_slot);
    PUSH_CMD(pdev);
    EngReleaseSemaphore(pdev->cmd_sem);
}

static QXLSurfaceCmd *GetSurfaceCmd(PDev *pdev)
{
    QXLOutput *output;

    output = (QXLOutput *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(QXLOutput) + sizeof(QXLSurfaceCmd));
    output->num_res = 0;
    RESOURCE_TYPE(output, RESOURCE_TYPE_SURFACE);
    ((QXLSurfaceCmd *)output->data)->release_info.id = (UINT64)output;
    DEBUG_PRINT((pdev, 9, "%s 0x%x\n", __FUNCTION__, output));
    ONDBG(pdev->num_outputs++); //todo: atomic
    return(QXLSurfaceCmd *)output->data;
}

QXLSurfaceCmd *SurfaceCmd(PDev *pdev, UINT8 type, UINT32 surface_id)
{
    QXLSurfaceCmd *surface_cmd;

    ASSERT(pdev, pdev);

    surface_cmd = GetSurfaceCmd(pdev);
    surface_cmd->surface_id = surface_id;
    surface_cmd->type = type;
    surface_cmd->flags = 0;

    return surface_cmd;
}

void PushSurfaceCmd(PDev *pdev, QXLSurfaceCmd *surface_cmd)
{
    QXLCommand *cmd;

    EngAcquireSemaphore(pdev->cmd_sem);
    WaitForCmdRing(pdev);
    cmd = SPICE_RING_PROD_ITEM(pdev->cmd_ring);
    cmd->type = QXL_CMD_SURFACE;
    cmd->data = PA(pdev, surface_cmd, pdev->main_mem_slot);
    PUSH_CMD(pdev);
    EngReleaseSemaphore(pdev->cmd_sem);
}

QXLPHYSICAL SurfaceToPhysical(PDev *pdev, UINT8 *base_mem)
{
    return PA(pdev, base_mem, pdev->vram_mem_slot);
}

_inline void GetSurfaceMemory(PDev *pdev, UINT32 x, UINT32 y, UINT32 depth, INT32 *stride,
                              UINT8 **base_mem, QXLPHYSICAL *phys_mem, UINT8 allocation_type)
{
    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    switch (allocation_type) {
    case DEVICE_BITMAP_ALLOCATION_TYPE_SURF0:
        ASSERT(pdev, x * y * depth /8 <= pdev->primary_memory_size);
        *base_mem = pdev->primary_memory_start;
        *phys_mem = PA(pdev, *base_mem, pdev->main_mem_slot);
        *stride = (x * depth / 8 + 3) & ~0x3; /* Pixman requires 4 byte aligned stride */
        break;
    case DEVICE_BITMAP_ALLOCATION_TYPE_DEVRAM:
        *stride = x * depth / 8;
        *stride = ALIGN(*stride, 4);
        *base_mem = AllocMem(pdev, MSPACE_TYPE_DEVRAM, (*stride) * y);
        *phys_mem = PA(pdev, *base_mem, pdev->main_mem_slot);
        break;
    case DEVICE_BITMAP_ALLOCATION_TYPE_VRAM:
        *stride = x * depth / 8;
        *stride = ALIGN(*stride, 4);
        *base_mem = __AllocMem(pdev, MSPACE_TYPE_VRAM, (*stride) * y, FALSE);
        *phys_mem = SurfaceToPhysical(pdev, *base_mem);
        break;
    case DEVICE_BITMAP_ALLOCATION_TYPE_RAM:
        /* used only before suspend to sleep (DrvAssertMode(FALSE)) and then released
         * and copied back to VRAM */
        *stride = x * depth / 8;
        *stride = ALIGN(*stride, 4);
        *base_mem = EngAllocMem(0 /* don't zero memory, will be copied over in a bit */,
                                (*stride) * y, ALLOC_TAG);
        *phys_mem = (QXLPHYSICAL)NULL; /* make sure no one uses it */
        break;
    default:
        PANIC(pdev, "No allocation type");
    }
}

void QXLGetSurface(PDev *pdev, QXLPHYSICAL *surface_phys, UINT32 x, UINT32 y, UINT32 depth,
                   INT32 *stride, UINT8 **base_mem, UINT8 allocation_type)
{
    GetSurfaceMemory(pdev, x, y, depth, stride, base_mem, surface_phys, allocation_type);
}

void QXLDelSurface(PDev *pdev, UINT8 *base_mem, UINT8 allocation_type)
{
    switch (allocation_type) {
    case DEVICE_BITMAP_ALLOCATION_TYPE_DEVRAM:
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, base_mem);
        break;
    case DEVICE_BITMAP_ALLOCATION_TYPE_VRAM:
        FreeMem(pdev, MSPACE_TYPE_VRAM, base_mem);
        break;
    case DEVICE_BITMAP_ALLOCATION_TYPE_RAM:
        EngFreeMem(base_mem);
        break;
    default:
        PANIC(pdev, "bad allocation type");
    }
}

typedef struct InternalDelSurface {
    UINT32 surface_id;
    UINT8 allocation_type;
} InternalDelSurface;


static void FreeDelSurface(PDev *pdev, Resource *res)
{
    InternalDelSurface *internal;
    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    internal = (InternalDelSurface *)res->res;
    QXLDelSurface(pdev, GetSurfaceInfo(pdev, internal->surface_id)->draw_area.base_mem,
        internal->allocation_type);
    FreeSurfaceInfo(pdev, internal->surface_id);
    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);

    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}

#define SURFACEDEL_ALLOC_BASE (sizeof(Resource) + sizeof(InternalDelSurface))

void QXLGetDelSurface(PDev *pdev, QXLSurfaceCmd *surface, UINT32 surface_id, UINT8 allocation_type)
{
    Resource *surface_res;
    InternalDelSurface *internal;
    size_t alloc_size;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    alloc_size = SURFACEDEL_ALLOC_BASE;
    surface_res = AllocMem(pdev, MSPACE_TYPE_DEVRAM, alloc_size);
    
    surface_res->refs = 1;
    surface_res->free = FreeDelSurface;
    RESOURCE_TYPE(surface_res, RESOURCE_TYPE_SURFACE);

    internal = (InternalDelSurface *)surface_res->res;
    internal->surface_id = surface_id;
    internal->allocation_type = allocation_type;

    SurfaceAddRes(pdev, surface, surface_res);
    RELEASE_RES(pdev, surface_res);
}

static void FreePath(PDev *pdev, Resource *res)
{
    QXLPHYSICAL chunk_phys;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    chunk_phys = ((QXLPath *)res->res)->chunk.next_chunk;
    while (chunk_phys) {
        QXLDataChunk *chunk = (QXLDataChunk *)VA(pdev, chunk_phys, pdev->main_mem_slot);
        chunk_phys = chunk->next_chunk;
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, chunk);
        ONDBG(pdev->num_path_pages--);
    }
    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
    ONDBG(pdev->num_path_pages--);

    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}

#define NEW_DATA_CHUNK(page_counter, size) {                                    \
    void *ptr = AllocMem(pdev, MSPACE_TYPE_DEVRAM, size + sizeof(QXLDataChunk));                    \
    ONDBG((*(page_counter))++);                                                 \
    chunk->next_chunk = PA(pdev, ptr, pdev->main_mem_slot);                     \
    ((QXLDataChunk *)ptr)->prev_chunk = PA(pdev, chunk, pdev->main_mem_slot);   \
    chunk = (QXLDataChunk *)ptr;                                                \
    chunk->data_size = 0;                                                       \
    chunk->next_chunk = 0;                                                      \
    now = chunk->data;                                                          \
    end = now + size;                                                           \
}

#ifdef DBG
    #define GetPathCommon __GetPathCommon
#else
    #define GetPathCommon(pdev, path, chunk_ptr, now_ptr, end_ptr, data_size, page_counter)\
        __GetPathCommon(pdev, path, chunk_ptr, now_ptr, end_ptr, data_size, NULL)
#endif

#define PATH_PREALLOC_PONTS 20
#define PATH_MAX_ALLOC_PONTS 128
#define PATH_ALLOC_SIZE (sizeof(Resource) + sizeof(QXLPath) + sizeof(QXLPathSeg) +\
                         sizeof(POINTFIX) * PATH_PREALLOC_PONTS)


static void __GetPathCommon(PDev *pdev, PATHOBJ *path, QXLDataChunk **chunk_ptr, UINT8 **now_ptr,
                            UINT8 **end_ptr, UINT32 *data_size, int *page_counter)
{
    QXLDataChunk *chunk = *chunk_ptr;
    UINT8 *now = *now_ptr;
    UINT8 *end = *end_ptr;
    PATHDATA data;
    int more;

    DEBUG_PRINT((pdev, 15, "%s\n", __FUNCTION__));
    PATHOBJ_vEnumStart(path);

    do {
        int pt_buf_size;
        UINT8 *pt_buf;
        QXLPathSeg *seg;

        more = PATHOBJ_bEnum(path, &data);
        if (data.count == 0) {
            break;
        }

        if (end - now < sizeof(QXLPathSeg)) {
            size_t alloc_size = MIN(data.count << 3, sizeof(POINTFIX) * PATH_MAX_ALLOC_PONTS);
            alloc_size += sizeof(QXLPathSeg);
            NEW_DATA_CHUNK(page_counter, alloc_size);
        }
        seg = (QXLPathSeg*)now;
        seg->flags = data.flags;
        seg->count = data.count;
        now = (UINT8 *)seg->points;
        chunk->data_size += sizeof(*seg);
        *data_size +=  sizeof(*seg);
        pt_buf_size = data.count << 3;
        pt_buf = (UINT8 *)data.pptfx;

        do {
            int cp_size;
            if (end == now ) {
                size_t alloc_size = MIN(pt_buf_size, sizeof(POINTFIX) * PATH_MAX_ALLOC_PONTS);
                NEW_DATA_CHUNK(page_counter, alloc_size);
            }

            cp_size = (int)MIN(end - now, pt_buf_size);
            memcpy(now, pt_buf, cp_size);
            chunk->data_size += cp_size;
            *data_size += cp_size;
            now += cp_size;
            pt_buf += cp_size;
            pt_buf_size -= cp_size;
        } while (pt_buf_size);
    } while (more);

    *chunk_ptr = chunk;
    *now_ptr = now;
    *end_ptr = end;
    DEBUG_PRINT((pdev, 17, "%s: done\n", __FUNCTION__));
}

static Resource *__GetPath(PDev *pdev, PATHOBJ *path)
{
    Resource *res;
    QXLPath *qxl_path;
    QXLDataChunk *chunk;
    PATHDATA data;
    UINT8 *now;
    UINT8 *end;
    int more;

    ASSERT(pdev, QXL_PATH_BEGIN == PD_BEGINSUBPATH && QXL_PATH_END == PD_ENDSUBPATH &&
           QXL_PATH_CLOSE == PD_CLOSEFIGURE && QXL_PATH_BEZIER == PD_BEZIERS);

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    res = AllocMem(pdev, MSPACE_TYPE_DEVRAM, PATH_ALLOC_SIZE);
    ONDBG(pdev->num_path_pages++);
    res->refs = 1;
    res->free = FreePath;
    RESOURCE_TYPE(res, RESOURCE_TYPE_PATH);

    qxl_path = (QXLPath *)res->res;
    qxl_path->data_size = 0;
    chunk = &qxl_path->chunk;
    chunk->data_size = 0;
    chunk->prev_chunk = 0;
    chunk->next_chunk = 0;

    now = chunk->data;
    end = (UINT8 *)res + PATH_ALLOC_SIZE;
    GetPathCommon(pdev, path, &chunk, &now, &end, &qxl_path->data_size,
                  &pdev->num_path_pages);

    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
    return res;
}

BOOL QXLGetPath(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *path_phys, PATHOBJ *path)
{
    Resource *path_res;
    ASSERT(pdev, pdev && drawable && path_phys && path);

    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));

    path_res = __GetPath(pdev, path);
    *path_phys = PA(pdev, path_res->res, pdev->main_mem_slot);
    DrawableAddRes(pdev, drawable, path_res);
    RELEASE_RES(pdev, path_res);
    return TRUE;
}


static void FreeClipRects(PDev *pdev, Resource *res)
{
    QXLPHYSICAL chunk_phys;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    chunk_phys = ((QXLClipRects *)res->res)->chunk.next_chunk;
    while (chunk_phys) {
        QXLDataChunk *chunk = (QXLDataChunk *)VA(pdev, chunk_phys, pdev->main_mem_slot);
        chunk_phys = chunk->next_chunk;
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, chunk);
        ONDBG(pdev->num_rects_pages--);
    }
    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
    ONDBG(pdev->num_rects_pages--);

    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}


#define RECTS_NUM_PREALLOC 8
#define RECTS_ALLOC_SIZE (sizeof(Resource) + sizeof(QXLClipRects) + \
                          sizeof(QXLRect) * RECTS_NUM_PREALLOC)
#define RECTS_NUM_ALLOC 20
#define RECTS_CHUNK_ALLOC_SIZE (sizeof(QXLDataChunk) + sizeof(QXLRect) * RECTS_NUM_ALLOC)

static Resource *GetClipRects(PDev *pdev, CLIPOBJ *clip)
{
    Resource *res;
    QXLClipRects *rects;
    QXLDataChunk *chunk;
    QXLRect *dest;
    QXLRect *dest_end;
    int more;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    res = (Resource *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, RECTS_ALLOC_SIZE);
    ONDBG(pdev->num_rects_pages++);
    res->refs = 1;
    res->free = FreeClipRects;
    RESOURCE_TYPE(res, RESOURCE_TYPE_CLIP_RECTS);
    rects = (QXLClipRects *)res->res;
    rects->num_rects = 0;

    chunk = &rects->chunk;
    chunk->data_size = 0;
    chunk->prev_chunk = 0;
    chunk->next_chunk = 0;

    dest = (QXLRect *)chunk->data;
    dest_end = dest + ((RECTS_ALLOC_SIZE - sizeof(Resource) - sizeof(QXLClipRects)) >> 4);

    CLIPOBJ_cEnumStart(clip, TRUE, CT_RECTANGLES, CD_RIGHTDOWN, 0);
    do {
        RECTL *now;
        RECTL *end;
        struct {
            ULONG  count;
            RECTL  rects[20];
        } buf;

        more = CLIPOBJ_bEnum(clip, sizeof(buf), (ULONG *)&buf);
        rects->num_rects += buf.count;
        for (now = buf.rects, end = now + buf.count; now < end; now++, dest++) {
            if (dest == dest_end) {
                void *page = AllocMem(pdev, MSPACE_TYPE_DEVRAM, RECTS_CHUNK_ALLOC_SIZE);
                ONDBG(pdev->num_rects_pages++);
                chunk->next_chunk = PA(pdev, page, pdev->main_mem_slot);
                ((QXLDataChunk *)page)->prev_chunk = PA(pdev, chunk, pdev->main_mem_slot);
                chunk = (QXLDataChunk *)page;
                chunk->data_size = 0;
                chunk->next_chunk = 0;
                dest = (QXLRect *)chunk->data;
                dest_end = dest + RECTS_NUM_ALLOC;
            }
            CopyRect(dest, now);
            chunk->data_size += sizeof(QXLRect);
        }
    } while (more);
    DEBUG_PRINT((pdev, 13, "%s: done, num_rects %d\n", __FUNCTION__, rects->num_rects));
    return res;
}

static BOOL SetClip(PDev *pdev, CLIPOBJ *clip, QXLDrawable *drawable)
{
    Resource *rects_res;

    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));

    if (clip == NULL) {
        drawable->clip.type = SPICE_CLIP_TYPE_NONE;
        DEBUG_PRINT((pdev, 10, "%s: QXL_CLIP_TYPE_NONE\n", __FUNCTION__));
        return TRUE;
    }

    if (clip->iDComplexity == DC_RECT) {
        QXLClipRects *rects;
        rects_res = (Resource *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(Resource) + sizeof(QXLClipRects) +
                                         sizeof(QXLRect));
        rects_res->refs = 1;
        rects_res->free = FreeClipRects;
        RESOURCE_TYPE(rects_res, RESOURCE_TYPE_CLIP_RECTS);
        rects = (QXLClipRects *)rects_res->res;
        rects->num_rects = 1;
        rects->chunk.data_size = sizeof(QXLRect);
        rects->chunk.prev_chunk = 0;
        rects->chunk.next_chunk = 0;
        CopyRect((QXLRect *)rects->chunk.data, &clip->rclBounds);
    } else {
      rects_res = GetClipRects(pdev, clip);
    }

    DrawableAddRes(pdev, drawable, rects_res);
    RELEASE_RES(pdev, rects_res);
    drawable->clip.type = SPICE_CLIP_TYPE_RECTS;
    drawable->clip.data = PA(pdev, rects_res->res, pdev->main_mem_slot);
    DEBUG_PRINT((pdev, 10, "%s: done\n", __FUNCTION__));
    return TRUE;
}

#ifndef _WIN64

static _inline void fast_memcpy_aligment(void *dest, const void *src, size_t len)
{
    _asm
    {
        mov ecx, len
        mov esi, src
        mov edi, dest

        cmp ecx, 128
        jb try_to_copy64
 
        prefetchnta [esi]
        copy_128:
            prefetchnta [esi + 64]

            movdqa xmm0, [esi]
            movdqa xmm1, [esi + 16]
            movdqa xmm2, [esi + 32]
            movdqa xmm3, [esi + 48]

            prefetchnta [esi + 128]

            movntdq [edi], xmm0
            movntdq [edi + 16], xmm1
            movntdq [edi + 32], xmm2
            movntdq [edi + 48], xmm3

            movdqa xmm0, [esi + 64]
            movdqa xmm1, [esi + 80]
            movdqa xmm2, [esi + 96]
            movdqa xmm3, [esi + 112]

            movntdq [edi + 64], xmm0
            movntdq [edi + 80], xmm1
            movntdq [edi + 96], xmm2
            movntdq [edi + 112], xmm3

            add edi, 128
            add esi, 128
            sub ecx, 128
            cmp ecx, 128
            jae copy_128
 
       try_to_copy64:
            cmp ecx, 64
            jb try_to_copy32

             movdqa xmm0, [esi]
             movdqa xmm1, [esi + 16]
             movdqa xmm2, [esi + 32]
             movdqa xmm3, [esi + 48]

             movntdq [edi], xmm0
             movntdq [edi + 16], xmm1
             movntdq [edi + 32], xmm2
             movntdq [edi + 48], xmm3
             
             add edi, 64
             add esi, 64
             sub ecx, 64
             prefetchnta [esi]

        try_to_copy32:
             cmp ecx, 32
             jb try_to_copy16

             movdqa xmm0, [esi]
             movdqa xmm1, [esi + 16] 
             movntdq [edi], xmm0
             movntdq [edi + 16], xmm1

             add edi, 32 
             add esi, 32 
             sub ecx, 32

        try_to_copy16:
             cmp ecx, 16
             jb try_to_copy4

             movdqa xmm0, [esi]
             movntdq [edi], xmm0

             add edi, 16
             add esi, 16
             sub ecx, 16


        try_to_copy4:
            cmp ecx, 4
            jb try_to_copy_1 
            movsd
            sub ecx, 4
            jmp try_to_copy4

        try_to_copy_1:     
            rep movsb

        sfence
    }
}

static _inline void fast_memcpy_unaligment(void *dest, const void *src, size_t len)
{
    _asm
    {
        mov ecx, len
        mov esi, src
        mov edi, dest

        cmp ecx, 128
        jb try_to_copy64
 
        prefetchnta [esi]
        copy_128:
            prefetchnta [esi + 64]

            movdqu xmm0, [esi]
            movdqu xmm1, [esi + 16]
            movdqu xmm2, [esi + 32]
            movdqu xmm3, [esi + 48]

            prefetchnta [esi + 128]

            movntdq [edi], xmm0
            movntdq [edi + 16], xmm1
            movntdq [edi + 32], xmm2
            movntdq [edi + 48], xmm3

            movdqu xmm0, [esi + 64]
            movdqu xmm1, [esi + 80]
            movdqu xmm2, [esi + 96]
            movdqu xmm3, [esi + 112]

            movntdq [edi + 64], xmm0
            movntdq [edi + 80], xmm1
            movntdq [edi + 96], xmm2
            movntdq [edi + 112], xmm3

            add edi, 128
            add esi, 128
            sub ecx, 128
            cmp ecx, 128
            jae copy_128
 
       try_to_copy64:
            cmp ecx, 64
            jb try_to_copy32

             movdqu xmm0, [esi]
             movdqu xmm1, [esi + 16]
             movdqu xmm2, [esi + 32]
             movdqu xmm3, [esi + 48]

             movntdq [edi], xmm0
             movntdq [edi + 16], xmm1
             movntdq [edi + 32], xmm2
             movntdq [edi + 48], xmm3
             
             add edi, 64
             add esi, 64
             sub ecx, 64
             prefetchnta [esi]

        try_to_copy32:
             cmp ecx, 32
             jb try_to_copy16

             movdqu xmm0, [esi]
             movdqu xmm1, [esi + 16] 
             movntdq [edi], xmm0
             movntdq [edi + 16], xmm1

             add edi, 32 
             add esi, 32 
             sub ecx, 32

        try_to_copy16:
             cmp ecx, 16
             jb try_to_copy4

             movdqu xmm0, [esi]
             movntdq [edi], xmm0

             add edi, 16
             add esi, 16
             sub ecx, 16


        try_to_copy4:
            cmp ecx, 4
            jb try_to_copy_1 
            movsd
            sub ecx, 4
            jmp try_to_copy4

        try_to_copy_1:     
            rep movsb

        sfence
    }
}

#endif

#ifdef DBG
    #define PutBytesAlign __PutBytesAlign
#define PutBytes(pdev, chunk, now, end, src, size, page_counter, alloc_size, use_sse)\
    __PutBytesAlign(pdev, chunk, now, end, src, size, page_counter, alloc_size, 1, use_sse)
#else
#define  PutBytesAlign(pdev, chunk, now, end, src, size, page_counter, alloc_size, alignment, use_sse)\
    __PutBytesAlign(pdev, chunk, now, end, src, size, NULL, alloc_size, alignment, use_sse)
#define  PutBytes(pdev, chunk, now, end, src, size, page_counter, alloc_size, use_sse)\
    __PutBytesAlign(pdev, chunk, now, end, src, size, NULL, alloc_size, 1, use_sse)
#endif

#define BITS_BUF_MAX (64 * 1024)

static void __PutBytesAlign(PDev *pdev, QXLDataChunk **chunk_ptr, UINT8 **now_ptr,
                            UINT8 **end_ptr, UINT8 *src, int size, int *page_counter,
                            size_t alloc_size, uint32_t alignment, BOOL use_sse)
{
    QXLDataChunk *chunk = *chunk_ptr;
    UINT8 *now = *now_ptr;
    UINT8 *end = *end_ptr;
    int offset;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    while (size) {
        int cp_size = (int)MIN(end - now, size);
        if (!cp_size) {
            size_t aligned_size;
            ASSERT(pdev, alloc_size > 0);
            ASSERT(pdev, BITS_BUF_MAX > alignment);
            aligned_size = (int)MIN(alloc_size + alignment - 1, BITS_BUF_MAX);
            aligned_size -=  aligned_size % alignment;
            NEW_DATA_CHUNK(page_counter, aligned_size);
            cp_size = (int)MIN(end - now, size);
        }
#ifndef _WIN64
        if (use_sse) {
            offset = (size_t)now & SSE_MASK;
            if (offset) {
                offset = SSE_ALIGN - offset;
                if (offset >= cp_size) {
                    RtlCopyMemory(now, src, cp_size);
                    src += cp_size;
                    now += cp_size;
                    chunk->data_size += cp_size;
                    size -= cp_size;
                    continue;
                }
                RtlCopyMemory(now, src,  offset);
                now += offset;
                src += offset;
                size -= offset;
                cp_size -= offset;
                chunk->data_size += offset;
            }
    
            if (((size_t)src & SSE_MASK) == 0) {
                fast_memcpy_aligment(now, src, cp_size);
            } else {
                fast_memcpy_unaligment(now, src, cp_size);
            }
        } else {
            RtlCopyMemory(now, src, cp_size);
        }
#else
        RtlCopyMemory(now, src, cp_size);
#endif
        src += cp_size;
        now += cp_size;
        chunk->data_size += cp_size;
        size -= cp_size;
    }
    *chunk_ptr = chunk;
    *now_ptr = now;
    *end_ptr = end;
    DEBUG_PRINT((pdev, 14, "%s: done\n", __FUNCTION__));
}

typedef struct InternalImage {
    CacheImage *cache;
    QXLImage image;
} InternalImage;

#define HSURF_HASH_VAL(h) (((unsigned long)h >> 4) ^ ((unsigned long)(h) >> 8) ^ \
                           ((unsigned long)(h) >> 16) ^ ((unsigned long)(h) >> 24))

#define IMAGE_KEY_HASH_VAL(hsurf) (HSURF_HASH_VAL(hsurf) & IMAGE_KEY_HASH_MASK)

static void ImageKeyPut(PDev *pdev, HSURF hsurf, UINT64 unique, UINT32 key)
{
    ImageKey *image_key;

    if (!unique) {
        return;
    }
    image_key = &pdev->image_key_lookup[IMAGE_KEY_HASH_VAL(hsurf)];
    image_key->hsurf = hsurf;
    image_key->unique = unique;
    image_key->key = key;
}

static BOOL ImageKeyGet(PDev *pdev, HSURF hsurf, UINT64 unique, UINT32 *key)
{
    ImageKey *image_key;
    BOOL res = FALSE;

    if (!unique) {
        return FALSE;
    }
    image_key = &pdev->image_key_lookup[IMAGE_KEY_HASH_VAL(hsurf)];
    if (image_key->hsurf == hsurf && image_key->unique == unique) {
        *key = image_key->key;
        res = TRUE;
    }
    return res;
}

#define IMAGE_HASH_VAL(hsurf) (HSURF_HASH_VAL(hsurf) & IMAGE_HASH_MASK)

static CacheImage *ImageCacheGetByKey(PDev *pdev, UINT32 key, BOOL check_rest,
                                      UINT8 format, UINT32 width, UINT32 height)
{
    CacheImage *cache_image;

    cache_image = pdev->image_cache[IMAGE_HASH_VAL(key)];
    while (cache_image) {
        if (cache_image->key == key && (!check_rest || (cache_image->format == format &&
            cache_image->width == width && cache_image->height == height))) {
            break;
        }
        cache_image = cache_image->next;
    }
    return cache_image;
}

static void ImageCacheAdd(PDev *pdev, CacheImage *cache_image)
{
    int key;

    key = IMAGE_HASH_VAL(cache_image->key);
    cache_image->next = pdev->image_cache[key];
    cache_image->hits = 1;
    pdev->image_cache[key] = cache_image;
}

static void ImageCacheRemove(PDev *pdev, CacheImage *cache_image)
{
    CacheImage **cache_img;

    if (!cache_image->hits) {
        return;
    }
    cache_img = &pdev->image_cache[IMAGE_HASH_VAL(cache_image->key)];
    while (*cache_img) {
        if ((*cache_img)->key == cache_image->key) {
            *cache_img = cache_image->next;
            break;
        }
        cache_img = &(*cache_img)->next;
    }
}

static CacheImage *AllocCacheImage(PDev* pdev)
{
    RingItem *item;
    while (!(item = RingGetTail(pdev, &pdev->cache_image_lru))) {
        /* malloc_sem protects release_ring too */
        EngAcquireSemaphore(pdev->malloc_sem);
        if (pdev->free_outputs == 0 &&
            SPICE_RING_IS_EMPTY(pdev->release_ring)) {
            WaitForReleaseRing(pdev);
        }
        FlushReleaseRing(pdev);
        EngReleaseSemaphore(pdev->malloc_sem);
    }
    RingRemove(pdev, item);
    return CONTAINEROF(item, CacheImage, lru_link);
}

#define IMAGE_HASH_INIT_VAL(width, height, format) \
    ((UINT32)((width) & 0x1FFF) | ((UINT32)((height) & 0x1FFF) << 13) |\
     ((UINT32)(format) << 26))

static _inline void SetImageId(InternalImage *internal, BOOL cache_me, LONG width, LONG height,
                               UINT8 format, UINT32 key)
{
    UINT32 image_info = IMAGE_HASH_INIT_VAL(width, height, format);

    if (cache_me) {
        QXL_SET_IMAGE_ID(&internal->image, ((UINT32)QXL_IMAGE_GROUP_DRIVER << 30) |
                         image_info, key);
        internal->image.descriptor.flags = QXL_IMAGE_CACHE;
    } else {
        QXL_SET_IMAGE_ID(&internal->image, ((UINT32)QXL_IMAGE_GROUP_DRIVER_DONT_CACHE  << 30) |
                         image_info, key);
        internal->image.descriptor.flags = 0;
    }
}

typedef struct InternalPalette {
    UINT32 refs;
    struct InternalPalette *next;
    RingItem lru_link;
    QXLPalette palette;
} InternalPalette;

#define PALETTE_HASH_VAL(unique) ((int)(unique) & PALETTE_HASH_NASKE)

static _inline void ReleasePalette(PDev *pdev, InternalPalette *palette)
{
    ASSERT(pdev, palette);
    DEBUG_PRINT((pdev, 15, "%s\n", __FUNCTION__));
    if (--palette->refs == 0) {
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, palette);
    }
}

static _inline void PaletteCacheRemove(PDev *pdev, InternalPalette *palette)
{
    InternalPalette **internal;
    BOOL found = FALSE;

    DEBUG_PRINT((pdev, 15, "%s\n", __FUNCTION__));

    ASSERT(pdev, palette->palette.unique);
    internal = &pdev->palette_cache[PALETTE_HASH_VAL(palette->palette.unique)];

    while (*internal) {
        if ((*internal)->palette.unique == palette->palette.unique) {
            *internal = palette->next;
            found = TRUE;
            break;
        }
        internal = &(*internal)->next;
    }

    RingRemove(pdev, &palette->lru_link);
    ReleasePalette(pdev, palette);
    pdev->num_palettes--;

    if (!found) {
        DEBUG_PRINT((pdev, 0, "%s: Error: palette 0x%x isn't in cache \n", __FUNCTION__, palette));
        ASSERT(pdev, FALSE);
    } else {
        DEBUG_PRINT((pdev, 16, "%s: done\n", __FUNCTION__));
    }
}

static _inline InternalPalette *PaletteCacheGet(PDev *pdev, UINT32 unique)
{
    InternalPalette *now;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    if (!unique) {
        return NULL;
    }

    now = pdev->palette_cache[PALETTE_HASH_VAL(unique)];
    while (now) {
        if (now->palette.unique == unique) {
            RingRemove(pdev, &now->lru_link);
            RingAdd(pdev, &pdev->palette_lru, &now->lru_link);
            now->refs++;
            DEBUG_PRINT((pdev, 13, "%s: found\n", __FUNCTION__));
            return now;
        }
        now = now->next;
    }
    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
    return NULL;
}

static void PaletteCacheClear(PDev *pdev)
{
    DEBUG_PRINT((pdev, 1, "%s\n", __FUNCTION__));
    while(pdev->num_palettes) {
        ASSERT(pdev, RingGetTail(pdev, &pdev->palette_lru));
        PaletteCacheRemove(pdev, CONTAINEROF(RingGetTail(pdev, &pdev->palette_lru),
                                             InternalPalette, lru_link));
    }
}

static _inline void PaletteCacheAdd(PDev *pdev, InternalPalette *palette)
{
    int key;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    if (!palette->palette.unique) {
        DEBUG_PRINT((pdev, 13, "%s: not unique\n", __FUNCTION__));
        return;
    }

    if (pdev->num_palettes == PALETTE_CACHE_SIZE) {
        ASSERT(pdev, RingGetTail(pdev, &pdev->palette_lru));
        PaletteCacheRemove(pdev, CONTAINEROF(RingGetTail(pdev, &pdev->palette_lru),
                                             InternalPalette, lru_link));
    }

    key = PALETTE_HASH_VAL(palette->palette.unique);
    palette->next = pdev->palette_cache[key];
    pdev->palette_cache[key] = palette;

    RingAdd(pdev, &pdev->palette_lru, &palette->lru_link);
    palette->refs++;
    pdev->num_palettes++;
    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}


static _inline void GetPallette(PDev *pdev, QXLBitmap *bitmap, XLATEOBJ *color_trans)
{
    InternalPalette *internal;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    if (!color_trans || !(color_trans->flXlate & XO_TABLE)) {
        bitmap->palette = 0;
        return;
    }

    if ((internal = PaletteCacheGet(pdev, color_trans->iUniq))) {
        DEBUG_PRINT((pdev, 12, "%s: from cache\n", __FUNCTION__));
        bitmap->palette = PA(pdev, &internal->palette, pdev->main_mem_slot);
        return;
    }

    internal = (InternalPalette *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(InternalPalette) +
                                           (color_trans->cEntries << 2));
    internal->refs = 1;
    RingItemInit(&internal->lru_link);
    bitmap->palette = PA(pdev, &internal->palette, pdev->main_mem_slot);
    internal->palette.unique = color_trans->iUniq;
    internal->palette.num_ents = (UINT16)color_trans->cEntries;

    RtlCopyMemory(internal->palette.ents, color_trans->pulXlate, color_trans->cEntries << 2);
    PaletteCacheAdd(pdev, internal);
    DEBUG_PRINT((pdev, 12, "%s: done\n", __FUNCTION__));
}

static void FreeQuicImage(PDev *pdev, Resource *res) // todo: defer
{
    InternalImage *internal;
    QXLPHYSICAL chunk_phys;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    internal = (InternalImage *)res->res;
    if (internal->cache) {
        RingAdd(pdev, &pdev->cache_image_lru, &internal->cache->lru_link);
        internal->cache->image = NULL;
    }

    chunk_phys = ((QXLDataChunk *)internal->image.quic.data)->next_chunk;
    while (chunk_phys) {
        QXLDataChunk *chunk = (QXLDataChunk *)VA(pdev, chunk_phys, pdev->main_mem_slot);
        chunk_phys = chunk->next_chunk;
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, chunk);
        ONDBG(pdev->num_bits_pages--);

    }
    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
    ONDBG(pdev->num_bits_pages--);
    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}

static _inline QuicImageType GetQuicImageType(UINT8 format)
{
    switch (format) {
    case SPICE_BITMAP_FMT_32BIT:
        return QUIC_IMAGE_TYPE_RGB32;
    case SPICE_BITMAP_FMT_16BIT:
        return QUIC_IMAGE_TYPE_RGB16;
    case SPICE_BITMAP_FMT_RGBA:
        return QUIC_IMAGE_TYPE_RGBA;
    case SPICE_BITMAP_FMT_24BIT:
        return QUIC_IMAGE_TYPE_RGB24;
    default:
        return QUIC_IMAGE_TYPE_INVALID;
    };
}

#define QUIC_ALLOC_BASE (sizeof(Resource) + sizeof(InternalImage) + sizeof(QXLDataChunk))
#define QUIC_BUF_MAX (64 * 1024)
#define QUIC_BUF_MIN 1024

struct QuicData {
    QuicUsrContext user;
    PDev *pdev;
    QuicContext *quic;
    QXLDataChunk *chunk;
    int chunk_io_words;
    int prev_chunks_io_words;
    int rows;
    int raw_row_size;
};

static int quic_usr_more_space(QuicUsrContext *usr, uint32_t **io_ptr, int rows_completed)
{
    QuicData *usr_data = (QuicData *)usr;
    PDev *pdev = usr_data->pdev;
    QXLDataChunk *new_chank;
    int alloc_size;
    int more;

    ASSERT(pdev, usr_data->rows >= rows_completed);
    more =  (rows_completed - usr_data->rows) * usr_data->raw_row_size;

    alloc_size = MIN(MAX(more >> 4, QUIC_BUF_MIN), QUIC_BUF_MAX);
    new_chank = AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(QXLDataChunk) + alloc_size);
    new_chank->data_size = 0;
    new_chank->prev_chunk = PA(pdev, usr_data->chunk, pdev->main_mem_slot);
    new_chank->next_chunk = 0;

    usr_data->prev_chunks_io_words += usr_data->chunk_io_words;
    usr_data->chunk->data_size = usr_data->chunk_io_words << 2;
    usr_data->chunk->next_chunk = PA(pdev, new_chank, pdev->main_mem_slot);
    usr_data->chunk = new_chank;

    usr_data->chunk_io_words = alloc_size >> 2;

    ONDBG(pdev->num_bits_pages++);

    *io_ptr = (UINT32 *)new_chank->data;
    return usr_data->chunk_io_words;
}

static int quic_usr_more_lines(QuicUsrContext *usr, uint8_t **lines)
{
    return 0;
}

static _inline Resource *GetQuicImage(PDev *pdev, SURFOBJ *surf, XLATEOBJ *color_trans,
                                      BOOL cache_me, LONG width, LONG height, UINT8 format,
                                      UINT8 *src, UINT32 line_size, UINT32 key)
{
    Resource *image_res;
    InternalImage *internal;
    QuicImageType type;
    size_t alloc_size;
    int data_size;
    QuicData *quic_data;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev->quic_data);

    if (!*pdev->compression_level) {
        return NULL;
    }

    if ((type = GetQuicImageType(format)) == QUIC_IMAGE_TYPE_INVALID) {
        DEBUG_PRINT((pdev, 13, "%s: unsupported\n", __FUNCTION__));
        return NULL;
    }

    EngAcquireSemaphore(pdev->quic_data_sem);

    quic_data = pdev->quic_data;

    alloc_size = MIN(QUIC_ALLOC_BASE + (height * line_size >> 4), QUIC_ALLOC_BASE + QUIC_BUF_MAX);
    alloc_size = MAX(alloc_size, QUIC_ALLOC_BASE + QUIC_BUF_MIN);

    image_res = AllocMem(pdev, MSPACE_TYPE_DEVRAM, alloc_size);
    ONDBG(pdev->num_bits_pages++);
    image_res->refs = 1;
    image_res->free = FreeQuicImage;
    RESOURCE_TYPE(image_res, RESOURCE_TYPE_QUIC_IMAGE);

    internal = (InternalImage *)image_res->res;
    SetImageId(internal, cache_me, width, height, format, key);
    internal->image.descriptor.type = SPICE_IMAGE_TYPE_QUIC;
    internal->image.descriptor.width = width;
    internal->image.descriptor.height = height;

    quic_data->chunk = (QXLDataChunk *)internal->image.quic.data;
    quic_data->chunk->data_size = 0;
    quic_data->chunk->prev_chunk = 0;
    quic_data->chunk->next_chunk = 0;
    quic_data->prev_chunks_io_words = 0;
    quic_data->chunk_io_words = (int)(((UINT8 *)image_res + alloc_size - quic_data->chunk->data) >> 2);
    quic_data->rows = height;
    quic_data->raw_row_size = line_size;

    ASSERT(pdev, quic_data->chunk_io_words > 0);
    data_size = quic_encode(quic_data->quic, type, width, height, src, height, surf->lDelta,
                            (UINT32 *)quic_data->chunk->data, quic_data->chunk_io_words);
    if (data_size == QUIC_ERROR) {
        FreeQuicImage(pdev, image_res);
        DEBUG_PRINT((pdev, 13, "%s: error\n", __FUNCTION__));
        image_res = NULL;
        goto out;
    }

    quic_data->chunk->data_size = (data_size - quic_data->prev_chunks_io_words) << 2;
    internal->image.quic.data_size = data_size << 2;
    DEBUG_PRINT((pdev, 13, "%s: done. row size %u quic size %u \n", __FUNCTION__,
                 line_size * height, data_size << 2));

 out:
    EngReleaseSemaphore(pdev->quic_data_sem);

    return image_res;
}

static void FreeBitmapImage(PDev *pdev, Resource *res) // todo: defer
{
    InternalImage *internal;
    QXLPHYSICAL chunk_phys;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    internal = (InternalImage *)res->res;
    if (internal->cache) {
        RingAdd(pdev, &pdev->cache_image_lru, &internal->cache->lru_link);
        internal->cache->image = NULL;
    }

    if (internal->image.bitmap.palette) {
        QXLPalette *palette = (QXLPalette *)VA(pdev, internal->image.bitmap.palette,
					       pdev->main_mem_slot);
        ReleasePalette(pdev, CONTAINEROF(palette, InternalPalette, palette));
    }

    chunk_phys = ((QXLDataChunk *)(&internal->image.bitmap + 1))->next_chunk;
    while (chunk_phys) {
        QXLDataChunk *chunk = (QXLDataChunk *)VA(pdev, chunk_phys, pdev->main_mem_slot);
        chunk_phys = chunk->next_chunk;
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, chunk);
        ONDBG(pdev->num_bits_pages--);

    }

    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
    ONDBG(pdev->num_bits_pages--);
    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}

#ifndef _WIN64

static _inline void RestoreFPU(PDev *pdev, UINT8 FPUSave[])
{
    void *align_addr =  (void *)ALIGN((size_t)(FPUSave), SSE_ALIGN);

    _asm
    {
        mov esi, align_addr

        movdqa xmm0, [esi]
        movdqa xmm1, [esi + 16]
        movdqa xmm2, [esi + 32]
        movdqa xmm3, [esi + 48]
    }
}

static _inline void SaveFPU(PDev *pdev, UINT8 FPUSave[])
{
    void *align_addr =  (void *)ALIGN((size_t)(FPUSave), SSE_ALIGN);

    _asm
    {
        mov edi, align_addr
    
        movdqa [edi], xmm0
        movdqa [edi + 16], xmm1
        movdqa [edi + 32], xmm2
        movdqa [edi + 48], xmm3
    }
}

#endif

static void FreeSurfaceImage(PDev *pdev, Resource *res)
{
    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);

    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}

#define BITMAP_ALLOC_BASE (sizeof(Resource) + sizeof(InternalImage) + sizeof(QXLDataChunk))

static _inline Resource *GetBitmapImage(PDev *pdev, SURFOBJ *surf, XLATEOBJ *color_trans,
                                        BOOL cache_me, LONG width, LONG height, UINT8 format,
                                        UINT8 *src, UINT32 line_size, UINT32 key)
{
    Resource *image_res;
    InternalImage *internal;
    size_t alloc_size;
    QXLDataChunk *chunk;
    UINT8 *src_end;
    UINT8 *dest;
    UINT8 *dest_end;
    UINT8 FPUSave[16 * 4 + 15];
    BOOL use_sse = FALSE;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    ASSERT(pdev, width > 0 && height > 0);

    if (line_size >= BITS_BUF_MAX) {
        DEBUG_PRINT((pdev, 0, "%s: line size (%u) exceeds max (%u)\n", __FUNCTION__,
                     line_size, BITS_BUF_MAX));
        return NULL;
    }
    alloc_size = BITMAP_ALLOC_BASE + BITS_BUF_MAX - BITS_BUF_MAX % line_size;
    alloc_size = MIN(BITMAP_ALLOC_BASE + height * line_size, alloc_size);
    image_res = AllocMem(pdev, MSPACE_TYPE_DEVRAM, alloc_size);
    ONDBG(pdev->num_bits_pages++);

    image_res->refs = 1;
    image_res->free = FreeBitmapImage;
    RESOURCE_TYPE(image_res, RESOURCE_TYPE_BITMAP_IMAGE);

    internal = (InternalImage *)image_res->res;
    SetImageId(internal, cache_me, width, height, format, key);
    internal->image.descriptor.type = SPICE_IMAGE_TYPE_BITMAP;
    chunk = (QXLDataChunk *)(&internal->image.bitmap + 1);
    chunk->data_size = 0;
    chunk->prev_chunk = 0;
    chunk->next_chunk = 0;
    internal->image.bitmap.data = PA(pdev, chunk, pdev->main_mem_slot);
    internal->image.bitmap.flags = 0;
    internal->image.descriptor.width = internal->image.bitmap.x = width;
    internal->image.descriptor.height = internal->image.bitmap.y = height;
    internal->image.bitmap.format = format;
    internal->image.bitmap.stride = line_size;
    src_end = src - surf->lDelta;
    src += surf->lDelta * (height - 1);
    dest = chunk->data;
    dest_end = (UINT8 *)image_res + alloc_size;
    alloc_size = height * line_size;

#ifndef _WIN64
    if (have_sse2 && alloc_size >= 1024) {
        use_sse = TRUE;
        SaveFPU(pdev, FPUSave);
    }
#endif
    for (; src != src_end; src -= surf->lDelta, alloc_size -= line_size) {
        PutBytesAlign(pdev, &chunk, &dest, &dest_end, src, line_size,
                      &pdev->num_bits_pages, alloc_size, line_size, use_sse);
    }
#ifndef _WIN64
    if (use_sse) {
        RestoreFPU(pdev, FPUSave);
    }
#endif

    GetPallette(pdev, &internal->image.bitmap, color_trans);
    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
    return image_res;
}

#define ADAPTIVE_HASH

static _inline UINT32 GetHash(UINT8 *src, INT32 width, INT32 height, UINT8 format, int high_bits_set,
                              UINT32 line_size, LONG stride, XLATEOBJ *color_trans)
{
    UINT32 hash_value = IMAGE_HASH_INIT_VAL(width, height, format);
    UINT8 *row_buf = src;
    UINT8 last_byte = 0;
    UINT8 reminder;
    UINT32 i;
    int row;

    if (color_trans && color_trans->flXlate == XO_TABLE) {
        hash_value = murmurhash2a(color_trans->pulXlate,
                                  sizeof(*color_trans->pulXlate) * color_trans->cEntries,
                                  hash_value);
    }

    if (format == SPICE_BITMAP_FMT_32BIT && stride == line_size) {
        hash_value = murmurhash2ajump3((UINT32 *)row_buf, width * height, hash_value);
    } else {
        for (row = 0; row < height; row++) {
    #ifdef ADAPTIVE_HASH
            if (format ==  SPICE_BITMAP_FMT_32BIT) {
                hash_value = murmurhash2ajump3((UINT32 *)row_buf, width, hash_value);
            } else {
                if (format == SPICE_BITMAP_FMT_4BIT_BE && (width & 0x1)) {
                    last_byte = row_buf[line_size - 1] & 0xF0;
                } else if (format == SPICE_BITMAP_FMT_1BIT_BE && (reminder = width & 0x7)) {
                    last_byte = row_buf[line_size - 1] & ~((1 << (8 - reminder)) - 1);
                }
                if (last_byte) {
                    hash_value = murmurhash2a(row_buf, line_size - 1, hash_value);
                    hash_value = murmurhash2a(&last_byte, 1, hash_value);
                } else {
                    hash_value = murmurhash2a(row_buf, line_size, hash_value);
                }
            }
    #else
            hash_value = murmurhash2a(row_buf, line_size, hash_value);
    #endif
            row_buf += stride;
        }
    }
    if (high_bits_set) {
        hash_value ^= 1;
    }
    return hash_value;
}

static _inline UINT32 GetFormatLineSize(INT32 width, ULONG bitmap_format, UINT8 *format)
{
    switch (bitmap_format) {
    case BMF_32BPP:
        *format = SPICE_BITMAP_FMT_32BIT;
        return width << 2;
    case BMF_24BPP:
        *format = SPICE_BITMAP_FMT_24BIT;
        return width * 3;
    case BMF_16BPP:
        *format = SPICE_BITMAP_FMT_16BIT;
        return width << 1;
    case BMF_8BPP:
        *format = SPICE_BITMAP_FMT_8BIT;
        return width;
    case BMF_4BPP:
        *format = SPICE_BITMAP_FMT_4BIT_BE;
        return ALIGN(width, 2) >> 1;
    case BMF_1BPP:
        *format = SPICE_BITMAP_FMT_1BIT_BE;
        return ALIGN(width, 8) >> 3;
    default:
        return 0;
    }
}

static BOOL CacheSizeTest(PDev *pdev, SURFOBJ *surf)
{
    BOOL ret = (UINT32)surf->sizlBitmap.cx * surf->sizlBitmap.cy <= pdev->max_bitmap_size;
    if (!ret) {
        DEBUG_PRINT((pdev, 1, "%s: cache size test failed x %d y %d max\n",
                     __FUNCTION__,
                     surf->sizlBitmap.cx,
                     surf->sizlBitmap.cy,
                     pdev->max_bitmap_size));
    }
    return ret;
}

static _inline UINT64 get_unique(SURFOBJ *surf, XLATEOBJ *color_trans)
{
    ULONG pallette_unique = color_trans ? color_trans->iUniq : 0;

    // NOTE: GDI sometimes gives many instances of the exactly same SURFOBJ (hsurf & iUniq),
    // but with (fjBitmap & BMF_DONTCACHE). This opposed to what documented in the MSDN.
    if (!surf->iUniq || (surf->fjBitmap & BMF_DONTCACHE) || !pallette_unique) {
        return 0;
    } else {
        return (surf->iUniq | ((UINT64)pallette_unique << 32));
    }
}

BOOL QXLCheckIfCacheImage(PDev *pdev, SURFOBJ *surf, XLATEOBJ *color_trans)
{
    CacheImage *cache_image;
    UINT64 gdi_unique;
    UINT32 key;
    UINT8 format;

    gdi_unique = get_unique(surf, color_trans);

    if (!ImageKeyGet(pdev, surf->hsurf, gdi_unique, &key)) {
        return FALSE;
    }

    switch (surf->iBitmapFormat) {
    case BMF_32BPP:
        format = SPICE_BITMAP_FMT_32BIT;
        break;
    case BMF_24BPP:
        format = SPICE_BITMAP_FMT_24BIT;
        break;
    case BMF_16BPP:
        format = SPICE_BITMAP_FMT_16BIT;
        break;
    case BMF_8BPP:
        format = SPICE_BITMAP_FMT_8BIT;
        break;
    case BMF_4BPP:
        format = SPICE_BITMAP_FMT_4BIT_BE;
        break;
    case BMF_1BPP:
        format = SPICE_BITMAP_FMT_1BIT_BE;
    }


    if ((cache_image = ImageCacheGetByKey(pdev, key, TRUE, format,
                                          surf->sizlBitmap.cx,
                                          surf->sizlBitmap.cy))) {
        return TRUE;
    }

    return FALSE;
}

static CacheImage *GetCacheImage(PDev *pdev, SURFOBJ *surf, XLATEOBJ *color_trans,
                                 BOOL has_alpha, BOOL high_bits_set, UINT32 *hash_key)
{
    CacheImage *cache_image;
    UINT64 gdi_unique;
    UINT32 key;
    UINT8 format;
    UINT32 line_size;

    gdi_unique = get_unique(surf, color_trans);

    if (!(line_size = GetFormatLineSize(surf->sizlBitmap.cx, surf->iBitmapFormat, &format))) {
        DEBUG_PRINT((pdev, 0, "%s: bitmap format err\n", __FUNCTION__));
        return FALSE;
    }

    if (has_alpha) {
        ASSERT(pdev, surf->iBitmapFormat == BMF_32BPP);
        format = SPICE_BITMAP_FMT_RGBA;
    }
    if (!ImageKeyGet(pdev, surf->hsurf, gdi_unique, &key)) {
        key = GetHash(surf->pvScan0, surf->sizlBitmap.cx, surf->sizlBitmap.cy, format,
                      high_bits_set, line_size, surf->lDelta, color_trans);
        ImageKeyPut(pdev, surf->hsurf, gdi_unique, key);
        DEBUG_PRINT((pdev, 11, "%s: ImageKeyPut %u\n", __FUNCTION__, key));
    } else {
        DEBUG_PRINT((pdev, 11, "%s: ImageKeyGet %u\n", __FUNCTION__, key));
    }

    if (hash_key) {
        *hash_key = key;
    }

    if ((cache_image = ImageCacheGetByKey(pdev, key, TRUE, format,
                                          surf->sizlBitmap.cx,
                                          surf->sizlBitmap.cy))) {
        cache_image->hits++;
        DEBUG_PRINT((pdev, 11, "%s: ImageCacheGetByKey %u hits %u\n", __FUNCTION__,
                     key, cache_image->hits));
        return cache_image;
    }

    if (CacheSizeTest(pdev, surf)) {
        CacheImage *cache_image;
        cache_image = AllocCacheImage(pdev);
        ImageCacheRemove(pdev, cache_image);
        cache_image->key = key;
        cache_image->image = NULL;
        cache_image->format = format;
        cache_image->width = surf->sizlBitmap.cx;
        cache_image->height = surf->sizlBitmap.cy;
        ImageCacheAdd(pdev, cache_image);
        RingAdd(pdev, &pdev->cache_image_lru, &cache_image->lru_link);
        DEBUG_PRINT((pdev, 11, "%s: ImageCacheAdd %u\n", __FUNCTION__, key));
    }
    return NULL;
}

// TODO: reconsider
static HSEMAPHORE image_id_sem = NULL;

static _inline UINT32 get_image_serial()
{
    static UINT32 image_id = 0; // move to dev mem and use InterlockedIncrement
    UINT32 ret = 0;

    EngAcquireSemaphore(image_id_sem);
    ret = ++image_id;
    EngReleaseSemaphore(image_id_sem);
    return ret;
}

static int rgb32_data_has_alpha(int width, int height, int stride,
                                UINT8 *data, int *all_set_out)
{
    UINT32 *line, *end, alpha;
    int has_alpha;

    has_alpha = FALSE;
    while (height-- > 0) {
        line = (UINT32 *)data;
        end = line + width;
        data += stride;
        while (line != end) {
            alpha = *line & 0xff000000U;
            if (alpha != 0) {
                has_alpha = TRUE;
                if (alpha != 0xff000000U) {
                    *all_set_out = FALSE;
                    return TRUE;
                }
            }
            line++;
        }
    }

    *all_set_out = has_alpha;
    return has_alpha;
}

BOOL QXLGetBitmap(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *image_phys, SURFOBJ *surf,
                  QXLRect *area, XLATEOBJ *color_trans, UINT32 *hash_key, BOOL use_cache,
                  INT32 *surface_dest)
{
    Resource *image_res;
    InternalImage *internal;
    CacheImage *cache_image;
    UINT32 key;
    UINT8 format;
    UINT32 line_size;
    UINT8 *src;
    int high_bits_set;
    INT32 width = area->right - area->left;
    INT32 height = area->bottom - area->top;

    ASSERT(pdev, !hash_key || use_cache);
    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    if (surf->iType != STYPE_BITMAP) {
        UINT32 alloc_size;

        DEBUG_PRINT((pdev, 9, "%s: copy from device\n", __FUNCTION__));

        alloc_size = sizeof(Resource) + sizeof(InternalImage);
        image_res = AllocMem(pdev, MSPACE_TYPE_DEVRAM, alloc_size);

        ONDBG(pdev->num_bits_pages++);
        image_res->refs = 1;
        image_res->free = FreeSurfaceImage;
        RESOURCE_TYPE(image_res, RESOURCE_TYPE_SURFACE_IMAGE);

        internal = (InternalImage *)image_res->res;

        SetImageId(internal, FALSE, 0, 0, 0, 0);
        internal->image.descriptor.type = SPICE_IMAGE_TYPE_SURFACE;
        internal->image.descriptor.width = 0;
        internal->image.descriptor.height = 0;
        *surface_dest = internal->image.surface_image.surface_id = GetSurfaceId(surf);

        *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);

        DrawableAddRes(pdev, drawable, image_res);

        RELEASE_RES(pdev, image_res);

        return TRUE;
    }

    if (area->left < 0 || area->right > surf->sizlBitmap.cx ||
        area->top < 0 || area->bottom > surf->sizlBitmap.cy) {
        DEBUG_PRINT((pdev, 0, "%s: bad dimensions\n", __FUNCTION__));
        return FALSE;
    }

    high_bits_set = FALSE;
    if (surf->iBitmapFormat == BMF_32BPP) {
        if (rgb32_data_has_alpha(width, height, surf->lDelta,
                                 (UINT8 *)surf->pvScan0 + area->left * 4,
                                 &high_bits_set) &&
            !high_bits_set) {
            return QXLGetAlphaBitmap(pdev, drawable, image_phys,
                                     surf, area, surface_dest, color_trans);
        }
    }

    DEBUG_PRINT((pdev, 11, "%s: iUniq=%x DONTCACHE=%x w=%d h=%d cx=%d cy=%d "
                 "hsurf=%x ctiUniq=%x XO_TABLE=%u format=%u\n", __FUNCTION__,
                 surf->iUniq, surf->fjBitmap & BMF_DONTCACHE, width, height,
                 surf->sizlBitmap.cx, surf->sizlBitmap.cy, surf->hsurf,
                 color_trans ? color_trans->iUniq : 0,
                 color_trans ? !!(color_trans->flXlate & XO_TABLE) : 0,
                 surf->iBitmapFormat));

    if (use_cache) {
        cache_image = GetCacheImage(pdev, surf, color_trans, FALSE, high_bits_set, hash_key);
        if (cache_image && cache_image->image) {
            DEBUG_PRINT((pdev, 11, "%s: cached image found %u\n", __FUNCTION__, cache_image->key));
            internal = cache_image->image;
            *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);
            image_res = (Resource *)((UINT8 *)internal - sizeof(Resource));
            DrawableAddRes(pdev, drawable, image_res);
            return TRUE;
        }
    } else {
        cache_image = NULL;
    }

    if (cache_image) {
        key = cache_image->key;
        width = surf->sizlBitmap.cx;
        height = surf->sizlBitmap.cy;
        src = surf->pvScan0;
    } else {
        int scan0_offset;
        int dx;

        key = get_image_serial();
        switch (surf->iBitmapFormat) {
        case BMF_32BPP:
            dx = 0;
            scan0_offset = area->left << 2;
            break;
        case BMF_24BPP:
            dx = 0;
            scan0_offset = area->left * 3;
            break;
        case BMF_16BPP:
            dx = 0;
            scan0_offset = area->left << 1;
            break;
        case BMF_8BPP:
            dx = 0;
            scan0_offset = area->left;
            break;
        case BMF_4BPP:
            dx = area->left & 0x01;
            scan0_offset = (area->left & ~0x01) >> 1;
            break;
        case BMF_1BPP:
            dx = area->left & 0x07;
            scan0_offset = (area->left & ~0x07) >> 3;
            break;
        default:
            DEBUG_PRINT((pdev, 0, "%s: bitmap format err\n", __FUNCTION__));
            return FALSE;
        }
        width = width + dx;
        src = (UINT8 *)surf->pvScan0 + area->top * surf->lDelta + scan0_offset;

        area->left = dx;
        area->right = width;

        area->top = 0;
        area->bottom = height;
    }

    if (!(line_size = GetFormatLineSize(width, surf->iBitmapFormat, &format))) {
        DEBUG_PRINT((pdev, 0, "%s: bitmap format err\n", __FUNCTION__));
        return FALSE;
    }

    if (!(image_res = GetQuicImage(pdev, surf, color_trans, !!cache_image, width, height, format,
                                   src, line_size, key))) {
        image_res = GetBitmapImage(pdev, surf, color_trans, !!cache_image, width, height, format,
                                   src, line_size, key);
        if (!image_res) {
            return FALSE;
        }
    }
    internal = (InternalImage *)image_res->res;
    if (high_bits_set) {
        internal->image.descriptor.flags |= QXL_IMAGE_HIGH_BITS_SET;
    }
    if ((internal->cache = cache_image)) {
        DEBUG_PRINT((pdev, 11, "%s: cache_me %u\n", __FUNCTION__, key));
        cache_image->image = internal;
        if (RingItemIsLinked(&cache_image->lru_link)) {
            RingRemove(pdev, &cache_image->lru_link);
        }
    }
    *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);
    DrawableAddRes(pdev, drawable, image_res);
    RELEASE_RES(pdev, image_res);
    return TRUE;
}

BOOL QXLGetAlphaBitmap(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *image_phys,
                       SURFOBJ *surf, QXLRect *area, INT32 *surface_dest, XLATEOBJ *color_trans)
{
    Resource *image_res;
    InternalImage *internal;
    CacheImage *cache_image;
    UINT64 gdi_unique;
    UINT32 key;
    UINT8 *src;
    INT32 width = area->right - area->left;
    INT32 height = area->bottom - area->top;

    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    ASSERT(pdev, area->left >= 0 && area->right <= surf->sizlBitmap.cx &&
           area->top >= 0 && area->bottom <= surf->sizlBitmap.cy);

    DEBUG_PRINT((pdev, 11, "%s: iUniq=%x DONTCACHE=%x w=%d h=%d cx=%d cy=%d "
                 "hsurf=%x format=%u color_trans->iUniq=%x\n", __FUNCTION__, surf->iUniq,
                 surf->fjBitmap & BMF_DONTCACHE, width, height,
                 surf->sizlBitmap.cx, surf->sizlBitmap.cy, surf->hsurf,
                 surf->iBitmapFormat,
                 color_trans ? color_trans->iUniq : 0));

    if (surf->iType != STYPE_BITMAP) {
        UINT32 alloc_size;

        DEBUG_PRINT((pdev, 9, "%s: copy from device\n", __FUNCTION__));

        alloc_size = sizeof(Resource) + sizeof(InternalImage);
        image_res = AllocMem(pdev, MSPACE_TYPE_DEVRAM, alloc_size);

        ONDBG(pdev->num_bits_pages++);
        image_res->refs = 1;
        image_res->free = FreeSurfaceImage;
        RESOURCE_TYPE(image_res, RESOURCE_TYPE_SURFACE_IMAGE);

        internal = (InternalImage *)image_res->res;

        SetImageId(internal, FALSE, 0, 0, 0, 0);
        internal->image.descriptor.type = SPICE_IMAGE_TYPE_SURFACE;
        internal->image.descriptor.width = 0;
        internal->image.descriptor.height = 0;
        *surface_dest = internal->image.surface_image.surface_id = GetSurfaceId(surf);

        *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);
        DrawableAddRes(pdev, drawable, image_res);
        RELEASE_RES(pdev, image_res);

        return TRUE;
    }

    ASSERT(pdev, surf->iBitmapFormat == BMF_32BPP && surf->iType == STYPE_BITMAP);
    cache_image = GetCacheImage(pdev, surf, color_trans, TRUE, FALSE, &key);

    if (cache_image) {
        if (internal = cache_image->image) {
            DEBUG_PRINT((pdev, 11, "%s: cached image found %u\n", __FUNCTION__, key));
            *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);
            image_res = (Resource *)((UINT8 *)internal - sizeof(Resource));
            DrawableAddRes(pdev, drawable, image_res);
            return TRUE;
        }
    }

    if (cache_image) {
        width = surf->sizlBitmap.cx;
        height = surf->sizlBitmap.cy;
        src = surf->pvScan0;
    } else {
        src = (UINT8 *)surf->pvScan0 + area->top * surf->lDelta + (area->left << 2);
        area->left = 0;
        area->right = width;
        area->top = 0;
        area->bottom = height;
    }

    if (!(image_res = GetQuicImage(pdev, surf, NULL, !!cache_image, width, height,
                                   SPICE_BITMAP_FMT_RGBA, src, width << 2, key))) {
        image_res = GetBitmapImage(pdev, surf, NULL, !!cache_image, width, height,
                                   SPICE_BITMAP_FMT_RGBA, src, width << 2, key);
        if (!image_res) {
            return FALSE;
        }
    }
    internal = (InternalImage *)image_res->res;
    if ((internal->cache = cache_image)) {
        DEBUG_PRINT((pdev, 11, "%s: cache_me %u\n", __FUNCTION__, key));
        cache_image->image = internal;
        if (RingItemIsLinked(&cache_image->lru_link)) {
            RingRemove(pdev, &cache_image->lru_link);
        }
    }
    *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);
    DrawableAddRes(pdev, drawable, image_res);
    RELEASE_RES(pdev, image_res);
    return TRUE;
}

BOOL QXLGetBitsFromCache(PDev *pdev, QXLDrawable *drawable, UINT32 hash_key, QXLPHYSICAL *image_phys)
{
    InternalImage *internal;
    CacheImage *cache_image;
    Resource *image_res;

    if ((cache_image = ImageCacheGetByKey(pdev, hash_key, FALSE, 0, 0, 0)) &&
        (internal = cache_image->image)) {
        cache_image->hits++;
        *image_phys = PA(pdev, &internal->image, pdev->main_mem_slot);
        image_res = (Resource *)((UINT8 *)internal - sizeof(Resource));
        DrawableAddRes(pdev, drawable, image_res);
        return TRUE;
    }
    return FALSE;
}

BOOL QXLGetMask(PDev *pdev, QXLDrawable *drawable, QXLQMask *qxl_mask, SURFOBJ *mask, POINTL *pos,
                BOOL invers, LONG width, LONG height, INT32 *surface_dest)
{
    QXLRect area;

    if (!mask) {
        qxl_mask->bitmap = 0;
        return TRUE;
    }

    ASSERT(pdev, pos && qxl_mask && drawable);
    if (mask->iBitmapFormat != BMF_1BPP) {
        DEBUG_PRINT((pdev, 0, "%s: bitmap format err\n", __FUNCTION__));
        return FALSE;
    }

    qxl_mask->flags = invers ? SPICE_MASK_FLAGS_INVERS : 0;

    area.left = pos->x;
    area.right = area.left + width;
    area.top = pos->y;
    area.bottom = area.top + height;

    if (QXLGetBitmap(pdev, drawable, &qxl_mask->bitmap, mask, &area, NULL, NULL, TRUE,
                     surface_dest)) {
        qxl_mask->pos.x = area.left;
        qxl_mask->pos.y = area.top;
        return TRUE;
    }
    return FALSE;
}

static void FreeBuf(PDev *pdev, Resource *res)
{
    ONDBG(pdev->num_buf_pages--);
    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
}

UINT8 *QXLGetBuf(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *buf_phys, UINT32 size)
{
    Resource *buf_res;

    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    if (size > PAGE_SIZE - sizeof(Resource)) {
        DEBUG_PRINT((pdev, 0, "%s: size err\n", __FUNCTION__));
        return NULL;
    }

    buf_res = (Resource *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(Resource) + size);
    ONDBG(pdev->num_buf_pages++);
    buf_res->refs = 1;
    buf_res->free = FreeBuf;
    RESOURCE_TYPE(buf_res, RESOURCE_TYPE_BUF);

    *buf_phys = PA(pdev, buf_res->res, pdev->main_mem_slot);
    DrawableAddRes(pdev, drawable, buf_res);
    RELEASE_RES(pdev, buf_res);
    return buf_res->res;
}

#ifdef UPDATE_CMD
void UpdateArea(PDev *pdev, RECTL *area, UINT32 surface_id)
{
    QXLCommand *cmd;
    QXLOutput *output;
    QXLUpdateCmd *updat_cmd;

    DEBUG_PRINT((pdev, 12, "%s UPDATE_CMD\n", __FUNCTION__));

    output = (QXLOutput *)AllocMem(pdev, sizeof(QXLOutput) + sizeof(QXLUpdateCmd));
    RESOURCE_TYPE(output, RESOURCE_TYPE_UPDATE);
    output->num_res = 0;
    updat_cmd = (QXLUpdateCmd *)output->data;
    updat_cmd->release_info.id = (UINT64)output;
    ONDBG(pdev->num_outputs++); //todo: atomic

    CopyRect(&updat_cmd->area, area);
    updat_cmd->update_id = ++pdev->update_id;
    updat_cmd->surface_id = surface_id;

    EngAcquireSemaphore(pdev->cmd_sem);
    WaitForCmdRing(pdev);
    cmd = SPICE_RING_PROD_ITEM(pdev->cmd_ring);
    cmd->type = QXL_CMD_UPDATE;
    cmd->data = PA(pdev, updat_cmd, pdev->main_mem_slot);
    PUSH_CMD(pdev);
    EngReleaseSemaphore(pdev->cmd_sem);
    do {
#ifdef DBG
        {
            LARGE_INTEGER timeout; // 1 => 100 nanoseconds
            timeout.QuadPart = -1 * (1000 * 1000 * 10); //negative  => relative // 1s
            WAIT_FOR_EVENT(pdev, pdev->display_event, &timeout);
            if (*pdev->dev_update_id != pdev->update_id) {
                DEBUG_PRINT((pdev, 0, "%s: 0x%lx: timeout\n", __FUNCTION__, pdev));
            }
        }
#else
        WAIT_FOR_EVENT(pdev, pdev->display_event, NULL);
#endif // DEBUG
        mb();
    } while (*pdev->dev_update_id != pdev->update_id);
}

#else

void UpdateArea(PDev *pdev, RECTL *area, UINT32 surface_id)
{
    DEBUG_PRINT((pdev, 12, "%s IO\n", __FUNCTION__));
    CopyRect(pdev->update_area, area);
    *pdev->update_surface = surface_id;
    async_io(pdev, ASYNCABLE_UPDATE_AREA, 0);
}

#endif

static _inline void add_rast_glyphs(PDev *pdev, QXLString *str, ULONG count, GLYPHPOS *glyps,
                                    QXLDataChunk **chunk_ptr, UINT8 **now_ptr,
                                    UINT8 **end_ptr, int bpp, POINTL *delta, QXLPoint **str_pos)
{
    GLYPHPOS *glyps_end = glyps + count;
    QXLDataChunk *chunk = *chunk_ptr;
    UINT8 *now = *now_ptr;
    UINT8 *end = *end_ptr;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    for (; glyps < glyps_end; glyps++) {
        QXLRasterGlyph *glyph;
        UINT8 *line;
        UINT8 *end_line;
        UINT32 stride;

        if (end - now < sizeof(*glyph)) {
            NEW_DATA_CHUNK(&pdev->num_glyphs_pages, PAGE_SIZE);
        }

        glyph = (QXLRasterGlyph *)now;
        if (delta) {
            if (*str_pos) {
                glyph->render_pos.x = (*str_pos)->x + delta->x;
                glyph->render_pos.y = (*str_pos)->y + delta->y;
            } else {
                glyph->render_pos.x = glyps->ptl.x;
                glyph->render_pos.y = glyps->ptl.y;
            }
            *str_pos = (QXLPoint *)&glyph->render_pos;
        } else {
            glyph->render_pos.x = glyps->ptl.x;
            glyph->render_pos.y = glyps->ptl.y;
        }
        glyph->glyph_origin.x = glyps->pgdf->pgb->ptlOrigin.x;
        glyph->glyph_origin.y = glyps->pgdf->pgb->ptlOrigin.y;
        glyph->width = (UINT16)glyps->pgdf->pgb->sizlBitmap.cx;
        glyph->height = (UINT16)glyps->pgdf->pgb->sizlBitmap.cy;
        now += sizeof(*glyph);
        chunk->data_size += sizeof(*glyph);
        str->data_size += sizeof(*glyph);
        if (!glyph->height) {
            continue;
        }

        stride = ALIGN(glyph->width * bpp, 8) >> 3;
        end_line = (UINT8 *)glyps->pgdf->pgb->aj - stride;
        line = (UINT8 *)glyps->pgdf->pgb->aj + stride * (glyph->height - 1);

        for (; line != end_line; line -= stride) {
            UINT8 *bits_pos = line;
            UINT8 *bits_end = bits_pos + stride;

            for (; bits_pos != bits_end; bits_pos++) {
                UINT8 val;
                int i;
                if (end - now < sizeof(*bits_pos)) {
                    NEW_DATA_CHUNK(&pdev->num_glyphs_pages, PAGE_SIZE);
                }
                *(UINT8 *)now = *bits_pos;
                now += sizeof(*bits_pos);
                chunk->data_size += sizeof(*bits_pos);
                str->data_size += sizeof(*bits_pos);
            }
        }
    }
    *chunk_ptr = chunk;
    *now_ptr = now;
    *end_ptr = end;
    DEBUG_PRINT((pdev, 14, "%s: done\n", __FUNCTION__));
}
static _inline BOOL add_glyphs(PDev *pdev, QXLString *str, ULONG count, GLYPHPOS *glyps,
                               QXLDataChunk **chunk, UINT8 **now, UINT8 **end, POINTL *delta,
                               QXLPoint  **str_pos)
{
    if (str->flags & SPICE_STRING_FLAGS_RASTER_A1) {
        add_rast_glyphs(pdev, str, count, glyps, chunk, now, end, 1, delta, str_pos);
    } else if (str->flags & SPICE_STRING_FLAGS_RASTER_A4) {
        add_rast_glyphs(pdev, str, count, glyps, chunk, now, end, 4, delta, str_pos);
    }
    return TRUE;
}

static void FreeSring(PDev *pdev, Resource *res)
{
    QXLPHYSICAL chunk_phys;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    chunk_phys = ((QXLString *)res->res)->chunk.next_chunk;
    while (chunk_phys) {
        QXLDataChunk *chunk = (QXLDataChunk *)VA(pdev, chunk_phys, pdev->main_mem_slot);
        chunk_phys = chunk->next_chunk;
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, chunk);
        ONDBG(pdev->num_glyphs_pages--);
    }

    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
    ONDBG(pdev->num_glyphs_pages--);

    DEBUG_PRINT((pdev, 14, "%s: done\n", __FUNCTION__));
}


#define TEXT_ALLOC_SIZE sizeof(Resource) + sizeof(QXLString) + 512

BOOL QXLGetStr(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *str_phys, FONTOBJ *font, STROBJ *str)
{
    Resource *str_res;
    QXLString *qxl_str;
    QXLDataChunk *chunk;
    UINT8 *now;
    UINT8 *end;
    BOOL more;
    static int id_QXLGetStr = 0;
    POINTL  delta;
    POINTL  *delta_ptr;
    QXLPoint  *str_pos;

    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));

    str_res = (Resource *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, TEXT_ALLOC_SIZE);
    ONDBG(pdev->num_glyphs_pages++);
    str_res->refs = 1;
    str_res->free = FreeSring;
    RESOURCE_TYPE(str_res, RESOURCE_TYPE_SRING);

    qxl_str = (QXLString *)str_res->res;
    qxl_str->data_size = 0;
    qxl_str->length = (UINT16)str->cGlyphs;
    qxl_str->flags = 0;

    if (font->flFontType & FO_TYPE_RASTER) {
        qxl_str->flags = (font->flFontType & FO_GRAY16) ?   SPICE_STRING_FLAGS_RASTER_A4 :
                         SPICE_STRING_FLAGS_RASTER_A1;
    }

    chunk = &qxl_str->chunk;
    chunk->data_size = 0;
    chunk->prev_chunk = 0;
    chunk->next_chunk = 0;

    now = chunk->data;
    end = (UINT8 *)str_res + TEXT_ALLOC_SIZE;

    if (str->ulCharInc) {
        str_pos = NULL;
        if (str->flAccel & SO_VERTICAL) {
            delta.x = 0;
            delta.y = (str->flAccel & SO_REVERSED) ? -(LONG)str->ulCharInc : str->ulCharInc;
        } else {
            delta.x = (str->flAccel & SO_REVERSED) ? -(LONG)str->ulCharInc : str->ulCharInc;
            delta.y = 0;
        }
        delta_ptr = &delta;
    } else {
        delta_ptr = NULL;
    }

    STROBJ_vEnumStart(str);

    do {
        ULONG count;
        GLYPHPOS *glyps;

        if (str->pgp) {
            count = str->cGlyphs;
            glyps = str->pgp;
            more = FALSE;
        } else {
            more = STROBJ_bEnum(str, &count, &glyps);

            if (more == DDI_ERROR) {
                goto error;
            }
        }
        if (!add_glyphs(pdev, qxl_str, count, glyps, &chunk, &now, &end, delta_ptr, &str_pos)) {
            goto error;
        }

    } while (more);

    *str_phys = PA(pdev, str_res->res, pdev->main_mem_slot);
    DrawableAddRes(pdev, drawable, str_res);
    RELEASE_RES(pdev, str_res);

    DEBUG_PRINT((pdev, 10, "%s: done size %u\n", __FUNCTION__, qxl_str->data_size));
    return TRUE;

    error:
    FreeSring(pdev, str_res);
    DEBUG_PRINT((pdev, 10, "%s: error\n", __FUNCTION__));
    return FALSE;
}

QXLCursorCmd *CursorCmd(PDev *pdev)
{
    QXLCursorCmd *cursor_cmd;
    QXLOutput *output;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    output = (QXLOutput *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(QXLOutput) + sizeof(QXLCursorCmd));
    output->num_res = 0;
    RESOURCE_TYPE(output, RESOURCE_TYPE_CURSOR);
    cursor_cmd = (QXLCursorCmd *)output->data;
    cursor_cmd->release_info.id = (UINT64)output;
    ONDBG(pdev->num_outputs++); //todo: atomic
    DEBUG_PRINT((pdev, 8, "%s: done\n", __FUNCTION__));
    return cursor_cmd;
}

void PushCursorCmd(PDev *pdev, QXLCursorCmd *cursor_cmd)
{
    QXLCommand *cmd;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    EngAcquireSemaphore(pdev->cursor_sem);
    WaitForCursorRing(pdev);
    cmd = SPICE_RING_PROD_ITEM(pdev->cursor_ring);
    cmd->type = QXL_CMD_CURSOR;
    cmd->data = PA(pdev, cursor_cmd, pdev->main_mem_slot);
    PUSH_CURSOR_CMD(pdev);
    EngReleaseSemaphore(pdev->cursor_sem);
    DEBUG_PRINT((pdev, 8, "%s: done\n", __FUNCTION__));
}

typedef struct InternalCursor {
    struct InternalCursor *next;
    RingItem lru_link;
    HSURF   hsurf;
    ULONG   unique;
    QXLCursor cursor;
} InternalCursor;


#define CURSOR_HASH_VAL(hsurf) (HSURF_HASH_VAL(hsurf) & CURSOR_HASH_NASKE)

static void CursorCacheRemove(PDev *pdev, InternalCursor *cursor)
{
    InternalCursor **internal;
    BOOL found = FALSE;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    ASSERT(pdev, cursor->unique);
    internal = &pdev->cursor_cache[CURSOR_HASH_VAL(cursor->hsurf)];

    while (*internal) {
        if ((*internal)->hsurf == cursor->hsurf) {
            if ((*internal) == cursor) {
                *internal = cursor->next;
                found = TRUE;
                break;
            }
            DEBUG_PRINT((pdev, 0, "%s: unexpected\n", __FUNCTION__));
        }
        internal = &(*internal)->next;
    }

    RingRemove(pdev, &cursor->lru_link);
    RELEASE_RES(pdev, (Resource *)((UINT8 *)cursor - sizeof(Resource)));
    pdev->num_cursors--;

    if (!found) {
        DEBUG_PRINT((pdev, 0, "%s: Error: cursor 0x%x isn't in cache \n", __FUNCTION__, cursor));
        ASSERT(pdev, FALSE);
    } else {
        DEBUG_PRINT((pdev, 16, "%s: done\n", __FUNCTION__));
    }

}

static void CursorCacheClear(PDev *pdev)
{
    DEBUG_PRINT((pdev, 1, "%s\n", __FUNCTION__));
    while (pdev->num_cursors) {
        ASSERT(pdev, RingGetTail(pdev, &pdev->cursors_lru));
        CursorCacheRemove(pdev, CONTAINEROF(RingGetTail(pdev, &pdev->cursors_lru),
                                            InternalCursor, lru_link));
    }
}

static void CursorCacheAdd(PDev *pdev, InternalCursor *cursor)
{
    int key;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));

    if (!cursor->unique) {
        return;
    }

    if (pdev->num_cursors == CURSOR_CACHE_SIZE) {
        ASSERT(pdev, RingGetTail(pdev, &pdev->cursors_lru));
        CursorCacheRemove(pdev, CONTAINEROF(RingGetTail(pdev, &pdev->cursors_lru),
                                            InternalCursor, lru_link));
    }

    key = CURSOR_HASH_VAL(cursor->hsurf);
    cursor->next = pdev->cursor_cache[key];
    pdev->cursor_cache[key] = cursor;

    RingAdd(pdev, &pdev->cursors_lru, &cursor->lru_link);
    GET_RES((Resource *)((UINT8 *)cursor - sizeof(Resource)));
    pdev->num_cursors++;
}

static InternalCursor *CursorCacheGet(PDev *pdev, HSURF hsurf, UINT32 unique)
{
    InternalCursor **internal;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    if (!unique) {
        return NULL;
    }

    internal = &pdev->cursor_cache[CURSOR_HASH_VAL(hsurf)];
    while (*internal) {
        InternalCursor *now = *internal;
        if (now->hsurf == hsurf) {
            if (now->unique == unique) {
                RingRemove(pdev, &now->lru_link);
                RingAdd(pdev, &pdev->cursors_lru, &now->lru_link);
                return now;
            }
            CursorCacheRemove(pdev, now);
            break;
        }
        internal = &now->next;
    }
    return NULL;
}

static void FreeCursor(PDev *pdev, Resource *res)
{
    QXLPHYSICAL chunk_phys;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    chunk_phys = ((InternalCursor *)res->res)->cursor.chunk.next_chunk;
    while (chunk_phys) {
        QXLDataChunk *chunk = (QXLDataChunk *)VA(pdev, chunk_phys, pdev->main_mem_slot);
        chunk_phys = chunk->next_chunk;
        FreeMem(pdev, MSPACE_TYPE_DEVRAM, chunk);
        ONDBG(pdev->num_cursor_pages--);
    }

    FreeMem(pdev, MSPACE_TYPE_DEVRAM, res);
    ONDBG(pdev->num_cursor_pages--);

    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
}


typedef struct NewCursorInfo {
    QXLCursor *cursor;
    QXLDataChunk *chunk;
    UINT8 *now;
    UINT8 *end;
} NewCursorInfo;

#define CURSOR_ALLOC_SIZE (PAGE_SIZE << 1)

static BOOL GetCursorCommon(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf,
                            UINT16 type, NewCursorInfo *info, BOOL *in_cach)
{
    InternalCursor *internal;
    QXLCursor *cursor;
    Resource *res;
    ULONG unique;
    UINT8 *src;
    UINT8 *src_end;
    int line_size;
    HSURF bitmap = 0;
    SURFOBJ *local_surf = surf;

    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    *in_cach = FALSE;
    unique = (surf->fjBitmap & BMF_DONTCACHE) ? 0 : surf->iUniq;

    if ((internal = CursorCacheGet(pdev, surf->hsurf, unique))) {
        res = (Resource *)((UINT8 *)internal - sizeof(Resource));
        CursorCmdAddRes(pdev, cmd, res);
        cmd->u.set.shape = PA(pdev, &internal->cursor, pdev->main_mem_slot);
        *in_cach = TRUE;
        return TRUE;
    }

    if (surf->iType != STYPE_BITMAP) {
        RECTL dest_rect;
        POINTL src_pos;
        ASSERT(pdev, surf->iBitmapFormat == BMF_32BPP || surf->iBitmapFormat == BMF_16BPP);

        /* copying the surface to a bitmap */

        bitmap = (HSURF)EngCreateBitmap(surf->sizlBitmap, surf->lDelta, surf->iBitmapFormat,
                                        0, NULL);
        if (!bitmap) {
            DEBUG_PRINT((pdev, 0, "%s: EngCreateBitmap failed\n", __FUNCTION__));
            return FALSE;
        }

        if (!EngAssociateSurface(bitmap, pdev->eng, 0)) {
            DEBUG_PRINT((pdev, 0, "%s: EngAssociateSurface failed\n", __FUNCTION__));
            goto error;
        }

        if (!(local_surf = EngLockSurface(bitmap))) {
            DEBUG_PRINT((pdev, 0, "%s: EngLockSurface failed\n", __FUNCTION__));
            goto error;
        }

        dest_rect.top = 0;
        dest_rect.left = 0;
        dest_rect.bottom = surf->sizlBitmap.cy;
        dest_rect.right = surf->sizlBitmap.cx;

        src_pos.x = 0;
        src_pos.y = 0;
            
        if (!BitBltFromDev(pdev, surf, local_surf, NULL, NULL, NULL, &dest_rect, src_pos,
                           NULL, NULL, NULL, 0xcccc)) {
            goto error;
        }
    }

    ASSERT(pdev, sizeof(Resource) + sizeof(InternalCursor) < CURSOR_ALLOC_SIZE);
    res = (Resource *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, CURSOR_ALLOC_SIZE);
    ONDBG(pdev->num_cursor_pages++);
    res->refs = 1;
    res->free = FreeCursor;
    RESOURCE_TYPE(res, RESOURCE_TYPE_CURSOR);

    internal = (InternalCursor *)res->res;
    internal->hsurf = surf->hsurf;
    internal->unique = unique;
    RingItemInit(&internal->lru_link);

    cursor = info->cursor = &internal->cursor;
    cursor->header.type = type;
    cursor->header.unique = unique ? ++pdev->last_cursor_id : 0;
    cursor->header.width = (UINT16)local_surf->sizlBitmap.cx;
    cursor->header.height = (type == SPICE_CURSOR_TYPE_MONO) ? (UINT16)local_surf->sizlBitmap.cy >> 1 :
                            (UINT16)local_surf->sizlBitmap.cy;
    cursor->header.hot_spot_x = (UINT16)hot_x;
    cursor->header.hot_spot_y = (UINT16)hot_y;

    cursor->data_size = 0;

    info->chunk = &cursor->chunk;
    info->chunk->data_size = 0;
    info->chunk->prev_chunk = 0;
    info->chunk->next_chunk = 0;

    info->now = info->chunk->data;
    info->end = (UINT8 *)res + CURSOR_ALLOC_SIZE;

    switch (type) {
    case SPICE_CURSOR_TYPE_ALPHA:
    case SPICE_CURSOR_TYPE_COLOR32:
        line_size = cursor->header.width << 2;
        break;
    case SPICE_CURSOR_TYPE_MONO:
        line_size = ALIGN(cursor->header.width, 8) >> 3;
        break;
    case SPICE_CURSOR_TYPE_COLOR4:
        line_size = ALIGN(cursor->header.width, 2) >> 1;
        break;
    case SPICE_CURSOR_TYPE_COLOR8:
        line_size = cursor->header.width;
        break;
    case SPICE_CURSOR_TYPE_COLOR16:
        line_size = cursor->header.width << 1;
        break;
    case SPICE_CURSOR_TYPE_COLOR24:
        line_size = cursor->header.width * 3;
        break;
    }

    cursor->data_size = line_size * local_surf->sizlBitmap.cy;
    src = local_surf->pvScan0;
    src_end = src + (local_surf->lDelta * local_surf->sizlBitmap.cy);
    for (; src != src_end; src += local_surf->lDelta) {
        PutBytes(pdev, &info->chunk, &info->now, &info->end, src, line_size,
                 &pdev->num_cursor_pages, PAGE_SIZE, FALSE);
    }

    CursorCacheAdd(pdev, internal);
    CursorCmdAddRes(pdev, cmd, res);
    RELEASE_RES(pdev, res);
    cmd->u.set.shape = PA(pdev, &internal->cursor, pdev->main_mem_slot);
    DEBUG_PRINT((pdev, 11, "%s: done, data_size %u\n", __FUNCTION__, cursor->data_size));

    if (local_surf != surf) {
        EngUnlockSurface(local_surf);
        EngDeleteSurface(bitmap);
    }

    return TRUE;
error:
    if (bitmap) {
        ASSERT(pdev, local_surf != surf);
        EngDeleteSurface(bitmap);
    }

    return FALSE;
}

BOOL GetAlphaCursor(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf)
{
    NewCursorInfo info;
    BOOL ret;
    BOOL in_cache;

    ASSERT(pdev, surf->iBitmapFormat == BMF_32BPP);
    ASSERT(pdev, surf->sizlBitmap.cx > 0 && surf->sizlBitmap.cy > 0);

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ret = GetCursorCommon(pdev, cmd, hot_x, hot_y, surf, SPICE_CURSOR_TYPE_ALPHA, &info, &in_cache);
    DEBUG_PRINT((pdev, 8, "%s: done\n", __FUNCTION__));
    return ret;
}

BOOL GetMonoCursor(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf)
{
    NewCursorInfo info;
    BOOL ret;
    BOOL in_cache;

    ASSERT(pdev, surf->iBitmapFormat == BMF_1BPP);
    ASSERT(pdev, surf->sizlBitmap.cy > 0 && (surf->sizlBitmap.cy & 1) == 0);
    ASSERT(pdev, surf->sizlBitmap.cx > 0);

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    ret = GetCursorCommon(pdev, cmd, hot_x, hot_y, surf, SPICE_CURSOR_TYPE_MONO, &info, &in_cache);
    DEBUG_PRINT((pdev, 8, "%s: done\n", __FUNCTION__));
    return ret;
}

BOOL GetColorCursor(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf,
                    SURFOBJ *mask, XLATEOBJ *color_trans)
{
    NewCursorInfo info;
    UINT16 type;
    BOOL in_cache;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    ASSERT(pdev, surf && mask);
    ASSERT(pdev, surf->sizlBitmap.cx > 0 && surf->sizlBitmap.cy);

    if ( mask->sizlBitmap.cx != surf->sizlBitmap.cx ||
         mask->sizlBitmap.cy != surf->sizlBitmap.cy * 2 ) {
        DEBUG_PRINT((pdev, 0, "%s: err mask size, surf(%d, %d) mask(%d, %d)\n",
                     __FUNCTION__,
                     surf->sizlBitmap.cx,
                     surf->sizlBitmap.cy,
                     mask->sizlBitmap.cx,
                     mask->sizlBitmap.cy));
        return FALSE;
    }

    switch (surf->iBitmapFormat) {
    case BMF_32BPP:
        type = SPICE_CURSOR_TYPE_COLOR32;
        break;
    case BMF_24BPP:
        type = SPICE_CURSOR_TYPE_COLOR24;
        break;
    case BMF_16BPP:
        type = SPICE_CURSOR_TYPE_COLOR16;
        break;
    case BMF_8BPP:
        type = SPICE_CURSOR_TYPE_COLOR8;
        break;
    case BMF_4BPP:
        type = SPICE_CURSOR_TYPE_COLOR4;
        break;
    default:
        DEBUG_PRINT((pdev, 0, "%s: unexpected format\n", __FUNCTION__));
        return FALSE;
    }

    if (!GetCursorCommon(pdev, cmd, hot_x, hot_y, surf, type, &info, &in_cache)) {
        return FALSE;
    }

    if (!in_cache) {
        int line_size;
        UINT8 *src;
        UINT8 *src_end;

        if (type == SPICE_CURSOR_TYPE_COLOR8) {

            DEBUG_PRINT((pdev, 8, "%s: SPICE_CURSOR_TYPE_COLOR8\n", __FUNCTION__));
            ASSERT(pdev, color_trans);
            ASSERT(pdev, color_trans->pulXlate);
            ASSERT(pdev, color_trans->flXlate & XO_TABLE);
            ASSERT(pdev, color_trans->cEntries == 256);

            if (pdev->bitmap_format == BMF_32BPP) {
                PutBytes(pdev, &info.chunk, &info.now, &info.end, (UINT8 *)color_trans->pulXlate,
                         256 << 2, &pdev->num_cursor_pages, PAGE_SIZE, FALSE);
            } else {
                int i;

                for (i = 0; i < 256; i++) {
                    UINT32 ent = _16bppTo32bpp(color_trans->pulXlate[i]);
                    PutBytes(pdev, &info.chunk, &info.now, &info.end, (UINT8 *)&ent,
                             4, &pdev->num_cursor_pages, PAGE_SIZE, FALSE);
                }
            }
            info.cursor->data_size += 256 << 2;
        } else if (type == SPICE_CURSOR_TYPE_COLOR4) {

            ASSERT(pdev, color_trans);
            ASSERT(pdev, color_trans->pulXlate);
            ASSERT(pdev, color_trans->flXlate & XO_TABLE);
            ASSERT(pdev, color_trans->cEntries == 16);

            if (pdev->bitmap_format == BMF_32BPP) {
                PutBytes(pdev, &info.chunk, &info.now, &info.end, (UINT8 *)color_trans->pulXlate,
                         16 << 2, &pdev->num_cursor_pages, PAGE_SIZE, FALSE);
            } else {
                int i;

                for (i = 0; i < 16; i++) {
                    UINT32 ent = _16bppTo32bpp(color_trans->pulXlate[i]);
                    PutBytes(pdev, &info.chunk, &info.now, &info.end, (UINT8 *)&ent,
                             4, &pdev->num_cursor_pages, PAGE_SIZE, FALSE);
                }
            }
            info.cursor->data_size += 16 << 2;
        }

        ASSERT(pdev, mask->iBitmapFormat == BMF_1BPP);
        ASSERT(pdev, mask->iType == STYPE_BITMAP);

        line_size = ALIGN(mask->sizlBitmap.cx, 8) >> 3;
        info.cursor->data_size += line_size * surf->sizlBitmap.cy;
        src = mask->pvScan0;
        src_end = src + (mask->lDelta * surf->sizlBitmap.cy);

        for (; src != src_end; src += mask->lDelta) {
            PutBytes(pdev, &info.chunk, &info.now, &info.end, src, line_size,
                     &pdev->num_cursor_pages, PAGE_SIZE, FALSE);
        }
    }

    DEBUG_PRINT((pdev, 8, "%s: done\n", __FUNCTION__));
    return TRUE;
}

BOOL GetTransparentCursor(PDev *pdev, QXLCursorCmd *cmd)
{
    Resource *res;
    InternalCursor *internal;
    QXLCursor *cursor;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, sizeof(Resource) + sizeof(InternalCursor) < PAGE_SIZE);

    res = (Resource *)AllocMem(pdev, MSPACE_TYPE_DEVRAM, sizeof(Resource) + sizeof(InternalCursor));
    ONDBG(pdev->num_cursor_pages++);
    res->refs = 1;
    res->free = FreeCursor;
    RESOURCE_TYPE(res, RESOURCE_TYPE_CURSOR);

    internal = (InternalCursor *)res->res;
    internal->hsurf = NULL;
    internal->unique = 0;
    RingItemInit(&internal->lru_link);

    cursor = &internal->cursor;
    cursor->header.type = SPICE_CURSOR_TYPE_MONO;
    cursor->header.unique = 0;
    cursor->header.width = 0;
    cursor->header.height = 0;
    cursor->header.hot_spot_x = 0;
    cursor->header.hot_spot_y = 0;
    cursor->data_size = 0;
    cursor->chunk.data_size = 0;

    CursorCmdAddRes(pdev, cmd, res);
    RELEASE_RES(pdev, res);
    cmd->u.set.shape = PA(pdev, &internal->cursor, pdev->main_mem_slot);

    DEBUG_PRINT((pdev, 8, "%s: done\n", __FUNCTION__));
    return TRUE;
}

void ReleaseCacheDeviceMemoryResources(PDev *pdev)
{
    DEBUG_PRINT((pdev, 0, "%s \n", __FUNCTION__));
    PaletteCacheClear(pdev);
    CursorCacheClear(pdev);
}

static void quic_usr_error(QuicUsrContext *usr, const char *format, ...)
{
    QuicData *quic_data = (QuicData *)usr;
    va_list ap;

    va_start(ap, format);
    DebugPrintV(quic_data->pdev, format, ap);
    va_end(ap);
    EngDebugBreak();
}

static void quic_usr_warn(QuicUsrContext *usr, const char *format, ...)
{
    QuicData *quic_data = (QuicData *)usr;
    va_list ap;

    va_start(ap, format);
    DebugPrintV(quic_data->pdev, format, ap);
    va_end(ap);
}

static void *quic_usr_malloc(QuicUsrContext *usr, int size)
{
    return EngAllocMem(0, size, ALLOC_TAG);
}

static void quic_usr_free(QuicUsrContext *usr, void *ptr)
{
    EngFreeMem(ptr);
}

BOOL ResInit(PDev *pdev)
{
    QuicData *usr_data;

    if (!(usr_data = EngAllocMem(FL_ZERO_MEMORY, sizeof(QuicData), ALLOC_TAG))) {
        return FALSE;
    }
    usr_data->user.error = quic_usr_error;
    usr_data->user.warn = quic_usr_warn;
    usr_data->user.info = quic_usr_warn;
    usr_data->user.malloc = quic_usr_malloc;
    usr_data->user.free = quic_usr_free;
    usr_data->user.more_space = quic_usr_more_space;
    usr_data->user.more_lines = quic_usr_more_lines;
    usr_data->pdev = pdev;
    if (!(usr_data->quic = quic_create(&usr_data->user))) {
        EngFreeMem(usr_data);
        return FALSE;
    }
    pdev->quic_data = usr_data;
    pdev->quic_data_sem = EngCreateSemaphore();
    if (!pdev->quic_data_sem) {
        PANIC(pdev, "quic_data_sem creation failed\n");
    }
    pdev->io_sem = EngCreateSemaphore();
    if (!pdev->io_sem) {
        PANIC(pdev, "io_sem creation failed\n");
    }

    return TRUE;
}

void ResDestroy(PDev *pdev)
{
    QuicData *usr_data = pdev->quic_data;
    quic_destroy(usr_data->quic);
    EngDeleteSemaphore(pdev->quic_data_sem);
    EngFreeMem(usr_data);
}

void ResInitGlobals()
{
    image_id_sem = EngCreateSemaphore();
    if (!image_id_sem) {
        EngDebugBreak();
    }
    quic_init();
}

void ResDestroyGlobals()
{
    EngDeleteSemaphore(image_id_sem);
    image_id_sem = NULL;
}

#ifndef _WIN64

void CheckAndSetSSE2()
{
    _asm
    {
        mov eax, 0x0000001
        cpuid
        and edx, 0x4000000
        mov have_sse2, edx
    }

    if (have_sse2) {
        have_sse2 = TRUE;
    }
}

#endif
