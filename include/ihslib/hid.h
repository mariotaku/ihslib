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

typedef struct IHS_StreamHIDDevice IHS_StreamHIDDevice;

typedef struct IHS_HIDManager IHS_HIDManager;

typedef struct IHS_StreamHIDDeviceInfo IHS_StreamHIDDeviceInfo;

typedef struct IHS_StreamHIDDeviceInfo {
    /** Platform-specific device path */
    const char *path;
    /** Device Vendor ID */
    uint16_t vendor_id;
    /** Device Product ID */
    uint16_t product_id;
} IHS_StreamHIDDeviceInfo;

typedef struct IHS_HIDDevice {
    const struct IHS_HIDDeviceClass *cls;
    IHS_HIDManager *manager;
    uint32_t id;
} IHS_HIDDevice;

typedef struct IHS_HIDDeviceClass {
    IHS_HIDDevice *(*alloc)(const struct IHS_HIDDeviceClass *cls);

    void (*free)(IHS_HIDDevice *device);

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

} IHS_HIDDeviceClass;

typedef struct IHS_StreamHIDProvider {
    const struct IHS_StreamHIDProviderClass *cls;
} IHS_HIDProvider;

typedef struct IHS_StreamHIDProviderClass {
    IHS_HIDProvider *(*alloc)(const struct IHS_StreamHIDProviderClass *cls);

    void (*free)(IHS_HIDProvider *provider);

    bool (*supportsDevice)(IHS_HIDProvider *provider, const char *path);

    IHS_HIDDevice *(*openDevice)(IHS_HIDProvider *provider, const char *path);

    bool (*hasChange)(IHS_HIDProvider *provider);

    IHS_Enumeration *(*enumerateDevices)(IHS_HIDProvider *provider);

    void (*deviceInfo)(IHS_HIDProvider *provider, IHS_Enumeration *enumeration, IHS_StreamHIDDeviceInfo *info);
} IHS_StreamHIDProviderClass;


bool IHS_SessionHIDNotifyChange(IHS_Session *session);

