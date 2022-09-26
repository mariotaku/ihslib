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

#pragma once

#include "channel.h"

#include "session/frame.h"
#include "session/window.h"
#include "ihs_timer.h"

#include "protobuf/remoteplay.pb-c.h"
#include "protobuf/hiddevices.pb-c.h"

typedef struct IHS_SessionChannelControl {
    IHS_SessionChannel base;
    uint64_t sendEncryptSequence;
    uint64_t recvEncryptSequence;
    IHS_SessionPacketsWindow *framePacketWindow;
    IHS_TimerTask *keepAliveTimer;
} IHS_SessionChannelControl;

IHS_SessionChannel *IHS_SessionChannelControlCreate(IHS_Session *session);

bool IHS_SessionChannelControlSend(IHS_SessionChannel *channel, EStreamControlMessage type,
                                   const ProtobufCMessage *message, int32_t packetId);

void IHS_SessionChannelControlHandshake(IHS_SessionChannel *channel, bool networkTest);


void IHS_SessionChannelControlRequestAuthentication(IHS_SessionChannel *channel);


void IHS_SessionChannelControlOnAuthentication(IHS_SessionChannel *channel, EStreamControlMessage type,
                                               IHS_Buffer *payload, const IHS_SessionPacketHeader *header);


void IHS_SessionChannelControlOnNegotiation(IHS_SessionChannel *channel, EStreamControlMessage type,
                                            IHS_Buffer *payload, const IHS_SessionPacketHeader *header);

void IHS_SessionChannelControlOnVideo(IHS_SessionChannel *channel, EStreamControlMessage type,
                                      IHS_Buffer *payload, const IHS_SessionPacketHeader *header);

void IHS_SessionChannelControlOnAudio(IHS_SessionChannel *channel, EStreamControlMessage type,
                                      IHS_Buffer *payload, const IHS_SessionPacketHeader *header);

void IHS_SessionChannelControlOnCursor(IHS_SessionChannel *channel, EStreamControlMessage type,
                                       IHS_Buffer *payload, const IHS_SessionPacketHeader *header);

void IHS_SessionChannelControlStartHeartbeat(IHS_SessionChannel *channel);

void IHS_SessionChannelControlStopHeartbeat(IHS_SessionChannel *channel);

void IHS_SessionChannelControlOnHIDMsg(IHS_SessionChannel *channel, const CHIDMessageToRemote *message);

bool IHS_SessionChannelControlSendHIDMsg(IHS_SessionChannel *channel, const CHIDMessageFromRemote *message);
