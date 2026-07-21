#pragma once

#include "Device.h"

NTSTATUS DispatchControlRequest(
    DeviceExtension* extension,
    PFILE_OBJECT fileObject,
    PIRP irp,
    PIO_STACK_LOCATION stack,
    ULONG_PTR* information);
