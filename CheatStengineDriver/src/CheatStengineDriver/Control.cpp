#include "Control.h"

#include "Memory.h"
#include "Process.h"

namespace {

using Handler = NTSTATUS (*)(
    DeviceExtension*,
    PFILE_OBJECT,
    PIRP,
    PIO_STACK_LOCATION,
    PULONG_PTR);

struct Route {
    ULONG Code;
    Handler Function;
    bool WakesDevice;
};

uint64_t GetCapabilities()
{
    uint64_t capabilities = Protocol::BindProcess
        | Protocol::QueryMemory
        | Protocol::AllocateMemory
        | Protocol::FreeMemory
        | Protocol::Ping
        | Protocol::Heartbeat
        | Protocol::Configure;
    if (CanCopyProcessMemory()) {
        capabilities |= Protocol::ReadMemory | Protocol::WriteMemory;
    }
    if (CanProtectProcessMemory()) {
        capabilities |= Protocol::ProtectMemory;
    }
    return capabilities;
}

template <typename T>
NTSTATUS GetRequest(PIRP irp, ULONG inputLength, const T** request, bool negotiate = false)
{
    if (!request || !irp->AssociatedIrp.SystemBuffer || inputLength < sizeof(T)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const T* packet = static_cast<const T*>(irp->AssociatedIrp.SystemBuffer);
    const Protocol::PacketHeader& header = packet->Header;
    if (header.Size < sizeof(T) || header.Size > inputLength
        || header.Flags != 0 || header.Reserved != 0) {
        return STATUS_INVALID_PARAMETER;
    }
    if (header.MajorVersion != Protocol::VersionMajor
        || (!negotiate && header.MinorVersion > Protocol::VersionMinor)) {
        return STATUS_REVISION_MISMATCH;
    }

    *request = packet;
    return STATUS_SUCCESS;
}

template <typename T>
T* GetResponse(PIRP irp, ULONG outputLength, PULONG_PTR information)
{
    if (!irp->AssociatedIrp.SystemBuffer || outputLength < sizeof(T)) {
        *information = sizeof(T);
        return nullptr;
    }
    return static_cast<T*>(irp->AssociatedIrp.SystemBuffer);
}

PVOID GetDirectBuffer(PIRP irp, ULONG length)
{
    if (!irp->MdlAddress || length == 0 || MmGetMdlByteCount(irp->MdlAddress) != length) {
        return nullptr;
    }
    return MmGetSystemAddressForMdlSafe(
        irp->MdlAddress,
        static_cast<MM_PAGE_PRIORITY>(NormalPagePriority | MdlMappingNoExecute));
}

NTSTATUS HandleInformation(
    DeviceExtension* extension,
    PFILE_OBJECT,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const Protocol::InformationRequest* request = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &request,
        true);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Protocol::InformationResponse* response = GetResponse<Protocol::InformationResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::InformationResponse>();
    response->Capabilities = GetCapabilities();
    response->MaximumTransferSize = Protocol::MaximumTransferSize;
    response->LoadMode = static_cast<uint32_t>(extension->Mode);
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

NTSTATUS HandlePing(
    DeviceExtension* extension,
    PFILE_OBJECT,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const Protocol::PingRequest* requestPointer = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &requestPointer);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    const Protocol::PingRequest request = *requestPointer;

    Protocol::PingResponse* response = GetResponse<Protocol::PingResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::PingResponse>();
    response->Nonce = request.Nonce;
    response->KernelTime = KeQueryInterruptTime();
    response->Capabilities = GetCapabilities();
    response->MaximumTransferSize = Protocol::MaximumTransferSize;
    response->DriverMajorVersion = Protocol::DriverVersionMajor;
    response->DriverMinorVersion = Protocol::DriverVersionMinor;
    response->DriverPatchVersion = Protocol::DriverVersionPatch;
    response->ActiveSessions = GetSessionCount(extension);
    response->IdleTimeoutSeconds = GetIdleTimeout(extension);
    response->RemainingIdleSeconds = GetRemainingIdleTime(extension);
    response->LoadMode = static_cast<uint32_t>(extension->Mode);
    response->State = static_cast<uint32_t>(GetRuntimeState(extension));
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

NTSTATUS HandleHeartbeat(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const Protocol::HeartbeatRequest* requestPointer = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &requestPointer);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    const Protocol::HeartbeatRequest request = *requestPointer;

    Protocol::HeartbeatResponse* response = GetResponse<Protocol::HeartbeatResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Session* session = nullptr;
    status = ReferenceSession(extension, fileObject, &session);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = UpdateHeartbeat(session, request.Sequence);
    ReleaseSession(session);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    TouchDevice(extension);
    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::HeartbeatResponse>();
    response->Sequence = request.Sequence;
    response->KernelTime = KeQueryInterruptTime();
    response->RemainingIdleSeconds = GetRemainingIdleTime(extension);
    response->State = static_cast<uint32_t>(GetRuntimeState(extension));
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

NTSTATUS HandleConfigure(
    DeviceExtension* extension,
    PFILE_OBJECT,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const Protocol::ConfigureRequest* requestPointer = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &requestPointer);
    if (!NT_SUCCESS(status) || requestPointer->Reserved != 0) {
        return NT_SUCCESS(status) ? STATUS_INVALID_PARAMETER : status;
    }
    const Protocol::ConfigureRequest request = *requestPointer;

    Protocol::ConfigureResponse* response = GetResponse<Protocol::ConfigureResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = SetIdleTimeout(extension, request.IdleTimeoutSeconds);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::ConfigureResponse>();
    response->IdleTimeoutSeconds = GetIdleTimeout(extension);
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

NTSTATUS HandleBind(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR)
{
    const Protocol::BindProcessRequest* request = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &request);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (request->ProcessId == 0 || request->Reserved != 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return BindSession(extension, fileObject, request->ProcessId);
}

NTSTATUS HandleRead(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    const ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const Protocol::ReadMemoryRequest* request = nullptr;
    NTSTATUS status = GetRequest(irp, inputLength, &request);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (request->Size == 0 || request->Size > Protocol::MaximumTransferSize
        || request->Size != outputLength) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    PVOID buffer = GetDirectBuffer(irp, outputLength);
    if (!buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Session* session = nullptr;
    PEPROCESS process = nullptr;
    status = ReferenceSessionProcess(extension, fileObject, &session, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    SIZE_T transferred = 0;
    status = ReadProcessMemory(
        process,
        request->Address,
        buffer,
        static_cast<SIZE_T>(request->Size),
        &transferred);
    ReleaseSession(session);
    *information = transferred;
    return status;
}

NTSTATUS HandleWrite(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    const ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const Protocol::WriteMemoryRequest* request = nullptr;
    NTSTATUS status = GetRequest(irp, inputLength, &request);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (request->Size == 0 || request->Size > Protocol::MaximumTransferSize
        || request->Size != outputLength) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    PVOID buffer = GetDirectBuffer(irp, outputLength);
    if (!buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Session* session = nullptr;
    PEPROCESS process = nullptr;
    status = ReferenceSessionProcess(extension, fileObject, &session, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    SIZE_T transferred = 0;
    status = WriteProcessMemory(
        process,
        request->Address,
        buffer,
        static_cast<SIZE_T>(request->Size),
        &transferred);
    ReleaseSession(session);
    *information = transferred;
    return status;
}

NTSTATUS HandleQuery(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const Protocol::QueryMemoryRequest* requestPointer = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &requestPointer);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    const Protocol::QueryMemoryRequest request = *requestPointer;

    Protocol::QueryMemoryResponse* response = GetResponse<Protocol::QueryMemoryResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Session* session = nullptr;
    PEPROCESS process = nullptr;
    status = ReferenceSessionProcess(extension, fileObject, &session, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    MEMORY_BASIC_INFORMATION memoryInformation {};
    status = QueryProcessMemory(process, request.Address, &memoryInformation);
    ReleaseSession(session);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::QueryMemoryResponse>();
    response->BaseAddress = reinterpret_cast<UINT64>(memoryInformation.BaseAddress);
    response->AllocationBase = reinterpret_cast<UINT64>(memoryInformation.AllocationBase);
    response->RegionSize = memoryInformation.RegionSize;
    response->AllocationProtect = memoryInformation.AllocationProtect;
    response->State = memoryInformation.State;
    response->Protect = memoryInformation.Protect;
    response->Type = memoryInformation.Type;
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

NTSTATUS HandleAllocate(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    const Protocol::AllocateMemoryRequest* requestPointer = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &requestPointer);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    const Protocol::AllocateMemoryRequest request = *requestPointer;

    Protocol::AllocateMemoryResponse* response = GetResponse<Protocol::AllocateMemoryResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Session* session = nullptr;
    PEPROCESS process = nullptr;
    status = ReferenceSessionProcess(extension, fileObject, &session, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    UINT64 address = 0;
    UINT64 size = 0;
    status = AllocateProcessMemory(
        process,
        request.Address,
        request.Size,
        request.AllocationType,
        request.Protect,
        &address,
        &size);
    ReleaseSession(session);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::AllocateMemoryResponse>();
    response->Address = address;
    response->Size = size;
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

NTSTATUS HandleFree(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR)
{
    const Protocol::FreeMemoryRequest* request = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &request);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (request->Reserved != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    Session* session = nullptr;
    PEPROCESS process = nullptr;
    status = ReferenceSessionProcess(extension, fileObject, &session, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = FreeProcessMemory(
        process,
        request->Address,
        request->Size,
        request->FreeType);
    ReleaseSession(session);
    return status;
}

NTSTATUS HandleProtect(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    PULONG_PTR information)
{
    if (!CanProtectProcessMemory()) {
        return STATUS_NOT_SUPPORTED;
    }

    const Protocol::ProtectMemoryRequest* requestPointer = nullptr;
    NTSTATUS status = GetRequest(
        irp,
        stack->Parameters.DeviceIoControl.InputBufferLength,
        &requestPointer);
    if (!NT_SUCCESS(status) || requestPointer->Reserved != 0) {
        return NT_SUCCESS(status) ? STATUS_INVALID_PARAMETER : status;
    }
    const Protocol::ProtectMemoryRequest request = *requestPointer;

    Protocol::ProtectMemoryResponse* response = GetResponse<Protocol::ProtectMemoryResponse>(
        irp,
        stack->Parameters.DeviceIoControl.OutputBufferLength,
        information);
    if (!response) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Session* session = nullptr;
    PEPROCESS process = nullptr;
    status = ReferenceSessionProcess(extension, fileObject, &session, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ULONG oldProtect = 0;
    UINT64 address = 0;
    UINT64 size = 0;
    status = ProtectProcessMemory(
        process,
        request.Address,
        request.Size,
        request.Protect,
        &oldProtect,
        &address,
        &size);
    ReleaseSession(session);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *response = {};
    response->Header = Protocol::CreateHeader<Protocol::ProtectMemoryResponse>();
    response->Address = address;
    response->Size = size;
    response->OldProtect = oldProtect;
    *information = sizeof(*response);
    return STATUS_SUCCESS;
}

const Route Routes[] = {
    { Protocol::InformationControlCode, HandleInformation, true },
    { Protocol::PingControlCode, HandlePing, true },
    { Protocol::HeartbeatControlCode, HandleHeartbeat, true },
    { Protocol::ConfigureControlCode, HandleConfigure, true },
    { Protocol::BindProcessControlCode, HandleBind, false },
    { Protocol::ReadMemoryControlCode, HandleRead, false },
    { Protocol::WriteMemoryControlCode, HandleWrite, false },
    { Protocol::QueryMemoryControlCode, HandleQuery, false },
    { Protocol::AllocateMemoryControlCode, HandleAllocate, false },
    { Protocol::FreeMemoryControlCode, HandleFree, false },
    { Protocol::ProtectMemoryControlCode, HandleProtect, false },
};

}

NTSTATUS DispatchControlRequest(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    ULONG_PTR* information)
{
    const ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    for (const Route& route : Routes) {
        if (route.Code != code) {
            continue;
        }
        if (GetRuntimeState(extension) == Protocol::RuntimeState::Idle
            && !route.WakesDevice) {
            return STATUS_DEVICE_NOT_READY;
        }
        if (route.WakesDevice) {
            TouchDevice(extension);
        }
        const NTSTATUS status = route.Function(
            extension,
            fileObject,
            irp,
            stack,
            information);
        if (NT_SUCCESS(status)) {
            TouchDevice(extension);
        }
        return status;
    }
    return STATUS_INVALID_DEVICE_REQUEST;
}
