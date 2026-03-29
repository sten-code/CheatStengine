#pragma once

#include <cstdint>
#include <ntifs.h>

NTSTATUS ReadProcessMemory(
    PEPROCESS process,
    uintptr_t address,
    void* buffer,
    size_t size);

NTSTATUS WriteProcessMemory(
    PEPROCESS process,
    uintptr_t address,
    void* buffer,
    size_t size);

NTSTATUS QueryProcessMemory(
    PEPROCESS process,
    uintptr_t address,
    PMEMORY_BASIC_INFORMATION mbi);

NTSTATUS AllocateProcessMemory(
    PEPROCESS process,
    uintptr_t address,
    size_t size,
    uint32_t allocationType,
    uint32_t protect,
    uintptr_t* outAddress);

NTSTATUS FreeProcessMemory(
    PEPROCESS process,
    uintptr_t address,
    size_t size,
    uint32_t freeType);

NTSTATUS ProtectProcessMemory(
    PEPROCESS process,
    uintptr_t address,
    size_t size,
    uint32_t newProtect,
    uint32_t* oldProtect);