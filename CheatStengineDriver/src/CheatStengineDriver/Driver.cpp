#include "Driver.h"

#include "Device.h"
#include "Mapper.h"
#include "Memory.h"
#include "Pnp.h"
#include "Process.h"

namespace {

NTSTATUS DispatchUnsupported(PDEVICE_OBJECT, PIRP irp)
{
    return CompleteIrp(irp, STATUS_INVALID_DEVICE_REQUEST);
}

}

NTSTATUS CompleteIrp(PIRP irp, NTSTATUS status, ULONG_PTR information)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = information;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

void ConfigureDispatch(PDRIVER_OBJECT driverObject, bool pnp)
{
    for (ULONG index = 0; index <= IRP_MJ_MAXIMUM_FUNCTION; ++index) {
        driverObject->MajorFunction[index] = pnp ? DispatchPassThrough : DispatchUnsupported;
    }

    driverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
    driverObject->MajorFunction[IRP_MJ_CLEANUP] = DispatchCleanup;
    driverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;
    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    if (pnp) {
        driverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
        driverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
        driverObject->DriverExtension->AddDevice = AddDevice;
    }
    driverObject->DriverUnload = pnp ? DriverUnload : nullptr;
}

void DriverUnload(PDRIVER_OBJECT driverObject)
{
    PDEVICE_OBJECT deviceObject = driverObject->DeviceObject;
    while (deviceObject) {
        PDEVICE_OBJECT nextDeviceObject = deviceObject->NextDevice;
        DeviceExtension* extension = GetExtension(deviceObject);
        if (extension->Mode == Protocol::LoadMode::Service) {
            DestroyControlDevice(deviceObject);
        }
        deviceObject = nextDeviceObject;
    }
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    if (!registryPath) {
        return BootstrapMappedDriver();
    }
    if (!driverObject || !driverObject->DriverExtension) {
        return STATUS_INVALID_PARAMETER;
    }

    InitializeMemorySupport();
    InitializeProcessSupport();
    ConfigureDispatch(driverObject, true);
    return CreateControlDevice(driverObject, Protocol::LoadMode::Service);
}
