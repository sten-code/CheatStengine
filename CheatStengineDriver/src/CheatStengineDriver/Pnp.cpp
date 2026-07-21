#include "Pnp.h"

namespace {

NTSTATUS SetInterface(DeviceExtension* extension, BOOLEAN enabled)
{
    if (extension->InterfaceEnabled == enabled) {
        return STATUS_SUCCESS;
    }

    const NTSTATUS status = IoSetDeviceInterfaceState(
        &extension->InterfaceName,
        enabled ? TRUE : FALSE);
    if (NT_SUCCESS(status)) {
        extension->InterfaceEnabled = enabled;
    }
    return status;
}

NTSTATUS ForwardSynchronously(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    KEVENT event {};
    KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(irp);
    IoSetCompletionRoutine(
        irp,
        CompleteSynchronousIrp,
        &event,
        TRUE,
        TRUE,
        TRUE);

    const NTSTATUS callStatus = IoCallDriver(extension->LowerDeviceObject, irp);
    if (callStatus == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, nullptr);
    }
    return irp->IoStatus.Status;
}

NTSTATUS ForwardWithRemoveLock(PDEVICE_OBJECT deviceObject, PIRP irp, bool power)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    IoCopyCurrentIrpStackLocationToNext(irp);
    IoSetCompletionRoutine(
        irp,
        ReleaseRemoveLock,
        extension,
        TRUE,
        TRUE,
        TRUE);
    return power
        ? PoCallDriver(extension->LowerDeviceObject, irp)
        : IoCallDriver(extension->LowerDeviceObject, irp);
}

bool BeginPendingState(DeviceExtension* extension, DeviceState state)
{
    if (GetDeviceState(extension) != DeviceState::Started) {
        return false;
    }

    SetDeviceState(extension, state);
    SetInterface(extension, FALSE);
    StopDevice(extension);
    return true;
}

void RestorePendingState(DeviceExtension* extension, DeviceState state)
{
    if (GetDeviceState(extension) != state) {
        return;
    }

    StartDevice(extension);
    SetInterface(extension, TRUE);
    SetDeviceState(extension, DeviceState::Started);
    TouchDevice(extension);
}

}

NTSTATUS CompleteSynchronousIrp(PDEVICE_OBJECT, PIRP, PVOID context)
{
    KeSetEvent(static_cast<PKEVENT>(context), IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS ReleaseRemoveLock(PDEVICE_OBJECT, PIRP irp, PVOID context)
{
    if (irp->PendingReturned) {
        IoMarkIrpPending(irp);
    }
    IoReleaseRemoveLock(&static_cast<DeviceExtension*>(context)->RemoveLock, irp);
    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    if (extension->Mode != Protocol::LoadMode::Pnp) {
        return CompleteIrp(irp, STATUS_INVALID_DEVICE_REQUEST);
    }
    NTSTATUS status = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        return CompleteIrp(irp, status);
    }

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    switch (stack->MinorFunction) {
        case IRP_MN_START_DEVICE:
            status = ForwardSynchronously(deviceObject, irp);
            if (NT_SUCCESS(status)) {
                StartDevice(extension);
                status = SetInterface(extension, TRUE);
                if (NT_SUCCESS(status)) {
                    SetDeviceState(extension, DeviceState::Started);
                    TouchDevice(extension);
                } else {
                    SetDeviceState(extension, DeviceState::Stopped);
                    StopDevice(extension);
                }
            }
            IoReleaseRemoveLock(&extension->RemoveLock, irp);
            return CompleteIrp(irp, status);

        case IRP_MN_QUERY_STOP_DEVICE: {
            const bool transitioned = BeginPendingState(extension, DeviceState::StopPending);
            status = ForwardSynchronously(deviceObject, irp);
            if (!NT_SUCCESS(status) && transitioned) {
                RestorePendingState(extension, DeviceState::StopPending);
            }
            IoReleaseRemoveLock(&extension->RemoveLock, irp);
            return CompleteIrp(irp, status);
        }

        case IRP_MN_QUERY_REMOVE_DEVICE: {
            const bool transitioned = BeginPendingState(extension, DeviceState::RemovePending);
            status = ForwardSynchronously(deviceObject, irp);
            if (!NT_SUCCESS(status) && transitioned) {
                RestorePendingState(extension, DeviceState::RemovePending);
            }
            IoReleaseRemoveLock(&extension->RemoveLock, irp);
            return CompleteIrp(irp, status);
        }

        case IRP_MN_CANCEL_STOP_DEVICE:
            status = ForwardSynchronously(deviceObject, irp);
            RestorePendingState(extension, DeviceState::StopPending);
            IoReleaseRemoveLock(&extension->RemoveLock, irp);
            return CompleteIrp(irp, status);

        case IRP_MN_CANCEL_REMOVE_DEVICE:
            status = ForwardSynchronously(deviceObject, irp);
            RestorePendingState(extension, DeviceState::RemovePending);
            IoReleaseRemoveLock(&extension->RemoveLock, irp);
            return CompleteIrp(irp, status);

        case IRP_MN_STOP_DEVICE:
            SetDeviceState(extension, DeviceState::Stopped);
            SetInterface(extension, FALSE);
            StopDevice(extension);
            status = ForwardSynchronously(deviceObject, irp);
            IoReleaseRemoveLock(&extension->RemoveLock, irp);
            return CompleteIrp(irp, status);

        case IRP_MN_SURPRISE_REMOVAL:
            SetDeviceState(extension, DeviceState::SurpriseRemoved);
            SetInterface(extension, FALSE);
            StopDevice(extension);
            return ForwardWithRemoveLock(deviceObject, irp, false);

        case IRP_MN_REMOVE_DEVICE:
            SetDeviceState(extension, DeviceState::Removed);
            SetInterface(extension, FALSE);
            StopDevice(extension);
            IoReleaseRemoveLockAndWait(&extension->RemoveLock, irp);
            IoSkipCurrentIrpStackLocation(irp);
            status = IoCallDriver(extension->LowerDeviceObject, irp);
            IoDetachDevice(extension->LowerDeviceObject);
            DestroyDevice(extension);
            RtlFreeUnicodeString(&extension->InterfaceName);
            IoDeleteDevice(deviceObject);
            return status;

        default:
            return ForwardWithRemoveLock(deviceObject, irp, false);
    }
}

NTSTATUS DispatchPower(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    if (extension->Mode != Protocol::LoadMode::Pnp) {
        return CompleteIrp(irp, STATUS_INVALID_DEVICE_REQUEST);
    }
    const NTSTATUS status = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        return CompleteIrp(irp, status);
    }
    return ForwardWithRemoveLock(deviceObject, irp, true);
}

NTSTATUS DispatchPassThrough(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    if (extension->Mode != Protocol::LoadMode::Pnp) {
        return CompleteIrp(irp, STATUS_INVALID_DEVICE_REQUEST);
    }
    const NTSTATUS status = IoAcquireRemoveLock(&extension->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        return CompleteIrp(irp, status);
    }
    return ForwardWithRemoveLock(deviceObject, irp, false);
}

NTSTATUS AddDevice(PDRIVER_OBJECT driverObject, PDEVICE_OBJECT physicalDeviceObject)
{
    PDEVICE_OBJECT deviceObject = nullptr;
    NTSTATUS status = IoCreateDevice(
        driverObject,
        sizeof(DeviceExtension),
        nullptr,
        Protocol::DeviceType,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = InitializeDevice(deviceObject, Protocol::LoadMode::Pnp, DeviceState::Added);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DeviceExtension* extension = GetExtension(deviceObject);
    extension->PhysicalDeviceObject = physicalDeviceObject;
    status = IoAttachDeviceToDeviceStackSafe(
        deviceObject,
        physicalDeviceObject,
        &extension->LowerDeviceObject);
    if (!NT_SUCCESS(status)) {
        DestroyDevice(extension);
        IoDeleteDevice(deviceObject);
        return status;
    }

    GUID interfaceGuid = Protocol::DeviceInterfaceGuid;
    status = IoRegisterDeviceInterface(
        physicalDeviceObject,
        &interfaceGuid,
        nullptr,
        &extension->InterfaceName);
    if (!NT_SUCCESS(status)) {
        IoDetachDevice(extension->LowerDeviceObject);
        DestroyDevice(extension);
        IoDeleteDevice(deviceObject);
        return status;
    }

    deviceObject->Flags |= DO_DIRECT_IO;
    if ((extension->LowerDeviceObject->Flags & DO_POWER_PAGABLE) != 0) {
        deviceObject->Flags |= DO_POWER_PAGABLE;
    }
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
