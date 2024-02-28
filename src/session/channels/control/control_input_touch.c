/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2024 Mariotaku <https://github.com/mariotaku>.
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

bool IHS_SessionSendTouchDown(IHS_Session *session, uint64_t fingerId, float x, float y) {
    CInputTouchFingerDownMsg message = CINPUT_TOUCH_FINGER_DOWN_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, fingerid, fingerId);
    PROTOBUF_C_SET_VALUE(message, x_normalized, x);
    PROTOBUF_C_SET_VALUE(message, y_normalized, y);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputTouchFingerDown,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendTouchUp(IHS_Session *session, uint64_t fingerId, float x, float y) {
    CInputTouchFingerUpMsg message = CINPUT_TOUCH_FINGER_UP_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, fingerid, fingerId);
    PROTOBUF_C_SET_VALUE(message, x_normalized, x);
    PROTOBUF_C_SET_VALUE(message, y_normalized, y);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputTouchFingerUp,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendTouchMotion(IHS_Session *session, uint64_t fingerId, float x, float y) {
    CInputTouchFingerMotionMsg message = CINPUT_TOUCH_FINGER_MOTION_MSG__INIT;
    PROTOBUF_C_SET_VALUE(message, fingerid, fingerId);
    PROTOBUF_C_SET_VALUE(message, x_normalized, x);
    PROTOBUF_C_SET_VALUE(message, y_normalized, y);
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputTouchFingerMotion,
                                         (const ProtobufCMessage *) &message);
}