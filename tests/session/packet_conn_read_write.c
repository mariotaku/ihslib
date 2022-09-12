#include "session/packet.h"
#include "session/session_pri.h"

#include "ihs_buffer.h"
#include "session/channels/ch_discovery.h"

#include <assert.h>

int main(int argc, char *argv[]) {
    IHS_Session session = {
            .state = {
                    .connectionId = 123,
                    .hostConnectionId = 234,
            }
    };
    IHS_SessionChannel *channel = IHS_SessionChannelDiscoveryCreate(&session);
    IHS_SessionPacket packet;
    IHS_SessionChannelPacketInitialize(channel, &packet, IHS_SessionPacketTypeConnect, false, 12);
    assert(packet.header.srcConnectionId == session.state.connectionId);
    assert(packet.header.dstConnectionId == session.state.hostConnectionId);

    packet.header.sendTimestamp = IHS_SessionPacketTimestamp();

    uint8_t body[4] = {0xc7, 0x3d, 0x8f, 0x3c};
    IHS_BufferAppendMem(&packet.body, body, 4);

    IHS_Buffer dest = {.data = NULL};
    IHS_SessionPacketSerialize(&packet, &dest);
    assert(dest.data != NULL);

    IHS_SessionPacket parsed;
    assert(IHS_SessionPacketParse(&parsed, &dest) == IHS_SessionPacketResultOK);


    assert(parsed.header.packetId == packet.header.packetId);
    assert(parsed.body.size == 4);
    assert(memcmp(IHS_BufferPointer(&parsed.body), body, 4) == 0);

    IHS_BufferClear(&parsed.body, true);

    IHS_SessionChannelDestroy(channel);
    return 0;
}