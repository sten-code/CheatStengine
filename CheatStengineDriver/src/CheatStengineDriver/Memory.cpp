#include "Memory.h"

namespace {

using CopyRoutine = NTSTATUS (NTAPI*)(
    PEPROCESS,
    PVOID,
    PEPROCESS,
    PVOID,
    SIZE_T,
    KPROCESSOR_MODE,
    PSIZE_T);

PVOID volatile CopyAddress = nullptr;

NTSTATUS TransferStatus(NTSTATUS status, SIZE_T transferred, SIZE_T requested)
{
    if (transferred == requested) {
        return STATUS_SUCCESS;
    }
    if (transferred != 0) {
        return STATUS_PARTIAL_COPY;
    }
    return NT_SUCCESS(status) ? STATUS_PARTIAL_COPY : status;
}

CopyRoutine GetCopyRoutine()
{
    return reinterpret_cast<CopyRoutine>(
        InterlockedCompareExchangePointer(&CopyAddress, nullptr, nullptr));
}

}

void InitializeMemorySupport()
{
    UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"MmCopyVirtualMemory");
    InterlockedExchangePointer(
        &CopyAddress,
        MmGetSystemRoutineAddress(&routineName));
}

bool CanCopyProcessMemory()
{
    return GetCopyRoutine() != nullptr;
}

bool IsUserAddressRange(UINT64 address, UINT64 size)
{
    if (address == 0 || size == 0) {
        return false;
    }

    const UINT64 maximumAddress = reinterpret_cast<UINT64>(MmHighestUserAddress);
    return address <= maximumAddress && size - 1 <= maximumAddress - address;
}

NTSTATUS ReadProcessAddressSpace(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred)
{
    if (!process || !buffer || !bytesTransferred || !IsUserAddressRange(address, size)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    const CopyRoutine copyRoutine = GetCopyRoutine();
    if (!copyRoutine) {
        return STATUS_NOT_SUPPORTED;
    }

    *bytesTransferred = 0;
    SIZE_T transferred = 0;
    const NTSTATUS status = copyRoutine(
        process,
        reinterpret_cast<PVOID>(address),
        PsGetCurrentProcess(),
        buffer,
        size,
        KernelMode,
        &transferred);
    *bytesTransferred = transferred;
    return TransferStatus(status, transferred, size);
}

NTSTATUS WriteProcessAddressSpace(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred)
{
    if (!process || !buffer || !bytesTransferred || !IsUserAddressRange(address, size)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    const CopyRoutine copyRoutine = GetCopyRoutine();
    if (!copyRoutine) {
        return STATUS_NOT_SUPPORTED;
    }

    *bytesTransferred = 0;
    SIZE_T transferred = 0;
    const NTSTATUS status = copyRoutine(
        PsGetCurrentProcess(),
        buffer,
        process,
        reinterpret_cast<PVOID>(address),
        size,
        KernelMode,
        &transferred);
    *bytesTransferred = transferred;
    return TransferStatus(status, transferred, size);
}
