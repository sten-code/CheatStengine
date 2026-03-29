#pragma once

#include <cstdint>
#include <ntifs.h>
#include <ntstrsafe.h>

#define IOCTL_CS_COMMAND \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x6969, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct CommandHeader {
    enum Type : uint32_t {
        ReadMemory = 0,
        WriteMemory = 1,
        QueryMemory = 2,
        AllocateMemory = 3,
        FreeMemory = 4,
        ProtectMemory = 5,
    } Type;

    uint32_t Pid;

    union {
        struct
        {
            uintptr_t Address;
            size_t Size;
            void* Buffer;
        } ReadMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            void* Buffer;
        } WriteMemoryData;

        struct
        {
            uintptr_t Address;
        } QueryMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            uint32_t AllocationType;
            uint32_t Protect;
            uintptr_t* AllocatedAddressPtr;
        } AllocateMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            uint32_t FreeType;
        } FreeMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            uint32_t NewProtect;
            uint32_t* OldProtectPtr;
        } ProtectMemoryData;
    };
};