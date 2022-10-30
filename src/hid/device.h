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

#include "ihs_buffer.h"
#include "ihslib/hid.h"

IHS_HIDDevice *IHS_HIDDeviceCreate(const IHS_HIDDeviceClass *cls);

void IHS_HIDDeviceOpened(IHS_HIDDevice *device);

void IHS_HIDDeviceClose(IHS_HIDDevice *device);

int IHS_HIDDeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

int IHS_HIDDeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs);

int IHS_HIDDeviceSendFeatureReport(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

/**
 *
 * @param device HID device
 * @param out Buffer to write value to
 * @return 0 If succeed, -1 if anything wrong happened
 */
int IHS_HIDDeviceGetFeatureReport(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                                  IHS_Buffer *dest, size_t length);

/**
 *
 * @param device HID device
 * @param out Buffer to write value to
 * @return 0 If succeed, -1 if anything wrong happened
 */
int IHS_HIDDeviceGetVendorString(IHS_HIDDevice *device, IHS_Buffer *out);

/**
 *
 * @param device HID device
 * @param out Buffer to write value to
 * @return 0 If succeed, -1 if anything wrong happened
 */
int IHS_HIDDeviceGetProductString(IHS_HIDDevice *device, IHS_Buffer *out);

/**
 *
 * @param device HID device
 * @param out Buffer to write value to
 * @return 0 If succeed, -1 if anything wrong happened
 */
int IHS_HIDDeviceGetSerialNumberString(IHS_HIDDevice *device, IHS_Buffer *out);

int IHS_HIDDeviceStartInputReports(IHS_HIDDevice *device, size_t length);

int IHS_HIDDeviceRequestFullReport(IHS_HIDDevice *device);

int IHS_HIDDeviceRequestDisconnect(IHS_HIDDevice *device, int method, const uint8_t *data, size_t dataLen);
