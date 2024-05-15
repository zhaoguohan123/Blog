#pragma once
#include <ntifs.h>

#pragma region IA64_PTE_STATEMENT
#define PTE_PER_PAGE_BITS 10
#define _HARDWARE_PTE_WORKING_SET_BITS  11
#define PXE_PER_PAGE 512
#define PXI_MASK (PXE_PER_PAGE - 1)
#define PDI_SHIFT 21
#define PTI_SHIFT 12
#define PPI_SHIFT 30
#define PXI_SHIFT 39
#define PTE_SHIFT 3
#define MM_PTE_VALID_MASK         0x1
#define MM_PTE_WRITE_MASK         0x800
#define MM_PTE_OWNER_MASK         0x4
#define MM_PTE_WRITE_THROUGH_MASK 0x8
#define MM_PTE_CACHE_DISABLE_MASK 0x10
#define MM_PTE_ACCESS_MASK        0x20
#define MM_PTE_DIRTY_MASK         0x42
#define MM_PTE_LARGE_PAGE_MASK    0x80
#define MM_PTE_GLOBAL_MASK        0x100
#define MM_PTE_COPY_ON_WRITE_MASK 0x200
#define MM_PTE_PROTOTYPE_MASK     0x400
#define MM_PTE_TRANSITION_MASK    0x800
#define PTE_BASE 0xFFFFF68000000000UI64
#define PTE_TOP  0xFFFFF6FFFFFFFFFFUI64
#define PDE_BASE 0xFFFFF6FB40000000UI64
#define PDE_TOP  0xFFFFF6FB7FFFFFFFUI64
#define PPE_BASE 0xFFFFF6FB7DA00000UI64
#define PPE_TOP  0xFFFFF6FB7DBFFFFFUI64
#define PXE_BASE 0xFFFFF6FB7DBED000UI64
#define PXE_TOP  0xFFFFF6FB7DBEDFFFUI64

#define VIRTUAL_ADDRESS_BITS 48
#define VIRTUAL_ADDRESS_MASK ((((ULONG_PTR)1) << VIRTUAL_ADDRESS_BITS) - 1)

typedef struct _MMPTE_HARDWARE {
	ULONGLONG Valid : 1;
#if defined(NT_UP)
	ULONGLONG Write : 1;			// UP version
#else
	ULONGLONG Writable : 1;			// changed for MP version
#endif
	ULONGLONG Owner : 1;
	ULONGLONG WriteThrough : 1;
	ULONGLONG CacheDisable : 1;
	ULONGLONG Accessed : 1;
	ULONGLONG Dirty : 1;
	ULONGLONG LargePage : 1;
	ULONGLONG Global : 1;
	ULONGLONG CopyOnWrite : 1;		// software field
	ULONGLONG Prototype : 1;		// software field
#if defined(NT_UP)
	ULONGLONG reserved0 : 1;		// software field
#else
	ULONGLONG Write : 1;			// software field - MP change
#endif
	ULONGLONG PageFrameNumber : 28;
	ULONG64 reserved1 : 24 - (_HARDWARE_PTE_WORKING_SET_BITS + 1);
	ULONGLONG SoftwareWsIndex : _HARDWARE_PTE_WORKING_SET_BITS;
	ULONG64 NoExecute : 1;
} MMPTE_HARDWARE, * PMMPTE_HARDWARE;

typedef struct _MMPTE_HARDWARE_LARGEPAGE {
	ULONGLONG Valid : 1;
	ULONGLONG Write : 1;
	ULONGLONG Owner : 1;
	ULONGLONG WriteThrough : 1;
	ULONGLONG CacheDisable : 1;
	ULONGLONG Accessed : 1;
	ULONGLONG Dirty : 1;
	ULONGLONG LargePage : 1;
	ULONGLONG Global : 1;
	ULONGLONG CopyOnWrite : 1; // software field
	ULONGLONG Prototype : 1;   // software field
	ULONGLONG reserved0 : 1;   // software field
	ULONGLONG PAT : 1;
	ULONGLONG reserved1 : 8;   // software field
	ULONGLONG PageFrameNumber : 19;
	ULONGLONG reserved2 : 24;   // software field
} MMPTE_HARDWARE_LARGEPAGE, * PMMPTE_HARDWARE_LARGEPAGE;

typedef struct _HARDWARE_PTE {
	ULONG64 Valid : 1;
	ULONG64 Write : 1;                // UP version
	ULONG64 Owner : 1;
	ULONG64 WriteThrough : 1;
	ULONG64 CacheDisable : 1;
	ULONG64 Accessed : 1;
	ULONG64 Dirty : 1;
	ULONG64 LargePage : 1;
	ULONG64 Global : 1;
	ULONG64 CopyOnWrite : 1;          // software field
	ULONG64 Prototype : 1;            // software field
	ULONG64 reserved0 : 1;            // software field
	ULONG64 PageFrameNumber : 28;
	ULONG64 reserved1 : 24 - (_HARDWARE_PTE_WORKING_SET_BITS + 1);
	ULONG64 SoftwareWsIndex : _HARDWARE_PTE_WORKING_SET_BITS;
	ULONG64 NoExecute : 1;
} HARDWARE_PTE, * PHARDWARE_PTE;

typedef struct _MMPTE_PROTOTYPE {
	ULONGLONG Valid : 1;
	ULONGLONG Unused0 : 7;
	ULONGLONG ReadOnly : 1;
	ULONGLONG Unused1 : 1;
	ULONGLONG Prototype : 1;
	ULONGLONG Protection : 5;
	LONGLONG ProtoAddress : 48;
} MMPTE_PROTOTYPE;

typedef struct _MMPTE_SOFTWARE {
	ULONGLONG Valid : 1;
	ULONGLONG PageFileLow : 4;
	ULONGLONG Protection : 5;
	ULONGLONG Prototype : 1;
	ULONGLONG Transition : 1;
	ULONGLONG UsedPageTableEntries : PTE_PER_PAGE_BITS;
	ULONGLONG Reserved : 20 - PTE_PER_PAGE_BITS;
	ULONGLONG PageFileHigh : 32;
} MMPTE_SOFTWARE;

typedef struct _MMPTE_TRANSITION {
	ULONGLONG Valid : 1;
	ULONGLONG Write : 1;
	ULONGLONG Owner : 1;
	ULONGLONG WriteThrough : 1;
	ULONGLONG CacheDisable : 1;
	ULONGLONG Protection : 5;
	ULONGLONG Prototype : 1;
	ULONGLONG Transition : 1;
	ULONGLONG PageFrameNumber : 28;
	ULONGLONG Unused : 24;
} MMPTE_TRANSITION;

typedef struct _MMPTE_SUBSECTION {
	ULONGLONG Valid : 1;
	ULONGLONG Unused0 : 4;
	ULONGLONG Protection : 5;
	ULONGLONG Prototype : 1;
	ULONGLONG Unused1 : 5;
	LONGLONG SubsectionAddress : 48;
} MMPTE_SUBSECTION;

typedef struct _MMPTE_LIST {
	ULONGLONG Valid : 1;
	ULONGLONG OneEntry : 1;
	ULONGLONG filler0 : 3;

	//
	// Note the Prototype bit must not be used for lists like freed nonpaged
	// pool because lookaside pops can legitimately reference bogus addresses
	// (since the pop is unsynchronized) and the fault handler must be able to
	// distinguish lists from protos so a retry status can be returned (vs a
	// fatal bugcheck).
	//
	// The same caveat applies to both the Transition and the Protection
	// fields as they are similarly examined in the fault handler and would
	// be misinterpreted if ever nonzero in the freed nonpaged pool chains.
	//

	ULONGLONG Protection : 5;
	ULONGLONG Prototype : 1;        // MUST BE ZERO as per above comment.
	ULONGLONG Transition : 1;

	ULONGLONG filler1 : 20;
	ULONGLONG NextEntry : 32;
} MMPTE_LIST;

typedef struct _MMPTE {
	union {
		ULONG_PTR Long;
		MMPTE_HARDWARE Hard;
		MMPTE_HARDWARE_LARGEPAGE HardLarge;
		HARDWARE_PTE Flush;
		MMPTE_PROTOTYPE Proto;
		MMPTE_SOFTWARE Soft;
		MMPTE_TRANSITION Trans;
		MMPTE_SUBSECTION Subsect;
		MMPTE_LIST List;
	} u;
} MMPTE, * PMMPTE;
#pragma endregion

//0x28 bytes (sizeof)
typedef struct _IMAGE_SECTION_HEADER64
{
	UCHAR Name[8];                                                          //0x0
	union {
		ULONG PhysicalAddress;                                              //0x8
		ULONG VirtualSize;                                                  //0x8
	} Misc;                                                                 //0x8
	ULONG VirtualAddress;                                                   //0xc
	ULONG SizeOfRawData;                                                    //0x10
	ULONG PointerToRawData;                                                 //0x14
	ULONG PointerToRelocations;                                             //0x18
	ULONG PointerToLinenumbers;                                             //0x1c
	USHORT NumberOfRelocations;                                             //0x20
	USHORT NumberOfLinenumbers;                                             //0x22
	ULONG Characteristics;                                                  //0x24
}IMAGE_SECTION_HEADER64, * PIMAGE_SECTION_HEADER64;

//0xc bytes (sizeof)
typedef struct _RUNTIME_FUNCTION
{
	ULONG BeginAddress;                                                     //0x0
	ULONG EndAddress;                                                       //0x4
	union
	{
		ULONG UnwindInfoAddress;                                            //0x8
		ULONG UnwindData;                                                   //0x8
	};
}RUNTIME_FUNCTION, * PRUNTIME_FUNCTION;

#pragma pack(1)
typedef struct _GDTR64
{
	USHORT GDTLimit;
	ULONG64 GDTBase;
}GDTR64, * PGDTR64;

typedef struct _GDT_ENTRY64
{
	USHORT OffsetLow;
	USHORT Selector;
	UCHAR IstIndex : 3;
	UCHAR ReservedZero : 5;
	UCHAR Type : 5;
	UCHAR DPL : 2;
	UCHAR Present : 1;
	USHORT OffsetMiddle;
	ULONG OffsetHigh;
	ULONG NotUsedZero;
}GDT_ENTRY64, * PGDT_ENTRY64;
#pragma pack()

typedef struct _KSERVICE_TABLE_DESCRIPTOR64 {
	PULONG_PTR TableBase;
	PULONG Count;
	ULONG Limit;
	PUCHAR Number;
} KSERVICE_TABLE_DESCRIPTOR64, * PKSERVICE_TABLE_DESCRIPTOR64;

typedef struct _KLDR_DATA_TABLE_ENTRY64
{
	struct _LIST_ENTRY InLoadOrderLinks;                                    //0x0
	VOID* ExceptionTable;                                                   //0x10
	ULONG ExceptionTableSize;                                               //0x18
	VOID* GpValue;                                                          //0x20
	struct _NON_PAGED_DEBUG_INFO* NonPagedDebugInfo;                        //0x28
	VOID* DllBase;                                                          //0x30
	VOID* EntryPoint;                                                       //0x38
	ULONG SizeOfImage;                                                      //0x40
	struct _UNICODE_STRING FullDllName;                                     //0x48
	struct _UNICODE_STRING BaseDllName;                                     //0x58
	ULONG Flags;                                                            //0x68
	USHORT LoadCount;                                                       //0x6c
	union
	{
		USHORT SignatureLevel : 4;                                          //0x6e
		USHORT SignatureType : 3;                                           //0x6e
		USHORT Unused : 9;                                                  //0x6e
		USHORT EntireField;                                                 //0x6e
	} u1;                                                                   //0x6e
	VOID* SectionPointer;                                                   //0x70
	ULONG CheckSum;                                                         //0x78
	ULONG CoverageSectionSize;                                              //0x7c
	VOID* CoverageSection;                                                  //0x80
	VOID* LoadedImports;                                                    //0x88
	VOID* Spare;                                                            //0x90
	ULONG SizeOfImageNotRounded;                                            //0x98
	ULONG TimeDateStamp;                                                    //0x9c
}KLDR_DATA_TABLE_ENTRY64, * PKLDR_DATA_TABLE_ENTRY64;

typedef struct _MI_SYSTEM_PTE_TYPE {
	RTL_BITMAP Bitmap;
	PMMPTE BasePte;
}MI_SYSTEM_PTE_TYPE, * PMI_SYSTEM_PTE_TYPE;

typedef struct _POOL_BIG_PAGES {
	PVOID Va;
	ULONG Key;
	ULONG PoolType;
	SIZE_T NumberOfPages;
} POOL_BIG_PAGES, * PPOOL_BIG_PAGES;

typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;					//下一个结构的偏移量，最后一个偏移量为0
	ULONG NumberOfThreads;
	LARGE_INTEGER SpareLi1;
	LARGE_INTEGER SpareLi2;
	LARGE_INTEGER SpareLi3;
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;				//进程名
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;					//进程ID
	HANDLE InheritedFromUniqueProcessId;	//父进程ID
	ULONG HandleCount;
	ULONG SessionId;						//会话ID                   
	ULONG_PTR PageDirectoryBase;
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation = 0x0,
	SystemProcessorInformation = 0x1,
	SystemPerformanceInformation = 0x2,
	SystemTimeOfDayInformation = 0x3,
	SystemPathInformation = 0x4,
	SystemProcessInformation = 0x5,
	SystemCallCountInformation = 0x6,
	SystemDeviceInformation = 0x7,
	SystemProcessorPerformanceInformation = 0x8,
	SystemFlagsInformation = 0x9,
	SystemCallTimeInformation = 0xa,
	SystemModuleInformation = 0xb,
	SystemLocksInformation = 0xc,
	SystemStackTraceInformation = 0xd,
	SystemPagedPoolInformation = 0xe,
	SystemNonPagedPoolInformation = 0xf,
	SystemHandleInformation = 0x10,
	SystemObjectInformation = 0x11,
	SystemPageFileInformation = 0x12,
	SystemVdmInstemulInformation = 0x13,
	SystemVdmBopInformation = 0x14,
	SystemFileCacheInformation = 0x15,
	SystemPoolTagInformation = 0x16,
	SystemInterruptInformation = 0x17,
	SystemDpcBehaviorInformation = 0x18,
	SystemFullMemoryInformation = 0x19,
	SystemLoadGdiDriverInformation = 0x1a,
	SystemUnloadGdiDriverInformation = 0x1b,
	SystemTimeAdjustmentInformation = 0x1c,
	SystemSummaryMemoryInformation = 0x1d,
	SystemMirrorMemoryInformation = 0x1e,
	SystemPerformanceTraceInformation = 0x1f,
	SystemObsolete0 = 0x20,
	SystemExceptionInformation = 0x21,
	SystemCrashDumpStateInformation = 0x22,
	SystemKernelDebuggerInformation = 0x23,
	SystemContextSwitchInformation = 0x24,
	SystemRegistryQuotaInformation = 0x25,
	SystemExtendServiceTableInformation = 0x26,
	SystemPrioritySeperation = 0x27,
	SystemVerifierAddDriverInformation = 0x28,
	SystemVerifierRemoveDriverInformation = 0x29,
	SystemProcessorIdleInformation = 0x2a,
	SystemLegacyDriverInformation = 0x2b,
	SystemCurrentTimeZoneInformation = 0x2c,
	SystemLookasideInformation = 0x2d,
	SystemTimeSlipNotification = 0x2e,
	SystemSessionCreate = 0x2f,
	SystemSessionDetach = 0x30,
	SystemSessionInformation = 0x31,
	SystemRangeStartInformation = 0x32,
	SystemVerifierInformation = 0x33,
	SystemVerifierThunkExtend = 0x34,
	SystemSessionProcessInformation = 0x35,
	SystemLoadGdiDriverInSystemSpace = 0x36,
	SystemNumaProcessorMap = 0x37,
	SystemPrefetcherInformation = 0x38,
	SystemExtendedProcessInformation = 0x39,
	SystemRecommendedSharedDataAlignment = 0x3a,
	SystemComPlusPackage = 0x3b,
	SystemNumaAvailableMemory = 0x3c,
	SystemProcessorPowerInformation = 0x3d,
	SystemEmulationBasicInformation = 0x3e,
	SystemEmulationProcessorInformation = 0x3f,
	SystemExtendedHandleInformation = 0x40,
	SystemLostDelayedWriteInformation = 0x41,
	SystemBigPoolInformation = 0x42,
	SystemSessionPoolTagInformation = 0x43,
	SystemSessionMappedViewInformation = 0x44,
	SystemHotpatchInformation = 0x45,
	SystemObjectSecurityMode = 0x46,
	SystemWatchdogTimerHandler = 0x47,
	SystemWatchdogTimerInformation = 0x48,
	SystemLogicalProcessorInformation = 0x49,
	SystemWow64SharedInformationObsolete = 0x4a,
	SystemRegisterFirmwareTableInformationHandler = 0x4b,
	SystemFirmwareTableInformation = 0x4c,
	SystemModuleInformationEx = 0x4d,
	SystemVerifierTriageInformation = 0x4e,
	SystemSuperfetchInformation = 0x4f,
	SystemMemoryListInformation = 0x50,
	SystemFileCacheInformationEx = 0x51,
	SystemThreadPriorityClientIdInformation = 0x52,
	SystemProcessorIdleCycleTimeInformation = 0x53,
	SystemVerifierCancellationInformation = 0x54,
	SystemProcessorPowerInformationEx = 0x55,
	SystemRefTraceInformation = 0x56,
	SystemSpecialPoolInformation = 0x57,
	SystemProcessIdInformation = 0x58,
	SystemErrorPortInformation = 0x59,
	SystemBootEnvironmentInformation = 0x5a,
	SystemHypervisorInformation = 0x5b,
	SystemVerifierInformationEx = 0x5c,
	SystemTimeZoneInformation = 0x5d,
	SystemImageFileExecutionOptionsInformation = 0x5e,
	SystemCoverageInformation = 0x5f,
	SystemPrefetchPatchInformation = 0x60,
	SystemVerifierFaultsInformation = 0x61,
	SystemSystemPartitionInformation = 0x62,
	SystemSystemDiskInformation = 0x63,
	SystemProcessorPerformanceDistribution = 0x64,
	SystemNumaProximityNodeInformation = 0x65,
	SystemDynamicTimeZoneInformation = 0x66,
	SystemCodeIntegrityInformation = 0x67,
	SystemProcessorMicrocodeUpdateInformation = 0x68,
	SystemProcessorBrandString = 0x69,
	SystemVirtualAddressInformation = 0x6a,
	SystemLogicalProcessorAndGroupInformation = 0x6b,
	SystemProcessorCycleTimeInformation = 0x6c,
	SystemStoreInformation = 0x6d,
	SystemRegistryAppendString = 0x6e,
	SystemAitSamplingValue = 0x6f,
	SystemVhdBootInformation = 0x70,
	SystemCpuQuotaInformation = 0x71,
	SystemNativeBasicInformation = 0x72,
	SystemErrorPortTimeouts = 0x73,
	SystemLowPriorityIoInformation = 0x74,
	SystemBootEntropyInformation = 0x75,
	SystemVerifierCountersInformation = 0x76,
	SystemPagedPoolInformationEx = 0x77,
	SystemSystemPtesInformationEx = 0x78,
	SystemNodeDistanceInformation = 0x79,
	SystemAcpiAuditInformation = 0x7a,
	SystemBasicPerformanceInformation = 0x7b,
	SystemQueryPerformanceCounterInformation = 0x7c,
	SystemSessionBigPoolInformation = 0x7d,
	SystemBootGraphicsInformation = 0x7e,
	SystemScrubPhysicalMemoryInformation = 0x7f,
	SystemBadPageInformation = 0x80,
	SystemProcessorProfileControlArea = 0x81,
	SystemCombinePhysicalMemoryInformation = 0x82,
	SystemEntropyInterruptTimingInformation = 0x83,
	SystemConsoleInformation = 0x84,
	SystemPlatformBinaryInformation = 0x85,
	SystemThrottleNotificationInformation = 0x86,
	SystemHypervisorProcessorCountInformation = 0x87,
	SystemDeviceDataInformation = 0x88,
	SystemDeviceDataEnumerationInformation = 0x89,
	SystemMemoryTopologyInformation = 0x8a,
	SystemMemoryChannelInformation = 0x8b,
	SystemBootLogoInformation = 0x8c,
	SystemProcessorPerformanceInformationEx = 0x8d,
	SystemSpare0 = 0x8e,
	SystemSecureBootPolicyInformation = 0x8f,
	SystemPageFileInformationEx = 0x90,
	SystemSecureBootInformation = 0x91,
	SystemEntropyInterruptTimingRawInformation = 0x92,
	SystemPortableWorkspaceEfiLauncherInformation = 0x93,
	SystemFullProcessInformation = 0x94,
	SystemKernelDebuggerInformationEx = 0x95,
	SystemBootMetadataInformation = 0x96,
	SystemSoftRebootInformation = 0x97,
	SystemElamCertificateInformation = 0x98,
	SystemOfflineDumpConfigInformation = 0x99,
	SystemProcessorFeaturesInformation = 0x9a,
	SystemRegistryReconciliationInformation = 0x9b,
	MaxSystemInfoClass = 0x9c,
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_MODULE_INFORMATION_ENTRY
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID Base;
	ULONG Size;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT PathLength;
	CHAR ImageName[256];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

#pragma region DBGKD_DATA_STATEMENT
typedef struct _DBGKD_DEBUG_DATA_HEADER64 {
	//
	// Link to other blocks
	//
	LIST_ENTRY64 List;
	//
	// This is a unique tag to identify the owner of the block.
	// If your component only uses one pool tag, use it for this, too.
	//
	ULONG           OwnerTag;
	//
	// This must be initialized to the size of the data block,
	// including this structure.
	//
	ULONG           Size;
} DBGKD_DEBUG_DATA_HEADER64, * PDBGKD_DEBUG_DATA_HEADER64;

typedef struct _KDDEBUGGER_DATA64 {

	DBGKD_DEBUG_DATA_HEADER64 Header;

	//
	// Base address of kernel image
	//

	ULONG64   KernBase;

	//
	// DbgBreakPointWithStatus is a function which takes an argument
	// and hits a breakpoint.  This field contains the address of the
	// breakpoint instruction.  When the debugger sees a breakpoint
	// at this address, it may retrieve the argument from the first
	// argument register, or on x86 the eax register.
	//

	ULONG64   BreakpointWithStatus;       // address of breakpoint

	//
	// Address of the saved context record during a bugcheck
	//
	// N.B. This is an automatic in KeBugcheckEx's frame, and
	// is only valid after a bugcheck.
	//

	ULONG64   SavedContext;

	//
	// help for walking stacks with user callbacks:
	//

	//
	// The address of the thread structure is provided in the
	// WAIT_STATE_CHANGE packet.  This is the offset from the base of
	// the thread structure to the pointer to the kernel stack frame
	// for the currently active usermode callback.
	//

	USHORT  ThCallbackStack;            // offset in thread data

	//
	// these values are offsets into that frame:
	//

	USHORT  NextCallback;               // saved pointer to next callback frame
	USHORT  FramePointer;               // saved frame pointer

	//
	// pad to a quad boundary
	//
	USHORT  PaeEnabled;

	//
	// Address of the kernel callout routine.
	//

	ULONG64   KiCallUserMode;             // kernel routine

	//
	// Address of the usermode entry point for callbacks.
	//

	ULONG64   KeUserCallbackDispatcher;   // address in ntdll


	//
	// Addresses of various kernel data structures and lists
	// that are of interest to the kernel debugger.
	//

	ULONG64   PsLoadedModuleList;
	ULONG64   PsActiveProcessHead;
	ULONG64   PspCidTable;

	ULONG64   ExpSystemResourcesList;
	ULONG64   ExpPagedPoolDescriptor;
	ULONG64   ExpNumberOfPagedPools;

	ULONG64   KeTimeIncrement;
	ULONG64   KeBugCheckCallbackListHead;
	ULONG64   KiBugcheckData;

	ULONG64   IopErrorLogListHead;

	ULONG64   ObpRootDirectoryObject;
	ULONG64   ObpTypeObjectType;

	ULONG64   MmSystemCacheStart;
	ULONG64   MmSystemCacheEnd;
	ULONG64   MmSystemCacheWs;

	ULONG64   MmPfnDatabase;
	ULONG64   MmSystemPtesStart;
	ULONG64   MmSystemPtesEnd;
	ULONG64   MmSubsectionBase;
	ULONG64   MmNumberOfPagingFiles;

	ULONG64   MmLowestPhysicalPage;
	ULONG64   MmHighestPhysicalPage;
	ULONG64   MmNumberOfPhysicalPages;

	ULONG64   MmMaximumNonPagedPoolInBytes;
	ULONG64   MmNonPagedSystemStart;
	ULONG64   MmNonPagedPoolStart;
	ULONG64   MmNonPagedPoolEnd;

	ULONG64   MmPagedPoolStart;
	ULONG64   MmPagedPoolEnd;
	ULONG64   MmPagedPoolInformation;
	ULONG64   MmPageSize;

	ULONG64   MmSizeOfPagedPoolInBytes;

	ULONG64   MmTotalCommitLimit;
	ULONG64   MmTotalCommittedPages;
	ULONG64   MmSharedCommit;
	ULONG64   MmDriverCommit;
	ULONG64   MmProcessCommit;
	ULONG64   MmPagedPoolCommit;
	ULONG64   MmExtendedCommit;

	ULONG64   MmZeroedPageListHead;
	ULONG64   MmFreePageListHead;
	ULONG64   MmStandbyPageListHead;
	ULONG64   MmModifiedPageListHead;
	ULONG64   MmModifiedNoWritePageListHead;
	ULONG64   MmAvailablePages;
	ULONG64   MmResidentAvailablePages;

	ULONG64   PoolTrackTable;
	ULONG64   NonPagedPoolDescriptor;

	ULONG64   MmHighestUserAddress;
	ULONG64   MmSystemRangeStart;
	ULONG64   MmUserProbeAddress;

	ULONG64   KdPrintCircularBuffer;
	ULONG64   KdPrintCircularBufferEnd;
	ULONG64   KdPrintWritePointer;
	ULONG64   KdPrintRolloverCount;

	ULONG64   MmLoadedUserImageList;

	// NT 5.1 Addition

	ULONG64   NtBuildLab;
	ULONG64   KiNormalSystemCall;

	// NT 5.0 hotfix addition

	ULONG64   KiProcessorBlock;
	ULONG64   MmUnloadedDrivers;
	ULONG64   MmLastUnloadedDriver;
	ULONG64   MmTriageActionTaken;
	ULONG64   MmSpecialPoolTag;
	ULONG64   KernelVerifier;
	ULONG64   MmVerifierData;
	ULONG64   MmAllocatedNonPagedPool;
	ULONG64   MmPeakCommitment;
	ULONG64   MmTotalCommitLimitMaximum;
	ULONG64   CmNtCSDVersion;

	// NT 5.1 Addition

	ULONG64   MmPhysicalMemoryBlock;
	ULONG64   MmSessionBase;
	ULONG64   MmSessionSize;
	ULONG64   MmSystemParentTablePage;

	// Server 2003 addition

	ULONG64   MmVirtualTranslationBase;

	USHORT    OffsetKThreadNextProcessor;
	USHORT    OffsetKThreadTeb;
	USHORT    OffsetKThreadKernelStack;
	USHORT    OffsetKThreadInitialStack;

	USHORT    OffsetKThreadApcProcess;
	USHORT    OffsetKThreadState;
	USHORT    OffsetKThreadBStore;
	USHORT    OffsetKThreadBStoreLimit;

	USHORT    SizeEProcess;
	USHORT    OffsetEprocessPeb;
	USHORT    OffsetEprocessParentCID;
	USHORT    OffsetEprocessDirectoryTableBase;

	USHORT    SizePrcb;
	USHORT    OffsetPrcbDpcRoutine;
	USHORT    OffsetPrcbCurrentThread;
	USHORT    OffsetPrcbMhz;

	USHORT    OffsetPrcbCpuType;
	USHORT    OffsetPrcbVendorString;
	USHORT    OffsetPrcbProcStateContext;
	USHORT    OffsetPrcbNumber;

	USHORT    SizeEThread;

	ULONG64   KdPrintCircularBufferPtr;
	ULONG64   KdPrintBufferSize;

	ULONG64   KeLoaderBlock;

	USHORT    SizePcr;
	USHORT    OffsetPcrSelfPcr;
	USHORT    OffsetPcrCurrentPrcb;
	USHORT    OffsetPcrContainedPrcb;

	USHORT    OffsetPcrInitialBStore;
	USHORT    OffsetPcrBStoreLimit;
	USHORT    OffsetPcrInitialStack;
	USHORT    OffsetPcrStackLimit;

	USHORT    OffsetPrcbPcrPage;
	USHORT    OffsetPrcbProcStateSpecialReg;
	USHORT    GdtR0Code;
	USHORT    GdtR0Data;

	USHORT    GdtR0Pcr;
	USHORT    GdtR3Code;
	USHORT    GdtR3Data;
	USHORT    GdtR3Teb;

	USHORT    GdtLdt;
	USHORT    GdtTss;
	USHORT    Gdt64R3CmCode;
	USHORT    Gdt64R3CmTeb;

	ULONG64   IopNumTriageDumpDataBlocks;
	ULONG64   IopTriageDumpDataBlocks;

	// Longhorn addition

	ULONG64   VfCrashDataBlock;
	ULONG64   MmBadPagesDetected;
	ULONG64   MmZeroedPageSingleBitErrorsDetected;

	// Windows 7 addition

	ULONG64   EtwpDebuggerData;
	USHORT    OffsetPrcbContext;

	// Windows 8 addition

	USHORT    OffsetPrcbMaxBreakpoints;
	USHORT    OffsetPrcbMaxWatchpoints;

	ULONG     OffsetKThreadStackLimit;
	ULONG     OffsetKThreadStackBase;
	ULONG     OffsetKThreadQueueListEntry;
	ULONG     OffsetEThreadIrpList;

	USHORT    OffsetPrcbIdleThread;
	USHORT    OffsetPrcbNormalDpcState;
	USHORT    OffsetPrcbDpcStack;
	USHORT    OffsetPrcbIsrStack;

	USHORT    SizeKDPC_STACK_FRAME;

	// Windows 8.1 Addition

	USHORT    OffsetKPriQueueThreadListHead;
	USHORT    OffsetKThreadWaitReason;

	// Windows 10 RS1 Addition

	USHORT    Padding;
	ULONG64   PteBase;

	// Windows 10 RS5 Addition

	ULONG64 RetpolineStubFunctionTable;
	ULONG RetpolineStubFunctionTableSize;
	ULONG RetpolineStubOffset;
	ULONG RetpolineStubSize;

} KDDEBUGGER_DATA64, * PKDDEBUGGER_DATA64;

typedef struct _DUMP_HEADER {
	ULONG Signature;
	ULONG ValidDump;
	ULONG MajorVersion;
	ULONG MinorVersion;
	ULONG_PTR DirectoryTableBase;
	ULONG_PTR PfnDataBase;
	PLIST_ENTRY PsLoadedModuleList;
	PLIST_ENTRY PsActiveProcessHead;
	ULONG MachineImageType;
	ULONG NumberProcessors;
	ULONG BugCheckCode;
	ULONG_PTR BugCheckParameter1;
	ULONG_PTR BugCheckParameter2;
	ULONG_PTR BugCheckParameter3;
	ULONG_PTR BugCheckParameter4;
	CHAR VersionUser[32];

#ifndef _WIN64
	ULONG PaeEnabled;
#endif // !_WIN64
	struct _KDDEBUGGER_DATA64* KdDebuggerDataBlock;
} DUMP_HEADER, * PDUMP_HEADER;
#pragma endregion

typedef VOID(*PKNORMAL_ROUTINE) (
	IN PVOID NormalContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	);

typedef VOID(*PKKERNEL_ROUTINE) (
	IN struct _KAPC* Apc,
	IN OUT PKNORMAL_ROUTINE* NormalRoutine,
	IN OUT PVOID* NormalContext,
	IN OUT PVOID* SystemArgument1,
	IN OUT PVOID* SystemArgument2
	);

typedef VOID(*PKRUNDOWN_ROUTINE) (
	IN struct _KAPC* Apc
	);

typedef enum _KAPC_ENVIRONMENT {
	OriginalApcEnvironment,
	AttachedApcEnvironment,
	CurrentApcEnvironment,
	InsertApcEnvironment
} KAPC_ENVIRONMENT;

// 导出函数但未文档化的
#ifdef  __cplusplus
	EXTERN_C{
#endif	
	NTKERNELAPI PIMAGE_NT_HEADERS NTAPI RtlImageNtHeader(PVOID);

	//extern VOID __security_check_cookie();
	extern POBJECT_TYPE* IoDriverObjectType;
	extern POBJECT_TYPE* ExMutantObjectType;
	extern POBJECT_TYPE* MmSectionObjectType;

	NTKERNELAPI PIMAGE_NT_HEADERS NTAPI RtlImageNtHeader(PVOID);
	NTKERNELAPI PVOID NTAPI PsGetProcessWow64Process(PEPROCESS);
	NTKERNELAPI PVOID NTAPI PsGetProcessPeb(PEPROCESS);
	NTKERNELAPI CHAR* NTAPI PsGetProcessImageFileName(PEPROCESS);
	NTKERNELAPI PVOID NTAPI PsGetCurrentThreadWin32Thread();
	NTKERNELAPI PETHREAD NTAPI PsGetThreadWin32Thread(PETHREAD);
	NTKERNELAPI BOOLEAN NTAPI KeIsAttachedProcess();
	NTKERNELAPI PVOID NTAPI ExfAcquirePushLockExclusive(PVOID);
	NTKERNELAPI PVOID NTAPI ExfReleasePushLockExclusive(PVOID);
	NTKERNELAPI PVOID NTAPI ExfAcquirePushLockShared(PVOID);
	NTKERNELAPI PVOID NTAPI ExfReleasePushLockShared(PVOID);
	NTKERNELAPI NTSTATUS NTAPI MmMarkPhysicalMemoryAsBad(IN PPHYSICAL_ADDRESS, IN OUT PLARGE_INTEGER);
	NTKERNELAPI NTSTATUS NTAPI MmMarkPhysicalMemoryAsGood(IN PPHYSICAL_ADDRESS, IN OUT PLARGE_INTEGER);

	NTKERNELAPI PRUNTIME_FUNCTION NTAPI RtlLookupFunctionEntry(
		DWORD64  ControlPc,
		PDWORD64 ImageBase,
		PVOID	 HistoryTable
	);
	NTKERNELAPI NTSTATUS NTAPI ZwQuerySystemInformation(
		_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
		_Out_ PVOID SystemInformation,
		_In_ ULONG SystemInformationLength,
		_Out_ PULONG ReturnLength
	);
	NTKERNELAPI NTSTATUS NTAPI NtQuerySystemInformation(
		IN ULONG SystemInformationClass,
		IN PVOID SystemInformation,
		IN ULONG SystemInformationLength,
		OUT PULONG ReturnLength
	);
	NTKERNELAPI NTSTATUS NTAPI ZwQueryInformationProcess(
		__in HANDLE ProcessHandle,
		__in PROCESSINFOCLASS ProcessInformationClass,
		__out_bcount(ProcessInformationLength) PVOID ProcessInformation,
		__in ULONG ProcessInformationLength,
		__out_opt PULONG ReturnLength
	);
	NTKERNELAPI NTSTATUS NTAPI NtQueryInformationProcess(
		__in HANDLE ProcessHandle,
		__in PROCESSINFOCLASS ProcessInformationClass,
		__out_bcount(ProcessInformationLength) PVOID ProcessInformation,
		__in ULONG ProcessInformationLength,
		__out_opt PULONG ReturnLength
	);
	NTKERNELAPI NTSTATUS NTAPI NtMapViewOfSection(
		_In_ HANDLE SectionHandle,
		_In_ HANDLE ProcessHandle,
		_Outptr_result_bytebuffer_(*ViewSize) PVOID* BaseAddress,
		_In_ ULONG_PTR ZeroBits,
		_In_ SIZE_T CommitSize,
		_Inout_opt_ PLARGE_INTEGER SectionOffset,
		_Inout_ PSIZE_T ViewSize,
		_In_ SECTION_INHERIT InheritDisposition,
		_In_ ULONG AllocationType,
		_In_ ULONG Win32Protect
	);
	NTKERNELAPI ULONG NTAPI KeCapturePersistentThreadState(
		__in PCONTEXT Context,
		__in_opt PKTHREAD Thread,
		__in ULONG BugCheckCode,
		__in ULONG_PTR BugCheckParameter1,
		__in ULONG_PTR BugCheckParameter2,
		__in ULONG_PTR BugCheckParameter3,
		__in ULONG_PTR BugCheckParameter4,
		__in PDUMP_HEADER DumpHeader
	);
	NTKERNELAPI VOID NTAPI KeInitializeApc(
		PKAPC 				Apc,
		PKTHREAD 			Thread,
		KAPC_ENVIRONMENT 	TargetEnvironment,
		PKKERNEL_ROUTINE 	KernelRoutine,
		PKRUNDOWN_ROUTINE 	RundownRoutine,
		PKNORMAL_ROUTINE 	NormalRoutine,
		KPROCESSOR_MODE 	Mode,
		PVOID 				Context
	);

	NTKERNELAPI BOOLEAN NTAPI KeInsertQueueApc(
		IN PKAPC Apc,
		IN PVOID SystemArgument1,
		IN PVOID SystemArgument2,
		IN KPRIORITY PriorityBoost
	);
	NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
		PEPROCESS SourceProcess,
		PVOID SourceAddress,
		PEPROCESS TargetProcess,
		PVOID TargetAddress,
		SIZE_T BufferSize,
		KPROCESSOR_MODE PreviousMode,
		PSIZE_T ReturnSize
	);
	NTKERNELAPI NTSTATUS NTAPI ObReferenceObjectByName(
		PUNICODE_STRING ObjectName,
		unsigned int Attributes,
		PACCESS_STATE PassedAccessState OPTIONAL,
		ACCESS_MASK DesiredAccess OPTIONAL,
		POBJECT_TYPE ObjectType,
		KPROCESSOR_MODE AccessMode,
		OUT PVOID ParseContext OPTIONAL,
		OUT PVOID* Object
	);
	NTKERNELAPI NTSTATUS NTAPI IoGetDeviceObjectPointer(
			IN PUNICODE_STRING  ObjectName,
			IN ACCESS_MASK  DesiredAccess,
			OUT PFILE_OBJECT* FileObject,
			OUT PDEVICE_OBJECT* DeviceObject
		);
	NTSTATUS ObQueryNameString(
		_In_ PVOID Object,
		_Out_writes_bytes_opt_(Length) POBJECT_NAME_INFORMATION ObjectNameInfo,
		_In_ ULONG Length,
		_Out_ PULONG ReturnLength
	);
#ifdef __cplusplus
}
#endif //  __cplusplus
