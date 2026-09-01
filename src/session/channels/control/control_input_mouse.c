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

#include "session/channels/ch_control.h"
#include "session/session_pri.h"
#include "protobuf/pb_utils.h"

bool IHS_SessionSendMouseMotion(IHS_Session *session, uint32_t timestamp, float x, float y, int dx, int dy) {
    if (!IHS_SessionInputEnabled(session)) return false;
    CInputMouseMotionMsg message = CINPUT_MOUSE_MOTION_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, input_mark, IHS_SessionInputMarkNext(&session->inputMarks, timestamp));
    PROTOBUF_C_SET_VALUE(message, x_normalized, x);
    PROTOBUF_C_SET_VALUE(message, y_normalized, y);
    PROTOBUF_C_SET_VALUE(message, dx, dx);
    PROTOBUF_C_SET_VALUE(message, dy, dy);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseMotion,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseMotionRelative(IHS_Session *session, uint32_t timestamp, int dx, int dy) {
    if (!IHS_SessionInputEnabled(session)) return false;
    CInputMouseMotionMsg message = CINPUT_MOUSE_MOTION_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, input_mark, IHS_SessionInputMarkNext(&session->inputMarks, timestamp));
    // x_normalized / y_normalized are deliberately left absent rather than zeroed: the second
    // SendMouseMotion overload @ 0x1f92f0 never touches them, so the host can tell "no absolute
    // position is known" from "the pointer is at the top-left corner".
    PROTOBUF_C_SET_VALUE(message, dx, dx);
    PROTOBUF_C_SET_VALUE(message, dy, dy);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseMotion,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionGetInputLatency(IHS_Session *session, IHS_SessionInputLatency *out) {
    return IHS_SessionInputMarksGetLatest(&session->inputMarks, out);
}

bool IHS_SessionSendMouseDown(IHS_Session *session, IHS_StreamInputMouseButton button) {
    if (!IHS_SessionInputEnabled(session)) return false;
    CInputMouseDownMsg message = CINPUT_MOUSE_DOWN_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, input_mark,
                         IHS_SessionInputMarkNext(&session->inputMarks, IHS_SessionPacketTimestamp()));
    message.button = (EStreamMouseButton) button;
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseDown,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseUp(IHS_Session *session, IHS_StreamInputMouseButton button) {
    if (!IHS_SessionInputEnabled(session)) return false;
    CInputMouseUpMsg message = CINPUT_MOUSE_UP_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, input_mark,
                         IHS_SessionInputMarkNext(&session->inputMarks, IHS_SessionPacketTimestamp()));
    message.button = (EStreamMouseButton) button;
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseUp,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseWheel(IHS_Session *session, IHS_StreamInputMouseWheelDirection direction) {
    if (!IHS_SessionInputEnabled(session)) return false;
    CInputMouseWheelMsg message = CINPUT_MOUSE_WHEEL_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, input_mark,
                         IHS_SessionInputMarkNext(&session->inputMarks, IHS_SessionPacketTimestamp()));
    switch (direction) {
        case IHS_MOUSE_WHEEL_UP:
            message.direction = k_EStreamMouseWheelUp;
            break;
        case IHS_MOUSE_WHEEL_DOWN:
            message.direction = k_EStreamMouseWheelDown;
            break;
        case IHS_MOUSE_WHEEL_LEFT:
            message.direction = k_EStreamMouseWheelLeft;
            break;
        case IHS_MOUSE_WHEEL_RIGHT:
            message.direction = k_EStreamMouseWheelRight;
            break;
        default:
            return false;
    }
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseWheel,
                                         (const ProtobufCMessage *) &message);
}