#pragma once

#include <ntifs.h>

NTSTATUS PhysRead(PVOID physAddress, PVOID buffer, SIZE_T size, SIZE_T* bytesRead);
NTSTATUS PhysWrite(PVOID physAddress, PVOID buffer, SIZE_T size, SIZE_T* bytesWritten);

UINT64 TranslateLinear(UINT64 dtb, UINT64 va);
bool IsPageNoAccess(UINT64 dtb, UINT64 va);
bool IsValidUserModeAddress(uintptr_t address, size_t size);
UINT64 GetProcessCr3(PEPROCESS process);