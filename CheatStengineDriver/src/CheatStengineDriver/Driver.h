#pragma once

#include <ntifs.h>

extern "C" DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

void ConfigureDispatch(PDRIVER_OBJECT driverObject, bool pnp);
NTSTATUS CompleteIrp(PIRP irp, NTSTATUS status, ULONG_PTR information = 0);