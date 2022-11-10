/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Ningyuan Li <https://github.com/mariotaku>.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <assert.h>
#include "device.h"
#include "manager.h"

IHS_HIDDevice *IHS_HIDDeviceCreate(const IHS_HIDDeviceClass *cls) {
    IHS_HIDDevice *device = cls->alloc(cls);
    assert(device->cls == cls);
    return device;
}

void IHS_HIDManagedDeviceOpened(IHS_HIDManagedDevice *managed) {
    assert(managed->manager != NULL);
    IHS_HIDDevice *device = managed->device;
    if (device->cls->opened != NULL) {
        device->cls->opened(device);
    }
}

void IHS_HIDManagedDeviceClose(IHS_HIDManagedDevice *managed) {
    IHS_HIDDevice *device = managed->device;
    assert(managed->manager != NULL);
    device->cls->close(device);
    IHS_HIDManagerRemoveClosedDevice(managed->manager, managed);
    device->cls->free(device);
}

int IHS_HIDDeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    assert(device->cls->write != NULL);
    return device->cls->write(device, data, dataLen);
}

int IHS_HIDDeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs) {
    assert(device->cls->read != NULL);
    return device->cls->read(device, dest, length, timeoutMs);
}

int IHS_HIDDeviceSendFeatureReport(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    return device->cls->sendFeatureReport(device, data, dataLen);
}

int IHS_HIDDeviceGetFeatureReport(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                                  IHS_Buffer *dest, size_t length) {
    return device->cls->getFeatureReport(device, reportNumber, reportNumberLen, dest, length);
}

int IHS_HIDDeviceGetVendorString(IHS_HIDDevice *device, IHS_Buffer *out) {
    return device->cls->getVendorString(device, out);
}

int IHS_HIDDeviceGetProductString(IHS_HIDDevice *device, IHS_Buffer *out) {
    return device->cls->getProductString(device, out);
}

int IHS_HIDDeviceGetSerialNumberString(IHS_HIDDevice *device, IHS_Buffer *out) {
    return device->cls->getSerialNumberString(device, out);
}

int IHS_HIDDeviceStartInputReports(IHS_HIDDevice *device, size_t length) {
    return device->cls->startInputReports(device, length);
}

int IHS_HIDDeviceRequestFullReport(IHS_HIDDevice *device) {
    return device->cls->requestFullReport(device);
}

int IHS_HIDDeviceRequestDisconnect(IHS_HIDDevice *device, int method, const uint8_t *data, size_t dataLen) {
    return device->cls->requestDisconnect(device, method, data, dataLen);
}

void IHS_HIDDeviceReportAddFull(IHS_HIDDevice *device, const uint8_t *current, size_t len) {
    IHS_HIDReportHolderAddFull(&device->managed->reportHolder, current, len);
}

void IHS_HIDDeviceReportAddDelta(IHS_HIDDevice *device, const uint8_t *previous, const uint8_t *current, size_t len) {
    IHS_HIDReportHolderAddDelta(&device->managed->reportHolder, previous, current, len);
}