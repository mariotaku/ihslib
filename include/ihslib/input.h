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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct IHS_Session IHS_Session;

typedef struct IHS_StreamInputCursorImage {
    uint64_t cursorId;
    int32_t width;
    int32_t height;
    int32_t hotX;
    int32_t hotY;
    const uint8_t *image;
    size_t imageLen;
} IHS_StreamInputCursorImage;

/**
 * Wire values, not SDL button numbers. SDL numbers its buttons 1..5 in the order
 * left/middle/right/x1/x2 and the reference translates them (ConvertSDLButtonToStreamButton @
 * 0x216a28); passing a raw SDL button number here would swap middle and right.
 */
typedef enum IHS_StreamInputMouseButton {
    /** The reference refuses to send this, and so does ihslib. */
    IHS_MOUSE_BUTTON_UNKNOWN = 4096,
    IHS_MOUSE_BUTTON_LEFT = 1,
    IHS_MOUSE_BUTTON_RIGHT = 2,
    IHS_MOUSE_BUTTON_MIDDLE = 16,
    IHS_MOUSE_BUTTON_X1 = 32,
    IHS_MOUSE_BUTTON_X2 = 64,
} IHS_StreamInputMouseButton;

typedef enum IHS_StreamInputMouseWheelDirection {
    IHS_MOUSE_WHEEL_UP = 1,
    IHS_MOUSE_WHEEL_DOWN,
    IHS_MOUSE_WHEEL_LEFT,
    IHS_MOUSE_WHEEL_RIGHT,
} IHS_StreamInputMouseWheelDirection;

typedef enum IHS_StreamInputControllerType {
    IHS_CONTROLLER_TYPE_NONE = 0x0,
    IHS_CONTROLLER_TYPE_GENERIC = 0x1e,
    IHS_CONTROLLER_TYPE_XBOX_360 = 0x1f,
    IHS_CONTROLLER_TYPE_XBOX_ONE = 0x20,
    IHS_CONTROLLER_TYPE_PS3 = 0x21,
    IHS_CONTROLLER_TYPE_PS4 = 0x22,
    IHS_CONTROLLER_TYPE_SWITCH = 0x26,
    IHS_CONTROLLER_TYPE_SWITCH_GEN = 0x2a,
    IHS_CONTROLLER_TYPE_PS5 = 0x2d,
} IHS_StreamInputControllerType;

typedef struct IHS_HIDPeripheralInfo {
    uint16_t vid, pid;
    bool xinput;
} IHS_HIDPeripheralInfo;

typedef struct IHS_StreamInputCallbacks {
    /**
     *
     * @param session
     * @param cursorId
     * @param context
     * @return `true` if cursor image exists, `false` will cause library to request for the icon
     */
    bool (*setCursor)(IHS_Session *session, uint64_t cursorId, void *context);

    bool (*deleteCursor)(IHS_Session *session, uint64_t cursorId, void *context);

    void (*cursorImage)(IHS_Session *session, const IHS_StreamInputCursorImage *image, void *context);

    void (*showCursor)(IHS_Session *session, float x, float y, void *context);

    void (*hideCursor)(IHS_Session *session, void *context);
} IHS_StreamInputCallbacks;

/**
 * Round-trip measurement for one input event, recovered by matching the input mark the host echoes
 * back in a video frame header. All durations are in 1/65536-second units, the same domain as
 * IHS_InputTimestampNow().
 */
typedef struct IHS_SessionInputLatency {
    /** The mark this measurement belongs to. */
    uint32_t inputMark;
    /** From the timestamp the caller supplied to the moment the message was handed to the wire. */
    uint32_t queuedToSent;
    /** From the caller's timestamp to the arrival of the first frame reflecting the input. */
    uint32_t roundTrip;
    /**
     * When the host received the input, in the *host's* clock. Not comparable against the two
     * fields above, which are measured locally; useful only relative to other host timestamps.
     */
    uint32_t hostRecvTimestamp;
} IHS_SessionInputLatency;

/**
 * Current value of the clock the `timestamp` parameters below are expressed in: CLOCK_MONOTONIC in
 * 1/65536-second units, the same domain as Steam's GetStreamTimestamp @ 0x26cd0c. Capture this when
 * an input event arrives and hand it to the send function, so the latency reported by
 * IHS_SessionGetInputLatency covers the time the event spent queued in the caller as well.
 * Multiply by 1000 and divide by 65536 for milliseconds.
 */
uint32_t IHS_InputTimestampNow(void);

/**
 * Send pointer motion carrying both an absolute position and the relative delta, as Steam's
 * CStreamClient::SendMouseMotion @ 0x1f910c does. This is the shape a host expects whenever the
 * client knows where the pointer is; use IHS_SessionSendMouseMotionRelative only when it does not.
 *
 * @param timestamp When the input occurred, in IHS_InputTimestampNow() units. Pass
 *                  IHS_InputTimestampNow() if the caller has no event time of its own.
 * @param x Horizontal position as a fraction of the host's capture rectangle — 0.0 at the left
 *          edge, 1.0 at the right, origin top-left. Multiply by the size most recently reported to
 *          IHS_StreamVideoCallbacks::setCaptureSize to convert to host pixels. Values slightly
 *          outside [0,1] are permitted, as when the pointer sits over letterbox bars.
 * @param y Vertical position, same convention, increasing downward.
 * @param dx Horizontal movement since the previous motion message, in device units.
 * @param dy Vertical movement since the previous motion message, in device units.
 */
bool IHS_SessionSendMouseMotion(IHS_Session *session, uint32_t timestamp, float x, float y, int dx, int dy);

/**
 * Send pointer motion as a delta only, leaving x_normalized/y_normalized absent from the message.
 * Mirrors the reference's second CStreamClient::SendMouseMotion overload @ 0x1f92f0, which it uses
 * while the pointer is captured in relative mode and no meaningful absolute position exists.
 *
 * @param timestamp When the input occurred, in IHS_InputTimestampNow() units.
 */
bool IHS_SessionSendMouseMotionRelative(IHS_Session *session, uint32_t timestamp, int dx, int dy);

/**
 * Fold pointer motion into a pending slot instead of sending it, as Steam's
 * CStreamPlayer::QueueMouseMotion @ 0x22d120 does. A high-DPI mouse or a trackpad can produce
 * hundreds of motion events a second, and the reference never puts more than one message per event
 * pump on the wire.
 *
 * Absolute position overwrites; relative movement accumulates. Nothing is sent until
 * IHS_SessionFlushMouseMotion runs, so a caller that queues and never flushes sends nothing —
 * except that the button and wheel senders flush for you, so a click can never overtake the motion
 * that positioned the pointer.
 *
 * There is deliberately no timestamp parameter. The reference stamps the mark when it flushes, not
 * when the event arrived, so the queued time cannot be recovered — IHS_SessionGetInputLatency's
 * queuedToSent then measures from the flush. Callers that care about that measurement more than
 * about wire traffic should keep using IHS_SessionSendMouseMotion, which is unchanged.
 *
 * @see IHS_SessionFlushMouseMotion
 */
void IHS_SessionQueueMouseMotion(IHS_Session *session, float x, float y, int dx, int dy);

/**
 * Queue motion as a delta only, leaving the absolute position as whatever was last queued.
 * @see IHS_SessionQueueMouseMotion
 */
void IHS_SessionQueueMouseMotionRelative(IHS_Session *session, int dx, int dy);

/**
 * Send whatever motion has been queued as a single message, and clear the pending state. The
 * reference does this at the tail of the same tick that drained the event queue
 * (CStreamPlayer::UpdateInput @ 0x21e080, rate-limited to 4 ms), so the natural place to call this
 * is once per iteration of the application's own event loop, after it has drained its input events.
 *
 * @return true if a message was sent; false when nothing was pending or input is not enabled.
 */
bool IHS_SessionFlushMouseMotion(IHS_Session *session);

/**
 * Most recent input latency measurement, or false if no frame has yet echoed back a mark this
 * session issued. Safe to call from any thread.
 */
bool IHS_SessionGetInputLatency(IHS_Session *session, IHS_SessionInputLatency *out);

/**
 * @param timestamp When the input occurred, in IHS_InputTimestampNow() units. Pass
 *                  IHS_InputTimestampNow() if the caller has no event time of its own; passing the
 *                  time the event was actually dequeued is what makes
 *                  IHS_SessionInputLatency.queuedToSent meaningful.
 */
bool IHS_SessionSendMouseDown(IHS_Session *session, uint32_t timestamp, IHS_StreamInputMouseButton button);

bool IHS_SessionSendMouseUp(IHS_Session *session, uint32_t timestamp, IHS_StreamInputMouseButton button);

bool IHS_SessionSendMouseWheel(IHS_Session *session, uint32_t timestamp,
                               IHS_StreamInputMouseWheelDirection direction);

/**
 * Press a key.
 *
 * @param timestamp When the input occurred, in IHS_InputTimestampNow() units.
 * @param scancode An SDL2 SDL_Scancode, which is a USB HID usage page 7 code — a physical key
 *                 position, not a character and not a keycode. The reference passes SDL's value
 *                 through untouched (ConvertSDLScancodeToStreamScancode @ 0x216abc is the identity
 *                 function), so no translation table is needed on either side. 0 is not a key and
 *                 is not sent. Characters that a layout produces rather than a key position belong
 *                 in IHS_SessionSendText.
 */
bool IHS_SessionSendKeyDown(IHS_Session *session, uint32_t timestamp, uint32_t scancode);

/** @see IHS_SessionSendKeyDown for the scancode space. */
bool IHS_SessionSendKeyUp(IHS_Session *session, uint32_t timestamp, uint32_t scancode);

/**
 * Send a UTF-8 text string to the host as a CInputTextMsg. Used for IME-composed text,
 * pasted clipboard content, soft-keyboard input, and any other path where the caller has a
 * committed string rather than per-key scancodes.
 *
 * The host normally also receives keyboard scancodes through IHS_SessionSendKeyDown/Up.
 * To avoid duplicating ASCII keys, Steam's own SDL adapter only forwards multi-byte UTF-8
 * (text bytes with the high bit set) through this path; the caller is responsible for the
 * equivalent filtering when both scancodes and text are being sent.
 *
 * @param utf8 NUL-terminated UTF-8 string. NULL or empty strings are dropped silently.
 * @return true if sent
 */
bool IHS_SessionSendText(IHS_Session *session, uint32_t timestamp, const char *utf8);

bool IHS_SessionSendTouchDown(IHS_Session *session, uint32_t timestamp, uint64_t fingerId, float x, float y);

bool IHS_SessionSendTouchUp(IHS_Session *session, uint32_t timestamp, uint64_t fingerId, float x, float y);

bool IHS_SessionSendTouchMotion(IHS_Session *session, uint32_t timestamp, uint64_t fingerId, float x, float y);