#pragma once

#include <ntifs.h>

void InitializeProcessSupport();
bool CanProtectProcessMemory();

NTSTATUS ReadProcessMemory(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred);

NTSTATUS WriteProcessMemory(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred);

NTSTATUS QueryProcessMemory(
    PEPROCESS process,
    UINT64 address,
    PMEMORY_BASIC_INFORMATION memoryInformation);

NTSTATUS AllocateProcessMemory(
    PEPROCESS process,
    UINT64 address,
    UINT64 size,
    ULONG allocationType,
    ULONG protect,
    PUINT64 allocatedAddress,
    PUINT64 allocatedSize);

NTSTATUS FreeProcessMemory(
    PEPROCESS process,
    UINT64 address,
    UINT64 size,
    ULONG freeType);

NTSTATUS ProtectProcessMemory(
    PEPROCESS process,
    UINT64 address,
    UINT64 size,
    ULONG newProtect,
    PULONG oldProtect,
    PUINT64 protectedAddress,
    PUINT64 protectedSize);
