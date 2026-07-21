#include "Mapper.h"

#include "Memory.h"
#include "Process.h"

#include <wdmsec.h>

namespace {

using CreateDriverRoutine = NTSTATUS (NTAPI*)(PUNICODE_STRING, PDRIVER_INITIALIZE);

constexpr GUID DeviceClassGuid = {
    0xa7e1dd95,
    0x2cb7,
    0x46a5,
    { 0x90, 0x1e, 0x23, 0x7a, 0x1f, 0xb7, 0x69, 0x5c }
};

UNICODE_STRING GetDeviceName()
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\MemoryAccess");
    return name;
}

UNICODE_STRING GetSymbolicLinkName()
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\DosDevices\\MemoryAccess");
    return name;
}

}

NTSTATUS BootstrapMappedDriver()
{
    UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"IoCreateDriver");
    const auto createDriver = reinterpret_cast<CreateDriverRoutine>(
        MmGetSystemRoutineAddress(&routineName));
    if (!createDriver) {
        return STATUS_NOT_SUPPORTED;
    }

    UNICODE_STRING driverName = RTL_CONSTANT_STRING(L"\\Driver\\CheatStengineDriver");
    return createDriver(&driverName, InitializeMappedDriver);
}

NTSTATUS InitializeMappedDriver(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    UNREFERENCED_PARAMETER(registryPath);
    if (!driverObject || !driverObject->DriverExtension) {
        return STATUS_INVALID_PARAMETER;
    }

    InitializeMemorySupport();
    InitializeProcessSupport();
    ConfigureDispatch(driverObject, false);

    return CreateControlDevice(driverObject, Protocol::LoadMode::Mapped);
}

NTSTATUS CreateControlDevice(PDRIVER_OBJECT driverObject, Protocol::LoadMode mode)
{
    if (!driverObject
        || (mode != Protocol::LoadMode::Service && mode != Protocol::LoadMode::Mapped)) {
        return STATUS_INVALID_PARAMETER;
    }

    UNICODE_STRING deviceName = GetDeviceName();
    PDEVICE_OBJECT deviceObject = nullptr;
    NTSTATUS status = IoCreateDeviceSecure(
        driverObject,
        sizeof(DeviceExtension),
        &deviceName,
        Protocol::DeviceType,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
        &DeviceClassGuid,
        &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = InitializeDevice(deviceObject, mode, DeviceState::Started);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    UNICODE_STRING symbolicLinkName = GetSymbolicLinkName();
    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DestroyDevice(GetExtension(deviceObject));
        IoDeleteDevice(deviceObject);
        return status;
    }

    deviceObject->Flags |= DO_DIRECT_IO | DO_POWER_PAGABLE;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}

void DestroyControlDevice(PDEVICE_OBJECT deviceObject)
{
    DeviceExtension* extension = GetExtension(deviceObject);
    const NTSTATUS lockStatus = IoAcquireRemoveLock(&extension->RemoveLock, deviceObject);
    SetDeviceState(extension, DeviceState::Removed);
    StopDevice(extension);
    if (NT_SUCCESS(lockStatus)) {
        IoReleaseRemoveLockAndWait(&extension->RemoveLock, deviceObject);
    }

    UNICODE_STRING symbolicLinkName = GetSymbolicLinkName();
    IoDeleteSymbolicLink(&symbolicLinkName);
    DestroyDevice(extension);
    IoDeleteDevice(deviceObject);
}
