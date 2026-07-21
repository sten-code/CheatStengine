#pragma once

#ifdef _KMODE
#include <wdm.h>
#else
#include <Windows.h>
#include <winioctl.h>
#endif

#include <cstdint>

namespace Protocol {

inline constexpr uint16_t VersionMajor = 1;
inline constexpr uint16_t VersionMinor = 2;
inline constexpr uint16_t DriverVersionMajor = 1;
inline constexpr uint16_t DriverVersionMinor = 0;
inline constexpr uint32_t DriverVersionPatch = 0;
inline constexpr uint32_t DeviceType = 0x8000;
inline constexpr uint64_t MaximumTransferSize = 16ULL * 1024ULL * 1024ULL;
inline constexpr uint32_t DefaultIdleTimeoutSeconds = 60;
inline constexpr uint32_t MinimumIdleTimeoutSeconds = 15;
inline constexpr uint32_t MaximumIdleTimeoutSeconds = 3600;

inline constexpr GUID DeviceInterfaceGuid = {
    0x8d3f3f2d,
    0x3d46,
    0x4a66,
    { 0x8f, 0x9b, 0x46, 0xe6, 0xd5, 0xa6, 0x78, 0x31 }
};

enum Capability : uint64_t {
    BindProcess = 1ULL << 0,
    ReadMemory = 1ULL << 1,
    WriteMemory = 1ULL << 2,
    QueryMemory = 1ULL << 3,
    AllocateMemory = 1ULL << 4,
    FreeMemory = 1ULL << 5,
    ProtectMemory = 1ULL << 6,
    Ping = 1ULL << 7,
    Heartbeat = 1ULL << 8,
    Configure = 1ULL << 9,
};

inline constexpr uint64_t RequiredCapabilities = BindProcess
    | ReadMemory
    | WriteMemory
    | QueryMemory
    | AllocateMemory
    | FreeMemory
    | Ping
    | Heartbeat
    | Configure;

inline constexpr uint64_t Capabilities = RequiredCapabilities | ProtectMemory;

enum class LoadMode : uint32_t {
    Unknown,
    Pnp,
    Service,
    Mapped,
};

enum class RuntimeState : uint32_t {
    Starting,
    Ready,
    Idle,
    Stopping,
    Removed,
};

struct PacketHeader {
    uint32_t Size;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Flags;
    uint32_t Reserved;
};

struct InformationRequest {
    PacketHeader Header;
};

struct InformationResponse {
    PacketHeader Header;
    uint64_t Capabilities;
    uint64_t MaximumTransferSize;
    uint32_t LoadMode;
    uint32_t Reserved;
};

struct PingRequest {
    PacketHeader Header;
    uint64_t Nonce;
};

struct PingResponse {
    PacketHeader Header;
    uint64_t Nonce;
    uint64_t KernelTime;
    uint64_t Capabilities;
    uint64_t MaximumTransferSize;
    uint16_t DriverMajorVersion;
    uint16_t DriverMinorVersion;
    uint32_t DriverPatchVersion;
    uint32_t ActiveSessions;
    uint32_t IdleTimeoutSeconds;
    uint32_t RemainingIdleSeconds;
    uint32_t LoadMode;
    uint32_t State;
    uint32_t Reserved;
};

struct HeartbeatRequest {
    PacketHeader Header;
    uint64_t Sequence;
};

struct HeartbeatResponse {
    PacketHeader Header;
    uint64_t Sequence;
    uint64_t KernelTime;
    uint32_t RemainingIdleSeconds;
    uint32_t State;
};

struct ConfigureRequest {
    PacketHeader Header;
    uint32_t IdleTimeoutSeconds;
    uint32_t Reserved;
};

struct ConfigureResponse {
    PacketHeader Header;
    uint32_t IdleTimeoutSeconds;
    uint32_t Reserved;
};

struct BindProcessRequest {
    PacketHeader Header;
    uint32_t ProcessId;
    uint32_t Reserved;
};

struct ReadMemoryRequest {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
};

struct WriteMemoryRequest {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
};

struct QueryMemoryRequest {
    PacketHeader Header;
    uint64_t Address;
};

struct QueryMemoryResponse {
    PacketHeader Header;
    uint64_t BaseAddress;
    uint64_t AllocationBase;
    uint64_t RegionSize;
    uint32_t AllocationProtect;
    uint32_t State;
    uint32_t Protect;
    uint32_t Type;
};

struct AllocateMemoryRequest {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
    uint32_t AllocationType;
    uint32_t Protect;
};

struct AllocateMemoryResponse {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
};

struct FreeMemoryRequest {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
    uint32_t FreeType;
    uint32_t Reserved;
};

struct ProtectMemoryRequest {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
    uint32_t Protect;
    uint32_t Reserved;
};

struct ProtectMemoryResponse {
    PacketHeader Header;
    uint64_t Address;
    uint64_t Size;
    uint32_t OldProtect;
    uint32_t Reserved;
};

inline constexpr uint32_t InformationControlCode = CTL_CODE(DeviceType, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS);
inline constexpr uint32_t BindProcessControlCode = CTL_CODE(DeviceType, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
inline constexpr uint32_t ReadMemoryControlCode = CTL_CODE(DeviceType, 0x802, METHOD_OUT_DIRECT, FILE_READ_ACCESS);
inline constexpr uint32_t WriteMemoryControlCode = CTL_CODE(DeviceType, 0x803, METHOD_IN_DIRECT, FILE_WRITE_ACCESS);
inline constexpr uint32_t QueryMemoryControlCode = CTL_CODE(DeviceType, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS);
inline constexpr uint32_t AllocateMemoryControlCode = CTL_CODE(DeviceType, 0x805, METHOD_BUFFERED, FILE_WRITE_ACCESS);
inline constexpr uint32_t FreeMemoryControlCode = CTL_CODE(DeviceType, 0x806, METHOD_BUFFERED, FILE_WRITE_ACCESS);
inline constexpr uint32_t ProtectMemoryControlCode = CTL_CODE(DeviceType, 0x807, METHOD_BUFFERED, FILE_WRITE_ACCESS);
inline constexpr uint32_t PingControlCode = CTL_CODE(DeviceType, 0x808, METHOD_BUFFERED, FILE_READ_ACCESS);
inline constexpr uint32_t HeartbeatControlCode = CTL_CODE(DeviceType, 0x809, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
inline constexpr uint32_t ConfigureControlCode = CTL_CODE(DeviceType, 0x80A, METHOD_BUFFERED, FILE_WRITE_ACCESS);

template <typename T>
constexpr PacketHeader CreateHeader()
{
    return {
        static_cast<uint32_t>(sizeof(T)),
        VersionMajor,
        VersionMinor,
        0,
        0,
    };
}

static_assert(sizeof(PacketHeader) == 16);
static_assert(sizeof(InformationRequest) == 16);
static_assert(sizeof(InformationResponse) == 40);
static_assert(sizeof(PingRequest) == 24);
static_assert(sizeof(PingResponse) == 80);
static_assert(sizeof(HeartbeatRequest) == 24);
static_assert(sizeof(HeartbeatResponse) == 40);
static_assert(sizeof(ConfigureRequest) == 24);
static_assert(sizeof(ConfigureResponse) == 24);
static_assert(sizeof(BindProcessRequest) == 24);
static_assert(sizeof(ReadMemoryRequest) == 32);
static_assert(sizeof(WriteMemoryRequest) == 32);
static_assert(sizeof(QueryMemoryRequest) == 24);
static_assert(sizeof(QueryMemoryResponse) == 56);
static_assert(sizeof(AllocateMemoryRequest) == 40);
static_assert(sizeof(AllocateMemoryResponse) == 32);
static_assert(sizeof(FreeMemoryRequest) == 40);
static_assert(sizeof(ProtectMemoryRequest) == 40);
static_assert(sizeof(ProtectMemoryResponse) == 40);

}
