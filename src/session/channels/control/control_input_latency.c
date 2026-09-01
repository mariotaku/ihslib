/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
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

uint16_t IHS_SessionSendLatencyTest(IHS_Session *session, uint32_t timestamp,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // SendLatencyTest @ 0x1f8a64 gates on IsStreaming alone — unlike every other input sender it
    // does not check BStreamingInput, because measuring latency stays useful with input disabled.
    if (session->state.connectionState != IHS_SessionConnectionStateConnected) {
        return 0;
    }
    uint16_t mark = IHS_SessionInputMarkNext(&session->inputMarks, timestamp);
    CInputLatencyTestMsg message = CINPUT_LATENCY_TEST_MSG__INIT;
    message.input_mark = mark;
    // CONCAT31(CONCAT21(CONCAT11(a, r), g), b) — alpha in the high byte, blue in the low.
    PROTOBUF_C_SET_VALUE(message, color,
                         ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b);
    if (!IHS_SessionSendControlMessage(session, k_EStreamControlInputLatencyTest,
                                       (const ProtobufCMessage *) &message)) {
        return 0;
    }
    return mark;
}
