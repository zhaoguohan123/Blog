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

#ifndef _H_QXLDD
#define _H_QXLDD


#include "stddef.h"

#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "ioaccess.h"
#include "qxl_driver.h"
#include "mspace.h"
#if (WINVER < 0x0501)
#include "wdmhelper.h"
#endif

#define ALLOC_TAG 'dlxq'

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))

#define DEBUG_PRINT(arg) DebugPrint arg

#ifdef DBG
#define ASSERT(pdev, x) if (!(x)) { \
    DebugPrint(pdev, 0, "ASSERT(%s) failed @ %s\n", #x, __FUNCTION__); \
    EngDebugBreak();\
}
#define ONDBG(x) x
#else
#define ASSERT(pdev, x)
#define ONDBG(x)
#endif

#define PANIC(pdev, str) {                                      \
    DebugPrint(pdev, 0, "PANIC: %s @ %s\n", str, __FUNCTION__); \
    EngDebugBreak();                                            \
}

#define PUNT_IF_DISABLED(pdev) \
    do { \
        if (!pdev->enabled) { \
            DEBUG_PRINT((pdev, 0, "%s: punting\n", __FUNCTION__)); \
            return FALSE; \
        } \
    } while (0)

typedef enum {
    QXL_SUCCESS,
    QXL_FAILED,
    QXL_UNSUPPORTED,
} QXLRESULT;

typedef struct Ring RingItem;
typedef struct Ring {
    RingItem *prev;
    RingItem *next;
} Ring;

#define IMAGE_HASH_SHIFT 15
#define IMAGE_HASH_SIZE (1 << IMAGE_HASH_SHIFT)
#define IMAGE_HASH_MASK (IMAGE_HASH_SIZE - 1)

#define IMAGE_POOL_SIZE (1 << 15)

#define CURSOR_CACHE_SIZE (1 << 6)
#define CURSOR_HASH_SIZE (CURSOR_CACHE_SIZE << 1)
#define CURSOR_HASH_NASKE (CURSOR_HASH_SIZE - 1)

#define PALETTE_CACHE_SIZE (1 << 6)
#define PALETTE_HASH_SIZE (PALETTE_CACHE_SIZE << 1)
#define PALETTE_HASH_NASKE (PALETTE_HASH_SIZE - 1)

//#define CALL_TEST

#ifdef CALL_TEST
enum {
    CALL_COUNTER_COPY_BITS,
    CALL_COUNTER_BIT_BLT,
    CALL_COUNTER_TEXT_OUT,
    CALL_COUNTER_STROKE_PATH,
    CALL_COUNTER_STRETCH_BLT,
    CALL_COUNTER_STRETCH_BLT_ROP,
    CALL_COUNTER_TRANSPARENT_BLT,
    CALL_COUNTER_ALPHA_BLEND,

    CALL_COUNTER_FILL_PATH,
    CALL_COUNTER_GRADIENT_FILL,
    CALL_COUNTER_LINE_TO,
    CALL_COUNTER_PLG_BLT,
    CALL_COUNTER_STROKE_AND_FILL_PATH,

    NUM_CALL_COUNTERS,
};
#endif

typedef struct QuicData QuicData;

#define IMAGE_KEY_HASH_SIZE (1 << 15)
#define IMAGE_KEY_HASH_MASK (IMAGE_KEY_HASH_SIZE - 1)

typedef struct ImageKey {
    HSURF hsurf;
    UINT64 unique;
    UINT32 key;
} ImageKey;

typedef struct CacheImage {
    struct CacheImage *next;
    RingItem lru_link;
    UINT32 key;
    UINT32 hits;
    UINT8 format;
    UINT32 width;
    UINT32 height;
    struct InternalImage *image;
} CacheImage;

#define NUM_UPDATE_TRACE_ITEMS 10
typedef struct UpdateTrace {
    RingItem link;
    UINT32 last_time;
    RECTL area;
    HSURF hsurf;
    UINT8 count;
} UpdateTrace;

typedef struct PMemSlot {
    MemSlot slot;
    QXLPHYSICAL high_bits;
} PMemSlot;

typedef struct MspaceInfo {
    mspace _mspace;
    UINT8 *mspace_start;
    UINT8 *mspace_end;
} MspaceInfo;

enum {
    MSPACE_TYPE_DEVRAM,
    MSPACE_TYPE_VRAM,

    NUM_MSPACES,
};

enum {
    SYNC = 0,
    ASYNC = 1
};

typedef enum {
    ASYNCABLE_UPDATE_AREA = 0,
    ASYNCABLE_MEMSLOT_ADD,
    ASYNCABLE_CREATE_PRIMARY,
    ASYNCABLE_DESTROY_PRIMARY,
    ASYNCABLE_DESTROY_SURFACE,
    ASYNCABLE_DESTROY_ALL_SURFACES,
    ASYNCABLE_FLUSH_SURFACES,
    ASYNCABLE_MONITOR_CONFIG,

    ASYNCABLE_COUNT
} asyncable_t;


typedef struct PDev PDev;

typedef struct DrawArea {
   HSURF bitmap;
   SURFOBJ* surf_obj;
   UINT8 *base_mem;
} DrawArea;

typedef struct SurfaceInfo SurfaceInfo;
struct SurfaceInfo {
    DrawArea    draw_area;
    HBITMAP     hbitmap;
    SIZEL       size;
    UINT8      *copy;
    ULONG       bitmap_format;
    INT32       stride;
    union {
        PDev *pdev;
        SurfaceInfo *next_free;
    } u;
};

#define SSE_MASK 15
#define SSE_ALIGN 16

typedef struct PDev {
    HANDLE driver;
    HDEV eng;
    HPALETTE palette;
    HSURF surf;
    UINT8 surf_enable;
    DWORD video_mode_index;
    SIZEL resolution;
    UINT32 max_bitmap_size;
    ULONG bitmap_format;
    UINT8 create_non_primary_surfaces;

    ULONG fb_size;
    BYTE* fb;
    UINT64 fb_phys;
    UINT8 vram_slot_initialized;
    UINT8 vram_mem_slot;

    ULONG stride;
    FLONG red_mask;
    FLONG green_mask;
    FLONG blue_mask;
    ULONG fp_state_size;

    QXLPHYSICAL surf_phys;
    UINT8 *surf_base;

    QuicData *quic_data;
    HSEMAPHORE quic_data_sem;

    QXLCommandRing *cmd_ring;
    QXLCursorRing *cursor_ring;
    QXLReleaseRing *release_ring;
    PUCHAR notify_cmd_port;
    PUCHAR notify_cursor_port;
    PUCHAR notify_oom_port;
    PEVENT display_event;
    PEVENT cursor_event;
    PEVENT sleep_event;
    PEVENT io_cmd_event;

    PUCHAR log_port;
    UINT8 *log_buf;
    UINT32 *log_level;

    PMemSlot *mem_slots;
    UINT8 num_mem_slot;
    UINT8 main_mem_slot;
    UINT8 slot_id_bits;
    UINT8 slot_gen_bits;
    UINT8 *slots_generation;
    UINT64 *ram_slot_start;
    UINT64 *ram_slot_end;
    QXLPHYSICAL va_slot_mask;

    UINT32 num_io_pages;
    UINT8 *io_pages_virt;
    UINT64 io_pages_phys;

    UINT32 *dev_update_id;

    QXLRect *update_area;
    UINT32 *update_surface;

    UINT32 *mm_clock;

    UINT32 *compression_level;

    FLONG cursor_trail;

#if (WINVER < 0x0501)
    PQXLWaitForEvent WaitForEvent;
#endif

    PUCHAR asyncable[ASYNCABLE_COUNT][2];
    HSEMAPHORE io_sem;
    PUCHAR memslot_del_port;
    PUCHAR flush_release_port;
    UINT32 use_async;

    UINT8* primary_memory_start;
    UINT32 primary_memory_size;

    QXLSurfaceCreate *primary_surface_create;

    UINT32 dev_id;

    Ring update_trace;
    UpdateTrace update_trace_items[NUM_UPDATE_TRACE_ITEMS];

    UINT64 free_outputs;

    MspaceInfo mspaces[NUM_MSPACES];

    /*
     * TODO: reconsider semaphores according to
     * http://msdn.microsoft.com/en-us/library/ff568281%28v=vs.85%29.aspx
     * 1) In order to protect the device log buffer,
     *     the print_sem must be shared between different pdevs and
     *     different display sessions.
     * 2) malloc_sem: not sure what it protects. Maybe globals in mspace?
     *    since only the enabled pdev is allocating memory, I don't
     *    think it is required (unless it is possible to have
     *    AssertMode(x, enable) before AssertMode(y, disable).
     * 3) cmd_sem, cursor_sem: again, since only the enabled pdev touches the cmd rings
     *    I don't think it is required.
     * 4) io_sem - same as print sem. Note that we should prevent starvation between
     *    print_sem and io_sem in DebugPrintV.
     *
     */
    HSEMAPHORE malloc_sem; /* Also protects release ring */
    HSEMAPHORE print_sem;
    HSEMAPHORE cmd_sem;
    HSEMAPHORE cursor_sem; /* Protects cursor_ring */

    CacheImage cache_image_pool[IMAGE_POOL_SIZE];
    Ring cache_image_lru;
    Ring cursors_lru;
    Ring palette_lru;
    ImageKey image_key_lookup[IMAGE_KEY_HASH_SIZE];
    struct CacheImage *image_cache[IMAGE_HASH_SIZE];
    struct InternalCursor *cursor_cache[CURSOR_HASH_SIZE];
    UINT32 num_cursors;
    UINT32 last_cursor_id;
    struct InternalPalette *palette_cache[PALETTE_HASH_SIZE];
    UINT32 num_palettes;

    UINT32 n_surfaces;
    SurfaceInfo surface0_info;
    SurfaceInfo *surfaces_info;
    SurfaceInfo *free_surfaces;

    UINT32 update_id;

    UINT32 enabled; /* 1 between DrvAssertMode(TRUE) and DrvAssertMode(FALSE) */


    UCHAR  pci_revision;

    QXLMonitorsConfig * monitor_config;
    QXLPHYSICAL * monitor_config_pa;

#ifdef DBG
    int num_free_pages;
    int num_outputs;
    int num_path_pages;
    int num_rects_pages;
    int num_bits_pages;
    int num_buf_pages;
    int num_glyphs_pages;
    int num_cursor_pages;
#endif

#ifdef CALL_TEST
    BOOL count_calls;
    UINT32 total_calls;
    UINT32 call_counters[NUM_CALL_COUNTERS];
#endif
} PDev;


void DebugPrintV(PDev *pdev, const char *message, va_list ap);
void DebugPrint(PDev *pdev, int level, const char *message, ...);

void InitResources(PDev *pdev);
void ClearResources(PDev *pdev);

#ifdef CALL_TEST
void CountCall(PDev *pdev, int counter);
#else
#define CountCall(a, b)
#endif

char *BitmapFormatToStr(int format);
char *BitmapTypeToStr(int type);

static _inline void RingInit(Ring *ring)
{
    ring->next = ring->prev = ring;
}

static _inline void RingItemInit(RingItem *item)
{
    item->next = item->prev = NULL;
}

static _inline BOOL RingItemIsLinked(RingItem *item)
{
    return !!item->next;
}

static _inline BOOL RingIsEmpty(PDev *pdev, Ring *ring)
{
    ASSERT(pdev, ring->next != NULL && ring->prev != NULL);
    return ring == ring->next;
}

static _inline void RingAdd(PDev *pdev, Ring *ring, RingItem *item)
{
    ASSERT(pdev, ring->next != NULL && ring->prev != NULL);
    ASSERT(pdev, item->next == NULL && item->prev == NULL);

    item->next = ring->next;
    item->prev = ring;
    ring->next = item->next->prev = item;
}

static _inline void RingRemove(PDev *pdev, RingItem *item)
{
    ASSERT(pdev, item->next != NULL && item->prev != NULL);
    ASSERT(pdev, item->next != item);

    item->next->prev = item->prev;
    item->prev->next = item->next;
    item->prev = item->next = 0;
}

static _inline RingItem *RingGetTail(PDev *pdev, Ring *ring)
{
    RingItem *ret;

    ASSERT(pdev, ring->next != NULL && ring->prev != NULL);

    if (RingIsEmpty(pdev, ring)) {
        return NULL;
    }
    ret = ring->prev;
    return ret;
}

#if (WINVER < 0x0501)
#define WAIT_FOR_EVENT(pdev, event, timeout) (pdev)->WaitForEvent(event, timeout)
#else
#define WAIT_FOR_EVENT(pdev, event, timeout) EngWaitForSingleObject(event, timeout)
#endif

/* Helpers for dealing with ENG_TIME_FIELDS */
static _inline ULONG64 eng_time_diff_ms(ENG_TIME_FIELDS *b, ENG_TIME_FIELDS *a)
{
    ULONG64 ret = 0;

    ret += b->usMilliseconds - a->usMilliseconds;
    ret += 1000 * (b->usSecond - a->usSecond);
    ret += 60000 * (b->usMinute - a->usMinute);
    ret += 3600000L * (b->usHour - a->usHour);
    // don't get into gregorian calendar, just ignore more then a single day difference
    if (b->usDay != a->usDay) {
        ret += (3600L * 24L * 1000L);
    }
    return ret;
}

#define INTERRUPT_NOT_PRESENT_TIMEOUT_MS 60000L
#define INTERRUPT_NOT_PRESENT_TIMEOUT_100NS (INTERRUPT_NOT_PRESENT_TIMEOUT_MS * 10000L)

/* Write to an ioport. For some operations we support a new port that returns
 * immediatly, and completion is signaled by an interrupt that sets io_cmd_event.
 * If the pci_revision is >= QXL_REVISION_STABLE_V10, we support it, else do
 * a regular ioport write.
 */
static _inline void async_io(PDev *pdev, asyncable_t op, UCHAR val)
{
    ENG_TIME_FIELDS start, finish;
    LARGE_INTEGER timeout;                      // 1 => 100 nanoseconds
    ULONG64 millis;
    BOOL is_async = FALSE;
    /*
     * calling DEBUG_PRINT after locking io_sem can cause deadlock because
     * DebugPrintV locks print_sem and then io_sem. Instead, we defer the
     * the DEBUG_PRINT calls.
     */
    BOOL error_timeout = FALSE;
    BOOL error_bad_sync = FALSE;

    DEBUG_PRINT((pdev, 3, "%s: start io op %d\n", __FUNCTION__, (int)op));
    EngAcquireSemaphore(pdev->io_sem);
    if (pdev->use_async) {
        is_async = TRUE;
        WRITE_PORT_UCHAR(pdev->asyncable[op][ASYNC], val);
        /* Our Interrupt may be taken from us unexpectedly, by a surprise removal.
         * in which case this event will never be set. This happens only during WHQL
         * tests (pnpdtest /surprise). So instead: Wait on a timer, if we fail, stop waiting, until
         * we get reset. We use EngQueryLocalTime because there is no way to differentiate a return on
         * timeout from a return on event set otherwise. */
        timeout.QuadPart = -INTERRUPT_NOT_PRESENT_TIMEOUT_100NS; // negative  => relative
        EngQueryLocalTime(&start);
        WAIT_FOR_EVENT(pdev, pdev->io_cmd_event, &timeout);
        EngQueryLocalTime(&finish);
        millis = eng_time_diff_ms(&finish, &start);
        if (millis >= INTERRUPT_NOT_PRESENT_TIMEOUT_MS) {
            error_timeout = TRUE;
            pdev->use_async = 0;
        }
    } else {
        is_async = FALSE;
        if (pdev->asyncable[op][SYNC] == NULL) {
            error_bad_sync = TRUE;
        } else {
            WRITE_PORT_UCHAR(pdev->asyncable[op][SYNC], val);
        }
    }
    EngReleaseSemaphore(pdev->io_sem);
    if (error_timeout) {
        DEBUG_PRINT((pdev, 0, "%s: timeout reached, disabling async io!\n", __FUNCTION__));
    } else if (error_bad_sync) {
        DEBUG_PRINT((pdev, 0, "%s: ERROR: trying calling sync io on NULL port %d\n", __FUNCTION__, op));
    } else {
        DEBUG_PRINT((pdev, 3, "%s: finished op %d async %d\n", __FUNCTION__, (int)op, is_async));
    }
}

/*
 * Before the introduction of QXL_IO_*_ASYNC all io writes would return
 * only when their function was complete. Since qemu would only allow
 * a single outstanding io operation between all vcpu threads, they were
 * also protected from simultaneous calls between different vcpus.
 *
 * With the introduction of _ASYNC we need to explicitly lock between different
 * threads running on different vcpus, this is what this helper accomplishes.
 */
static _inline void sync_io(PDev *pdev, PUCHAR port, UCHAR val)
{
    EngAcquireSemaphore(pdev->io_sem);
    WRITE_PORT_UCHAR(port, val);
    EngReleaseSemaphore(pdev->io_sem);
}

#ifdef DBG
#define DUMP_VRAM_MSPACE(pdev) \
    do { \
        DEBUG_PRINT((pdev, 0, "%s: dumping mspace vram (%p)\n", __FUNCTION__, pdev)); \
        if (pdev) {  \
            mspace_malloc_stats(pdev->mspaces[MSPACE_TYPE_VRAM]._mspace); \
        } else { \
            DEBUG_PRINT((pdev, 0, "nothing\n")); \
        }\
    } while (0)

#define DUMP_DEVRAM_MSPACE(pdev) \
    do { \
        DEBUG_PRINT((pdev, 0, "%s: dumping mspace devram (%p)\n", __FUNCTION__, pdev)); \
        if (pdev) {  \
            mspace_malloc_stats(pdev->mspaces[MSPACE_TYPE_DEVRAM]._mspace); \
        } else { \
            DEBUG_PRINT((pdev, 0, "nothing\n")); \
        }\
    } while (0)
#else
#define DUMP_VRAM_MSPACE
#define DUMP_DEVRAM_MSPACE
#endif

#endif
