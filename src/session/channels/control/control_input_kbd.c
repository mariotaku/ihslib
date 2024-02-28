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

bool IHS_SessionSendKeyDown(IHS_Session *session, uint32_t scancode) {
    CInputKeyDownMsg message = CINPUT_KEY_DOWN_MSG__INIT;
    message.scancode = scancode;
    // TODO: is inputMark needed?
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputKeyDown,
                                         (const ProtobufCMessage *) &message);
}

bool IHS_SessionSendKeyUp(IHS_Session *session, uint32_t scancode) {
    CInputKeyUpMsg message = CINPUT_KEY_UP_MSG__INIT;
    message.scancode = scancode;
    // TODO: is inputMark needed?
    return IHS_SessionSendControlMessage(session, k_EStreamControlInputKeyUp,
                                         (const ProtobufCMessage *) &message);
}
