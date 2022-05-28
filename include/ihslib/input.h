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

#include <stdint.h>
#include <stddef.h>

typedef struct IHS_Session IHS_Session;

typedef struct IHS_StreamInputCursorImage {
    uint64_t cursorId;
    int32_t width;
    int32_t height;
    int32_t hotX;
    int32_t hotY;
    const uint8_t *image;
    size_t imageLen;
} IHS_StreamInputCursorImage;

typedef struct IHS_StreamInputCallbacks {
    /**
     *
     * @param session
     * @param cursorId
     * @param context
     * @return `true` if cursor image exists, `false` will cause library to request for the icon
     */
    bool (*setCursor)(IHS_Session *session, uint64_t cursorId, void *context);

    bool (*deleteCursor)(IHS_Session *session, uint64_t cursorId, void *context);

    void (*cursorImage)(IHS_Session *session, const IHS_StreamInputCursorImage *image, void *context);

    void (*showCursor)(IHS_Session *session, float x, float y, void *context);

    void (*hideCursor)(IHS_Session *session, void *context);
} IHS_StreamInputCallbacks;