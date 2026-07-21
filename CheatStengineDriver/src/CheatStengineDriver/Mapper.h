#pragma once

#include "Device.h"

NTSTATUS InitializeMappedDriver(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath);
NTSTATUS BootstrapMappedDriver();
NTSTATUS CreateControlDevice(PDRIVER_OBJECT driverObject, Protocol::LoadMode mode);
void DestroyControlDevice(PDEVICE_OBJECT deviceObject);
