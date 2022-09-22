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
#include "session/channels/ch_control.h"
#include "session/session_pri.h"

#include <stdlib.h>

void IHS_SessionChannelControlOnHIDMsg(IHS_SessionChannel *channel, const CHIDMessageToRemote *message) {
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID", "Message type: %u", message->command_case);
}

bool IHS_SessionChannelControlSendHIDMsg(IHS_SessionChannel *channel, const CHIDMessageFromRemote *message) {
    CRemoteHIDMsg wrapped = CREMOTE_HIDMSG__INIT;
    size_t messageSize = chidmessage_from_remote__get_packed_size(message);
    wrapped.has_data = true;
    wrapped.data.data = malloc(messageSize);
    wrapped.data.len = messageSize;
    chidmessage_from_remote__pack(message, wrapped.data.data);
    bool ret = IHS_SessionChannelControlSend(channel, k_EStreamControlRemoteHID, (const ProtobufCMessage *) message,
                                             IHS_PACKET_ID_NEXT);
    free(wrapped.data.data);
    return ret;
}