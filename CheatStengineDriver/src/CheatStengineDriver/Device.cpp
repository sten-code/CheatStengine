#include "Device.h"

#include "Control.h"

namespace {

constexpr ULONG PoolTag = 'sSeC';
constexpr LONG64 TimeUnitsPerSecond = 10'000'000;

VOID IdleTimerCallback(PEX_TIMER, PVOID context)
{
    DeviceExtension* extension = static_cast<DeviceExtension*>(context);
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeAcquireSpinLock(&extension->IdleLock, &oldIrql);
    const LONG64 deadline = InterlockedCompareExchange64(&extension->IdleDeadline, 0, 0);
    if (deadline == 0
        || static_cast<LONG64>(KeQueryInterruptTime()) < deadline
        || GetDeviceState(extension) != DeviceState::Started) {
        KeReleaseSpinLock(&extension->IdleLock, oldIrql);
        return;
    }

    InterlockedCompareExchange(
        &extension->Runtime,
        static_cast<LONG>(Protocol::RuntimeState::Idle),
        static_cast<LONG>(Protocol::RuntimeState::Ready));
    InterlockedCompareExchange64(&extension->IdleDeadline, 0, deadline);
    KeReleaseSpinLock(&extension->IdleLock, oldIrql);
}

Session* TakeSession(DeviceExtension* extension, PFILE_OBJECT fileObject)
{
    if (!fileObject) {
        return nullptr;
    }

    ExAcquireFastMutex(&extension->SessionLock);
    Session* session = static_cast<Session*>(InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(&fileObject->FsContext),
        nullptr));
    ExReleaseFastMutex(&extension->SessionLock);
    return session;
}

void DestroySession(Session* session)
{
    if (!session) {
        return;
    }

    ExWaitForRundownProtectionRelease(&session->Rundown);
    PEPROCESS process = static_cast<PEPROCESS>(InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(&session->Process),
        nullptr));
    if (process) {
        ObDereferenceObject(process);
    }

    DeviceExtension* extension = session->Device;
    InterlockedDecrement(&extension->SessionCount);
    TouchDevice(extension);
    ExFreePoolWithTag(session, PoolTag);
}

}

DeviceExtension* GetExtension(PDEVICE_OBJECT deviceObject)
{
    return static_cast<DeviceExtension*>(deviceObject->DeviceExtension);
}

DeviceState GetDeviceState(DeviceExtension* extension)
{
    return static_cast<DeviceState>(InterlockedCompareExchange(&extension->State, 0, 0));
}

void SetDeviceState(DeviceExtension* extension, DeviceState state)
{
    InterlockedExchange(&extension->State, static_cast<LONG>(state));
}

NTSTATUS InitializeDevice(PDEVICE_OBJECT deviceObject, Protocol::LoadMode mode, DeviceState state)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    RtlZeroMemory(extension, sizeof(*extension));
    extension->DeviceObject = deviceObject;
    extension->Mode = mode;
    extension->State = static_cast<LONG>(state);
    extension->Runtime = static_cast<LONG>(
        state == DeviceState::Started
            ? Protocol::RuntimeState::Ready
            : Protocol::RuntimeState::Starting);
    extension->IdleTimeoutSeconds = Protocol::DefaultIdleTimeoutSeconds;

    IoInitializeRemoveLock(&extension->RemoveLock, PoolTag, 0, 0);
    ExInitializeRundownProtection(&extension->IoRundown);
    ExInitializeFastMutex(&extension->SessionLock);
    KeInitializeSpinLock(&extension->IdleLock);
    extension->IdleTimer = ExAllocateTimer(IdleTimerCallback, extension, EX_TIMER_NO_WAKE);
    if (!extension->IdleTimer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    extension->LastActivity = static_cast<LONG64>(KeQueryInterruptTime());
    if (state == DeviceState::Started) {
        TouchDevice(extension);
    }
    return STATUS_SUCCESS;
}

void DestroyDevice(DeviceExtension* extension)
{
    SetDeviceState(extension, DeviceState::Removed);
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeAcquireSpinLock(&extension->IdleLock, &oldIrql);
    InterlockedExchange(
        &extension->Runtime,
        static_cast<LONG>(Protocol::RuntimeState::Removed));
    InterlockedExchange64(&extension->IdleDeadline, 0);
    PEX_TIMER timer = extension->IdleTimer;
    extension->IdleTimer = nullptr;
    KeReleaseSpinLock(&extension->IdleLock, oldIrql);

    if (timer) {
        ExDeleteTimer(timer, TRUE, TRUE, nullptr);
    }

    if (InterlockedCompareExchange(&extension->IoDrained, 1, 0) == 0) {
        ExWaitForRundownProtectionRelease(&extension->IoRundown);
    }
}

void StartDevice(DeviceExtension* extension)
{
    if (InterlockedCompareExchange(&extension->IoDrained, 0, 1) == 1) {
        ExReInitializeRundownProtection(&extension->IoRundown);
    }
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeAcquireSpinLock(&extension->IdleLock, &oldIrql);
    InterlockedExchange(
        &extension->Runtime,
        static_cast<LONG>(Protocol::RuntimeState::Ready));
    KeReleaseSpinLock(&extension->IdleLock, oldIrql);
    TouchDevice(extension);
}

void StopDevice(DeviceExtension* extension)
{
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeAcquireSpinLock(&extension->IdleLock, &oldIrql);
    InterlockedExchange(
        &extension->Runtime,
        static_cast<LONG>(Protocol::RuntimeState::Stopping));
    InterlockedExchange64(&extension->IdleDeadline, 0);
    if (extension->IdleTimer) {
        ExCancelTimer(extension->IdleTimer, nullptr);
    }
    KeReleaseSpinLock(&extension->IdleLock, oldIrql);
    if (InterlockedCompareExchange(&extension->IoDrained, 1, 0) == 0) {
        ExWaitForRundownProtectionRelease(&extension->IoRundown);
    }
}

bool AcquireDeviceIo(DeviceExtension* extension)
{
    if (!ExAcquireRundownProtection(&extension->IoRundown)) {
        return false;
    }
    if (GetDeviceState(extension) != DeviceState::Started) {
        ExReleaseRundownProtection(&extension->IoRundown);
        return false;
    }
    return true;
}

void ReleaseDeviceIo(DeviceExtension* extension)
{
    ExReleaseRundownProtection(&extension->IoRundown);
}

void TouchDevice(DeviceExtension* extension)
{
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeAcquireSpinLock(&extension->IdleLock, &oldIrql);
    if (GetDeviceState(extension) != DeviceState::Started || !extension->IdleTimer) {
        KeReleaseSpinLock(&extension->IdleLock, oldIrql);
        return;
    }

    const ULONG seconds = static_cast<ULONG>(InterlockedCompareExchange(
        &extension->IdleTimeoutSeconds,
        0,
        0));
    const LONG64 now = static_cast<LONG64>(KeQueryInterruptTime());
    InterlockedExchange64(&extension->LastActivity, now);
    InterlockedExchange(
        &extension->Runtime,
        static_cast<LONG>(Protocol::RuntimeState::Ready));

    if (seconds == 0) {
        InterlockedExchange64(&extension->IdleDeadline, 0);
        ExCancelTimer(extension->IdleTimer, nullptr);
        KeReleaseSpinLock(&extension->IdleLock, oldIrql);
        return;
    }

    const LONG64 duration = static_cast<LONG64>(seconds) * TimeUnitsPerSecond;
    InterlockedExchange64(&extension->IdleDeadline, now + duration);
    ExSetTimer(extension->IdleTimer, -duration, 0, nullptr);
    KeReleaseSpinLock(&extension->IdleLock, oldIrql);
}

NTSTATUS SetIdleTimeout(DeviceExtension* extension, ULONG seconds)
{
    if (seconds != 0
        && (seconds < Protocol::MinimumIdleTimeoutSeconds
            || seconds > Protocol::MaximumIdleTimeoutSeconds)) {
        return STATUS_INVALID_PARAMETER;
    }

    InterlockedExchange(&extension->IdleTimeoutSeconds, static_cast<LONG>(seconds));
    TouchDevice(extension);
    return STATUS_SUCCESS;
}

ULONG GetIdleTimeout(DeviceExtension* extension)
{
    return static_cast<ULONG>(InterlockedCompareExchange(
        &extension->IdleTimeoutSeconds,
        0,
        0));
}

ULONG GetRemainingIdleTime(DeviceExtension* extension)
{
    const LONG64 deadline = InterlockedCompareExchange64(&extension->IdleDeadline, 0, 0);
    if (deadline == 0) {
        return 0;
    }

    const LONG64 now = static_cast<LONG64>(KeQueryInterruptTime());
    if (now >= deadline) {
        return 0;
    }

    const ULONGLONG remaining = static_cast<ULONGLONG>(deadline - now);
    return static_cast<ULONG>((remaining + TimeUnitsPerSecond - 1) / TimeUnitsPerSecond);
}

Protocol::RuntimeState GetRuntimeState(DeviceExtension* extension)
{
    const DeviceState state = GetDeviceState(extension);
    if (state == DeviceState::Removed) {
        return Protocol::RuntimeState::Removed;
    }
    if (state != DeviceState::Started) {
        return state == DeviceState::Added
            ? Protocol::RuntimeState::Starting
            : Protocol::RuntimeState::Stopping;
    }
    return static_cast<Protocol::RuntimeState>(
        InterlockedCompareExchange(&extension->Runtime, 0, 0));
}

ULONG GetSessionCount(DeviceExtension* extension)
{
    return static_cast<ULONG>(InterlockedCompareExchange(&extension->SessionCount, 0, 0));
}

NTSTATUS ReferenceSession(DeviceExtension* extension, PFILE_OBJECT fileObject, Session** session)
{
    if (!extension || !fileObject || !session) {
        return STATUS_INVALID_PARAMETER;
    }

    ExAcquireFastMutex(&extension->SessionLock);
    Session* context = static_cast<Session*>(fileObject->FsContext);
    const bool acquired = context && ExAcquireRundownProtection(&context->Rundown);
    ExReleaseFastMutex(&extension->SessionLock);
    if (!acquired) {
        return STATUS_FILE_CLOSED;
    }

    *session = context;
    return STATUS_SUCCESS;
}

void ReleaseSession(Session* session)
{
    ExReleaseRundownProtection(&session->Rundown);
}

NTSTATUS BindSession(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    ULONG processId)
{
    if (processId == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    Session* session = nullptr;
    NTSTATUS status = ReferenceSession(extension, fileObject, &session);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    PEPROCESS process = nullptr;
    status = PsLookupProcessByProcessId(
        reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(processId)),
        &process);
    if (NT_SUCCESS(status)) {
        PVOID previous = InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(&session->Process),
            process,
            nullptr);
        if (previous) {
            ObDereferenceObject(process);
            status = STATUS_ALREADY_REGISTERED;
        }
    }

    ReleaseSession(session);
    if (NT_SUCCESS(status)) {
        TouchDevice(extension);
    }
    return status;
}

NTSTATUS ReferenceSessionProcess(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    Session** session,
    PEPROCESS* process)
{
    if (!process) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = ReferenceSession(extension, fileObject, session);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    PEPROCESS boundProcess = static_cast<PEPROCESS>(InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(&(*session)->Process),
        nullptr,
        nullptr));
    if (!boundProcess) {
        ReleaseSession(*session);
        *session = nullptr;
        return STATUS_INVALID_DEVICE_STATE;
    }

    *process = boundProcess;
    return STATUS_SUCCESS;
}

NTSTATUS UpdateHeartbeat(Session* session, UINT64 sequence)
{
    if (sequence == 0 || sequence > MAXLONG64) {
        return STATUS_INVALID_PARAMETER;
    }

    LONG64 current = InterlockedCompareExchange64(&session->HeartbeatSequence, 0, 0);
    while (sequence > static_cast<UINT64>(current)) {
        const LONG64 previous = InterlockedCompareExchange64(
            &session->HeartbeatSequence,
            static_cast<LONG64>(sequence),
            current);
        if (previous == current) {
            return STATUS_SUCCESS;
        }
        current = previous;
    }
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS DispatchCreate(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    NTSTATUS status = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        return CompleteIrp(irp, status);
    }

    const bool ioAcquired = AcquireDeviceIo(extension);
    if (!ioAcquired) {
        status = STATUS_INVALID_DEVICE_STATE;
    } else {
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
        if (!stack->FileObject) {
            status = STATUS_INVALID_PARAMETER;
        } else {
            ExAcquireFastMutex(&extension->SessionLock);
            if (stack->FileObject->FsContext) {
                status = STATUS_INVALID_DEVICE_STATE;
            } else {
                Session* session = static_cast<Session*>(ExAllocatePool2(
                    POOL_FLAG_NON_PAGED,
                    sizeof(Session),
                    PoolTag));
                if (!session) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                } else {
                    RtlZeroMemory(session, sizeof(*session));
                    ExInitializeRundownProtection(&session->Rundown);
                    session->Device = extension;
                    stack->FileObject->FsContext = session;
                    InterlockedIncrement(&extension->SessionCount);
                    status = STATUS_SUCCESS;
                }
            }
            ExReleaseFastMutex(&extension->SessionLock);
        }
        ReleaseDeviceIo(extension);
    }

    if (NT_SUCCESS(status)) {
        TouchDevice(extension);
    }
    IoReleaseRemoveLock(&extension->RemoveLock, irp);
    return CompleteIrp(irp, status);
}

NTSTATUS DispatchCleanup(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    const NTSTATUS lockStatus = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    DestroySession(TakeSession(extension, stack->FileObject));
    if (NT_SUCCESS(lockStatus)) {
        IoReleaseRemoveLock(&extension->RemoveLock, irp);
    }
    return CompleteIrp(irp, STATUS_SUCCESS);
}

NTSTATUS DispatchClose(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    const NTSTATUS lockStatus = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    DestroySession(TakeSession(extension, stack->FileObject));
    if (NT_SUCCESS(lockStatus)) {
        IoReleaseRemoveLock(&extension->RemoveLock, irp);
    }
    return CompleteIrp(irp, STATUS_SUCCESS);
}

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    NTSTATUS status = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        return CompleteIrp(irp, status);
    }

    ULONG_PTR information = 0;
    if (!AcquireDeviceIo(extension)) {
        status = STATUS_INVALID_DEVICE_STATE;
    } else {
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
        if (!stack->FileObject) {
            status = STATUS_INVALID_PARAMETER;
        } else {
            status = DispatchControlRequest(
                extension,
                stack->FileObject,
                irp,
                stack,
                &information);
        }
        ReleaseDeviceIo(extension);
    }

    IoReleaseRemoveLock(&extension->RemoveLock, irp);
    return CompleteIrp(irp, status, information);
}
