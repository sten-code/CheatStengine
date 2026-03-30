#include "process.h"
#include "memory.h"

#include <ntifs.h>

extern "C" NTSYSAPI NTSTATUS NTAPI ZwProtectVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID* BaseAddress,
    IN OUT PSIZE_T NumberOfBytesToProtect,
    IN ULONG NewAccessProtection,
    OUT PULONG OldAccessProtection);

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

NTSTATUS ReadProcessMemory(PEPROCESS process, uintptr_t address, void* buffer, size_t size)
{
    DbgPrint("[CheatStengine] ReadProcessMemory  addr=0x%p  size=%zu\n",
        reinterpret_cast<void*>(address), size);

    if (!buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsValidUserModeAddress(address, size)) {
        DbgPrint("[CheatStengine] ReadProcessMemory: Invalid user mode address 0x%p\n",
            reinterpret_cast<void*>(address));
        return STATUS_ACCESS_VIOLATION;
    }

    const UINT64 dtb = GetProcessCr3(process);
    if (IsPageNoAccess(dtb, address)) {
        DbgPrint("[CheatStengine] ReadProcessMemory: Address 0x%p is marked as PAGE_NOACCESS\n",
            reinterpret_cast<void*>(address));
        return STATUS_ACCESS_DENIED;
    }

    const UINT64 physAddr = TranslateLinear(dtb, address);
    if (!physAddr) {
        DbgPrint("[CheatStengine] ReadProcessMemory: Failed to translate linear address 0x%p\n",
            reinterpret_cast<void*>(address));
        return STATUS_UNSUCCESSFUL;
    }

    // Clamp to the end of the current physical page.
    const ULONG64 chunk = MIN(PAGE_SIZE - static_cast<INT32>(physAddr & 0xFFF), size);
    SIZE_T bytesRead = 0;
    NTSTATUS status = PhysRead(reinterpret_cast<PVOID>(physAddr), buffer, chunk, &bytesRead);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] PhysRead failed: 0x%08X\n", status);
    }
    return status;
}

NTSTATUS WriteProcessMemory(PEPROCESS process, uintptr_t address, void* buffer, size_t size)
{
    DbgPrint("[CheatStengine] WriteProcessMemory  addr=0x%p  size=%zu\n",
        reinterpret_cast<void*>(address), size);

    if (!buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsValidUserModeAddress(address, size)) {
        return STATUS_ACCESS_VIOLATION;
    }

    const UINT64 dtb = GetProcessCr3(process);
    if (IsPageNoAccess(dtb, address)) {
        return STATUS_ACCESS_DENIED;
    }

    const UINT64 physAddr = TranslateLinear(dtb, address);
    if (!physAddr) {
        return STATUS_UNSUCCESSFUL;
    }

    const ULONG64 chunk = MIN(PAGE_SIZE - static_cast<INT32>(physAddr & 0xFFF), size);
    SIZE_T bytesWritten = 0;
    NTSTATUS status = PhysWrite(reinterpret_cast<PVOID>(physAddr), buffer, chunk, &bytesWritten);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] PhysWrite failed: 0x%08X\n", status);
    }
    return status;
}

NTSTATUS QueryProcessMemory(PEPROCESS process, uintptr_t address, MEMORY_BASIC_INFORMATION* mbi)
{
    DbgPrint("[CheatStengine] QueryProcessMemory  addr=0x%p, mbi=0x%p\n",
        reinterpret_cast<void*>(address), reinterpret_cast<void*>(mbi));

    if (!process || !mbi) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsValidUserModeAddress(address, 1)) {
        DbgPrint("[CheatStengine] QueryProcessMemory: Invalid user mode address 0x%p\n",
            reinterpret_cast<void*>(address));
        return STATUS_ACCESS_VIOLATION;
    }

    NTSTATUS status = STATUS_SUCCESS;

    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    __try {
        SIZE_T returnLength = 0;

        status = ZwQueryVirtualMemory(
            ZwCurrentProcess(),
            reinterpret_cast<PVOID>(address),
            MemoryBasicInformation,
            mbi,
            sizeof(MEMORY_BASIC_INFORMATION),
            &returnLength);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrint("[CheatStengine] Exception in QueryProcessMemory: 0x%08X\n", status);
    }
    KeUnstackDetachProcess(&apcState);

    DbgPrint("[CheatStengine] ZwQueryVirtualMemory returned 0x%08X, BaseAddress=0x%p, RegionSize=0x%p, State=0x%X, Protect=0x%X, Type=0x%X\n",
        status, mbi->BaseAddress, reinterpret_cast<void*>(mbi->RegionSize), mbi->State, mbi->Protect, mbi->Type);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] ZwQueryVirtualMemory failed: 0x%08X\n", status);
    }
    return status;
}

NTSTATUS AllocateProcessMemory(PEPROCESS process, uintptr_t address, size_t size,
    uint32_t allocationType, uint32_t protect,
    uintptr_t* outAddress)
{
    if (!process || !outAddress || size == 0)
        return STATUS_INVALID_PARAMETER;

    NTSTATUS status = STATUS_SUCCESS;
    PVOID baseAddress = reinterpret_cast<PVOID>(address);
    SIZE_T regionSize = size;

    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    __try {
        status = ZwAllocateVirtualMemory(
            ZwCurrentProcess(),
            &baseAddress,
            0,
            &regionSize,
            allocationType,
            protect);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrint("[CheatStengine] Exception in AllocateProcessMemory: 0x%08X\n", status);
    }
    KeUnstackDetachProcess(&apcState);

    if (NT_SUCCESS(status)) {
        *outAddress = reinterpret_cast<uintptr_t>(baseAddress);
    } else {
        DbgPrint("[CheatStengine] ZwAllocateVirtualMemory failed: 0x%08X\n", status);
    }
    return status;
}

NTSTATUS FreeProcessMemory(PEPROCESS process, uintptr_t address, size_t size, uint32_t freeType)
{
    if (!process || address == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_SUCCESS;
    PVOID baseAddress = reinterpret_cast<PVOID>(address);
    SIZE_T regionSize = size;

    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    __try {
        status = ZwFreeVirtualMemory(
            ZwCurrentProcess(),
            &baseAddress,
            &regionSize,
            freeType);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrint("[CheatStengine] Exception in FreeProcessMemory: 0x%08X\n", status);
    }
    KeUnstackDetachProcess(&apcState);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] ZwFreeVirtualMemory failed: 0x%08X\n", status);
    }
    return status;
}

NTSTATUS ProtectProcessMemory(PEPROCESS process, uintptr_t address, size_t size,
    uint32_t newProtect, uint32_t* oldProtect)
{
    if (!process || address == 0 || size == 0 || !oldProtect) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsValidUserModeAddress(address, size)) {
        return STATUS_ACCESS_VIOLATION;
    }

    NTSTATUS status = STATUS_SUCCESS;
    ULONG oldProtectValue = 0;

    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    __try {
        status = ZwProtectVirtualMemory(
            ZwCurrentProcess(),
            reinterpret_cast<PVOID*>(&address),
            &size,
            newProtect,
            &oldProtectValue);

        if (NT_SUCCESS(status)) {
            *oldProtect = oldProtectValue;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrint("[CheatStengine] Exception in ProtectProcessMemory: 0x%08X\n", status);
    }
    KeUnstackDetachProcess(&apcState);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] ZwProtectVirtualMemory failed: 0x%08X\n", status);
    }
    return status;
}