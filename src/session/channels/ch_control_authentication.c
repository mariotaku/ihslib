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

#include <stdio.h>

#include "ch_control.h"

#include "session/frame.h"
#include "session/session_pri.h"
#include "protobuf/pb_utils.h"

static void OnAuthenticationResponse(IHS_SessionChannel *channel, const CAuthenticationResponseMsg *message);

void IHS_SessionChannelControlRequestAuthentication(IHS_SessionChannel *channel) {
    CAuthenticationRequestMsg request = CAUTHENTICATION_REQUEST_MSG__INIT;
    PROTOBUF_C_SET_VALUE(request, version, k_EStreamVersionCurrent);
    PROTOBUF_C_SET_VALUE(request, steamid, channel->session->config.steamId);
    request.has_token = true;
    static const unsigned char plain[] = {'S', 't', 'e', 'a', 'm', ' ', 'I', 'n',
                                          '-', 'H', 'o', 'm', 'e', ' ', 'S', 't',
                                          'r', 'e', 'a', 'm', 'i', 'n', 'g'};
    uint8_t token[32];
    request.token.data = token;
    request.token.len = sizeof(token);
    if (IHS_SessionFrameHMACSHA256(channel->session, plain, sizeof(plain), token, &request.token.len) != 0) {
        IHS_SessionDisconnect(channel->session);
        return;
    }
    IHS_SessionChannelControlSend(channel, k_EStreamControlAuthenticationRequest, (const ProtobufCMessage *) &request,
                                  IHS_PACKET_ID_NEXT);
}

void IHS_SessionChannelControlOnAuthentication(IHS_SessionChannel *channel, EStreamControlMessage type,
                                               IHS_Buffer *payload, const IHS_SessionPacketHeader *header) {
    IHS_UNUSED(header);
    assert(type == k_EStreamControlAuthenticationResponse);
    CAuthenticationResponseMsg *message = cauthentication_response_msg__unpack(NULL, payload->size,
                                                                               IHS_BufferPointer(payload));
    OnAuthenticationResponse(channel, message);
    cauthentication_response_msg__free_unpacked(message, NULL);
}

static void OnAuthenticationResponse(IHS_SessionChannel *channel, const CAuthenticationResponseMsg *message) {
    if (!message->has_result || message->result != CAUTHENTICATION_RESPONSE_MSG__AUTHENTICATION_RESULT__SUCCEEDED) {
        IHS_SessionLog(channel->session, IHS_LogLevelError, "Session", "Failed to authenticate");
        return;
    }
    IHS_SessionLog(channel->session, IHS_LogLevelInfo, "Session", "Authenticated");
}
