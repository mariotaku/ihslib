/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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
/**
 * What a gamepad can do, as the host is told it.
 *
 * The 2022 streaming_client derives most of these from the SDL mapping string, and the rest from
 * the controller's type or GUID — which makes several of them proxies rather than real answers. The
 * newer streaming_client_920 replaces those proxies with SDL3 capability queries whose property
 * names (`SDL.joystick.cap.rumble`, `.mono_led`, `.rgb_led`) are what actually named the bits below;
 * before that they were known only by which controllers happened to set them.
 *
 * Three are still unnamed, and are likely to stay that way. The client only ever writes them; no
 * consumer exists in any binary available here, the protobuf carries caps_bits as a bare uint32
 * with no descriptor-level enum, and no public naming exists. What each one is gated on is recorded
 * below, which is the most that can honestly be said.
 */
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
    /** Any paddle at all — set for any of `,paddle1:` .. `,paddle4:`, not paddle 1 specifically. */
    IHS_HID_CAP_PADDLES = 0x00000400,
    /** Motion sensing. Fires with IHS_HID_CAP_GYRO; what separates the two is not established. */
    IHS_HID_CAP_MOTION = 0x00000800,
    /**
     * Unnamed. Set for PS4 / PS5 pads, read by nothing. Three independent capability builders —
     * streaming_client, streaming_client_920 and streaming_client.pi — each set it in one
     * statement, under one predicate, as part of the same inseparable composite `0x02071800`, and
     * no build ever splits that set. The other four bits of it became SDL capability queries, so
     * this and IHS_HID_CAP_UNK_7 are real hardware features SDL does not model (touchpad, speaker,
     * headphone jack, microphone and adaptive triggers all fit), with nothing to tell them apart.
     */
    IHS_HID_CAP_UNK_2 = 0x00001000,
    /** `SDL.joystick.cap.rumble`. The old build used "is XInput or HIDAPI" as a stand-in for it. */
    IHS_HID_CAP_RUMBLE = 0x00004000,
    /** `SDL.joystick.cap.mono_led`. */
    IHS_HID_CAP_LED = 0x00010000,
    /** `SDL.joystick.cap.rgb_led`. Never set without IHS_HID_CAP_LED. */
    IHS_HID_CAP_LED_RGB = 0x00020000,
    /** `SDL_GamepadHasSensor(GYRO)`. */
    IHS_HID_CAP_GYRO = 0x00040000,
    /**
     * Probably the absence of rumble, though not proved. streaming_client.pi sets it when a pad has
     * neither rumble nor trigger rumble, and the 2022 build's "neither XInput nor HIDAPI" is the
     * same stand-in it used, inverted, for IHS_HID_CAP_RUMBLE. Against that: streaming_client_920
     * sets it unconditionally, which no reading of it survives. Not renamed on that conflict.
     */
    IHS_HID_CAP_UNK_8 = 0x00100000,
    /** All four paddles, not paddle 3 specifically. */
    IHS_HID_CAP_PADDLES_4 = 0x00400000,
    IHS_HID_CAP_MISC_1 = 0x00800000 /*Misc 1*/,
    /**
     * Unnamed. Never set by the old build; the newer one sets it only for the HORI Wireless
     * HORIPAD For Steam (0f0d:0196 / 0f0d:01ab, SDL's k_eControllerType_HoriSteamController) with
     * all four paddles present.
     */
    IHS_HID_CAP_UNK_6 = 0x01000000,
    /** Unnamed. Set for PS4 / PS5 pads, read by nothing. @see IHS_HID_CAP_UNK_2. */
    IHS_HID_CAP_UNK_7 = 0x02000000,
} IHS_HIDDeviceCaps;
#pragma clang diagnostic pop

/**
 * The bits UpdateHIDDeviceInfo @ 0x21d58c ORs into whatever the enumerator reported (`orr r3,#0x3f0`
 * @ 0x21da94), for every device it announces. A provider does not need to report them.
 */
#define IHS_HID_CAPS_ALWAYS (IHS_HID_CAP_STICKBTNS | IHS_HID_CAP_SHOULDERS | IHS_HID_CAP_TRIGGERS | \
                             IHS_HID_CAP_BACK | IHS_HID_CAP_START | IHS_HID_CAP_GUIDE)

typedef struct IHS_HIDDeviceInfo {
    /** Platform-specific device path */
    const char *path;
    const char *product_string;
    const char *serial_number;
    /** Device Vendor ID */
    uint16_t vendor_id;
    /** Device Product ID */
    uint16_t product_id;
    uint16_t product_version;
    /**
     * What this device can actually do, as a mask of IHS_HIDDeviceCaps. IHS_HID_CAPS_ALWAYS is
     * added on top before the device is announced, so a provider only reports what it knows; 0 is
     * valid and means "nothing beyond the always-set bits", which is what the reference sends for a
     * plain joystick (CHIDDeviceListSDL @ 0x1551bc leaves the base at 0 unless SDL recognises the
     * device as a game controller).
     */
    uint32_t caps;
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

    /**
     * Read at most \p length bytes from the device into \p dest.
     *
     * @attention Returns the **number of bytes read**, 0 when nothing was available, negative on
     * error — hidraw's convention, and what the host expects: it takes the return value as the
     * length of the data it is handed. An implementation must write exactly that many bytes into
     * \p dest starting at its current size, and keep \p dest's size in step.
     *
     * @param device Device instance
     * @param dest Buffer to append into
     * @param length Maximum number of bytes to read
     * @param timeoutMs How long to block waiting for data
     * @return Bytes read, or negative on error
     */
    int (*read)(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs);

    int (*sendFeatureReport)(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

    /**
     * Fetch a feature report identified by \p reportNumber.
     *
     * @attention Same convention as `read`: returns the **number of bytes** written into \p dest,
     * negative on error. Returning 0 tells the host there was nothing to report, so a successful
     * fetch must return a positive count.
     *
     * @param device Device instance
     * @param reportNumber Report id requested by the host
     * @param reportNumberLen Length of \p reportNumber
     * @param dest Buffer to write into
     * @param length Maximum number of bytes to produce
     * @return Bytes written, or negative on error
     */
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

    /**
     * Drain any pending input from the device. Called from the periodic HID poll task on the
     * timer thread. NULL for backends driven by external events (e.g. the SDL backend, which is
     * push-based via IHS_HIDHandleSDLEvent).
     *
     * Return values mirror Steam's CHIDDeviceReportGenerator::BCollectReports semantics:
     *   > 0  data was added to the report holder this tick
     *   = 0  no data available, device is healthy
     *   < 0  device is dead — the manager will close and remove it
     */
    int (*poll)(IHS_HIDDevice *device);

};

struct IHS_HIDProvider {
    const IHS_HIDProviderClass *cls;
    IHS_HIDManager *manager;
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

IHS_Session *IHS_HIDProviderGetSession(IHS_HIDProvider *provider);

void IHS_HIDDeviceReportAddFull(IHS_HIDDevice *device, const uint8_t *current, size_t len);

void IHS_HIDDeviceReportAddDelta(IHS_HIDDevice *device, const uint8_t *previous, const uint8_t *current, size_t len);

void IHS_HIDDeviceLock(IHS_HIDDevice *device);

void IHS_HIDDeviceUnlock(IHS_HIDDevice *device);

IHS_Session *IHS_HIDDeviceGetSession(IHS_HIDDevice *device);

#define IHS_HIDDeviceLog(device, level, tag, ...) IHS_SessionLog(IHS_HIDDeviceGetSession(device), (level), (tag), __VA_ARGS__)