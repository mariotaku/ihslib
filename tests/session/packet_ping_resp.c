#include "session/packet.h"
#include "session/session_pri.h"
#include "session/channels/ch_discovery.h"

#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"
#include "protobuf/pb_utils.h"

#include <assert.h>

int main(int argc, char *argv[]) {
    IHS_Session session = {
            .state = {
                    .connectionId = 123,
                    .hostConnectionId = 234,
            }
    };
    IHS_SessionChannel *channel = IHS_SessionChannelDiscoveryCreate(&session);

    size_t packet_size_requested = 1540;

    CDiscoveryPingResponse response = CDISCOVERY_PING_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, sequence, 12345678);
    PROTOBUF_C_SET_VALUE(response, packet_size_received, IHS_PACKET_HEADER_SIZE + 90);
    size_t msgSize = cdiscovery_ping_response__get_packed_size(&response);

    IHS_SessionPacket outPacket;
    IHS_SessionChannelInitializePacket(channel, &outPacket, IHS_SessionPacketTypeUnconnected, true, 0);
    IHS_BufferAppendUInt8(&outPacket.body, k_EStreamDiscoveryPingResponse);
    IHS_BufferAppendUInt32LE(&outPacket.body, msgSize);
    IHS_BufferAppendMessage(&outPacket.body, (const ProtobufCMessage *) &response);

    IHS_SessionPacketPadTo(&outPacket, packet_size_requested);

    IHS_SessionPacketPopulateBuffer(&outPacket);
    IHS_Buffer dest = outPacket.body;
    IHS_BufferClear(&outPacket.body, false);
    assert(dest.data != NULL);
    IHS_BufferExtendSize(&dest);
    assert(dest.size == packet_size_requested + 4);

    IHS_SessionPacket parsed;
    assert(IHS_SessionPacketParse(&parsed, &dest) == IHS_SessionPacketResultOK);

    IHS_BufferClear(&parsed.body, true);

    IHS_SessionChannelDestroy(channel);
    return 0;
}