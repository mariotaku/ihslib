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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include "buffer.h"
#include "enumeration.h"

typedef struct IHS_Session IHS_Session;

typedef struct IHS_HIDDevice IHS_HIDDevice;

typedef struct IHS_HIDManager IHS_HIDManager;

typedef struct IHS_HIDDeviceInfo IHS_HIDDeviceInfo;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
typedef enum IHS_HIDDeviceCaps {
    IHS_HID_CAP_ABXY = 0x00000001,
    IHS_HID_CAP_DPAD = 0x00000002,
    IHS_HID_CAP_LSTICK = 0x00000004,
    IHS_HID_CAP_RSTICK = 0x00000008,
    IHS_HID_CAP_STICKBTNS = 0x00000010,
    IHS_HID_CAP_SHOULDERS = 0x00000020,
    IHS_HID_CAP_TRIGGERS = 0x00000040,
    IHS_HID_CAP_BACK = 0x00000080,
    IHS_HID_CAP_START = 0x00000100,
    IHS_HID_CAP_GUIDE = 0x00000200,
    IHS_HID_CAP_PADDLE_1 = 0x00000400 /*Paddle 1*/,
    IHS_HID_CAP_XINPUT_HIDAPI = 0x00004000 /*XInput and HIDAPI*/,
    IHS_HID_CAP_PADDLE_3 = 0x00400000 /*Paddle 3*/,
    IHS_HID_CAP_MISC_1 = 0x00800000 /*Misc 1*/,
    IHS_HID_CAP_XBOX_ELITE_2 = 0x00030000 /*Xbox elite 2?*/,
    IHS_HID_CAP_PS4_PS5 = 0x02071800 /*PS4/PS5, gyro and touchpad?*/,
    IHS_HID_CAP_NOT_XINPUT_NOT_HIDAPI = 0x00010000 /*Not XInput and not HIDAPI?*/,
} IHS_HIDDeviceCaps;
#pragma clang diagnostic pop

typedef struct IHS_HIDDeviceInfo {
    /** Platform-specific device path */
    const char *path;
    const char *name;
    /** Device Vendor ID */
    uint16_t vendor_id;
    /** Device Product ID */
    uint16_t product_id;
    uint16_t product_version;
} IHS_HIDDeviceInfo;

typedef struct IHS_HIDDeviceClass IHS_HIDDeviceClass;
typedef struct IHS_HIDDevice IHS_HIDDevice;

typedef struct IHS_HIDManagedDevice IHS_HIDManagedDevice;

typedef struct IHS_HIDProviderClass IHS_HIDProviderClass;
typedef struct IHS_HIDProvider IHS_HIDProvider;

struct IHS_HIDDevice {
    const IHS_HIDDeviceClass *cls;
    /**
     * Opaque pointer to report holder
     */
    IHS_HIDManagedDevice *managed;
};

struct IHS_HIDDeviceClass {
    IHS_HIDDevice *(*alloc)(const IHS_HIDDeviceClass *cls);

    void (*free)(IHS_HIDDevice *device);

    void (*opened)(IHS_HIDDevice *device);

    /**
     * Close underlying resources
     * @param device Device instance
     */
    void (*close)(IHS_HIDDevice *device);

    int (*write)(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

    int (*read)(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs);

    int (*sendFeatureReport)(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

    int (*getFeatureReport)(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                            IHS_Buffer *dest, size_t length);

    /**
     *
     * @param device HID device
     * @param out Buffer to write value to
     * @return 0 If succeed, -1 if anything wrong happened
     */
    int (*getVendorString)(IHS_HIDDevice *device, IHS_Buffer *out);

    /**
     *
     * @param device HID device
     * @param out Buffer to write value to
     * @return 0 If succeed, -1 if anything wrong happened
     */
    int (*getProductString)(IHS_HIDDevice *device, IHS_Buffer *out);

    /**
     *
     * @param device HID device
     * @param out Buffer to write value to
     * @return 0 If succeed, -1 if anything wrong happened
     */
    int (*getSerialNumberString)(IHS_HIDDevice *device, IHS_Buffer *out);

    int (*startInputReports)(IHS_HIDDevice *device, size_t length);

    int (*requestFullReport)(IHS_HIDDevice *device);

    int (*requestDisconnect)(IHS_HIDDevice *device, int method, const uint8_t *data, size_t dataLen);

};

struct IHS_HIDProvider {
    const IHS_HIDProviderClass *cls;
};

struct IHS_HIDProviderClass {
    IHS_HIDProvider *(*alloc)(const IHS_HIDProviderClass *cls);

    void (*free)(IHS_HIDProvider *provider);

    bool (*supportsDevice)(IHS_HIDProvider *provider, const char *path);

    IHS_HIDDevice *(*openDevice)(IHS_HIDProvider *provider, const char *path);

    bool (*hasChange)(IHS_HIDProvider *provider);

    IHS_Enumeration *(*enumerateDevices)(IHS_HIDProvider *provider);

    void (*deviceInfo)(IHS_HIDProvider *provider, IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info);
};

bool IHS_SessionHIDNotifyDeviceChange(IHS_Session *session);

bool IHS_SessionHIDSendReport(IHS_Session *session);

void IHS_SessionHIDAddProvider(IHS_Session *session, IHS_HIDProvider *provider);

void IHS_HIDDeviceReportAddFull(IHS_HIDDevice *device, const uint8_t *current, size_t len);

void IHS_HIDDeviceReportAddDelta(IHS_HIDDevice *device, const uint8_t *previous, const uint8_t *current, size_t len);

void IHS_HIDDeviceLock(IHS_HIDDevice *device);

void IHS_HIDDeviceUnlock(IHS_HIDDevice *device);