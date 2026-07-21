#include "Process.h"

#include "Memory.h"

namespace {

using ProtectRoutine = NTSTATUS (NTAPI*)(
    HANDLE,
    PVOID*,
    PSIZE_T,
    ULONG,
    PULONG);

PVOID volatile ProtectAddress = nullptr;

bool IsNativeSize(UINT64 size)
{
    return size <= static_cast<UINT64>(MAXULONG_PTR);
}

bool IsUserAddress(UINT64 address)
{
    return address <= reinterpret_cast<UINT64>(MmHighestUserAddress);
}

}

void InitializeProcessSupport()
{
    UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"ZwProtectVirtualMemory");
    PVOID routine = MmGetSystemRoutineAddress(&routineName);
    InterlockedExchangePointer(&ProtectAddress, routine);
}

bool CanProtectProcessMemory()
{
    return InterlockedCompareExchangePointer(&ProtectAddress, nullptr, nullptr) != nullptr;
}

NTSTATUS ReadProcessMemory(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred)
{
    return ReadProcessAddressSpace(process, address, buffer, size, bytesTransferred);
}

NTSTATUS WriteProcessMemory(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred)
{
    return WriteProcessAddressSpace(process, address, buffer, size, bytesTransferred);
}

NTSTATUS QueryProcessMemory(
    PEPROCESS process,
    UINT64 address,
    PMEMORY_BASIC_INFORMATION memoryInformation)
{
    if (!process || !memoryInformation || !IsUserAddress(address)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    NTSTATUS status = STATUS_SUCCESS;
    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    SIZE_T returnLength = 0;
    status = ZwQueryVirtualMemory(
        ZwCurrentProcess(),
        reinterpret_cast<PVOID>(address),
        MemoryBasicInformation,
        memoryInformation,
        sizeof(*memoryInformation),
        &returnLength);
    KeUnstackDetachProcess(&apcState);
    return status;
}

NTSTATUS AllocateProcessMemory(
    PEPROCESS process,
    UINT64 address,
    UINT64 size,
    ULONG allocationType,
    ULONG protect,
    PUINT64 allocatedAddress,
    PUINT64 allocatedSize)
{
    if (!process || !allocatedAddress || !allocatedSize || size == 0
        || !IsNativeSize(size)
        || (address != 0 && !IsUserAddressRange(address, size))) {
        return STATUS_INVALID_PARAMETER;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    NTSTATUS status = STATUS_SUCCESS;
    PVOID baseAddress = reinterpret_cast<PVOID>(address);
    SIZE_T regionSize = static_cast<SIZE_T>(size);
    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    status = ZwAllocateVirtualMemory(
        ZwCurrentProcess(),
        &baseAddress,
        0,
        &regionSize,
        allocationType,
        protect);
    KeUnstackDetachProcess(&apcState);

    if (NT_SUCCESS(status)) {
        *allocatedAddress = reinterpret_cast<UINT64>(baseAddress);
        *allocatedSize = regionSize;
    }
    return status;
}

NTSTATUS FreeProcessMemory(
    PEPROCESS process,
    UINT64 address,
    UINT64 size,
    ULONG freeType)
{
    const bool release = freeType == MEM_RELEASE && size == 0;
    const bool decommit = freeType == MEM_DECOMMIT
        && size != 0
        && IsUserAddressRange(address, size);
    if (!process || address == 0 || !IsNativeSize(size) || (!release && !decommit)
        || !IsUserAddress(address)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    NTSTATUS status = STATUS_SUCCESS;
    PVOID baseAddress = reinterpret_cast<PVOID>(address);
    SIZE_T regionSize = static_cast<SIZE_T>(size);
    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    status = ZwFreeVirtualMemory(
        ZwCurrentProcess(),
        &baseAddress,
        &regionSize,
        freeType);
    KeUnstackDetachProcess(&apcState);
    return status;
}

NTSTATUS ProtectProcessMemory(
    PEPROCESS process,
    UINT64 address,
    UINT64 size,
    ULONG newProtect,
    PULONG oldProtect,
    PUINT64 protectedAddress,
    PUINT64 protectedSize)
{
    if (!process || !oldProtect || !protectedAddress || !protectedSize
        || !IsNativeSize(size) || !IsUserAddressRange(address, size)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    const auto protectRoutine = reinterpret_cast<ProtectRoutine>(
        InterlockedCompareExchangePointer(&ProtectAddress, nullptr, nullptr));
    if (!protectRoutine) {
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS status = STATUS_SUCCESS;
    PVOID baseAddress = reinterpret_cast<PVOID>(address);
    SIZE_T regionSize = static_cast<SIZE_T>(size);
    ULONG previousProtect = 0;
    KAPC_STATE apcState {};
    KeStackAttachProcess(process, &apcState);
    status = protectRoutine(
        ZwCurrentProcess(),
        &baseAddress,
        &regionSize,
        newProtect,
        &previousProtect);
    KeUnstackDetachProcess(&apcState);

    if (NT_SUCCESS(status)) {
        *oldProtect = previousProtect;
        *protectedAddress = reinterpret_cast<UINT64>(baseAddress);
        *protectedSize = regionSize;
    }
    return status;
}
