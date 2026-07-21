#pragma once

#include "Driver.h"

#include <Protocol.h>

enum class DeviceState : LONG {
    Added,
    Started,
    StopPending,
    Stopped,
    RemovePending,
    SurpriseRemoved,
    Removed,
};

struct DeviceExtension {
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_OBJECT LowerDeviceObject;
    PDEVICE_OBJECT PhysicalDeviceObject;
    IO_REMOVE_LOCK RemoveLock;
    EX_RUNDOWN_REF IoRundown;
    FAST_MUTEX SessionLock;
    KSPIN_LOCK IdleLock;
    UNICODE_STRING InterfaceName;
    PEX_TIMER IdleTimer;
    volatile LONG State;
    volatile LONG IoDrained;
    volatile LONG Runtime;
    volatile LONG SessionCount;
    volatile LONG IdleTimeoutSeconds;
    volatile LONG64 LastActivity;
    volatile LONG64 IdleDeadline;
    Protocol::LoadMode Mode;
    BOOLEAN InterfaceEnabled;
};

struct Session {
    EX_RUNDOWN_REF Rundown;
    DeviceExtension* Device;
    PEPROCESS Process;
    volatile LONG64 HeartbeatSequence;
};

DeviceExtension* GetExtension(PDEVICE_OBJECT deviceObject);
DeviceState GetDeviceState(DeviceExtension* extension);
void SetDeviceState(DeviceExtension* extension, DeviceState state);

NTSTATUS InitializeDevice(PDEVICE_OBJECT deviceObject, Protocol::LoadMode mode, DeviceState state);
void DestroyDevice(DeviceExtension* extension);
void StartDevice(DeviceExtension* extension);
void StopDevice(DeviceExtension* extension);
bool AcquireDeviceIo(DeviceExtension* extension);
void ReleaseDeviceIo(DeviceExtension* extension);
void TouchDevice(DeviceExtension* extension);
NTSTATUS SetIdleTimeout(DeviceExtension* extension, ULONG seconds);
ULONG GetIdleTimeout(DeviceExtension* extension);
ULONG GetRemainingIdleTime(DeviceExtension* extension);
Protocol::RuntimeState GetRuntimeState(DeviceExtension* extension);
ULONG GetSessionCount(DeviceExtension* extension);

NTSTATUS BindSession(DeviceExtension* extension, PFILE_OBJECT fileObject, ULONG processId);
NTSTATUS ReferenceSessionProcess(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    Session** session,
    PEPROCESS* process);
NTSTATUS ReferenceSession(DeviceExtension* extension, PFILE_OBJECT fileObject, Session** session);
void ReleaseSession(Session* session);
NTSTATUS UpdateHeartbeat(Session* session, UINT64 sequence);

_Dispatch_type_(IRP_MJ_CREATE)
DRIVER_DISPATCH DispatchCreate;

_Dispatch_type_(IRP_MJ_CLEANUP)
DRIVER_DISPATCH DispatchCleanup;

_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH DispatchClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH DispatchDeviceControl;
