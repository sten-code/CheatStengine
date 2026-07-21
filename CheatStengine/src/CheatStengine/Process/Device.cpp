#include "Device.h"

#include <cfgmgr32.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr DWORD ControlTimeoutMilliseconds = 5000;
constexpr DWORD CancellationTimeoutMilliseconds = 1000;
constexpr wchar_t ControlDevicePath[] = L"\\\\.\\MemoryAccess";

enum class InterfaceResult {
    Found,
    Missing,
    Failed,
};

struct InterfacePaths {
    InterfaceResult Result = InterfaceResult::Failed;
    std::vector<std::wstring> Paths;
    DWORD Error = ERROR_GEN_FAILURE;
};

struct PendingControl {
    OVERLAPPED Overlapped {};
    HANDLE Event = nullptr;
    std::vector<uint8_t> Input;
    std::vector<uint8_t> Buffer;

    ~PendingControl()
    {
        if (Event) {
            CloseHandle(Event);
        }
    }
};

InterfacePaths FindInterfacePaths()
{
    GUID interfaceGuid = Protocol::DeviceInterfaceGuid;
    ULONG pathLength = 0;
    CONFIGRET result = CM_Get_Device_Interface_List_SizeW(
        &pathLength,
        &interfaceGuid,
        nullptr,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (result != CR_SUCCESS) {
        return {
            InterfaceResult::Failed,
            {},
            CM_MapCrToWin32Err(result, ERROR_GEN_FAILURE),
        };
    }
    if (pathLength <= 1) {
        return { InterfaceResult::Missing, {}, ERROR_FILE_NOT_FOUND };
    }

    std::vector<wchar_t> buffer(pathLength, L'\0');
    result = CM_Get_Device_Interface_ListW(
        &interfaceGuid,
        nullptr,
        buffer.data(),
        pathLength,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (result != CR_SUCCESS) {
        return {
            InterfaceResult::Failed,
            {},
            CM_MapCrToWin32Err(result, ERROR_GEN_FAILURE),
        };
    }

    InterfacePaths paths;
    paths.Result = InterfaceResult::Found;
    paths.Error = ERROR_SUCCESS;
    const wchar_t* current = buffer.data();
    const wchar_t* end = buffer.data() + buffer.size();
    while (current < end && *current) {
        const size_t remaining = static_cast<size_t>(end - current);
        const size_t length = wcsnlen_s(current, remaining);
        if (length == remaining) {
            return { InterfaceResult::Failed, {}, ERROR_INVALID_DATA };
        }
        paths.Paths.emplace_back(current, length);
        current += length + 1;
    }

    if (paths.Paths.empty()) {
        paths.Result = InterfaceResult::Missing;
        paths.Error = ERROR_FILE_NOT_FOUND;
    }
    return paths;
}

bool IsResponseValid(
    const Protocol::PacketHeader& header,
    size_t expectedSize,
    DWORD transferred)
{
    return header.Size >= expectedSize
        && header.Size <= transferred
        && header.MajorVersion == Protocol::VersionMajor
        && header.MinorVersion >= Protocol::VersionMinor
        && header.Flags == 0
        && header.Reserved == 0;
}

bool IsAddressRangeValid(uintptr_t address, size_t size)
{
    return address != 0
        && size != 0
        && size - 1 <= std::numeric_limits<uintptr_t>::max() - address;
}

void WaitForAbandonedControl(std::shared_ptr<PendingControl> control)
{
    WaitForSingleObject(control->Event, INFINITE);
}

}

Device::Device()
{
    static_cast<void>(Open());
}

Device::~Device()
{
    Close();
}

bool Device::IsOpen() const
{
    std::scoped_lock lock(m_ControlMutex);
    return m_Handle != INVALID_HANDLE_VALUE;
}

bool Device::Bind(uint32_t processId) const
{
    if (processId == 0) {
        SetError(ERROR_INVALID_PARAMETER);
        return false;
    }
    if (!HasCapability(Protocol::BindProcess)) {
        SetError(ERROR_NOT_SUPPORTED);
        return false;
    }

    Protocol::BindProcessRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::BindProcessRequest>();
    request.ProcessId = processId;

    DWORD transferred = 0;
    if (!Control(
            Protocol::BindProcessControlCode,
            &request,
            sizeof(request),
            nullptr,
            nullptr,
            0,
            transferred)) {
        return false;
    }
    if (transferred != 0) {
        SetError(ERROR_INVALID_DATA);
        return false;
    }
    return true;
}

std::optional<Protocol::PingResponse> Device::Ping(uint64_t nonce) const
{
    if (!HasCapability(Protocol::Ping)) {
        SetError(ERROR_NOT_SUPPORTED);
        return std::nullopt;
    }

    Protocol::PingRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::PingRequest>();
    request.Nonce = nonce;
    Protocol::PingResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::PingControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return std::nullopt;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.Nonce != nonce
        || (response.Capabilities & Protocol::RequiredCapabilities) != Protocol::RequiredCapabilities
        || response.MaximumTransferSize == 0
        || response.IdleTimeoutSeconds < Protocol::MinimumIdleTimeoutSeconds
        || response.IdleTimeoutSeconds > Protocol::MaximumIdleTimeoutSeconds
        || response.RemainingIdleSeconds > response.IdleTimeoutSeconds
        || response.LoadMode == static_cast<uint32_t>(Protocol::LoadMode::Unknown)
        || response.LoadMode > static_cast<uint32_t>(Protocol::LoadMode::Mapped)
        || response.State > static_cast<uint32_t>(Protocol::RuntimeState::Removed)
        || response.Reserved != 0) {
        SetError(ERROR_INVALID_DATA);
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return response;
}

std::optional<Protocol::HeartbeatResponse> Device::Heartbeat(uint64_t sequence) const
{
    if (!HasCapability(Protocol::Heartbeat)) {
        SetError(ERROR_NOT_SUPPORTED);
        return std::nullopt;
    }

    Protocol::HeartbeatRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::HeartbeatRequest>();
    request.Sequence = sequence;
    Protocol::HeartbeatResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::HeartbeatControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return std::nullopt;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.Sequence != sequence
        || response.State > static_cast<uint32_t>(Protocol::RuntimeState::Removed)) {
        SetError(ERROR_INVALID_DATA);
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return response;
}

std::optional<Protocol::ConfigureResponse> Device::Configure(uint32_t idleTimeoutSeconds) const
{
    if (idleTimeoutSeconds < Protocol::MinimumIdleTimeoutSeconds
        || idleTimeoutSeconds > Protocol::MaximumIdleTimeoutSeconds) {
        SetError(ERROR_INVALID_PARAMETER);
        return std::nullopt;
    }
    if (!HasCapability(Protocol::Configure)) {
        SetError(ERROR_NOT_SUPPORTED);
        return std::nullopt;
    }

    Protocol::ConfigureRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::ConfigureRequest>();
    request.IdleTimeoutSeconds = idleTimeoutSeconds;
    Protocol::ConfigureResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::ConfigureControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return std::nullopt;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.IdleTimeoutSeconds < Protocol::MinimumIdleTimeoutSeconds
        || response.IdleTimeoutSeconds > Protocol::MaximumIdleTimeoutSeconds
        || response.Reserved != 0) {
        SetError(ERROR_INVALID_DATA);
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return response;
}

bool Device::Read(uintptr_t address, void* buffer, size_t size) const
{
    if (size == 0) {
        SetError(ERROR_SUCCESS);
        return true;
    }
    if (!buffer || !IsAddressRangeValid(address, size)) {
        SetError(ERROR_INVALID_PARAMETER);
        return false;
    }
    if (!HasCapability(Protocol::ReadMemory)) {
        SetError(ERROR_NOT_SUPPORTED);
        return false;
    }

    const uint64_t maximumTransferSize = GetMaximumTransferSize();
    if (maximumTransferSize == 0) {
        SetError(ERROR_INVALID_HANDLE);
        return false;
    }

    size_t offset = 0;
    while (offset < size) {
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(
            maximumTransferSize,
            static_cast<uint64_t>(size - offset)));
        Protocol::ReadMemoryRequest request {};
        request.Header = Protocol::CreateHeader<Protocol::ReadMemoryRequest>();
        request.Address = static_cast<uint64_t>(address + offset);
        request.Size = static_cast<uint64_t>(chunk);

        DWORD transferred = 0;
        if (!Control(
                Protocol::ReadMemoryControlCode,
                &request,
                sizeof(request),
                nullptr,
                static_cast<uint8_t*>(buffer) + offset,
                static_cast<DWORD>(chunk),
                transferred)) {
            return false;
        }
        if (transferred != chunk) {
            SetError(ERROR_PARTIAL_COPY);
            return false;
        }
        offset += chunk;
    }

    SetError(ERROR_SUCCESS);
    return true;
}

bool Device::Write(uintptr_t address, const void* buffer, size_t size) const
{
    if (size == 0) {
        SetError(ERROR_SUCCESS);
        return true;
    }
    if (!buffer || !IsAddressRangeValid(address, size)) {
        SetError(ERROR_INVALID_PARAMETER);
        return false;
    }
    if (!HasCapability(Protocol::WriteMemory)) {
        SetError(ERROR_NOT_SUPPORTED);
        return false;
    }

    const uint64_t maximumTransferSize = GetMaximumTransferSize();
    if (maximumTransferSize == 0) {
        SetError(ERROR_INVALID_HANDLE);
        return false;
    }

    size_t offset = 0;
    while (offset < size) {
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(
            maximumTransferSize,
            static_cast<uint64_t>(size - offset)));
        Protocol::WriteMemoryRequest request {};
        request.Header = Protocol::CreateHeader<Protocol::WriteMemoryRequest>();
        request.Address = static_cast<uint64_t>(address + offset);
        request.Size = static_cast<uint64_t>(chunk);

        DWORD transferred = 0;
        if (!Control(
                Protocol::WriteMemoryControlCode,
                &request,
                sizeof(request),
                static_cast<const uint8_t*>(buffer) + offset,
                nullptr,
                static_cast<DWORD>(chunk),
                transferred)) {
            return false;
        }
        if (transferred != chunk) {
            SetError(ERROR_PARTIAL_COPY);
            return false;
        }
        offset += chunk;
    }

    SetError(ERROR_SUCCESS);
    return true;
}

std::optional<MEMORY_BASIC_INFORMATION> Device::Query(uintptr_t address) const
{
    if (!HasCapability(Protocol::QueryMemory)) {
        SetError(ERROR_NOT_SUPPORTED);
        return std::nullopt;
    }

    Protocol::QueryMemoryRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::QueryMemoryRequest>();
    request.Address = static_cast<uint64_t>(address);
    Protocol::QueryMemoryResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::QueryMemoryControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return std::nullopt;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.RegionSize == 0
        || response.RegionSize > std::numeric_limits<SIZE_T>::max()) {
        SetError(ERROR_INVALID_DATA);
        return std::nullopt;
    }

    MEMORY_BASIC_INFORMATION information {};
    information.BaseAddress = reinterpret_cast<PVOID>(static_cast<uintptr_t>(response.BaseAddress));
    information.AllocationBase = reinterpret_cast<PVOID>(static_cast<uintptr_t>(response.AllocationBase));
    information.AllocationProtect = response.AllocationProtect;
    information.RegionSize = static_cast<SIZE_T>(response.RegionSize);
    information.State = response.State;
    information.Protect = response.Protect;
    information.Type = response.Type;
    SetError(ERROR_SUCCESS);
    return information;
}

uintptr_t Device::Allocate(
    size_t size,
    uint32_t protection,
    uint32_t allocationType,
    uintptr_t preferredAddress) const
{
    if (size == 0
        || (preferredAddress != 0
            && size - 1 > std::numeric_limits<uintptr_t>::max() - preferredAddress)) {
        SetError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!HasCapability(Protocol::AllocateMemory)) {
        SetError(ERROR_NOT_SUPPORTED);
        return 0;
    }

    Protocol::AllocateMemoryRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::AllocateMemoryRequest>();
    request.Address = static_cast<uint64_t>(preferredAddress);
    request.Size = static_cast<uint64_t>(size);
    request.AllocationType = allocationType;
    request.Protect = protection;
    Protocol::AllocateMemoryResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::AllocateMemoryControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return 0;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.Address == 0
        || response.Address > std::numeric_limits<uintptr_t>::max()
        || response.Size < size) {
        SetError(ERROR_INVALID_DATA);
        return 0;
    }

    SetError(ERROR_SUCCESS);
    return static_cast<uintptr_t>(response.Address);
}

bool Device::Free(uintptr_t address, uint32_t freeType, size_t size) const
{
    if (address == 0) {
        SetError(ERROR_INVALID_PARAMETER);
        return false;
    }
    if (!HasCapability(Protocol::FreeMemory)) {
        SetError(ERROR_NOT_SUPPORTED);
        return false;
    }

    Protocol::FreeMemoryRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::FreeMemoryRequest>();
    request.Address = static_cast<uint64_t>(address);
    request.Size = static_cast<uint64_t>(size);
    request.FreeType = freeType;

    DWORD transferred = 0;
    if (!Control(
            Protocol::FreeMemoryControlCode,
            &request,
            sizeof(request),
            nullptr,
            nullptr,
            0,
            transferred)) {
        return false;
    }
    if (transferred != 0) {
        SetError(ERROR_INVALID_DATA);
        return false;
    }
    return true;
}

std::optional<uint32_t> Device::Protect(
    uintptr_t address,
    size_t size,
    uint32_t protection) const
{
    if (!IsAddressRangeValid(address, size)) {
        SetError(ERROR_INVALID_PARAMETER);
        return std::nullopt;
    }
    if (!HasCapability(Protocol::ProtectMemory)) {
        SetError(ERROR_NOT_SUPPORTED);
        return std::nullopt;
    }

    Protocol::ProtectMemoryRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::ProtectMemoryRequest>();
    request.Address = static_cast<uint64_t>(address);
    request.Size = static_cast<uint64_t>(size);
    request.Protect = protection;
    Protocol::ProtectMemoryResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::ProtectMemoryControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return std::nullopt;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.Address == 0
        || response.Size == 0
        || response.Reserved != 0) {
        SetError(ERROR_INVALID_DATA);
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return response.OldProtect;
}

Protocol::LoadMode Device::GetMode() const
{
    return m_Mode.load(std::memory_order_relaxed);
}

uint64_t Device::GetCapabilities() const
{
    return m_Capabilities.load(std::memory_order_relaxed);
}

uint64_t Device::GetMaximumTransferSize() const
{
    return m_MaximumTransferSize.load(std::memory_order_relaxed);
}

DWORD Device::GetError() const
{
    return m_Error.load(std::memory_order_relaxed);
}

bool Device::Open()
{
    const InterfacePaths interfaces = FindInterfacePaths();
    DWORD pnpError = interfaces.Error;

    if (interfaces.Result == InterfaceResult::Found) {
        for (const std::wstring& path : interfaces.Paths) {
            if (OpenPath(path, Protocol::LoadMode::Pnp)) {
                return true;
            }
            pnpError = GetError();
        }
    }

    if (OpenPath(ControlDevicePath, Protocol::LoadMode::Mapped)) {
        return true;
    }

    const DWORD mappedError = GetError();
    const bool mappedMissing = mappedError == ERROR_FILE_NOT_FOUND
        || mappedError == ERROR_PATH_NOT_FOUND;
    const bool pnpMissing = pnpError == ERROR_FILE_NOT_FOUND
        || pnpError == ERROR_PATH_NOT_FOUND;
    if (mappedMissing && !pnpMissing && pnpError != ERROR_SUCCESS) {
        SetError(pnpError);
    }
    return false;
}

bool Device::OpenPath(const std::wstring& path, Protocol::LoadMode mode)
{
    HANDLE handle = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        SetError(GetLastError());
        return false;
    }

    {
        std::scoped_lock lock(m_ControlMutex);
        m_Handle = handle;
        m_Mode.store(mode, std::memory_order_relaxed);
    }

    if (ReadInformation()) {
        return true;
    }

    const DWORD error = GetError();
    Close();
    SetError(error);
    return false;
}

bool Device::ReadInformation()
{
    Protocol::InformationRequest request {};
    request.Header = Protocol::CreateHeader<Protocol::InformationRequest>();
    Protocol::InformationResponse response {};
    DWORD transferred = 0;
    if (!Control(
            Protocol::InformationControlCode,
            &request,
            sizeof(request),
            nullptr,
            &response,
            sizeof(response),
            transferred)) {
        return false;
    }
    if (transferred != sizeof(response)
        || !IsResponseValid(response.Header, sizeof(response), transferred)
        || response.LoadMode == static_cast<uint32_t>(Protocol::LoadMode::Unknown)
        || response.LoadMode > static_cast<uint32_t>(Protocol::LoadMode::Mapped)
        || response.Reserved != 0) {
        SetError(ERROR_REVISION_MISMATCH);
        return false;
    }
    if ((response.Capabilities & Protocol::RequiredCapabilities)
            != Protocol::RequiredCapabilities
        || response.MaximumTransferSize == 0) {
        SetError(ERROR_NOT_SUPPORTED);
        return false;
    }

    const uint64_t maximumTransferSize = std::min({
        response.MaximumTransferSize,
        Protocol::MaximumTransferSize,
        static_cast<uint64_t>(std::numeric_limits<DWORD>::max()),
    });
    if (maximumTransferSize == 0) {
        SetError(ERROR_INVALID_DATA);
        return false;
    }

    m_Capabilities.store(response.Capabilities, std::memory_order_relaxed);
    m_MaximumTransferSize.store(maximumTransferSize, std::memory_order_relaxed);
    m_Mode.store(static_cast<Protocol::LoadMode>(response.LoadMode), std::memory_order_relaxed);
    SetError(ERROR_SUCCESS);
    return true;
}

bool Device::HasCapability(uint64_t capability) const
{
    return (GetCapabilities() & capability) == capability;
}

bool Device::Control(
    DWORD code,
    const void* input,
    DWORD inputSize,
    const void* directInput,
    void* output,
    DWORD bufferSize,
    DWORD& transferred) const
{
    transferred = 0;
    if ((inputSize != 0 && !input)
        || (bufferSize != 0 && !directInput && !output)
        || (directInput && output)) {
        SetError(ERROR_INVALID_PARAMETER);
        return false;
    }

    std::shared_ptr<PendingControl> control;
    try {
        control = std::make_shared<PendingControl>();
        control->Input.resize(inputSize);
        control->Buffer.resize(bufferSize);
    } catch (const std::bad_alloc&) {
        SetError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }

    if (inputSize != 0) {
        std::memcpy(control->Input.data(), input, inputSize);
    }
    if (bufferSize != 0 && directInput) {
        std::memcpy(control->Buffer.data(), directInput, bufferSize);
    }

    control->Event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!control->Event) {
        SetError(GetLastError());
        return false;
    }
    control->Overlapped.hEvent = control->Event;

    std::scoped_lock lock(m_ControlMutex);
    if (m_Handle == INVALID_HANDLE_VALUE) {
        SetError(ERROR_INVALID_HANDLE);
        return false;
    }

    DWORD immediateTransferred = 0;
    const BOOL started = DeviceIoControl(
        m_Handle,
        code,
        inputSize == 0 ? nullptr : control->Input.data(),
        inputSize,
        bufferSize == 0 ? nullptr : control->Buffer.data(),
        bufferSize,
        &immediateTransferred,
        &control->Overlapped);
    if (!started) {
        const DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            SetError(error);
            return false;
        }
    }

    const DWORD waitResult = WaitForSingleObject(
        control->Event,
        ControlTimeoutMilliseconds);
    if (waitResult != WAIT_OBJECT_0) {
        const DWORD waitError = waitResult == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
        CancelIoEx(m_Handle, &control->Overlapped);
        const DWORD cancellationResult = WaitForSingleObject(
            control->Event,
            CancellationTimeoutMilliseconds);
        if (cancellationResult != WAIT_OBJECT_0) {
            CloseLocked();
            try {
                std::thread(WaitForAbandonedControl, control).detach();
            } catch (...) {
                WaitForSingleObject(control->Event, INFINITE);
            }
        }
        SetError(waitError);
        return false;
    }

    if (!GetOverlappedResult(
            m_Handle,
            &control->Overlapped,
            &transferred,
            FALSE)) {
        SetError(GetLastError());
        return false;
    }
    if (transferred > bufferSize) {
        SetError(ERROR_INVALID_DATA);
        return false;
    }
    if (output && transferred != 0) {
        std::memcpy(output, control->Buffer.data(), transferred);
    }

    SetError(ERROR_SUCCESS);
    return true;
}

void Device::Close()
{
    std::scoped_lock lock(m_ControlMutex);
    CloseLocked();
}

void Device::CloseLocked() const
{
    if (m_Handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_Handle);
        m_Handle = INVALID_HANDLE_VALUE;
    }
    m_Mode.store(Protocol::LoadMode::Unknown, std::memory_order_relaxed);
    m_Capabilities.store(0, std::memory_order_relaxed);
    m_MaximumTransferSize.store(0, std::memory_order_relaxed);
}

void Device::SetError(DWORD error) const
{
    m_Error.store(error, std::memory_order_relaxed);
}
