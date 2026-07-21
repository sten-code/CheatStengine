#pragma once

#include <ntifs.h>

void InitializeMemorySupport();
bool CanCopyProcessMemory();

bool IsUserAddressRange(UINT64 address, UINT64 size);

NTSTATUS ReadProcessAddressSpace(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred);

NTSTATUS WriteProcessAddressSpace(
    PEPROCESS process,
    UINT64 address,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesTransferred);
