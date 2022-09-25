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

#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>

#include "ihslib/session.h"
#include "ihslib/common.h"
#include "base.h"
#include "packet.h"
#include "session_pri.h"
#include "session/channels/channel.h"
#include "session/channels/ch_discovery.h"
#include "session/channels/ch_control.h"
#include "session/channels/ch_stats.h"
#include "crypto.h"

static void SessionRecvCallback(IHS_Base *base, const IHS_SocketAddress *address, IHS_Buffer *data);

static void SessionInitialized(IHS_Base *base, void *context);

static void SessionFinalized(IHS_Base *base, void *context);

static void SessionSendWorker(void *context);

static bool SessionDoSendPacket(IHS_SessionPacket *packet, void *context);

static const IHS_BaseRunCallbacks SessionRunCallbacks = {
        .initialized = SessionInitialized,
        .finalized = SessionFinalized,
};

IHS_Session *IHS_SessionCreate(const IHS_ClientConfig *clientConfig, const IHS_SessionInfo *sessionInfo) {
    IHS_Session *session = calloc(1, sizeof(IHS_Session));
    IHS_BaseInit(&session->base, clientConfig, SessionRecvCallback, false);
    IHS_BaseSetRunCallbacks(&session->base, &SessionRunCallbacks, NULL);
    session->info = *sessionInfo;
    session->sender = IHS_SessionPacketSenderCreate(256);
    session->timers = IHS_TimersCreate();

    session->numChannels = 3;
    session->channels[IHS_SessionChannelIdDiscovery] = IHS_SessionChannelDiscoveryCreate(session);
    session->channels[IHS_SessionChannelIdControl] = IHS_SessionChannelControlCreate(session);
    session->channels[IHS_SessionChannelIdStats] = IHS_SessionChannelStatsCreate(session);
    char *ipStr = IHS_IPAddressToString(&sessionInfo->address.ip);
    IHS_SessionLog(session, IHS_LogLevelInfo, "Session", "Session created. IP address: %s", ipStr);
    free(ipStr);
    return session;
}


bool IHS_SessionConnect(IHS_Session *session) {
    IHS_SessionLog(session, IHS_LogLevelInfo, "Session", "Starting session thread");
    // After worker ready, send connect packet
    return IHS_BaseStartWorker(&session->base, "IHSSession");
}

void IHS_SessionDisconnect(IHS_Session *session) {
    IHS_SessionChannel *discovery = IHS_SessionChannelFor(session, IHS_SessionChannelIdDiscovery);
    IHS_SessionChannelDiscoveryDisconnect(discovery);
}

void IHS_SessionThreadedJoin(IHS_Session *session) {
    IHS_BaseWaitWorker(&session->base);
}

void IHS_SessionDestroy(IHS_Session *session) {
    for (int i = 0; i < session->numChannels; ++i) {
        IHS_SessionChannelDestroy(session->channels[i]);
    }
    IHS_TimersDestroy(session->timers);
    IHS_SessionPacketSenderDestroy(session->sender);
    IHS_SessionLog(session, IHS_LogLevelInfo, "Session", "Destroying session, bye!");
    IHS_BaseDestroy(&session->base);
    free(session);
}

/*
 * Private functions
 */

void IHS_SessionInterrupt(IHS_Session *session) {
    IHS_BaseInterruptWorker(&session->base);
    for (int i = session->numChannels - 1; i >= 0; --i) {
        IHS_SessionChannelStop(session->channels[i]);
    }
}

uint32_t IHS_SessionPacketTimestamp() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    uint64_t nsec = tp.tv_nsec * 65536 / 1000000000;
    uint32_t sec = tp.tv_sec * 65536;
    return sec + nsec;
}

bool IHS_SessionQueuePacket(IHS_Session *session, IHS_SessionPacket *packet, bool retransmit) {
    assert(packet->body.offset == IHS_PACKET_HEADER_SIZE);
    assert(!packet->header.hasCrc || packet->body.suffix == 4);
    IHS_MutexLock(session->sendThreadMutex);
    bool ret = IHS_SessionPacketSenderQueue(session->sender, packet, retransmit);
    IHS_CondSignal(session->sendThreadCond);
    IHS_MutexUnlock(session->sendThreadMutex);
    return ret;
}

bool IHS_SessionSendControlMessage(IHS_Session *session, EStreamControlMessage type, const ProtobufCMessage *message) {
    IHS_SessionChannel *channel = IHS_SessionChannelFor(session, IHS_SessionChannelIdControl);
    return IHS_SessionChannelControlSend(channel, type, message, IHS_PACKET_ID_NEXT);
}

void IHS_SessionSetSessionCallbacks(IHS_Session *session, const IHS_StreamSessionCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.session = callbacks;
    session->callbackContexts.session = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetAudioCallbacks(IHS_Session *session, const IHS_StreamAudioCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.audio = callbacks;
    session->callbackContexts.audio = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetVideoCallbacks(IHS_Session *session, const IHS_StreamVideoCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.video = callbacks;
    session->callbackContexts.video = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetInputCallbacks(IHS_Session *session, const IHS_StreamInputCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.input = callbacks;
    session->callbackContexts.input = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetLogFunction(IHS_Session *session, IHS_LogFunction *logFunction) {
    IHS_BaseSetLogFunction(&session->base, logFunction);
}

const IHS_SessionInfo *IHS_SessionGetInfo(const IHS_Session *session) {
    return &session->info;
}

static void SessionRecvCallback(IHS_Base *base, const IHS_SocketAddress *address, IHS_Buffer *data) {
    (void) address;
    IHS_Session *session = (IHS_Session *) base;
    IHS_SessionPacket packet;
    IHS_SessionPacketReturn ret = IHS_SessionPacketParse(&packet, data);
    if (ret != IHS_SessionPacketResultOK) {
        return;
    }

    IHS_SessionChannelId channelId = packet.header.channelId;
    IHS_SessionPacketType packetType = packet.header.type;
    if (packetType == IHS_SessionPacketTypeACK || packetType == IHS_SessionPacketTypeNACK) {
        IHS_SessionPacketSenderRemove(session->sender, channelId, packet.header.packetId);
    }
    IHS_SessionChannel *channel = IHS_SessionChannelFor(session, channelId);
    if (!channel) {
        return;
    }
    IHS_SessionChannelReceivedPacket(channel, &packet);
    IHS_SessionPacketClear(&packet, true);
}

static void SessionInitialized(IHS_Base *base, void *context) {
    (void) context;
    IHS_Session *session = (IHS_Session *) base;
    session->sendThread = IHS_ThreadCreate(SessionSendWorker, "ihs-sess-send", session);

    if (session->callbacks.session && session->callbacks.session->connecting) {
        session->callbacks.session->connecting(session, session->callbackContexts.session);
    }

    IHS_BaseLock(&session->base);
    session->state.connectionId = IHS_CryptoRandomUInt32();
    IHS_BaseUnlock(&session->base);

    /* crc32c(b'Connect') */
    const static uint8_t body[4] = {0xc7, 0x3d, 0x8f, 0x3c};

    IHS_SessionChannel *discovery = IHS_SessionChannelFor(session, IHS_SessionChannelIdDiscovery);
    IHS_SessionPacket packet;
    IHS_SessionChannelInitializePacket(discovery, &packet, IHS_SessionPacketTypeConnect, true, 0);
    IHS_BufferAppendMem(&packet.body, body, sizeof(body));
    IHS_SessionQueuePacket(session, &packet, true);
    IHS_SessionPacketClear(&packet, true);
}

static void SessionFinalized(IHS_Base *base, void *context) {
    (void) context;
    IHS_Session *session = (IHS_Session *) base;
    IHS_ThreadJoin(session->sendThread);
    session->sendThread = NULL;
    if (session->callbacks.session && session->callbacks.session->finalized) {
        session->callbacks.session->finalized(session, session->callbackContexts.session);
    }
}

static void SessionSendWorker(void *context) {
    IHS_Session *session = (IHS_Session *) context;
    while (!session->base.interrupted) {
        IHS_TimersTick(session->timers);
        IHS_MutexLock(session->sendThreadMutex);
        while (IHS_SessionPacketSenderEmpty(session->sender)) {
            IHS_CondTimedWait(session->sendThreadCond, session->sendThreadMutex, 1000);
        }
        IHS_MutexUnlock(session->sendThreadMutex);
        IHS_SessionPacketSenderFlush(session->sender, SessionDoSendPacket, session);
    }
}

static bool SessionDoSendPacket(IHS_SessionPacket *packet, void *context) {
    IHS_Session *session = (IHS_Session *) context;
    const IHS_SessionInfo *config = &session->info;
    IHS_SessionPacketPopulateBuffer(packet);
    IHS_Buffer serialized = packet->body;
    IHS_BufferExtendSize(&serialized);
    return IHS_BaseSend(&session->base, config->address, &serialized);
}