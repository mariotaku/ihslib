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

typedef enum IHS_StreamInputMouseButton {
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

bool IHS_SessionSendMousePosition(IHS_Session *session, float x, float y);

bool IHS_SessionSendMouseMovement(IHS_Session *session, int dx, int dy);

bool IHS_SessionSendMouseDown(IHS_Session *session, IHS_StreamInputMouseButton button);

bool IHS_SessionSendMouseUp(IHS_Session *session, IHS_StreamInputMouseButton button);

bool IHS_SessionSendMouseWheel(IHS_Session *session, IHS_StreamInputMouseWheelDirection direction);

bool IHS_SessionSendKeyDown(IHS_Session *session, uint32_t scancode);

bool IHS_SessionSendKeyUp(IHS_Session *session, uint32_t scancode);

bool IHS_SessionSendTouchDown(IHS_Session *session, uint64_t fingerId, float x, float y);

bool IHS_SessionSendTouchUp(IHS_Session *session, uint64_t fingerId, float x, float y);

bool IHS_SessionSendTouchMotion(IHS_Session *session, uint64_t fingerId, float x, float y);