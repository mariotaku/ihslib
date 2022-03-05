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
#include "ch_data.h"
#include "ch_data_audio.h"
#include "ch_data_video.h"

void IHS_SessionChannelControlOnDataControl(IHS_SessionChannel *channel, EStreamControlMessage type,
                                            const uint8_t *payload, size_t payloadLen,
                                            const IHS_SessionPacketHeader *header) {
    IHS_Session *session = channel->session;
    switch (type) {
        case k_EStreamControlStartAudioData: {
            IHS_SessionChannel *audio = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataAudio);
            if (!audio) break;
            CStartAudioDataMsg *message = cstart_audio_data_msg__unpack(NULL, payloadLen, payload);
            audio = IHS_SessionChannelDataAudioCreate(session, message->channel);
            IHS_SessionChannelAdd(session, audio);
            cstart_audio_data_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlStopAudioData: {
            IHS_SessionChannel *audio = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataAudio);
            if (!audio) break;
            IHS_SessionChannelRemove(session, audio->id);
            break;
        }
        case k_EStreamControlStartVideoData: {
            IHS_SessionChannel *video = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataVideo);
            if (video) break;
            CStartVideoDataMsg *message = cstart_video_data_msg__unpack(NULL, payloadLen, payload);
            video = IHS_SessionChannelDataVideoCreate(session, message->channel);
            IHS_SessionChannelAdd(session, video);
            cstart_video_data_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlStopVideoData: {
            IHS_SessionChannel *video = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataVideo);
            if (!video) break;
            IHS_SessionChannelRemove(session, video->id);
            break;
        }
        default: {
            abort();
        }
    }
}