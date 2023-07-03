#ifndef _LIST_KERNEL_OBJ_
#define _LIST_KERNEL_OBJ_
#include "CommonHead.h"
https://github.com/hfiref0x/WinObjEx64/tree/master/Compiled
namespace list_kernel_obj 
{
    typedef enum _SYSTEM_INFORMATION_CLASS {
        SystemBasicInformation,
        SystemProcessorInformation,             // obsolete...delete
        SystemPerformanceInformation,
        SystemTimeOfDayInformation,
        SystemPathInformation,
        SystemProcessInformation,
        SystemCallCountInformation,
        SystemDeviceInformation,
        SystemProcessorPerformanceInformation,
        SystemFlagsInformation,
        SystemCallTimeInformation,
        SystemModuleInformation,
        SystemLocksInformation,
        SystemStackTraceInformation,
        SystemPagedPoolInformation,
        SystemNonPagedPoolInformation,
        SystemHandleInformation,
        SystemObjectInformation,
        SystemPageFileInformation,
        SystemVdmInstemulInformation,
        SystemVdmBopInformation,
        SystemFileCacheInformation,
        SystemPoolTagInformation,
        SystemInterruptInformation,
        SystemDpcBehaviorInformation,
        SystemFullMemoryInformation,
        SystemLoadGdiDriverInformation,
        SystemUnloadGdiDriverInformation,
        SystemTimeAdjustmentInformation,
        SystemSummaryMemoryInformation,
        SystemMirrorMemoryInformation,
        SystemPerformanceTraceInformation,
        SystemObsolete0,
        SystemExceptionInformation,
        SystemCrashDumpStateInformation,
        SystemKernelDebuggerInformation,
        SystemContextSwitchInformation,
        SystemRegistryQuotaInformation,
        SystemExtendServiceTableInformation,
        SystemPrioritySeperation,
        SystemVerifierAddDriverInformation,
        SystemVerifierRemoveDriverInformation,
        SystemProcessorIdleInformation,
        SystemLegacyDriverInformation,
        SystemCurrentTimeZoneInformation,
        SystemLookasideInformation,
        SystemTimeSlipNotification,
        SystemSessionCreate,
        SystemSessionDetach,
        SystemSessionInformation,
        SystemRangeStartInformation,
        SystemVerifierInformation,
        SystemVerifierThunkExtend,
        SystemSessionProcessInformation,
        SystemLoadGdiDriverInSystemSpace,
        SystemNumaProcessorMap,
        SystemPrefetcherInformation,
        SystemExtendedProcessInformation,
        SystemRecommendedSharedDataAlignment,
        SystemComPlusPackage,
        SystemNumaAvailableMemory,
        SystemProcessorPowerInformation,
        SystemEmulationBasicInformation,
        SystemEmulationProcessorInformation,
        SystemExtendedHandleInformation,
        SystemLostDelayedWriteInformation,
        SystemBigPoolInformation,
        SystemSessionPoolTagInformation,
        SystemSessionMappedViewInformation,
        SystemHotpatchInformation,
        SystemObjectSecurityMode,
        SystemWatchdogTimerHandler,
        SystemWatchdogTimerInformation,
        SystemLogicalProcessorInformation,
        SystemWow64SharedInformation,
        SystemRegisterFirmwareTableInformationHandler,
        SystemFirmwareTableInformation,
        SystemModuleInformationEx,
        SystemVerifierTriageInformation,
        SystemSuperfetchInformation,
        SystemMemoryListInformation,
        SystemFileCacheInformationEx,
        MaxSystemInfoClass  // MaxSystemInfoClass should always be the last enum
    } SYSTEM_INFORMATION_CLASS;

    typedef struct _SYSTEM_HANDLE_INFORMATION {
        ULONG ProcessId;
        BYTE ObjectTypeNumber;
        BYTE Flags;
        USHORT Handle;
        PVOID Object;
        ACCESS_MASK GrantedAccess;
    } SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

    // 定义 NtQuerySystemInformation 函数类型
    typedef NTSTATUS(WINAPI* PFN_NtQuerySystemInformation)(
        ULONG SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength,
        PULONG ReturnLength
        );

    int get_list_obj() {
        // 获取 NtQuerySystemInformation 函数地址
        PFN_NtQuerySystemInformation pNtQuerySystemInformation =
            reinterpret_cast<PFN_NtQuerySystemInformation>(
                GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation")
                );

        if (!pNtQuerySystemInformation) {
            std::cout << "Failed to get NtQuerySystemInformation function address:"<< GetLastError() << std::endl;
            return 1;
        }

        // 获取系统信息的大小
        ULONG bufferSize = 0;
        pNtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &bufferSize); //  
        std::cout << "bufferSize" << bufferSize <<std::endl;

        // 分配足够大的缓冲区来存储系统信息
        std::vector<BYTE> buffer(bufferSize);

        // 调用 NtQuerySystemInformation 函数获取系统信息
        NTSTATUS status = pNtQuerySystemInformation(SystemProcessInformation, buffer.data(), bufferSize, nullptr);
        if (status != 0) {
            std::cout << "Failed to retrieve system information:"<< status << std::endl;
            return 1;
        }

        // 解析系统信息并输出内核对象句柄
        PSYSTEM_HANDLE_INFORMATION handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(buffer.data());

        for (ULONG i = 0; i < handleInfo->ObjectTypeNumber; i++) {
            HANDLE handle = reinterpret_cast<HANDLE>(handleInfo[i].Handle);
            // 在这里处理句柄，可以获取句柄所对应的内核对象信息或执行其他操作
            std::cout << "Handle: " << handle << std::endl;
        }

        return 0;
    }

}
#endif