#include "memory.h"

#include <ntifs.h>

// Credit to https://github.com/ryan-weil/ReadWriteDriver

static constexpr UINT32 PAGE_OFFSET_BITS = 12;
static constexpr UINT64 PTE_PHYS_MASK = (~0xFULL << 8) & 0xFFF'FFFF'FFULL;
static constexpr UINT64 PTE_PHYS_MASK_WIDE = 0x000F'FFFF'FFFF'F000ULL;

namespace {
    // RTL_OSVERSIONINFOW build numbers
    enum WindowsBuild : ULONG {
        Build_1803 = 17134,
        Build_1809 = 17763,
        Build_1903 = 18362,
        Build_1909 = 18363,
        Build_2004 = 19041,
        Build_20H2 = 19569,
        Build_21H1 = 20180,
        Build_22H1 = 22000,
        Build_22H2 = 22621,
        Build_22H22 = 19045,
        Build_Win11 = 24345,
    };

    INT32 UserDirBaseOffset()
    {
        RTL_OSVERSIONINFOW ver {};
        ver.dwOSVersionInfoSize = sizeof(ver);
        RtlGetVersion(&ver);

        switch (ver.dwBuildNumber) {
            case Build_1803:
            case Build_1809:
                return 0x0278;

            case Build_1903:
            case Build_1909:
                return 0x0280;

            case Build_2004:
            case Build_20H2:
            case Build_21H1:
            case Build_22H1:
            case Build_22H2:
            case Build_22H22:
            case Build_Win11:
            default:
                return 0x0388;
        }
    }
}

UINT64 GetProcessCr3(PEPROCESS process)
{
    PUCHAR processBytes = reinterpret_cast<PUCHAR>(process);
    ULONG_PTR dirBase = *reinterpret_cast<PULONG_PTR>(processBytes + 0x28);
    if (dirBase != 0) {
        return dirBase;
    }

    INT32 offset = UserDirBaseOffset();
    return *reinterpret_cast<PULONG_PTR>(processBytes + offset);
}

bool IsValidUserModeAddress(uintptr_t address, size_t size)
{
    uintptr_t end = address + size;
    if (end < address) {
        // overflow check
        return false;
    }

    return address < MmUserProbeAddress && end <= MmUserProbeAddress;
}

NTSTATUS PhysRead(PVOID physAddress, PVOID buffer, SIZE_T size, SIZE_T* bytesRead)
{
    MM_COPY_ADDRESS src {};
    src.PhysicalAddress.QuadPart = reinterpret_cast<LONGLONG>(physAddress);
    return MmCopyMemory(buffer, src, size, MM_COPY_MEMORY_PHYSICAL, bytesRead);
}

NTSTATUS PhysWrite(PVOID physAddress, PVOID buffer, SIZE_T size, SIZE_T* bytesWritten)
{
    if (!physAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    PHYSICAL_ADDRESS pa {};
    pa.QuadPart = reinterpret_cast<LONGLONG>(physAddress);

    PVOID mapped = MmMapIoSpaceEx(pa, size, PAGE_READWRITE);
    if (!mapped) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(mapped, buffer, size);
    *bytesWritten = size;
    MmUnmapIoSpace(mapped, size);
    return STATUS_SUCCESS;
}

UINT64 TranslateLinear(UINT64 dtb, UINT64 va)
{
    dtb &= ~0xFULL;

    const UINT64 pageOffset = va & ~(~0ULL << PAGE_OFFSET_BITS);
    const UINT64 pteIndex = (va >> 12) & 0x1FFULL;
    const UINT64 ptIndex = (va >> 21) & 0x1FFULL;
    const UINT64 pdIndex = (va >> 30) & 0x1FFULL;
    const UINT64 pdpIndex = (va >> 39) & 0x1FFULL;

    SIZE_T br = 0;

    // --- Level 4 -> PDPTE ---
    UINT64 pdpe = 0;
    PhysRead(reinterpret_cast<PVOID>(dtb + 8 * pdpIndex), &pdpe, sizeof(pdpe), &br);
    if (!(pdpe & 1)) {
        return 0;
    }

    // --- Level 3 --> PDE ---
    UINT64 pde = 0;
    PhysRead(reinterpret_cast<PVOID>((pdpe & PTE_PHYS_MASK) + 8 * pdIndex), &pde, sizeof(pde), &br);
    if (!(pde & 1)) {
        return 0;
    }

    if (pde & (1ULL << 7)) {
        // 1 GiB huge page
        return (pde & (~0ULL << 42 >> 12)) + (va & ~(~0ULL << 30));
    }

    // --- Level 2 -> PTE ---
    UINT64 pteAddr = 0;
    PhysRead(reinterpret_cast<PVOID>((pde & PTE_PHYS_MASK) + 8 * ptIndex), &pteAddr, sizeof(pteAddr), &br);
    if (!(pteAddr & 1)) {
        return 0;
    }

    if (pteAddr & (1ULL << 7)) {
        // 2 MiB large page
        return (pteAddr & PTE_PHYS_MASK) + (va & ~(~0ULL << 21));
    }

    // --- Level 1 -> physical page ---
    UINT64 pte = 0;
    PhysRead(reinterpret_cast<PVOID>((pteAddr & PTE_PHYS_MASK) + 8 * pteIndex), &pte, sizeof(pte), &br);
    UINT64 physBase = pte & PTE_PHYS_MASK;
    if (!physBase) {
        return 0;
    }

    return physBase + pageOffset;
}

bool IsPageNoAccess(UINT64 dtb, UINT64 va)
{
    const UINT64 pteIndex = (va >> 12) & 0x1FFULL;
    const UINT64 ptIndex = (va >> 21) & 0x1FFULL;
    const UINT64 pdIndex = (va >> 30) & 0x1FFULL;
    const UINT64 pdpIndex = (va >> 39) & 0x1FFULL;

    SIZE_T br = 0;
    UINT64 entry = 0;

    // PDPTE
    PhysRead(reinterpret_cast<PVOID>((dtb & ~0xFFFULL) + pdpIndex * sizeof(UINT64)),
        &entry, sizeof(entry), &br);
    if (!(entry & 1)) {
        return true;
    }

    // PDE
    {
        UINT64 base = entry & PTE_PHYS_MASK_WIDE;
        PhysRead(reinterpret_cast<PVOID>(base + pdIndex * sizeof(UINT64)),
            &entry, sizeof(entry), &br);
        if (!(entry & 1)) {
            return true;
        }
        if (entry & (1ULL << 7)) {
            return false; // 1 GiB page – present
        }
    }

    // PT entry (2 MiB check)
    {
        UINT64 base = entry & PTE_PHYS_MASK_WIDE;
        PhysRead(reinterpret_cast<PVOID>(base + ptIndex * sizeof(UINT64)),
            &entry, sizeof(entry), &br);
        if (!(entry & 1)) {
            return true;
        }
        if (entry & (1ULL << 7)) {
            return false; // 2 MiB page – present
        }
    }

    // PTE
    {
        UINT64 base = entry & PTE_PHYS_MASK_WIDE;
        PhysRead(reinterpret_cast<PVOID>(base + pteIndex * sizeof(UINT64)),
            &entry, sizeof(entry), &br);
        return !(entry & 1);
    }
}