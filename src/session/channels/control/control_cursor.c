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
#include "client/client_pri.h"

void IHS_SessionChannelControlOnCursor(IHS_SessionChannel *channel, EStreamControlMessage type,
                                       const uint8_t *payload, size_t payloadLen,
                                       const IHS_SessionPacketHeader *header) {
    IHS_UNUSED(header);
    IHS_Session *session = channel->session;
    switch (type) {
        case k_EStreamControlSetCursor: {
            uint64_t cursorId;
            CSetCursorMsg *message = cset_cursor_msg__unpack(NULL, payloadLen, payload);
            cursorId = message->cursor_id;
            cset_cursor_msg__free_unpacked(message, NULL);
            const IHS_StreamInputCallbacks *cb = session->callbacks.input;
            bool requestImage = false;
            if (cb && cb->setCursor) {
                requestImage = !cb->setCursor(session, cursorId, session->callbackContexts.input);
            }
            if (requestImage) {
                CGetCursorImageMsg request = CGET_CURSOR_IMAGE_MSG__INIT;
                request.cursor_id = cursorId;
                IHS_SessionSendControlMessage(session, k_EStreamControlGetCursorImage,
                                              (const ProtobufCMessage *) &request,
                                              IHS_PACKET_ID_NEXT);
            }
            break;
        }
        case k_EStreamControlShowCursor: {
            CShowCursorMsg *message = cshow_cursor_msg__unpack(NULL, payloadLen, payload);
            const IHS_StreamInputCallbacks *cb = session->callbacks.input;
            if (cb && cb->showCursor) {
                cb->showCursor(session, message->x_normalized, message->y_normalized, session->callbackContexts.input);
            }
            cshow_cursor_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlHideCursor: {
            CHideCursorMsg *message = chide_cursor_msg__unpack(NULL, payloadLen, payload);
            const IHS_StreamInputCallbacks *cb = session->callbacks.input;
            if (cb && cb->hideCursor) {
                cb->hideCursor(session, session->callbackContexts.input);
            }
            chide_cursor_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlDeleteCursor: {
            CDeleteCursorMsg *message = cdelete_cursor_msg__unpack(NULL, payloadLen, payload);
            const IHS_StreamInputCallbacks *cb = session->callbacks.input;
            if (cb && cb->deleteCursor) {
                cb->deleteCursor(session, message->cursor_id, session->callbackContexts.input);
            }
            cdelete_cursor_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetCursorImage: {
            CSetCursorImageMsg *message = cset_cursor_image_msg__unpack(NULL, payloadLen, payload);
            const IHS_StreamInputCallbacks *cb = session->callbacks.input;
            const IHS_StreamInputCursorImage image = {
                    .cursorId = message->cursor_id,
                    .width = message->width,
                    .height = message->height,
                    .hotX = message->hot_x,
                    .hotY = message->hot_y,
                    .image = message->image.data,
                    .imageLen = message->image.len,
            };
            if (cb && cb->cursorImage) {
                cb->cursorImage(session, &image, session->callbackContexts.input);
            }
            cset_cursor_image_msg__free_unpacked(message, NULL);
            break;
        }
        default: {
            break;
        }
    }
}