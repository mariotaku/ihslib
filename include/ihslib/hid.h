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

typedef struct IHS_Session IHS_Session;

typedef struct IHS_StreamHIDDeviceEnumeration IHS_StreamHIDDeviceEnumeration;
typedef struct IHS_StreamHIDDevice IHS_StreamHIDDevice;

typedef struct IHS_SessionHIDManager IHS_SessionHIDManager;

/** hidapi info structure 
 *  @brief  Information about a connected HID device
 */
typedef struct IHS_StreamHIDDeviceInfo {
    /** Platform-specific device path */
    const char *path;
    /** Device Vendor ID */
    uint16_t vendor_id;
    /** Device Product ID */
    uint16_t product_id;
    /** Serial Number */
    const wchar_t *serial_number;
    /** Device Release Number in binary-coded decimal,
        also known as Device Version Number */
    uint16_t release_number;
    /** Manufacturer String */
    const wchar_t *manufacturer_string;
    /** Product string */
    const wchar_t *product_string;
    /** Usage Page for this Device/Interface
        (Windows/Mac only). */
    uint16_t usage_page;
    /** Usage for this Device/Interface
        (Windows/Mac only).*/
    uint16_t usage;
    /** The USB interface which this logical device
        represents.

        * Valid on both Linux implementations in all cases.
        * Valid on the Windows implementation only if the device
          contains more than one interface. */
    int interface_number;

    /** Additional information about the USB interface.
        Valid on libusb and Android implementations. */
    int interface_class;
    int interface_subclass;
    int interface_protocol;
} IHS_StreamHIDDeviceInfo;

typedef struct IHS_StreamHIDInterface {
    IHS_StreamHIDDeviceEnumeration *(*enumerate)(uint16_t vendor_id, uint16_t product_id, void *context);

    int (*enumeration_length)(IHS_StreamHIDDeviceEnumeration *devices, void *context);

    IHS_StreamHIDDeviceEnumeration *(*enumeration_next)(IHS_StreamHIDDeviceEnumeration *devices, void *context);

    void (*enumeration_getinfo)(const IHS_StreamHIDDeviceEnumeration *devices, IHS_StreamHIDDeviceInfo *info,
                                void *context);

    void (*free_enumeration)(IHS_StreamHIDDeviceEnumeration *devices, void *context);

    IHS_StreamHIDDevice *(*open)(uint16_t vendor_id, uint16_t product_id, const char *serial_number, void *context);

    IHS_StreamHIDDevice *(*open_path)(const char *path, int bExclusive, void *context);

    int (*write)(IHS_StreamHIDDevice *dev, const unsigned char *data, size_t length, void *context);

    int (*read_timeout)(IHS_StreamHIDDevice *dev, unsigned char *data, size_t length, int milliseconds, void *context);

    int (*read)(IHS_StreamHIDDevice *dev, unsigned char *data, size_t length, void *context);

    int (*set_nonblocking)(IHS_StreamHIDDevice *dev, int nonblock, void *context);

    int (*send_feature_report)(IHS_StreamHIDDevice *dev, const unsigned char *data, size_t length, void *context);

    int (*get_feature_report)(IHS_StreamHIDDevice *dev, unsigned char *data, size_t length, void *context);

    void (*close)(IHS_StreamHIDDevice *dev, void *context);
} IHS_StreamHIDInterface;

bool IHS_SessionHIDNotifyChange(IHS_Session *session);

void IHS_SessionSetHIDManager(IHS_Session *session, const IHS_SessionHIDManager *manager);

