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
#include "base.h"
#include "session/session_pri.h"

static uint64_t SendKeepAlive(void *data);

void IHS_SessionChannelControlStartHeartbeat(IHS_SessionChannel *channel) {
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    if (control->keepAliveTimer) return;
    control->keepAliveTimer = IHS_TimerStart(channel->session->timers, SendKeepAlive, NULL,
                                             5000, control);
}

void IHS_SessionChannelControlStopHeartbeat(IHS_SessionChannel *channel) {
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    if (!control->keepAliveTimer) return;
    IHS_TimerStop(control->keepAliveTimer);
    control->keepAliveTimer = NULL;
}

static uint64_t SendKeepAlive(void *data) {
    IHS_SessionChannel *channel = data;
    CKeepAliveMsg message = CKEEP_ALIVE_MSG__INIT;
    IHS_SessionChannelControlSend(channel, k_EStreamControlKeepAlive, (const ProtobufCMessage *) &message,
                                  IHS_PACKET_ID_NEXT);
    return 10000;
}