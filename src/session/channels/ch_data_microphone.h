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

#pragma once

#include "channel.h"
#include "protobuf/remoteplay.pb-c.h"

/**
 * Mic channel is send-only (client → server) so it doesn't reuse the receive-oriented
 * ch_data.c base class. Created when the server sends k_EStreamControlStartMicrophoneData;
 * destroyed when it sends k_EStreamControlStopMicrophoneData. The init message carries
 * the negotiated codec/freq/channel-count which we hand to the user via the registered
 * IHS_StreamMicrophoneCallbacks.start hook.
 */
IHS_SessionChannel *IHS_SessionChannelDataMicrophoneCreate(IHS_Session *session, const CStartAudioDataMsg *message);

/**
 * Wrap one user-supplied encoded mic payload in the data-frame layout Steam expects
 * (`k_EStreamDataPacket` type byte + 12-byte frame info trailer) and queue it on the
 * mic UDP channel as Unreliable. Reliability is intentional — Steam's mic path also
 * runs unreliable, voice frames are time-sensitive and a re-sent old frame is worse
 * than a dropped new one.
 *
 * Returns false if the queue refuses the packet. Safe to call from any thread.
 */
bool IHS_SessionChannelDataMicrophoneSend(IHS_SessionChannel *channel, const uint8_t *data, size_t len);
