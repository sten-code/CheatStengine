#include "driver.h"
#include "process.h"

static PDEVICE_OBJECT g_DeviceObject = nullptr;
static UNICODE_STRING g_DevName {};
static UNICODE_STRING g_SymLink {};

static NTSTATUS CreateClose(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(deviceObject);

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS DeviceControl(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(deviceObject);

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);

    const ULONG ioCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
    if (ioCode != IOCTL_CS_COMMAND) {
        DbgPrint("[CheatStengine] Unknown IOCTL: 0x%08X\n", ioCode);
        irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    const ULONG inputLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    const ULONG outputLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    if (inputLen < sizeof(CommandHeader) || !irp->AssociatedIrp.SystemBuffer) {
        DbgPrint("[CheatStengine] Buffer too small or null (%lu bytes)\n", inputLen);
        irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_BUFFER_TOO_SMALL;
    }

    CommandHeader* cmd = static_cast<CommandHeader*>(irp->AssociatedIrp.SystemBuffer);
    DbgPrint("[CheatStengine] Command type=%d  PID=%lu\n", cmd->Type, cmd->Pid);

    // Resolve target process.
    PEPROCESS targetProcess = nullptr;
    NTSTATUS status = PsLookupProcessByProcessId(reinterpret_cast<HANDLE>(cmd->Pid), &targetProcess);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] PsLookupProcessByProcessId failed for PID %lu: 0x%08X\n",
            cmd->Pid, status);
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    ULONG bytesReturned = 0;

    // Dispatch command.
    switch (cmd->Type) {
        case CommandHeader::ReadMemory:
            status = ReadProcessMemory(
                targetProcess,
                cmd->ReadMemoryData.Address,
                cmd->ReadMemoryData.Buffer,
                cmd->ReadMemoryData.Size);
            break;

        case CommandHeader::WriteMemory:
            status = WriteProcessMemory(
                targetProcess,
                cmd->WriteMemoryData.Address,
                cmd->WriteMemoryData.Buffer,
                cmd->WriteMemoryData.Size);
            break;

        case CommandHeader::QueryMemory:
            if (outputLen != sizeof(MEMORY_BASIC_INFORMATION)) {
                DbgPrint("[CheatStengine] Buffer isn't the size of a MEMORY_BASIC_INFORMATION: %lu bytes\n", outputLen);
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            status = QueryProcessMemory(
                targetProcess,
                cmd->QueryMemoryData.Address,
                static_cast<MEMORY_BASIC_INFORMATION*>(irp->AssociatedIrp.SystemBuffer));
            bytesReturned = sizeof(MEMORY_BASIC_INFORMATION);
            break;

        case CommandHeader::AllocateMemory:
            status = AllocateProcessMemory(
                targetProcess,
                cmd->AllocateMemoryData.Address,
                cmd->AllocateMemoryData.Size,
                cmd->AllocateMemoryData.AllocationType,
                cmd->AllocateMemoryData.Protect,
                cmd->AllocateMemoryData.AllocatedAddressPtr);
            break;

        case CommandHeader::FreeMemory:
            status = FreeProcessMemory(
                targetProcess,
                cmd->FreeMemoryData.Address,
                cmd->FreeMemoryData.Size,
                cmd->FreeMemoryData.FreeType);
            break;

        case CommandHeader::ProtectMemory:
            status = ProtectProcessMemory(
                targetProcess,
                cmd->ProtectMemoryData.Address,
                cmd->ProtectMemoryData.Size,
                cmd->ProtectMemoryData.NewProtect,
                cmd->ProtectMemoryData.OldProtectPtr);
            break;

        default:
            DbgPrint("[CheatStengine] Unhandled command type: %d\n", cmd->Type);
            status = STATUS_INVALID_PARAMETER;
            break;
    }

    ObDereferenceObject(targetProcess);

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static void DriverUnload(PDRIVER_OBJECT driverObject)
{
    UNREFERENCED_PARAMETER(driverObject);

    NTSTATUS status = IoDeleteSymbolicLink(&g_SymLink);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] IoDeleteSymbolicLink failed: 0x%08X\n", status);
        return;
    }
    IoDeleteDevice(driverObject->DeviceObject);

    DbgPrint("[CheatStengine] Driver unloaded.\n");
}

NTSTATUS UnsupportedDispatch(PDEVICE_OBJECT driverObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(driverObject);
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}

NTSTATUS InitializeDriver(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    DbgPrint("[CheatStengine] DriverEntry.\n");

    RtlInitUnicodeString(&g_DevName, L"\\Device\\CheatStengineDriver");
    RtlInitUnicodeString(&g_SymLink, L"\\DosDevices\\CheatStengineDriver");

    PDEVICE_OBJECT deviceObject = nullptr;
    NTSTATUS status = IoCreateDevice(
        driverObject,
        0,
        &g_DevName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] IoCreateDevice failed: 0x%08X\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&g_SymLink, &g_DevName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[CheatStengine] IoCreateSymbolicLink failed: 0x%08X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        driverObject->MajorFunction[i] = UnsupportedDispatch;
    }
    driverObject->MajorFunction[IRP_MJ_CREATE] = CreateClose;
    driverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    driverObject->DriverUnload = DriverUnload;

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("[CheatStengine] Driver loaded.\n");
    return STATUS_SUCCESS;
}

extern "C" NTSTATUS NTAPI IoCreateDriver(PUNICODE_STRING driverName, PDRIVER_INITIALIZE initializationFunction);
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    UNREFERENCED_PARAMETER(registryPath);
    // return InitializeDriver(driverObject, registryPath);
    return IoCreateDriver(nullptr, InitializeDriver);
}