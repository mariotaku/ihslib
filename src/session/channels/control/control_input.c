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

#include "session/channels/ch_control.h"
#include "client/client_pri.h"
#include "protobuf/pb_utils.h"

bool IHS_SessionSendMousePosition(IHS_Session *session, float x, float y) {
    CInputMouseMotionMsg message = CINPUT_MOUSE_MOTION_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, x_normalized, x);
    PROTOBUF_C_SET_VALUE(message, y_normalized, y);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseMotion,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseMovement(IHS_Session *session, int dx, int dy) {
    CInputMouseMotionMsg message = CINPUT_MOUSE_MOTION_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, dx, dx);
    PROTOBUF_C_SET_VALUE(message, dy, dy);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseMotion,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseDown(IHS_Session *session, IHS_StreamInputMouseButton button) {
    CInputMouseDownMsg message = CINPUT_MOUSE_DOWN_MSG__INIT;
    message.button = (EStreamMouseButton) button;
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseDown,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseUp(IHS_Session *session, IHS_StreamInputMouseButton button) {
    CInputMouseUpMsg message = CINPUT_MOUSE_UP_MSG__INIT;
    message.button = (EStreamMouseButton) button;
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputMouseUp,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendMouseWheel(IHS_Session *session, IHS_StreamInputMouseWheelDirection direction) {
    CInputMouseWheelMsg message = CINPUT_MOUSE_WHEEL_MSG__INIT;
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