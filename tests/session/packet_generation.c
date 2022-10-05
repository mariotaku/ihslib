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

#include "session/packet.h"
#include "session/frame.h"
#include "session/channels/channel.h"
#include "session/channels/ch_discovery.h"
#include "protobuf/remoteplay.pb-c.h"
#include "session/session_pri.h"
#include "protobuf/pb_utils.h"
#include "ihs_buffer_ext.h"

static void test_frame_initialize() {
    IHS_Session session = {
            .state = {
                    .connectionId = 123,
                    .hostConnectionId = 234,
            }
    };
    IHS_SessionChannel *channel = IHS_SessionChannelDiscoveryCreate(&session);


    CDiscoveryPingResponse response = CDISCOVERY_PING_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, sequence, 12345678);
    PROTOBUF_C_SET_VALUE(response, packet_size_received, IHS_PACKET_HEADER_SIZE + 90);
    size_t msgSize = cdiscovery_ping_response__get_packed_size(&response);

    IHS_SessionFrame outFrame;
    IHS_SessionChannelInitializeFrame(channel, &outFrame, IHS_SessionPacketTypeUnconnected, true, 0);
    IHS_BufferAppendUInt8(&outFrame.body, k_EStreamDiscoveryPingResponse);
    IHS_BufferAppendUInt32LE(&outFrame.body, msgSize);
    IHS_BufferAppendMessage(&outFrame.body, (const ProtobufCMessage *) &response);

    IHS_Buffer dest = outFrame.body;
    IHS_BufferClear(&outFrame.body, false);
    assert(dest.data != NULL);
    IHS_BufferExtendSize(&dest);

    IHS_SessionPacket parsed;
    assert(IHS_SessionPacketParse(&parsed, &dest) == IHS_SessionPacketResultOK);

    IHS_BufferClear(&parsed.body, true);

    IHS_SessionChannelDestroy(channel);
}

int main(int argc, char *argv[]) {
    test_frame_initialize();
    return 0;
}