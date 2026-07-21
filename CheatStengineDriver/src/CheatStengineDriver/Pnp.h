#pragma once

#include "Device.h"

DRIVER_ADD_DEVICE AddDevice;

_Dispatch_type_(IRP_MJ_PNP)
DRIVER_DISPATCH DispatchPnp;

_Dispatch_type_(IRP_MJ_POWER)
DRIVER_DISPATCH DispatchPower;

DRIVER_DISPATCH DispatchPassThrough;
IO_COMPLETION_ROUTINE CompleteSynchronousIrp;
IO_COMPLETION_ROUTINE ReleaseRemoveLock;
